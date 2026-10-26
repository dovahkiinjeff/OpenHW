"""Stage without imported RM backgrounds; preserve existing ships and source."""
import argparse
from pathlib import Path
import shutil
import struct
from audit_local_import_release import audit, prohibited


def filtered_pack(source, destination):
    entries=[]
    with source.open('rb') as original:
        header=original.read(24)
        if len(header)!=24: raise ValueError('Truncated texture pack')
        magic,version,count,base=struct.unpack('<8sIIQ',header)
        if magic!=b'HWTPACK1' or version!=1 or count>100000 or base!=24+count*272:
            raise ValueError('Invalid texture pack')
        for _ in range(count):
            row=original.read(272)
            if len(row)!=272: raise ValueError('Truncated texture index')
            name,offset,size,reserved=struct.unpack('<256sQII',row)
            if offset<base or offset+size>source.stat().st_size:
                raise ValueError('Texture payload outside pack')
            if not prohibited(name.split(b'\0')[0].decode('utf-8')):
                entries.append((name,offset,size))
        destination.parent.mkdir(parents=True,exist_ok=True)
        with destination.open('wb') as output:
            data_base=24+len(entries)*272
            output.write(struct.pack('<8sIIQ',magic,version,len(entries),data_base))
            cursor=data_base
            for name,offset,size in entries:
                output.write(struct.pack('<256sQII',name,cursor,size,0)); cursor+=size
            for name,offset,size in entries:
                original.seek(offset)
                while size:
                    chunk=original.read(min(size,8*1024*1024))
                    if not chunk: raise ValueError('Truncated packed texture')
                    output.write(chunk); size-=len(chunk)


def stage(source, output):
    source,output=source.resolve(),output.resolve()
    if not source.is_dir() or output.exists() or output.is_relative_to(source):
        raise ValueError('Source must exist and output must be a new directory outside it')
    if not (source/'AssetImporter/HomeworldAssetImporter.exe').is_file():
        raise ValueError('Standalone importer must be packaged before stripping imported assets')
    output.mkdir(parents=True)
    excluded=[]
    for path in source.rglob('*'):
        if not path.is_file(): continue
        if path.is_symlink(): raise ValueError('Release stage refuses symlinked files')
        relative=path.relative_to(source)
        if prohibited(relative.as_posix()):
            excluded.append(relative.as_posix()); continue
        destination=output/relative
        destination.parent.mkdir(parents=True,exist_ok=True)
        if path.suffix.lower()=='.hwt': filtered_pack(path,destination)
        else: shutil.copy2(path,destination)
    failures=audit(output)
    if failures: raise ValueError('Release asset exclusion audit failed: '+str(failures))
    print(f'Staged {output}; excluded {len(excluded)} imported files; source unchanged')


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('source',type=Path)
    cli.add_argument('output',type=Path)
    args=cli.parse_args()
    stage(args.source,args.output)


if __name__=='__main__': main()
