#!/usr/bin/env python3
"""Public, read-only GitHub metadata cache. No account tokens or arbitrary proxy."""
import argparse
import json
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = 'https://api.github.com/repos/dovahkiinjeff/OpenHW'
SOURCES = {
    '/api/releases': (REPO + '/releases?per_page=100', 'releases.json'),
    '/api/update-feed': ('https://openhw.tech/updates/stable.json', 'update-feed.json'),
}
cache = {}
locks = {key: threading.Lock() for key in SOURCES}
root = None

def normalize(route, data):
    if route == '/api/releases':
        if not isinstance(data, list):
            raise ValueError('Invalid release response')
        releases = []
        for row in data:
            if row.get('draft') or row.get('prerelease'):
                continue
            releases.append(dict(version=row['tag_name'], name=row['name'], published=row['published_at'], url=row['html_url'], body=row.get('body', ''), assets=[dict(name=a['name'], size=a['size'], url=a['browser_download_url'], digest=a.get('digest')) for a in row.get('assets', [])]))
        catalog_path = root / 'data' / 'server-downloads.json'
        catalog = json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.is_file() else {}
        for release in releases:
            hosted = catalog.get(release['version'], [])
            names = {row['name'] for row in hosted}
            release['assets'] = [row for row in release['assets'] if row['name'] not in names] + hosted
        if not releases:
            raise ValueError('Empty release response')
        return releases
    if not isinstance(data, dict) or data.get('schema') != 1 or not isinstance(data.get('patches'), list):
        raise ValueError('Invalid update feed')
    return data

def refresh(route):
    try:
        req = urllib.request.Request(SOURCES[route][0], headers={'User-Agent': 'OpenHW-Website/1.0 (public release metadata)', 'Accept': 'application/json'})
        with urllib.request.urlopen(req, timeout=15) as response:
            raw = response.read(8 * 1024 * 1024 + 1)
        if len(raw) > 8 * 1024 * 1024:
            raise ValueError('Metadata exceeds limit')
        data = normalize(route, json.loads(raw))
        cache[route] = (time.time(), json.dumps(data, ensure_ascii=False, separators=(',', ':')).encode())
    except Exception as error:
        print('Metadata refresh unavailable:', route, type(error).__name__, flush=True)
        old = cache.get(route)
        if old:
            cache[route] = (time.time() - 540, old[1])
    finally:
        locks[route].release()

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        route = self.path.split('?', 1)[0]
        if route == '/api/health':
            body = b'{"status":"ok","service":"openhw-public-metadata"}'
        elif route in SOURCES:
            stamp, body = cache[route]
            if time.time() - stamp > 600 and locks[route].acquire(blocking=False):
                threading.Thread(target=refresh, args=(route,), daemon=True).start()
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'public, max-age=60, stale-while-revalidate=600')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        if args and str(args[1] if len(args) > 1 else '') != '200':
            super().log_message(fmt, *args)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--port', type=int, default=8766)
    args = parser.parse_args()
    root = args.root.resolve()
    for route, (_, filename) in SOURCES.items():
        data = json.loads((root / 'data' / filename).read_text(encoding='utf-8'))
        cache[route] = (0, json.dumps(data, ensure_ascii=False, separators=(',', ':')).encode())
    print('OpenHW public metadata listening on loopback', args.port, flush=True)
    ThreadingHTTPServer(('127.0.0.1', args.port), Handler).serve_forever()
