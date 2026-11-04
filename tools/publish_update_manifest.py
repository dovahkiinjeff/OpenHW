"""Generate the portable update feed from published GitHub release assets."""
from pathlib import Path
import argparse
import base64
import json
import os
import re
import urllib.request
import urllib.error
from openhw_updater import version_key, validate_manifest, official_url

REPO = 'dovahkiinjeff/OpenHW'
API = 'https://api.github.com/repos/'+REPO


def api(path, method='GET', data=None):
    headers = {'User-Agent':'OpenHW-update-feed','Accept':'application/vnd.github+json'}
    token = os.environ.get('GITHUB_TOKEN') or os.environ.get('GH_TOKEN')
    if token: headers['Authorization']='Bearer '+token
    request = urllib.request.Request(API+path, headers=headers, method=method,
        data=json.dumps(data).encode() if data is not None else None)
    with urllib.request.urlopen(request, timeout=45) as response:
        return json.load(response)


def public_json(asset):
    official_url(asset['browser_download_url'])
    request = urllib.request.Request(asset['browser_download_url'],headers={'User-Agent':'OpenHW-update-feed'})
    with urllib.request.urlopen(request,timeout=45) as response:
        data = response.read(1024*1024+1)
    if len(data)>1024*1024: raise ValueError('Release metadata is oversized')
    return json.loads(data)


def generate(releases, metadata=public_json, prior=None):
    games = []; updater = (prior or {}).get('updater')
    for release in releases:
        if release.get('draft') or release.get('prerelease'): continue
        assets = {a['name']:a for a in release.get('assets',[]) if a.get('state','uploaded')=='uploaded'}
        if 'OpenHW-Updater.json' in assets:
            candidate = metadata(assets['OpenHW-Updater.json'])
            if not updater or version_key(candidate['version']) >= version_key(updater['version']): updater=candidate
        match = re.fullmatch(r'v(\d+(?:\.\d+)+)',release['tag_name'])
        if not match: continue
        version = match[1]; name = 'OpenHW-Patch-'+version+'.exe'
        if name not in assets: continue
        asset = assets[name]; checksum = asset.get('digest','')
        if not checksum.startswith('sha256:'): raise ValueError('Release asset lacks a GitHub SHA-256: '+name)
        details = metadata(assets['OpenHW-Update.json']) if 'OpenHW-Update.json' in assets else {}
        games.append((version,asset,details))
    games.sort(key=lambda item:version_key(item[0]))
    if not games: raise ValueError('No published OpenHW patches found')
    patches = []; previous = '1.0'; minimum=1
    for version, asset, details in games:
        row = dict(to=version, **{'from':details.get('from',[previous])},
            format=details.get('format','openhw-exe-zip-v1'),
            url=asset['browser_download_url'],size=asset['size'],sha256=asset['digest'].split(':',1)[1],
            post_actions=details.get('post_actions',[{'kind':'repair-mission-skies-v1'}]))
        patches.append(row); previous=version; minimum=max(minimum,details.get('minimum_protocol',1))
    feed = dict(schema=1,channel='stable',latest=previous,minimum_protocol=minimum,patches=patches)
    if updater: feed['updater']=updater
    validate_manifest(feed)
    return feed


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('updates/stable.json'))
    parser.add_argument('--publish',action='store_true')
    args=parser.parse_args()
    releases=[]; page=1
    while True:
        rows=api('/releases?per_page=100&page='+str(page)); releases.extend(rows)
        if len(rows)<100: break
        page+=1
    prior=json.loads(args.output.read_text()) if args.output.is_file() else None
    feed=generate(releases,prior=prior); data=(json.dumps(feed,indent=2)+'\n').encode()
    args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(data)
    if args.publish:
        path='/contents/updates/stable.json'; current=None
        try: current=api(path+'?ref=main')
        except urllib.error.HTTPError as error:
            if error.code!=404: raise
        existing=base64.b64decode(current['content']) if current else None
        if existing!=data:
            request=dict(message='Update portable updater release feed',content=base64.b64encode(data).decode(),branch='main')
            if current: request['sha']=current['sha']
            api(path,method='PUT',data=request)
    print(json.dumps(dict(latest=feed['latest'],patches=len(feed['patches']),output=str(args.output),published=args.publish)))


if __name__=='__main__': main()
