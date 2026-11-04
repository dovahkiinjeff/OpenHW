"""Build Updater.exe using an installed game's existing frozen Python support."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import marshal
import shutil
import struct
import sys
import zipfile
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source','installed','bootstrap','pyinstaller-modules','output','work'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args(); sys.path.insert(0,str(args.pyinstaller_modules.resolve()))
    from PyInstaller.archive.readers import CArchiveReader
    from PyInstaller.archive.writers import CArchiveWriter
    from PyInstaller.loader.pyimod01_archive import ZlibArchiveReader
    version = '1.0.0'
    source = args.source.resolve(); args.work.mkdir(parents=True, exist_ok=True)
    for path in ('tools/openhw_updater.py','tools/UpdaterVersion.h'):
        if version not in (source/path).read_text(encoding='utf-8'): raise ValueError('Updater version mismatch')
    original = args.installed/'Development/Installer/OpenHW-SetupEngine.exe'
    engine = original.read_bytes(); pkg = CArchiveReader(str(original))
    name = next(n for n,r in pkg.toc.items() if r[4] == 'z')
    pyz_path = args.work/'original.pyz'; pyz_path.write_bytes(pkg.extract(name))
    pyz = ZlibArchiveReader(str(pyz_path), check_pymagic=True); raw = pyz_path.read_bytes()
    required = ('urllib.request','ssl','tkinter','zipfile','ctypes','generate_mission_skies')
    if any(module not in pyz.toc for module in required): raise ValueError('Installer runtime is missing required modules')
    replacements = {module:compile((source/'tools'/(module+'.py')).read_text(encoding='utf-8'),module+'.py','exec')
                    for module in ('openhw_updater','release_sky_lighting')}
    new = io.BytesIO(); new.write(raw[:17]); toc = []
    for module,(kind,start,size) in pyz.toc.items():
        data = zlib.compress(marshal.dumps(replacements.pop(module)),6) if module in replacements else raw[start:start+size]
        toc.append((module,(kind,new.tell(),len(data)))); new.write(data)
    for module,code in replacements.items():
        data = zlib.compress(marshal.dumps(code),6); toc.append((module,(0,new.tell(),len(data)))); new.write(data)
    start = new.tell(); new.write(marshal.dumps(toc)); new.seek(8); new.write(struct.pack('!i',start))
    entry = compile('from openhw_updater import main\nmain()\n','updater_entry.py','exec')
    blob = io.BytesIO(); toc = []; entry_replaced = False
    for member,(start,size,expanded,compressed,kind) in pkg.toc.items():
        if member == name:
            data = new.getvalue(); expanded = len(data); data = zlib.compress(data,9) if compressed else data
        elif member == 'release_setup' and kind == 's':
            entry_replaced = True
            data = marshal.dumps(entry); expanded = len(data); data = zlib.compress(data,9) if compressed else data
        else: data = engine[pkg._start_offset+start:pkg._start_offset+start+size]
        toc.append((blob.tell(),len(data),expanded,compressed,kind,member)); blob.write(data)
    if not entry_replaced: raise ValueError('Installer entry point was not found')
    toc.extend((0,0,0,0,'o',member) for member in pkg.options)
    start = blob.tell(); table = CArchiveWriter._serialize_toc(toc); blob.write(table)
    cookie = struct.unpack(pkg._COOKIE_FORMAT,engine[pkg._end_offset-pkg._COOKIE_LENGTH:pkg._end_offset])
    blob.write(struct.pack(pkg._COOKIE_FORMAT,cookie[0],blob.tell()+pkg._COOKIE_LENGTH,start,len(table),cookie[4],cookie[5]))
    helper = args.work/'UpdaterEngine.exe'; helper.write_bytes(engine[:pkg._start_offset]+blob.getvalue())
    archive = args.work/'updater.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as out:
        out.write(helper,'Development/Updater/Runtime/'+version+'/UpdaterEngine.exe')
    payload = archive.read_bytes(); bootstrap = args.bootstrap.read_bytes()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(bootstrap+payload+struct.pack('<16sQQ32s',b'OPENHW1SETUPZIP\0',len(bootstrap),len(payload),hashlib.sha256(payload).digest()))
    receipt = dict(version=version,protocol=1,file=str(args.output),size=args.output.stat().st_size,
        sha256=hashlib.sha256(args.output.read_bytes()).hexdigest(), gameRun=False, liveInstallationUpdated=False)
    (args.work/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))


if __name__ == '__main__': main()
