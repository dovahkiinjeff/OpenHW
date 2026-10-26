"""Append a verified, local-extraction payload to the static Windows launcher."""
from pathlib import Path
import argparse,hashlib,json,shutil,struct,zipfile
from release_content_policy import audit_archive
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('stage','frozen','bootstrap','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
    payload=a.output.with_suffix('.payload.zip');manifest=json.loads((a.stage/'Development/release.json').read_text())
    names=set()
    with zipfile.ZipFile(payload,'w',zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
        def add(source,name):
            key=name.lower()
            if key in names:raise ValueError('Duplicate installer path: '+name)
            names.add(key);archive.write(source,name)
        for row in manifest['files']:add(a.stage/row['path'],row['path'])
        add(a.stage/'Development/release.json','Development/release.json')
        add(a.stage/'READ ME.txt','READ ME.txt')
        for source in a.frozen.rglob('*'):
            if not source.is_file():continue
            relative=source.relative_to(a.frozen)
            name='Development/Installer/OpenHW-SetupEngine.exe' if relative.as_posix()=='OpenHW-Setup.exe' else 'Development/Installer/'+relative.as_posix()
            add(source,name)
    with zipfile.ZipFile(payload) as archive:
        if archive.testzip():raise ValueError('Corrupt installer ZIP payload')
    content_audit=audit_archive(payload)
    with payload.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').digest()
    with a.output.open('wb') as out,a.bootstrap.open('rb') as bootstrap,payload.open('rb') as incoming:
        shutil.copyfileobj(bootstrap,out);offset=out.tell();shutil.copyfileobj(incoming,out)
        out.write(struct.pack('<16sQQ32s',b'OPENHW1SETUPZIP\0',offset,payload.stat().st_size,digest))
    with a.output.open('rb') as stream:result=hashlib.file_digest(stream,'sha256').hexdigest()
    receipt={'version':'1.0','file':a.output.name,'size':a.output.stat().st_size,'sha256':result,'payloadSha256':digest.hex(),'payloadFiles':len(names),'installRoot':'directory containing the installer','gameExecutables':manifest['executables'],'contentAudit':content_audit}
    a.output.with_suffix('.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
