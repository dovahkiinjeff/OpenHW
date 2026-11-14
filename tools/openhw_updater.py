"""Portable, manifest-driven OpenHW updater. No game launch or system installation."""
from pathlib import Path, PurePosixPath
import collections
import contextlib
import datetime
import hashlib
import json
import os
import shutil
import stat
import struct
import subprocess
import sys
import threading
import time
import urllib.request
import urllib.parse
import uuid
import zipfile

UPDATER_VERSION = '1.1.4'
PROTOCOL = 3
MANIFEST_URL = 'https://openhw.tech/updates/stable.json'
FALLBACK_MANIFEST_URL = 'https://raw.githubusercontent.com/dovahkiinjeff/OpenHW/main/updates/stable.json'
REPOSITORY_DOWNLOAD = 'https://github.com/dovahkiinjeff/OpenHW/releases/download/'
FOOTER = struct.Struct('<16sQQ32s')
MAX_JSON = 4 * 1024 * 1024


class UpdateError(RuntimeError): pass
class Cancelled(UpdateError): pass
class CollectionRequired(UpdateError): pass
class FreshInstallRequired(UpdateError): pass


def version_key(value):
    import re
    if not isinstance(value, str) or not re.fullmatch(r'\d+(?:\.\d+){1,7}', value):
        raise UpdateError('Invalid update version: '+str(value))
    parts = [int(p) for p in value.split('.')]
    if any(p > 2147483647 for p in parts): raise UpdateError('Invalid version component')
    while len(parts) > 1 and parts[-1] == 0: parts.pop()
    return tuple(parts)


def compare_versions(left, right):
    a, b = version_key(left), version_key(right)
    length = max(len(a), len(b))
    a += (0,) * (length-len(a)); b += (0,) * (length-len(b))
    return (a > b) - (a < b)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    if Path(path).stat().st_size > MAX_JSON: raise UpdateError('Oversized JSON file')
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def atomic_json(path, value):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    pending = path.with_name(path.name+'.pending')
    with pending.open('w', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2); stream.write('\n'); stream.flush(); os.fsync(stream.fileno())
    os.replace(pending, path)


def relative_path(name):
    if not isinstance(name, str) or not name or '\x00' in name: raise UpdateError('Invalid patch path')
    name = name.replace('\\', '/')
    if name.startswith('/') or ':' in name: raise UpdateError('Absolute patch path refused')
    parts = name.split('/')
    reserved = {'con','prn','aux','nul', *(f'com{i}' for i in range(1,10)), *(f'lpt{i}' for i in range(1,10))}
    if any(p in ('', '.', '..') or p[-1:] in (' ', '.') or p.split('.')[0].lower() in reserved for p in parts):
        raise UpdateError('Unsafe patch path: '+name)
    return Path(*parts)


def inside(root, name):
    root = Path(root).resolve(); relative = relative_path(name)
    current = root
    for part in relative.parts:
        current /= part
        if current.exists() or current.is_symlink():
            info = current.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
                raise UpdateError('An update path contains a link or junction: '+str(relative))
    result = (root/relative).resolve()
    if not result.is_relative_to(root): raise UpdateError('Patch path escapes the OpenHW folder')
    return result


def writable_member(name):
    p = relative_path(name); parts = [x.lower() for x in p.parts]
    if p.suffix.lower() in ('.cfg','.rtxmap','.rtxlight','.rtxdust'):
        return False
    if parts[0] in ('savedgames','mods','modding','source','rtxexports','rtxlighting','screenshots'):
        return False
    if len(parts) == 1:
        return parts[0] in ('openhw.exe','openhw-daignostics.exe','openhw-diagnostics.exe',
            'launcher.exe','third_party_notices.md','license.txt','openhw-hw1.obig','openhw-presentation.obig') or p.suffix.lower() == '.dll'
    if parts[0] == 'development':
        return len(parts) > 2 and parts[1] in ('installer','symbols','content','setuptools')
    return parts[0] in ('data','licenses','documentation','ui','cursors','planets','asteroids',
                       'remasteredbackgrounds','subsystems','missions','misc')


def official_url(url, manifest=False):
    if not isinstance(url, str): raise UpdateError('Missing download URL')
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme != 'https' or parsed.username or parsed.password or parsed.fragment:
        raise UpdateError('Only official HTTPS downloads are supported')
    decoded = urllib.parse.unquote(parsed.path)
    if '\\' in decoded or any(part in ('.','..') for part in decoded.split('/')):
        raise UpdateError('Unsafe official download path')
    if manifest:
        valid = (parsed.netloc == 'raw.githubusercontent.com' and parsed.path.startswith('/dovahkiinjeff/OpenHW/')) or (parsed.netloc == 'openhw.tech' and parsed.path == '/updates/stable.json')
    else:
        valid = url.startswith(REPOSITORY_DOWNLOAD) or (parsed.netloc == 'openhw.tech' and parsed.path.startswith('/downloads/'))
    if not valid: raise UpdateError('The update feed points outside the official OpenHW repository')
    return url


class HttpsRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        parsed = urllib.parse.urlsplit(newurl)
        host = parsed.hostname or ''
        if parsed.scheme != 'https' or parsed.username or parsed.password or parsed.port not in (None,443) or not (host == 'github.com' or host.endswith('.githubusercontent.com') or (parsed.netloc == 'openhw.tech' and parsed.path.startswith(('/downloads/','/updates/')))):
            raise UpdateError('Unexpected download redirect')
        return super().redirect_request(request, fp, code, msg, headers, newurl)


def network_open(url, headers=None):
    request = urllib.request.Request(url, headers={'User-Agent':'OpenHW-Updater/'+UPDATER_VERSION,
        'Accept':'application/octet-stream', **(headers or {})})
    return urllib.request.build_opener(HttpsRedirects()).open(request, timeout=45)


def fetch_manifest(url=MANIFEST_URL):
    urls=[url,FALLBACK_MANIFEST_URL] if url==MANIFEST_URL else [url]
    failures=[]
    for endpoint in urls:
        official_url(endpoint, manifest=True)
        try:
            with network_open(endpoint+'?check='+str(int(time.time()))) as stream:
                data=stream.read(MAX_JSON+1)
            if len(data)>MAX_JSON: raise UpdateError('Update feed is too large')
            return validate_manifest(json.loads(data.decode('utf-8-sig')))
        except (OSError,ValueError,KeyError,UpdateError) as error:
            failures.append(str(error))
    raise UpdateError('Cannot read the OpenHW or GitHub update feed: '+'; '.join(failures))


def check_update_feed(root, component):
    """Read-only packaged-runtime check; never download or install an update."""
    import traceback
    results = []
    for endpoint in (MANIFEST_URL, FALLBACK_MANIFEST_URL):
        try:
            with network_open(endpoint) as response:
                data = response.read(MAX_JSON + 1)
            if len(data) > MAX_JSON: raise UpdateError('Update feed is too large')
            manifest = validate_manifest(json.loads(data.decode('utf-8-sig')))
            results.append(dict(endpoint=endpoint, status='passed', latest=manifest['latest']))
        except Exception as error:
            results.append(dict(endpoint=endpoint, status='failed', error=str(error), traceback=traceback.format_exc()))
    result = dict(status='passed' if all(x['status']=='passed' for x in results) else 'failed',
                  endpoints=results, gameRun=False, gameUpdated=False)
    atomic_json(inside(root, 'Development/'+component+'/update-feed-check.json'), result)
    return result


def validate_download(row):
    official_url(row.get('url'))
    mirrors=row.get('mirrors',[])
    if not isinstance(mirrors,list) or len(mirrors)>4: raise UpdateError('Invalid download mirrors')
    for mirror in mirrors: official_url(mirror)
    sha = row.get('sha256', '')
    if len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha): raise UpdateError('Missing SHA-256 in update feed')
    if not isinstance(row.get('size'), int) or not 0 < row['size'] <= 64*1024**3:
        raise UpdateError('Invalid download size')


def validate_manifest(manifest):
    if manifest.get('schema') != 1 or manifest.get('channel') != 'stable': raise UpdateError('Unsupported update feed')
    version_key(manifest['latest'])
    updater = manifest.get('updater')
    if updater:
        version_key(updater['version']); validate_download(updater)
    if manifest.get('launcher'):
        version_key(manifest['launcher']['version']); validate_download(manifest['launcher'])
    fresh=manifest.get('fresh_install')
    if fresh:
        version_key(fresh['version']); validate_download(fresh)
        if compare_versions(fresh['version'], manifest['latest']) > 0:
            raise UpdateError('Fresh installer is newer than the published release')
    patches = manifest.get('patches', [])
    if not isinstance(patches, list) or len(patches) > 4096: raise UpdateError('Invalid update chain')
    seen = set()
    for patch in patches:
        version_key(patch['to'])
        if patch['to'] in seen: raise UpdateError('Duplicate patch version')
        seen.add(patch['to']); validate_download(patch)
        if patch.get('format') != 'openhw-exe-zip-v1': raise UpdateError('This patch needs a newer updater')
        if not isinstance(patch.get('from'), list) or not patch['from']: raise UpdateError('Missing patch prerequisites')
        for prior in patch['from']:
            if compare_versions(patch['to'], prior) <= 0: raise UpdateError('Patch chain contains a downgrade')
        for action in patch.get('post_actions', []):
            if action.get('kind') not in ('repair-mission-skies-v1','verified-helper-v1'):
                raise UpdateError('A newer updater is required for this patch action')
    return manifest


def update_plan(manifest, installed):
    if compare_versions(installed, manifest['latest']) >= 0: return []
    start, goal = version_key(installed), version_key(manifest['latest'])
    pending = collections.deque([(start, [])]); visited = {start}
    while pending:
        version, route = pending.popleft()
        for patch in manifest['patches']:
            if version not in [version_key(v) for v in patch['from']]: continue
            target = version_key(patch['to'])
            if target == goal: return route+[patch]
            if target not in visited:
                visited.add(target); pending.append((target, route+[patch]))
    raise UpdateError('No supported update path from '+installed+' to '+manifest['latest'])


def detect_version(root):
    root = Path(root).resolve()
    if not (root/'setup-complete.json').is_file() or not (root/'OpenHW.exe').is_file():
        raise UpdateError('Place Updater.exe in an installed OpenHW folder beside OpenHW.exe. Install the base game first.')
    record = root/'version.json'
    if record.is_file():
        value = read_json(record).get('version'); version_key(value); return value
    candidates = [read_json(root/'setup-complete.json').get('version','1.0')]
    for path in (root/'Development/Patches').glob('*/applied.json'):
        value = read_json(path).get('version'); version_key(value); candidates.append(value)
    return max(candidates, key=version_key)


def game_running():
    if os.name != 'nt': return False
    result = subprocess.run(['tasklist','/FO','CSV','/NH'], capture_output=True, text=True,
        creationflags=subprocess.CREATE_NO_WINDOW, check=True)
    import csv
    names = {'openhw.exe','openhw-daignostics.exe','openhw-diagnostics.exe','homeworldmodern.exe'}
    return any(row and row[0].lower() in names for row in csv.reader(result.stdout.splitlines()))


def stop_if_game_running():
    if game_running(): raise UpdateError('Close OpenHW and its diagnostics version, then run the updater again.')


def launcher_running():
    if os.name != 'nt': return False
    import csv
    result = subprocess.run(['tasklist','/FO','CSV','/NH'], capture_output=True, text=True,
        creationflags=subprocess.CREATE_NO_WINDOW, check=True)
    return any(row and row[0].lower() == 'launcher.exe' for row in csv.reader(result.stdout.splitlines()))


def download(row, cache, progress, cancelled):
    validate_download(row); cache = Path(cache); cache.mkdir(parents=True, exist_ok=True)
    final = inside(cache, row['sha256']+'.exe')
    if final.is_file() and final.stat().st_size == row['size'] and digest(final) == row['sha256']: return final
    pending = inside(cache, row['sha256']+'.part')
    urls = list(dict.fromkeys(row.get('mirrors', []) + [row['url']]))
    attempts = max(3, len(urls))
    for attempt in range(attempts):
        try:
            offset = pending.stat().st_size if pending.is_file() else 0
            if offset > row['size']: pending.unlink(); offset = 0
            if offset == row['size']:
                if digest(pending) != row['sha256']: pending.unlink(); offset = 0
                else: os.replace(pending, final); return final
            headers = {'Range':f'bytes={offset}-'} if offset else {}
            with network_open(urls[min(attempt,len(urls)-1)], headers) as incoming:
                if offset and (incoming.status != 206 or not incoming.headers.get('Content-Range','').startswith(f'bytes {offset}-')):
                    offset = 0
                with pending.open('ab' if offset else 'wb') as output:
                    while True:
                        if cancelled(): raise Cancelled('Update paused. Run the updater again to resume.')
                        data = incoming.read(1024*1024)
                        if not data: break
                        output.write(data); offset += len(data)
                        if offset > row['size']: raise UpdateError('Download exceeds its published size')
                        progress('Downloading '+row.get('to','updater')+f': {offset/1024**2:.1f} / {row["size"]/1024**2:.1f} MB', offset/row['size'])
            if offset != row['size']: raise OSError('Incomplete download')
            if digest(pending) != row['sha256']:
                pending.unlink(); raise UpdateError('Download checksum failed. This patch was not installed.')
            os.replace(pending, final); return final
        except Cancelled:
            raise
        except (OSError, urllib.error.URLError, UpdateError):
            if attempt == attempts-1: raise UpdateError('Both download hosts failed or returned damaged data. No update was installed. Run the updater again to resume.')
            time.sleep(attempt+1)


def unpack_payload(package, destination, patch):
    destination = Path(destination); destination.mkdir(parents=True, exist_ok=True)
    archive_path = destination/'payload.zip'
    with Path(package).open('rb') as source:
        source.seek(0, 2); size = source.tell()
        if size < FOOTER.size: raise UpdateError('Incomplete patch executable')
        source.seek(-FOOTER.size, 2); magic, offset, count, expected = FOOTER.unpack(source.read(FOOTER.size))
        if magic.rstrip(b'\0') != b'OPENHW1SETUPZIP' or offset+count != size-FOOTER.size:
            raise UpdateError('Unsupported or damaged patch package')
        source.seek(offset); sha = hashlib.sha256()
        with archive_path.open('wb') as target:
            remaining = count
            while remaining:
                data = source.read(min(1024*1024, remaining))
                if not data: raise UpdateError('Truncated patch payload')
                sha.update(data); target.write(data); remaining -= len(data)
        if sha.digest() != expected: raise UpdateError('Patch payload checksum failed')
    manifest_name = 'Development/Patches/'+patch['to']+'/patch.json'
    with zipfile.ZipFile(archive_path) as archive:
        entries = [e for e in archive.infolist() if not e.is_dir()]
        names = [e.filename for e in entries]
        if len(names) > 100000 or len({n.lower() for n in names}) != len(names): raise UpdateError('Duplicate patch paths')
        for entry in entries:
            relative_path(entry.filename)
            if stat.S_ISLNK(entry.external_attr >> 16): raise UpdateError('Patch links are not supported')
        header = archive.getinfo(manifest_name)
        if header.file_size > MAX_JSON: raise UpdateError('Oversized patch manifest')
        manifest_bytes = archive.read(header); manifest = json.loads(manifest_bytes)
        if manifest.get('version') != patch['to']: raise UpdateError('Patch version mismatch')
        rows = manifest.get('files', [])
        paths = [row['path'] for row in rows]
        if not rows or len({n.lower() for n in paths}) != len(paths) or set(names) != {manifest_name, *paths}:
            raise UpdateError('Patch contains missing or unexpected files')
        required_space = sum(archive.getinfo(n).file_size for n in names)
        if required_space > 64*1024**3 or shutil.disk_usage(destination).free < required_space*2+64*1024**2:
            raise UpdateError('Not enough free space to stage and back up this patch')
        for row in rows:
            if not writable_member(row['path']): raise UpdateError('Patch cannot replace player files: '+row['path'])
            entry = archive.getinfo(row['path'])
            if entry.file_size != row['size']: raise UpdateError('Patch file size mismatch')
            target = inside(destination/'new', row['path']); target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as source, target.open('wb') as output: shutil.copyfileobj(source, output, 1024*1024)
            if digest(target) != row['sha256']: raise UpdateError('Patch file checksum failed: '+row['path'])
        metadata = inside(destination/'new', manifest_name); metadata.parent.mkdir(parents=True, exist_ok=True); metadata.write_bytes(manifest_bytes)
    archive_path.unlink()
    return manifest, manifest_name


def rollback(root, transaction, journal):
    for row in reversed(journal['files']):
        target = inside(root, row['path'])
        if row['existed']:
            backup = inside(transaction/'backup', row['path'])
            if not backup.is_file() or digest(backup) != row['old_sha256']:
                raise UpdateError('Update recovery needs the retained backup: '+str(transaction))
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name+'.updater-restore')
            shutil.copyfile(backup, temporary); os.replace(temporary, target)
        elif target.is_file(): target.unlink()
    journal['state'] = 'rolled-back'; atomic_json(transaction/'transaction.json', journal)


def recover(root, progress):
    transactions = inside(root, 'Development/Updater/Transactions')
    for path in sorted(transactions.glob('*/transaction.json')):
        journal = read_json(path)
        if journal.get('state') == 'applying':
            stop_if_game_running(); progress('Restoring an interrupted update...', None)
            rollback(root, path.parent, journal)


def repair_skies(root, select_collection, progress):
    from release_sky_lighting import ensure, valid_pair
    if all(valid_pair(Path(root)/'Data/Missions', m) for m in range(1,17)): return
    receipt = Path(root)/'Development/build-source-receipt.json'
    info = read_json(receipt) if receipt.is_file() else {}
    classic = Path(info.get('classic',''))/'Homeworld.big'
    selected = None
    if not classic.is_file():
        selected = select_collection()
        if not selected: raise Cancelled('The Collection folder is required to finish mission-lighting repair.')
    progress('Preparing mission lighting from your installed Collection...', None)
    ensure(root, selected=selected)


def run_actions(root, patch, rows, select_collection, progress):
    paths = {r['path']:r for r in rows}
    for action in patch.get('post_actions', []):
        if action['kind'] == 'repair-mission-skies-v1':
            repair_skies(root, select_collection, progress)
        elif action['kind'] == 'verified-helper-v1':
            name = action['path']
            if name not in paths or Path(name).suffix.lower() != '.exe': raise UpdateError('Patch helper is not verified by its payload')
            executable = inside(root, name)
            if digest(executable) != paths[name]['sha256']: raise UpdateError('Patch helper changed after verification')
            args = action.get('args', [])
            if not isinstance(args, list) or not all(isinstance(a, str) and '\x00' not in a for a in args): raise UpdateError('Invalid helper arguments')
            progress('Finishing '+patch['to']+'...', None)
            result = subprocess.run([str(executable), *args], cwd=root,
                creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0), timeout=action.get('timeout_seconds',7200),
                stdout=sys.stdout, stderr=sys.stderr)
            if result.returncode not in action.get('success_codes',[0]): raise UpdateError('Patch maintenance failed')


def apply_patch(root, patch, package, progress, select_collection=lambda: None, actions=run_actions):
    root = Path(root).resolve(); stop_if_game_running()
    installed = detect_version(root)
    if version_key(installed) not in [version_key(v) for v in patch['from']]: raise UpdateError('Installed version changed during the update')
    transaction = inside(root, 'Development/Updater/Transactions/'+uuid.uuid4().hex)
    transaction.mkdir(parents=True)
    progress('Checking patch '+patch['to']+'...', None)
    payload, metadata = unpack_payload(package, transaction, patch)
    rows = payload['files']
    if any(row['path'].lower() == 'launcher.exe' for row in rows) and launcher_running():
        raise UpdateError('This patch updates the Launcher. Close Launcher.exe and run Updater.exe beside OpenHW.exe to finish it.')
    names = [r['path'] for r in rows]+[metadata,'version.json',
        'Development/Patches/'+patch['to']+'/applied.json']
    journal = dict(schema=1, state='prepared', prior=installed, target=patch['to'], files=[])
    for name in names:
        target = inside(root, name); existed = target.is_file()
        if target.exists() and not existed: raise UpdateError('Update target is not a file: '+name)
        record = dict(path=name, existed=existed)
        if existed:
            backup = inside(transaction/'backup', name); backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(target, backup); record['old_sha256'] = digest(backup)
        journal['files'].append(record)
    for row in rows:
        target = inside(root, row['path'])
        if 'previous_sha256' in row and (digest(target) if target.is_file() else None) != row['previous_sha256']:
            raise UpdateError('Locally modified content preserved: '+row['path'])
    stop_if_game_running()
    journal['state'] = 'applying'; atomic_json(transaction/'transaction.json', journal)
    try:
        progress('Installing '+patch['to']+'...', None)
        for name in names[:-2]:
            target = inside(root, name); target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(inside(transaction/'new', name), target)
        actions(root, patch, rows, select_collection, progress)
        for row in rows:
            if digest(inside(root,row['path'])) != row['sha256']: raise UpdateError('Installed file verification failed')
        current = read_json(root/'version.json') if (root/'version.json').is_file() else {}
        hashes = {r['path']:r['sha256'] for r in rows}
        current.update(version=patch['to'], previous=installed)
        if 'OpenHW.exe' in hashes: current['normal'] = hashes['OpenHW.exe']
        if 'OpenHW-Daignostics.exe' in hashes: current['diagnostic'] = hashes['OpenHW-Daignostics.exe']
        atomic_json(root/'version.json', current)
        atomic_json(inside(root,names[-1]), dict(version=patch['to'], applied=datetime.datetime.now().isoformat(), via='Updater.exe'))
        journal['state'] = 'complete'; atomic_json(transaction/'transaction.json', journal)
    except BaseException:
        rollback(root, transaction, journal)
        raise
    return patch['to']


def request_self_update(root, candidate, bootstrap):
    root = Path(root).resolve(); bootstrap = Path(bootstrap).resolve()
    if bootstrap.parent != root or bootstrap.suffix.lower() != '.exe': raise UpdateError('Cannot locate this updater executable')
    helper = inside(root,'Development/Updater/self-update.ps1')
    helper.write_text('''param([int]$EngineId,[int]$BootstrapId,[string]$Candidate,[string]$Target,[string]$Root)
$ErrorActionPreference='Stop'
try {
  foreach($processId in @($EngineId,$BootstrapId)) {
    if($processId -le 0) { continue }
    while(Get-Process -Id $processId -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 150 }
  }
  $pending=$Target+'.new'
  Copy-Item -LiteralPath $Candidate -Destination $pending -Force
  Move-Item -LiteralPath $pending -Destination $Target -Force
  Start-Process -FilePath $Target -WorkingDirectory $Root -ArgumentList @('--launcher-update') -WindowStyle Hidden
} catch { $_ | Out-File -LiteralPath (Join-Path $Root 'Development/Logs/updater-self-update.log') }
''', encoding='utf-8')
    powershell = Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    subprocess.Popen([str(powershell),'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(helper),
        '-EngineId',str(os.getpid()),'-BootstrapId',os.environ.get('OPENHW_UPDATER_BOOTSTRAP_PID','0'),
        '-Candidate',str(candidate),'-Target',str(bootstrap),'-Root',str(root)],
        creationflags=subprocess.CREATE_NO_WINDOW, cwd=root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


@contextlib.contextmanager
def update_lock(root):
    folder = inside(root,'Development/Updater'); folder.mkdir(parents=True, exist_ok=True)
    stream = (folder/'update.lock').open('a+b')
    try:
        if os.name == 'nt':
            import msvcrt
            stream.seek(0); stream.write(b'0'); stream.flush(); stream.seek(0)
            try: msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError: raise UpdateError('Another OpenHW updater is already running in this folder')
        yield
    finally:
        stream.close()


def update(root, progress, cancelled=lambda: False, select_collection=lambda: None, manifest=None, self_update=True):
    root = Path(root).resolve(); detect_version(root); stop_if_game_running()
    with update_lock(root):
        recover(root, progress)
        progress('Checking official OpenHW releases...', None)
        manifest = manifest if manifest is not None else fetch_manifest()
        # Self-update before validating actions introduced by a newer protocol.
        if manifest.get('schema') != 1: raise UpdateError('Unrecognized update feed')
        own = manifest.get('updater')
        if self_update and own and compare_versions(own['version'], UPDATER_VERSION) > 0:
            validate_download(own)
            candidate = download(own, inside(root,'Development/Updater/Downloads'), progress, cancelled)
            request_self_update(root, candidate, os.environ.get('OPENHW_UPDATER_EXE', str(root/'Updater.exe')))
            return 'restart'
        if manifest.get('minimum_protocol',1) > PROTOCOL: raise UpdateError('A newer updater is required; see the official release page')
        validate_manifest(manifest)
        installed = detect_version(root)
        fresh=manifest.get('fresh_install')
        if fresh and compare_versions(installed,fresh['version'])<0:
            raise FreshInstallRequired('OpenHW '+fresh['version']+' requires a NEW installation in a separate empty folder. '
                'Your existing game, saves and settings have not been replaced. Download the new installer from '
                'https://openhw.tech or https://github.com/dovahkiinjeff/OpenHW/releases/latest. '
                'After installing, use the Launcher and Updater supplied with the new installation.')
        route = update_plan(manifest, installed)
        for index, patch in enumerate(route, 1):
            if cancelled(): raise Cancelled('Update paused. Run the updater again to continue.')
            progress(f'Patch {index} of {len(route)}: {installed} → {patch["to"]}', None)
            package = download(patch, inside(root,'Development/Updater/Downloads'), progress, cancelled)
            if cancelled(): raise Cancelled('Update paused. Run the updater again to continue.')
            installed = apply_patch(root, patch, package, progress, select_collection)
        install_launcher_component(root, manifest, progress, cancelled)
        return installed


def install_launcher_component(root, manifest, progress, cancelled=lambda: False):
    row = manifest.get('launcher')
    if not row: return
    validate_download(row)
    target = inside(root, 'Launcher.exe')
    if target.is_file() and digest(target) == row['sha256']: return
    if launcher_running(): raise UpdateError('The Launcher has not finished its automatic restart handoff yet.')
    candidate = download(row, inside(root, 'Development/Updater/Downloads'), progress, cancelled)
    if cancelled(): raise Cancelled('Update cancelled before replacing the Launcher')
    if digest(candidate) != row['sha256']: raise UpdateError('Launcher download failed verification')
    if target.exists():
        backup = inside(root, 'Development/Updater/ComponentBackups/'+digest(target)+'/Launcher.exe')
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists(): shutil.copyfile(target, backup)
    pending = inside(root, 'Launcher.exe.update-pending')
    try:
        shutil.copyfile(candidate, pending)
        if digest(pending) != row['sha256']: raise UpdateError('Launcher copy failed verification')
        os.replace(pending, target)
    finally: pending.unlink(missing_ok=True)


def run_gui(root, manifest=None):
    import queue
    import tkinter as tk
    from tkinter import filedialog, ttk
    root = Path(root).resolve(); temporary = inside(root,'Development/Temp'); temporary.mkdir(parents=True, exist_ok=True)
    for key in ('TEMP','TMP','TMPDIR'): os.environ[key] = str(temporary)
    os.environ['HW_PORTABLE_ROOT'] = str(root); os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
    logs = inside(root,'Development/Logs'); logs.mkdir(parents=True, exist_ok=True)
    log = (logs/'updater.log').open('a', encoding='utf-8', buffering=1)
    messages = queue.Queue(); responses = queue.Queue(); cancel = threading.Event()
    window = tk.Tk(); window.title('OpenHW Updater'); window.geometry('720x300'); window.configure(bg='#0d171c')
    style = ttk.Style(); style.theme_use('clam')
    style.configure('OpenHW.Horizontal.TProgressbar', background='#dfa934', troughcolor='#1b2c34', bordercolor='#526a73')
    tk.Label(window,text='OPENHW / AUTOMATIC UPDATE',bg='#0d171c',fg='#dfa934',font=('Segoe UI',17,'bold')).pack(anchor='w',padx=24,pady=(18,8))
    tk.Label(window,text=str(root),bg='#0d171c',fg='#aab9bb',wraplength=670,justify='left').pack(anchor='w',padx=24)
    status = tk.StringVar(value='Checking your installation...')
    tk.Label(window,textvariable=status,bg='#0d171c',fg='#e4e8e7',wraplength=670,justify='left',height=3).pack(fill='x',padx=24,pady=12)
    bar = ttk.Progressbar(window,style='OpenHW.Horizontal.TProgressbar',maximum=100,mode='indeterminate'); bar.pack(fill='x',padx=24); bar.start()
    finished = False
    def progress(text, value=None): messages.put(('progress',text,value)); log.write(text+'\n')
    class Output:
        def write(self, text):
            log.write(text)
            if text.strip(): messages.put(('progress',text.strip()[-500:],None))
        def flush(self): log.flush()
        def fileno(self): return log.fileno()
    def select_collection():
        messages.put(('collection','',None))
        return responses.get()
    def worker():
        with contextlib.redirect_stdout(Output()), contextlib.redirect_stderr(Output()):
            try:
                value = update(root, progress, cancel.is_set, select_collection, manifest=manifest, self_update=manifest is None)
                messages.put(('restart' if value == 'restart' else 'done',
                    'Restarting the updated updater...' if value == 'restart' else 'OpenHW '+value+' is up to date. You can start the game.', None))
            except FreshInstallRequired as error:
                messages.put(('fresh',str(error),None))
            except Exception as error:
                import traceback
                traceback.print_exc(); messages.put(('error',str(error),None))
    def close():
        if finished: window.destroy()
        else: cancel.set(); status.set('Stopping after the current safe step...')
    def poll():
        nonlocal finished
        while not messages.empty():
            kind, text, value = messages.get()
            if kind == 'collection':
                responses.put(filedialog.askdirectory(parent=window,title='Select your Homeworld Remastered Collection folder to repair missing mission skies'))
                continue
            status.set(text)
            if kind == 'progress':
                if value is None:
                    if str(bar['mode']) != 'indeterminate': bar.configure(mode='indeterminate'); bar.start()
                else:
                    bar.stop(); bar.configure(mode='determinate'); bar['value'] = max(0,min(100,value*100))
            else:
                finished = True; bar.stop(); bar.configure(mode='determinate'); bar['value'] = 100 if kind == 'done' else 0
                if kind == 'restart': window.after(150,window.destroy)
                else:
                    if kind == 'fresh':
                        tk.Button(window,text='Open 1.1 downloads',command=lambda:os.startfile('https://openhw.tech/#/download'),bg='#dfa934',fg='#0d171c',padx=18).pack(pady=4)
                    tk.Button(window,text='Close',command=close,bg='#24363e',fg='#e4e8e7',padx=28).pack(pady=8)
        if not finished: window.after(100,poll)
    window.protocol('WM_DELETE_WINDOW',close)
    threading.Thread(target=worker,daemon=False).start(); window.after(100,poll); window.mainloop(); log.close()


def main(bundled_manifest=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--verify-payload',action='store_true')
    parser.add_argument('--check-update-feed',action='store_true')
    parser.add_argument('--launcher-update',action='store_true')
    parser.add_argument('--wait-engine',type=int,default=0)
    parser.add_argument('--wait-bootstrap',type=int,default=0)
    args = parser.parse_args()
    if args.check_update_feed:
        check_update_feed(args.root, 'Updater')
    elif args.verify_payload:
        import ssl
        from release_sky_lighting import ensure
        if args.launcher_update:
            from openhw_launcher_ui import wait_for_launcher_exit
            wait_for_launcher_exit([args.wait_engine,args.wait_bootstrap])
        atomic_json(args.root/'Development/Updater/self-check.json', dict(status='passed', version=UPDATER_VERSION,
            protocol=PROTOCOL, tls=ssl.OPENSSL_VERSION, noNetwork=True, noGameUpdate=True, handoff=args.launcher_update, wait_engine=args.wait_engine))
    else:
        from openhw_launcher_ui import run_update
        bootstrap_feed=None
        if bundled_manifest and not args.launcher_update:
            candidate=validate_manifest(read_json(inside(args.root,bundled_manifest)))
            installed=detect_version(args.root)
            component=candidate.get('launcher')
            needs_repair=component and (not (args.root/'Launcher.exe').is_file() or digest(args.root/'Launcher.exe')!=component['sha256'])
            if compare_versions(installed,candidate['latest'])<0 or (compare_versions(installed,candidate['latest'])==0 and needs_repair):bootstrap_feed=candidate
        run_update(args.root, [args.wait_engine,args.wait_bootstrap], bootstrap_feed)

