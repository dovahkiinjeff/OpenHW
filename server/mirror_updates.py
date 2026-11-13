"""Mirror the verified public GitHub feed/assets, then atomically publish it.

The server needs no GitHub token or repository write access. It never executes
downloaded files, and an incomplete sync leaves the previous hosted feed intact.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import sys
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))
if (Path(__file__).resolve().parents[1]/'tools').is_dir():
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from openhw_updater import FALLBACK_MANIFEST_URL, REPOSITORY_DOWNLOAD, MAX_JSON, validate_manifest, compare_versions, official_url

ROOT=Path('/srv/openhw-downloads')
QUOTA=30*1024**3


def hosted_path(url, root=ROOT):
    official_url(url)
    if not url.startswith(REPOSITORY_DOWNLOAD): raise ValueError('Canonical mirror source must be the official GitHub release')
    from urllib.parse import unquote
    relative=unquote(url[len(REPOSITORY_DOWNLOAD):])
    if not relative or any(p in ('','.','..') for p in relative.split('/')) or '\\' in relative or ':' in relative:
        raise ValueError('Unsafe mirrored asset path')
    base=(root/'downloads').resolve(); result=(base/relative).resolve()
    if not result.is_relative_to(base): raise ValueError('Mirrored file escapes download storage')
    return result


def digest(path):
    with path.open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()


def sync(root=ROOT):
    # One timer instance at a time; systemd supplies the process lock.
    request=urllib.request.Request(FALLBACK_MANIFEST_URL,headers={'User-Agent':'OpenHW-Official-Mirror/1.1','Cache-Control':'no-cache'})
    with urllib.request.urlopen(request,timeout=45) as response:data=response.read(MAX_JSON+1)
    if len(data)>MAX_JSON:raise ValueError('Feed too large')
    feed=validate_manifest(json.loads(data))
    existing=root/'updates/stable.json'
    if existing.is_file():
        previous=validate_manifest(json.loads(existing.read_text(encoding='utf-8')))
        if compare_versions(feed['latest'],previous['latest'])<0:
            raise ValueError('Upstream feed is older than the hosted release; existing feed retained')
        if feed.get('updater') and previous.get('updater') and compare_versions(feed['updater']['version'],previous['updater']['version'])<0:
            raise ValueError('Upstream updater is older than the hosted updater; existing feed retained')
    floor=feed.get('fresh_install',{}).get('version','1.0')
    rows=[r for r in (feed.get('updater'),feed.get('fresh_install')) if r]
    rows += [p for p in feed.get('patches',[]) if compare_versions(p['to'],floor)>0]
    root.mkdir(exist_ok=True,parents=True)
    for row in rows:
        target=hosted_path(row['url'],root)
        if target.is_file() and target.stat().st_size==row['size'] and digest(target)==row['sha256']:continue
        used=sum(p.stat().st_size for p in root.rglob('*') if p.is_file())
        if used+row['size']>QUOTA or shutil.disk_usage(root).free<row['size']+1024**3:
            raise ValueError('Update mirror storage limit reached; existing feed retained')
        target.parent.mkdir(parents=True,exist_ok=True);pending=target.with_suffix(target.suffix+'.pending')
        try:
            request=urllib.request.Request(row['url'],headers={'User-Agent':'OpenHW-Official-Mirror/1.1'})
            sha=hashlib.sha256(); count=0
            with urllib.request.urlopen(request,timeout=90) as incoming,pending.open('wb') as output:
                while chunk:=incoming.read(1024*1024):
                    count+=len(chunk)
                    if count>row['size']:raise ValueError('Mirror source exceeds published size')
                    output.write(chunk);sha.update(chunk)
                output.flush();os.fsync(output.fileno())
            if count!=row['size'] or sha.hexdigest()!=row['sha256']:raise ValueError('Mirror checksum mismatch')
            pending.replace(target);target.chmod(0o644)
        finally:pending.unlink(missing_ok=True)
    folder=root/'updates';folder.mkdir(exist_ok=True)
    pending=folder/'stable.json.pending'
    pending.write_bytes((json.dumps(feed,indent=2)+'\n').encode())
    pending.chmod(0o644);pending.replace(folder/'stable.json')
    print('Published official hosted feed for',feed['latest'],'with',len(rows),'verified active-branch assets')


if __name__=='__main__':sync()
