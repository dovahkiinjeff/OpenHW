"""Package a standalone patch using the installed frozen helper's dependencies.

Only program files and original converter code are distributed. Mission sky
resources are regenerated on the player's computer from their Collection.
"""
import argparse,hashlib,io,json,marshal,shutil,struct,sys,zipfile,zlib
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source','installed','normal','diagnostic','bootstrap','output','work','pyinstaller-modules'):
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();sys.path.insert(0,str(a.pyinstaller_modules.resolve()))
    from PyInstaller.archive.readers import CArchiveReader
    from PyInstaller.archive.writers import CArchiveWriter
    from PyInstaller.loader.pyimod01_archive import ZlibArchiveReader
    a.work.mkdir(parents=True,exist_ok=True);a.output.parent.mkdir(parents=True,exist_ok=True)
    original=a.installed/'Development/Installer/OpenHW-SetupEngine.exe'
    engine=original.read_bytes();pkg=CArchiveReader(str(original))
    pyz_name=next(n for n,r in pkg.toc.items() if r[4]=='z')
    pyz_file=a.work/'original.pyz';pyz_file.write_bytes(pkg.extract(pyz_name))
    pyz=ZlibArchiveReader(str(pyz_file),check_pymagic=True);raw_pyz=pyz_file.read_bytes()
    replacements={name:compile((a.source/'tools'/(name+'.py')).read_text(),name+'.py','exec') for name in ('release_patch','release_sky_lighting')}
    if 'generate_mission_skies' not in pyz.toc:raise ValueError('Installed converter lacks the required sky generator')
    new_pyz=io.BytesIO();new_pyz.write(raw_pyz[:17]);toc=[]
    for name,(kind,start,size) in pyz.toc.items():
        data=zlib.compress(marshal.dumps(replacements.pop(name)),6) if name in replacements else raw_pyz[start:start+size]
        toc.append((name,(kind,new_pyz.tell(),len(data))));new_pyz.write(data)
    for name,code in replacements.items():
        data=zlib.compress(marshal.dumps(code),6);toc.append((name,(0,new_pyz.tell(),len(data))));new_pyz.write(data)
    start=new_pyz.tell();new_pyz.write(marshal.dumps(toc));new_pyz.seek(8);new_pyz.write(struct.pack('!i',start))
    entry_code=compile((a.source/'tools/release_setup.py').read_text(),'release_setup.py','exec')
    blob=io.BytesIO();toc=[];entry_replaced=False
    for name,(start,size,expanded,compressed,kind) in pkg.toc.items():
        if name==pyz_name:data=new_pyz.getvalue();expanded=len(data);data=zlib.compress(data,9) if compressed else data
        elif name=='release_setup' and kind=='s':
            data=marshal.dumps(entry_code);expanded=len(data);data=zlib.compress(data,9) if compressed else data;entry_replaced=True
        else:data=engine[pkg._start_offset+start:pkg._start_offset+start+size]
        toc.append((blob.tell(),len(data),expanded,compressed,kind,name));blob.write(data)
    if not entry_replaced:raise ValueError('Unrecognized frozen installer entry point')
    toc.extend((0,0,0,0,'o',n) for n in pkg.options)
    start=blob.tell();table=CArchiveWriter._serialize_toc(toc);blob.write(table)
    cookie=struct.unpack(pkg._COOKIE_FORMAT,engine[pkg._end_offset-pkg._COOKIE_LENGTH:pkg._end_offset])
    blob.write(struct.pack(pkg._COOKIE_FORMAT,cookie[0],blob.tell()+pkg._COOKIE_LENGTH,start,len(table),cookie[4],cookie[5]))
    rebuilt=a.work/'OpenHW-SetupEngine.exe';rebuilt.write_bytes(engine[:pkg._start_offset]+blob.getvalue())
    files={
        'OpenHW.exe':a.normal/'HomeworldModern.exe',
        'OpenHW-Daignostics.exe':a.diagnostic/'HomeworldModern.exe',
        'Development/Symbols/OpenHW.pdb':a.normal/'HomeworldModern.pdb',
        'Development/Symbols/OpenHW-Daignostics.pdb':a.diagnostic/'HomeworldModern.pdb',
        'Development/Installer/OpenHW-SetupEngine.exe':rebuilt,
        'Development/Installer/Development/sky_materials/btg_microstructure.png':a.source/'tools/sky_materials/btg_microstructure.png',
    }
    rows=[]
    for name,path in files.items():
        with path.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
        rows.append(dict(path=name,size=path.stat().st_size,sha256=digest))
    payload=a.work/'patch.zip'
    with zipfile.ZipFile(payload,'w',zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as z:
        for name,path in files.items():z.write(path,name)
        z.writestr('Development/Patches/1.0.4/patch.json',json.dumps(dict(version='1.0.4',files=rows),indent=2))
    with payload.open('rb') as f:digest=hashlib.file_digest(f,'sha256').digest()
    pending=a.work/'OpenHW-Patch-1.0.4.exe'
    with pending.open('wb') as out,a.bootstrap.open('rb') as bootstrap,payload.open('rb') as archive:
        shutil.copyfileobj(bootstrap,out);offset=out.tell();shutil.copyfileobj(archive,out)
        out.write(struct.pack('<16sQQ32s',b'OPENHW1SETUPZIP\0',offset,payload.stat().st_size,digest))
    shutil.copyfile(pending,a.output)
    with a.output.open('rb') as f:checksum=hashlib.file_digest(f,'sha256').hexdigest()
    report=dict(file=str(a.output),bytes=a.output.stat().st_size,sha256=checksum,version='1.0.4',files=rows,gameLaunched=False,patchExecuted=False)
    (a.work/'build-receipt.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

if __name__=='__main__':main()
