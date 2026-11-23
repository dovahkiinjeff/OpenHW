"""Refresh installer Python code while retaining the reviewed frozen dependency image."""
import argparse,io,marshal,struct,zlib,shutil,sys
from pathlib import Path

def refresh(source,original,output,work):
    from PyInstaller.archive.readers import CArchiveReader
    from PyInstaller.archive.writers import CArchiveWriter
    from PyInstaller.loader.pyimod01_archive import ZlibArchiveReader
    work.mkdir(parents=True,exist_ok=True);output.parent.mkdir(parents=True,exist_ok=True)
    engine=original.read_bytes();pkg=CArchiveReader(str(original))
    pyz_name=next(name for name,row in pkg.toc.items() if row[4]=='z')
    raw=pkg.extract(pyz_name);pyz_file=work/'original.pyz';pyz_file.write_bytes(raw)
    pyz=ZlibArchiveReader(str(pyz_file),check_pymagic=True)
    modules=('release_full','release_setup','openhw_updater','openhw_launcher','openhw_launcher_ui')
    replacements={name:compile((source/'tools'/f'{name}.py').read_text(encoding='utf-8'),name+'.py','exec') for name in modules}
    rebuilt=io.BytesIO();rebuilt.write(raw[:17]);toc=[]
    for name,(kind,start,size) in pyz.toc.items():
        data=zlib.compress(marshal.dumps(replacements.pop(name)),6) if name in replacements else raw[start:start+size]
        toc.append((name,(kind,rebuilt.tell(),len(data))));rebuilt.write(data)
    for name,code in replacements.items():
        data=zlib.compress(marshal.dumps(code),6);toc.append((name,(0,rebuilt.tell(),len(data))));rebuilt.write(data)
    start=rebuilt.tell();rebuilt.write(marshal.dumps(toc));rebuilt.seek(8);rebuilt.write(struct.pack('!i',start))
    blob=io.BytesIO();toc=[];entry=False
    for name,(start,size,expanded,compressed,kind) in pkg.toc.items():
        if name==pyz_name:data=rebuilt.getvalue();expanded=len(data);data=zlib.compress(data,9) if compressed else data
        elif name=='release_setup' and kind=='s':
            data=marshal.dumps(compile((source/'tools/release_setup.py').read_text(encoding='utf-8'),'release_setup.py','exec'))
            expanded=len(data);data=zlib.compress(data,9) if compressed else data;entry=True
        else:data=engine[pkg._start_offset+start:pkg._start_offset+start+size]
        toc.append((blob.tell(),len(data),expanded,compressed,kind,name));blob.write(data)
    if not entry:raise ValueError('Unrecognized installer entry point')
    toc.extend((0,0,0,0,'o',name) for name in pkg.options)
    start=blob.tell();table=CArchiveWriter._serialize_toc(toc);blob.write(table)
    cookie=struct.unpack(pkg._COOKIE_FORMAT,engine[pkg._end_offset-pkg._COOKIE_LENGTH:pkg._end_offset])
    blob.write(struct.pack(pkg._COOKIE_FORMAT,cookie[0],blob.tell()+pkg._COOKIE_LENGTH,start,len(table),cookie[4],cookie[5]))
    output.write_bytes(engine[:pkg._start_offset]+blob.getvalue())

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','original','output','work','pyinstaller-modules'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();sys.path.insert(0,str(args.pyinstaller_modules.resolve()))
    refresh(args.source,args.original,args.output,args.work)
