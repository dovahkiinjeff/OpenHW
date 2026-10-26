"""Repackage the current compact installer with the corrected authored-asset step.

Preserve every other frozen module and asset from the owner's current installer.
This avoids reverting the compact importer to an older source snapshot.
"""
import ast
import hashlib
import io
import json
import marshal
from pathlib import Path
import shutil
import struct
import sys
import types
import zipfile
import zlib

repo=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(repo/'out/importer-build-deps'))
from PyInstaller.archive.readers import CArchiveReader
from PyInstaller.archive.writers import CArchiveWriter
from PyInstaller.loader.pyimod01_archive import ZlibArchiveReader

installer=Path(r'F:\OpenHW1\OpenHW-Setup.exe')
work=repo/'out/asteroid-installer-path-fix'
work.mkdir(parents=True,exist_ok=True)
footer=struct.Struct('<16sQQ32s')
with installer.open('rb') as source:
    source.seek(-footer.size,2)
    magic,offset,length,old_digest=footer.unpack(source.read())
    if magic!=b'OPENHW1SETUPZIP\0':raise ValueError('Unrecognized setup footer')
    source.seek(offset)
    with (work/'original-payload.zip').open('wb') as target:
        remaining=length
        while remaining:
            chunk=source.read(min(8*1024*1024,remaining))
            if not chunk:raise ValueError('Truncated setup payload')
            target.write(chunk);remaining-=len(chunk)

engine_name='Development/Installer/OpenHW-SetupEngine.exe'
with zipfile.ZipFile(work/'original-payload.zip') as archive:
    engine_bytes=archive.read(engine_name)
original_engine=work/'original-engine.exe';original_engine.write_bytes(engine_bytes)
pkg=CArchiveReader(str(original_engine))
pyz_name=next(name for name,row in pkg.toc.items() if row[4]=='z')
original_pyz=work/'original.pyz';original_pyz.write_bytes(pkg.extract(pyz_name))
pyz=ZlibArchiveReader(str(original_pyz),check_pymagic=True)
module=pyz.extract('release_import')
source_tree=ast.parse((repo/'tools/release_import.py').read_text())
function=next(n for n in source_tree.body if isinstance(n,ast.FunctionDef) and n.name=='apply_authored')
replacement_module=compile(ast.Module(body=[function],type_ignores=[]),'release_import.py','exec')
replacement=next(c for c in replacement_module.co_consts if isinstance(c,types.CodeType))
prior=next(c for c in module.co_consts if isinstance(c,types.CodeType) and c.co_name=='apply_authored')
if prior.co_argcount!=replacement.co_argcount or prior.co_freevars!=replacement.co_freevars:
    raise ValueError('Authored-asset function interface changed')
module=module.replace(co_consts=tuple(replacement if c is prior else c for c in module.co_consts))
original_pyz_bytes=original_pyz.read_bytes()
new_pyz=io.BytesIO();new_pyz.write(original_pyz_bytes[:17]);pyz_toc=[]
for name,(kind,start,size) in pyz.toc.items():
    data=zlib.compress(marshal.dumps(module),6) if name=='release_import' else original_pyz_bytes[start:start+size]
    pyz_toc.append((name,(kind,new_pyz.tell(),len(data))));new_pyz.write(data)
toc_offset=new_pyz.tell();new_pyz.write(marshal.dumps(pyz_toc));new_pyz.seek(8);new_pyz.write(struct.pack('!i',toc_offset))

blob=io.BytesIO();pkg_toc=[]
for name,(start,size,expanded,compressed,kind) in pkg.toc.items():
    if name==pyz_name:
        data=new_pyz.getvalue();expanded=len(data)
        if compressed:data=zlib.compress(data,9)
    else:data=engine_bytes[pkg._start_offset+start:pkg._start_offset+start+size]
    pkg_toc.append((blob.tell(),len(data),expanded,compressed,kind,name));blob.write(data)
pkg_toc.extend((0,0,0,0,'o',option) for option in pkg.options)
toc_start=blob.tell();toc=CArchiveWriter._serialize_toc(pkg_toc);blob.write(toc)
old_cookie=struct.unpack(pkg._COOKIE_FORMAT,engine_bytes[pkg._end_offset-pkg._COOKIE_LENGTH:pkg._end_offset])
blob.write(struct.pack(pkg._COOKIE_FORMAT,old_cookie[0],blob.tell()+pkg._COOKIE_LENGTH,toc_start,len(toc),old_cookie[4],old_cookie[5]))
rebuilt_engine=engine_bytes[:pkg._start_offset]+blob.getvalue()

new_payload=work/'payload.zip'
with zipfile.ZipFile(work/'original-payload.zip') as source,zipfile.ZipFile(new_payload,'w',allowZip64=True) as target:
    for info in source.infolist():
        target.writestr(info,rebuilt_engine if info.filename==engine_name else source.read(info))
with new_payload.open('rb') as source:new_digest=hashlib.file_digest(source,'sha256').digest()
pending=work/'OpenHW-Setup.exe'
with installer.open('rb') as source,pending.open('wb') as target:
    target.write(source.read(offset))
    with new_payload.open('rb') as payload:shutil.copyfileobj(payload,target,8*1024*1024)
    target.write(footer.pack(magic,offset,new_payload.stat().st_size,new_digest))
with pending.open('rb') as source:digest=hashlib.file_digest(source,'sha256').hexdigest()
shutil.copyfile(installer,work/'OpenHW-Setup-before-asteroid-fix.exe')
shutil.copyfile(pending,installer)
receipt=dict(file=str(installer),bytes=installer.stat().st_size,sha256=digest,
             change='Install owner-created asteroid models, all LODs and PBR textures at physical Data/Asteroids',
             gameLaunched=False,installationRun=False)
(work/'build-receipt.json').write_text(json.dumps(receipt,indent=2))
print(json.dumps(receipt,indent=2))
