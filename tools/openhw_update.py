"""Portable local update packages, with transactional backup and rollback."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import zipfile
from extract_remastered_big import safe_path


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def allowed(name):
    path=safe_path(name)
    parts=[part.lower() for part in path.parts]
    if parts[0] in ('savedgames','modding','mods','logs','diagnostics','setupsources','setupwork','updates','shadercache'):
        return False
    if len(parts)==1:
        return parts[0] in ('openhw.exe','openhw.pdb','openhw-hw1.obig','openhw-presentation.obig','version.json') or path.suffix.lower()=='.dll'
    # Only shipped native definitions, never arbitrary loose data or mods.
    if parts[0]=='data':
        return (len(parts)==5 and parts[1] in ('r1','r2','p1','p2','p3','traders')
                and parts[3:] == ['rmnative','ship.owshipdef'])
    # Packaged game artwork and documentation can receive updates. Authored
    # mission settings/configuration always remain user-owned and excluded.
    return parts[0] in ('documentation','licenses','ui','cursors','planets','asteroids','remasteredbackgrounds','subsystems') and path.suffix.lower() not in ('.cfg','.rtxmap','.rtxlight','.rtxdust')


def game_running():
    if os.name!='nt':return False
    flags=getattr(subprocess,'CREATE_NO_WINDOW',0)
    result=subprocess.run(['tasklist','/FI','IMAGENAME eq OpenHW.exe','/FO','CSV','/NH'],capture_output=True,text=True,creationflags=flags,check=True)
    return '"openhw.exe"' in result.stdout.lower()


def apply(root, package):
    root=Path(root).resolve();package=Path(package).resolve()
    if not (root/'setup-complete.json').is_file():raise ValueError('Complete the initial setup first')
    if game_running():raise ValueError('Close OpenHW before applying an update')
    installed=json.loads((root/'version.json').read_text(encoding='utf-8'))['version']
    stage=root/'Updates'/('pending-'+uuid.uuid4().hex);stage.mkdir(parents=True)
    with zipfile.ZipFile(package) as archive:
        entries=archive.infolist()
        if len(entries)>100000 or len({e.filename.lower() for e in entries})!=len(entries):raise ValueError('Duplicate/oversized update file list')
        header=archive.getinfo('update.json')
        if header.file_size>1024*1024:raise ValueError('Oversized update manifest')
        manifest=json.loads(archive.read(header))
        if manifest.get('schema')!=1 or manifest.get('from')!=installed:raise ValueError('Update is for a different installed version')
        version=manifest.get('to','')
        if not version or len(version)>64 or not all(c.isascii() and (c.isalnum() or c in '._-') for c in version):raise ValueError('Invalid update version')
        rows=manifest.get('files',[])
        names=[r['path'] for r in rows]
        if not rows or len({n.lower() for n in names})!=len(rows) or set(e.filename for e in entries)!={'update.json',*names}:raise ValueError('Unexpected/missing update members')
        if sum(e.file_size for e in entries)>64*1024**3:raise ValueError('Update exceeds portable package limit')
        for row in rows:
            name=row['path']
            if not allowed(name):raise ValueError('Update cannot replace user data: '+name)
            target=(root/safe_path(name)).resolve()
            if not target.is_relative_to(root):raise ValueError('Update target escapes installation')
            if name.lower().startswith('data/'):
                if 'previous_sha256' not in row:raise ValueError('Definition update lacks baseline hash: '+name)
                actual=digest(target) if target.is_file() else None
                if actual!=row['previous_sha256']:raise ValueError('Locally modified definition preserved; update refused: '+name)
            entry=archive.getinfo(name)
            if entry.file_size!=row['size']:raise ValueError('Update size mismatch')
            output=stage/'new'/safe_path(name);output.parent.mkdir(parents=True,exist_ok=True)
            with archive.open(entry) as incoming,output.open('wb') as stream:shutil.copyfileobj(incoming,stream,8*1024*1024)
            if digest(output)!=row['sha256']:raise ValueError('Update hash mismatch: '+name)
    if game_running():raise ValueError('OpenHW started during update preparation; close it and retry')
    completed=[]
    prior_version=(root/'version.json').read_bytes()
    try:
        for row in rows:
            name=safe_path(row['path']);target=root/name;backup=stage/'backup'/name
            existed=target.is_file()
            if existed:backup.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(target,backup)
            (stage/'transaction.json').write_text(json.dumps(dict(original_version=installed,
                planned=[dict(path=str(p).replace('\\','/'),existed=e) for p,e in completed]+[dict(path=str(name).replace('\\','/'),existed=existed)]),indent=2))
            target.parent.mkdir(parents=True,exist_ok=True)
            os.replace(stage/'new'/name,target);completed.append((name,existed))
        version_data=dict(version=version,previous=installed)
        temporary=root/'version.pending';temporary.write_text(json.dumps(version_data,indent=2));os.replace(temporary,root/'version.json')
        manifest['rollback_files']=[dict(path=str(name).replace('\\','/'),existed=existed) for name,existed in completed]
        (stage/'applied.json').write_text(json.dumps(manifest,indent=2))
        final=root/'Updates'/('applied-'+version+'-'+uuid.uuid4().hex[:8]);os.replace(stage,final)
        (root/'Updates'/'last-update.txt').write_text(final.name)
        return final
    except BaseException:
        for name,existed in reversed(completed):
            target=root/name
            if existed:shutil.copyfile(stage/'backup'/name,target)
            elif target.is_file():target.unlink()
        (root/'version.json').write_bytes(prior_version)
        raise


def rollback(root):
    root=Path(root).resolve()
    if game_running():raise ValueError('Close OpenHW before rolling back')
    name=(root/'Updates'/'last-update.txt').read_text().strip()
    stage=(root/'Updates'/safe_path(name)).resolve()
    if stage.parent!=root/'Updates':raise ValueError('Invalid rollback pointer')
    manifest=json.loads((stage/'applied.json').read_text())
    if json.loads((root/'version.json').read_text())['version']!=manifest['to']:raise ValueError('Rollback version mismatch')
    for row in manifest['rollback_files']:
        if not allowed(row['path']):raise ValueError('Invalid rollback member')
        path=safe_path(row['path']);target=root/path
        if row['existed']:shutil.copyfile(stage/'backup'/path,target)
        elif target.is_file():target.unlink()
    (root/'version.json').write_text(json.dumps(dict(version=manifest['from']),indent=2))
    (root/'Updates'/'last-update.txt').unlink()
    return manifest['from']


def create(installed, changed_root, version, output):
    installed,changed_root=Path(installed),Path(changed_root)
    rows=[]
    for path in sorted(changed_root.rglob('*')):
        if not path.is_file():continue
        name=path.relative_to(changed_root).as_posix()
        if not allowed(name):raise ValueError('Update builder refuses user data: '+name)
        prior=installed/safe_path(name)
        checksum=digest(path)
        if prior.is_file() and digest(prior)==checksum:continue
        row=dict(path=name,size=path.stat().st_size,sha256=checksum)
        if name.lower().startswith('data/'):
            row['previous_sha256']=digest(prior) if prior.is_file() else None
        rows.append(row)
    manifest=dict(schema=1,**{'from':json.loads((installed/'version.json').read_text())['version'],'to':version},files=rows)
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=1,allowZip64=True) as archive:
        archive.writestr('update.json',json.dumps(manifest,indent=2))
        for row in rows:archive.write(changed_root/safe_path(row['path']),row['path'])
    return manifest


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed',type=Path,required=True)
    parser.add_argument('--changed',type=Path,required=True)
    parser.add_argument('--version',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(create(args.installed,args.changed,args.version,args.output),indent=2))
