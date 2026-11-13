"""Private, bounded diagnostics ZIP intake. No extraction or public retrieval.

Run behind the exact Caddy route in configure_diagnostics.py. Only that local
proxy may supply X-Forwarded-For; it overwrites the header with its remote IP.
"""
from collections import deque
from dataclasses import dataclass
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import hashlib
import ipaddress
import json
import logging
import os
from pathlib import Path
import re
import shutil
import socket
import stat
import struct
import tempfile
import threading
import time
import unicodedata
import uuid
import zipfile
import zlib

ENDPOINT = '/diagnostics/v1/reports'
MIB = 1024 * 1024
MAX_METADATA = 64 * 1024
ALLOWED_LOG_EXTENSIONS = {'.log', '.txt', '.json', '.dmp'}
ALLOWED_SAVE_EXTENSIONS = {'', '.sav', '.save', '.game'}
LOG = logging.getLogger('openhw.diagnostics')


@dataclass(frozen=True)
class Limits:
    body_bytes: int = 64 * MIB
    expanded_bytes: int = 256 * MIB
    members: int = 64
    storage_bytes: int = 10 * 1024 * MIB
    free_bytes: int = 1024 * MIB
    upload_seconds: float = 90
    validation_seconds: float = 30
    socket_seconds: float = 15
    connections: int = 4
    uploads: int = 2
    per_ip_requests: int = 6
    global_requests: int = 60
    rate_seconds: float = 600
    rate_keys: int = 4096


class ReportError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


def _json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ReportError(400, 'Duplicate JSON key')
        result[key] = value
    return result


def _member_name(name):
    if (not isinstance(name, str) or not name or len(name) > 240 or
            any(ord(ch) < 32 for ch in name) or '\\' in name or
            any(ch in name for ch in '<>:"|?*')):
        raise ReportError(400, 'Unsafe archive member name')
    parts = name.split('/')
    if any(part in ('', '.', '..') or part.endswith((' ', '.')) for part in parts):
        raise ReportError(400, 'Unsafe archive member path')
    if name == 'report.json':
        return name
    if len(parts) != 2:
        raise ReportError(400, 'Only flat logs/ and saves/ members are accepted')
    extension = Path(parts[1]).suffix.lower()
    if ((parts[0] == 'logs' and extension in ALLOWED_LOG_EXTENSIONS) or
            (parts[0] == 'saves' and extension in ALLOWED_SAVE_EXTENSIONS)):
        return name
    raise ReportError(400, 'Unsupported report file type')


def validate_archive(path, limits):
    """Stream every member for CRC/bounds validation; never extract a path."""
    deadline = time.monotonic() + limits.validation_seconds
    with path.open('rb') as source:
        if source.read(4) != b'PK\x03\x04':
            raise ReportError(400, 'Not a supported ZIP archive')
        # Bound central-directory parsing BEFORE ZipFile creates ZipInfo
        # objects. A tiny-entry archive can otherwise allocate millions of
        # objects even though its HTTP body fits the compressed upload cap.
        source.seek(0, os.SEEK_END)
        size = source.tell()
        source.seek(max(0, size - 65557))
        tail = source.read(65557)
        end = tail.rfind(b'PK\x05\x06')
        if end < 0 or len(tail) - end < 22:
            raise ReportError(400, 'ZIP directory is missing')
        _, disk, central_disk, disk_entries, entries, central_size, central_offset, comment_size = struct.unpack_from('<4s4H2IH', tail, end)
        end_offset = size - len(tail) + end
        if (disk or central_disk or disk_entries != entries or not 2 <= entries <= limits.members or
                central_size > limits.members * 4096 or central_offset + central_size != end_offset or
                len(tail) - end != 22 + comment_size):
            raise ReportError(400, 'ZIP directory exceeds supported bounds')
    try:
        with zipfile.ZipFile(path) as archive:
            members = archive.infolist()
            if not 2 <= len(members) <= limits.members:
                raise ReportError(400, 'Invalid archive member count')
            names, actual, declared, total, metadata = set(), {}, 0, 0, None
            for member in members:
                name = _member_name(member.orig_filename)
                key = unicodedata.normalize('NFC', name).casefold()
                mode = member.external_attr >> 16
                if (key in names or member.is_dir() or stat.S_IFMT(mode) not in (0, stat.S_IFREG)
                        or member.flag_bits & 1 or member.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED)):
                    raise ReportError(400, 'Duplicate, encrypted or unsupported ZIP member')
                names.add(key)
                if member.file_size < 0 or member.compress_size < 0:
                    raise ReportError(400, 'Invalid ZIP size')
                declared += member.file_size
                if declared > limits.expanded_bytes:
                    raise ReportError(413, 'Expanded report exceeds the limit')
                if name == 'report.json' and member.file_size > MAX_METADATA:
                    raise ReportError(413, 'Report metadata is too large')
                count, chunks = 0, []
                with archive.open(member) as stream:
                    while chunk := stream.read(64 * 1024):
                        if time.monotonic() > deadline:
                            raise ReportError(408, 'ZIP validation deadline exceeded')
                        count += len(chunk)
                        total += len(chunk)
                        if total > limits.expanded_bytes or count > member.file_size:
                            raise ReportError(413, 'Expanded report exceeds its declared bounds')
                        if name == 'report.json':
                            chunks.append(chunk)
                if count != member.file_size:
                    raise ReportError(400, 'ZIP member size mismatch')
                if name == 'report.json':
                    try:
                        metadata = json.loads(b''.join(chunks).decode('utf-8'), object_pairs_hook=_json_object)
                    except (UnicodeError, ValueError, RecursionError) as error:
                        raise ReportError(400, 'Invalid report metadata') from error
                else:
                    actual[name] = count
            if not isinstance(metadata, dict) or type(metadata.get('schema')) is not int or metadata['schema'] != 1:
                raise ReportError(400, 'Unsupported report schema')
            if set(metadata) - {'schema', 'alias', 'comment', 'files', 'created_utc', 'game_version'}:
                raise ReportError(400, 'Unsupported metadata fields')
            alias, comment = metadata.get('alias'), metadata.get('comment')
            if (not isinstance(alias, str) or not alias.strip() or len(alias) > 80 or
                    any(ord(ch) < 32 for ch in alias) or not isinstance(comment, str) or len(comment) > 4000 or '\0' in comment):
                raise ReportError(400, 'Invalid community alias or comment')
            metadata['alias'] = alias.strip()
            if 'game_version' in metadata and (not isinstance(metadata['game_version'], str) or
                    len(metadata['game_version']) > 32 or any(ord(ch) < 32 for ch in metadata['game_version'])):
                raise ReportError(400, 'Invalid game version')
            if 'created_utc' in metadata:
                try:
                    value = metadata['created_utc']
                    if not isinstance(value, str) or len(value) > 64:
                        raise ValueError()
                    parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
                    if parsed.utcoffset() is None or parsed.utcoffset().total_seconds() != 0:
                        raise ValueError()
                except (ValueError, TypeError) as error:
                    raise ReportError(400, 'Invalid creation timestamp') from error
            entries = metadata.get('files')
            if not isinstance(entries, list) or not 1 <= len(entries) <= limits.members - 1:
                raise ReportError(400, 'Invalid report file inventory')
            inventory = {}
            for entry in entries:
                if not isinstance(entry, dict) or set(entry) != {'path', 'size'}:
                    raise ReportError(400, 'Invalid report file inventory')
                name = _member_name(entry['path'])
                if name == 'report.json' or name in inventory or type(entry['size']) is not int or entry['size'] < 0:
                    raise ReportError(400, 'Invalid report file inventory')
                inventory[name] = entry['size']
            if inventory != actual or sum(name.startswith('logs/') for name in inventory) > 60 or sum(name.startswith('saves/') for name in inventory) > 1:
                raise ReportError(400, 'Report inventory does not match ZIP contents')
            return metadata
    except (zipfile.BadZipFile, NotImplementedError, RuntimeError, EOFError, OSError, zlib.error) as error:
        raise ReportError(400, 'Invalid or damaged ZIP archive') from error


class IntakeServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True
    request_queue_size = 8

    def __init__(self, address, root, limits=None):
        if not ipaddress.ip_address(address[0]).is_loopback:
            raise ValueError('Diagnostics intake must bind to loopback')
        self.root = Path(root).absolute()
        self.root.mkdir(parents=True, exist_ok=True, mode=0o700)
        if self.root.is_symlink() or not self.root.is_dir():
            raise ValueError('Report storage must be a real private directory')
        self.limits = limits or Limits()
        self._connections = threading.BoundedSemaphore(self.limits.connections)
        self._uploads = threading.BoundedSemaphore(self.limits.uploads)
        self._lock = threading.Lock()
        self._reserved = 0
        self._rates, self._global_rate = {}, deque()
        super().__init__(address, IntakeHandler)

    def process_request(self, request, client_address):
        if not self._connections.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self._connections.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self._connections.release()

    def admit(self, client, size):
        now = time.monotonic()
        with self._lock:
            cutoff = now - self.limits.rate_seconds
            self._rates = {key: values for key, values in self._rates.items() if values and values[-1] > cutoff}
            for values in (self._global_rate, self._rates.setdefault(client, deque())):
                while values and values[0] <= cutoff:
                    values.popleft()
            values = self._rates[client]
            if (len(values) >= self.limits.per_ip_requests or len(self._global_rate) >= self.limits.global_requests or
                    len(self._rates) > self.limits.rate_keys):
                raise ReportError(429, 'Report rate limit reached; try again later')
            values.append(now)
            self._global_rate.append(now)
            used = 0
            # Server-owned report directories only, bounded depth; never follow symlinks.
            for item in os.scandir(self.root):
                if item.is_file(follow_symlinks=False):
                    used += item.stat(follow_symlinks=False).st_size
                elif item.is_dir(follow_symlinks=False):
                    for child in os.scandir(item.path):
                        if child.is_file(follow_symlinks=False):
                            used += child.stat(follow_symlinks=False).st_size
            reservation = size + MAX_METADATA
            if (used + self._reserved + reservation > self.limits.storage_bytes or
                    shutil.disk_usage(self.root).free - self._reserved - reservation < self.limits.free_bytes):
                raise ReportError(507, 'Private report storage is currently full')
            self._reserved += reservation
            return reservation

    def release(self, reservation):
        with self._lock:
            self._reserved -= reservation


class IntakeHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    server_version = 'OpenHWReports/1'
    sys_version = ''

    def setup(self):
        super().setup()
        self.connection.settimeout(self.server.limits.socket_seconds)

    def handle_one_request(self):
        # Socket timeouts alone reset on each received byte. Bound the entire
        # header phase as well so trickled headers cannot occupy every slot.
        self._headers_expired = False
        self._header_timer = threading.Timer(self.server.limits.socket_seconds, self.expire_headers)
        self._header_timer.daemon = True
        self._header_timer.start()
        try:
            super().handle_one_request()
        except OSError:
            if not self._headers_expired:
                raise
        finally:
            self._header_timer.cancel()

    def expire_headers(self):
        self._headers_expired = True
        try:
            self.connection.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

    def parse_request(self):
        try:
            valid = super().parse_request()
            return valid and not self._headers_expired
        finally:
            self._header_timer.cancel()

    def log_message(self, fmt, *args):
        LOG.info('%s %s', self.client_address[0], fmt % args)

    def reply(self, status, **body):
        payload = (json.dumps(body, ensure_ascii=True) + '\n').encode('utf-8')
        self.close_connection = True
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(payload)))
        self.send_header('Connection', 'close')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        if status == 429:
            self.send_header('Retry-After', str(int(self.server.limits.rate_seconds)))
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(payload)

    def do_GET(self):
        self.reply(404, error='No public report retrieval')

    do_HEAD = do_GET

    def client_identity(self):
        peer = ipaddress.ip_address(self.client_address[0])
        forwarded = self.headers.get_all('X-Forwarded-For', [])
        if peer.is_loopback and len(forwarded) == 1 and len(forwarded[0]) <= 64:
            try:
                peer = ipaddress.ip_address(forwarded[0].strip())
            except ValueError:
                pass
        return str(peer.ipv4_mapped if isinstance(peer, ipaddress.IPv6Address) and peer.ipv4_mapped else peer)

    def body_size(self):
        if self.path != ENDPOINT:
            raise ReportError(404, 'Unknown diagnostics endpoint')
        if self.headers.get('Content-Type', '').split(';')[0].strip().lower() != 'application/zip':
            raise ReportError(415, 'Expected application/zip')
        if self.headers.get('Transfer-Encoding') or self.headers.get('Content-Encoding', 'identity').lower() != 'identity':
            raise ReportError(400, 'Transfer or content encoding is not supported')
        values = self.headers.get_all('Content-Length', [])
        if len(values) != 1 or not re.fullmatch(r'[0-9]{1,12}', values[0]):
            raise ReportError(411, 'One valid Content-Length is required')
        size = int(values[0])
        if not 1 <= size <= self.server.limits.body_bytes:
            raise ReportError(413, 'Compressed report exceeds 64 MiB')
        return size

    def handle_expect_100(self):
        try:
            self.body_size()
        except ReportError as error:
            self.reply(error.status, error=error.message)
            return False
        self.send_response_only(100)
        self.end_headers()
        return True

    def do_POST(self):
        reservation, staging, completed = 0, None, None
        acquired = False
        try:
            size = self.body_size()
            acquired = self.server._uploads.acquire(blocking=False)
            if not acquired:
                raise ReportError(503, 'Report intake is busy; try again shortly')
            reservation = self.server.admit(self.client_identity(), size)
            fd, temporary = tempfile.mkstemp(prefix='.upload-', suffix='.zip', dir=self.server.root)
            staging = Path(temporary)
            deadline = time.monotonic() + self.server.limits.upload_seconds
            digest = hashlib.sha256()
            with os.fdopen(fd, 'wb') as destination:
                remaining = size
                while remaining:
                    if time.monotonic() > deadline:
                        raise ReportError(408, 'Upload deadline exceeded')
                    self.connection.settimeout(min(self.server.limits.socket_seconds, max(.01, deadline - time.monotonic())))
                    chunk = self.rfile.read1(min(64 * 1024, remaining))
                    if not chunk:
                        raise ReportError(400, 'Incomplete upload')
                    destination.write(chunk)
                    digest.update(chunk)
                    remaining -= len(chunk)
                destination.flush()
                os.fsync(destination.fileno())
            metadata = validate_archive(staging, self.server.limits)
            timestamp = datetime.now(timezone.utc)
            alias = re.sub(r'[^A-Za-z0-9_-]+', '-', unicodedata.normalize('NFKD', metadata['alias']).encode('ascii', 'ignore').decode()).strip('-_')[:32] or 'reporter'
            report_id = alias + '-' + timestamp.strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex[:16]
            completed = self.server.root / report_id
            completed.mkdir(mode=0o700)
            record = dict(metadata, report_id=report_id, received_utc=timestamp.isoformat(), archive_sha256=digest.hexdigest())
            with (completed / 'report.json').open('x', encoding='utf-8') as output:
                json.dump(record, output, ensure_ascii=True, indent=2)
                output.write('\n')
            staging.replace(completed / 'report.zip')
            staging = None
            completed = None
            LOG.info('Accepted private diagnostics report %s (%d bytes)', report_id, size)
            self.reply(201, report_id=report_id)
        except ReportError as error:
            self.reply(error.status, error=error.message)
        except (TimeoutError, socket.timeout):
            self.reply(408, error='Upload timed out')
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception:
            LOG.exception('Diagnostics intake failed')
            try:
                self.reply(500, error='Report intake failed')
            except OSError:
                pass
        finally:
            if staging is not None:
                staging.unlink(missing_ok=True)
            if completed is not None:
                # Only the private directory created by this request is removed.
                for name in ('report.json', 'report.zip'):
                    (completed / name).unlink(missing_ok=True)
                completed.rmdir()
            if reservation:
                self.server.release(reservation)
            if acquired:
                self.server._uploads.release()


def main():
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(name)s %(message)s')
    root = os.environ.get('OPENHW_REPORTS_ROOT', '/srv/openhw-reports')
    server = IntakeServer(('127.0.0.1', 8768), root)
    LOG.info('Private diagnostics intake listening on 127.0.0.1:8768')
    try:
        server.serve_forever()
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
