"""Build private server content from an owned installation; never a release asset.

Preserves simulation definitions and the same native RM geometry/hardpoints.
Doesn't expand tens of GB of DDS textures, video or audio for a headless worker.
"""
from pathlib import Path
import argparse
import json
import shutil
import struct
import zlib

HEADER=struct.Struct('<8sIIQ');ENTRY=struct.Struct('<256sQII')
EXCLUDE={'.dds','.wav','.wxd','.vce','.fda','.bnk','.png','.jpg','.jpeg','.tga','.mp4','.avi','.bik','.webm'}
def wanted(name):
    p=Path(name)
    return p.suffix.lower() not in EXCLUDE and not name.lower().startswith(('remasteredbackgrounds/','remasteredaudio/','movies/'))

def build(root,destination):
    root=root.resolve();destination=destination.resolve()
    if root==destination or root in destination.parents and 'Development' not in destination.parts:
        raise ValueError('Use a separate Development server-content directory')
    destination.mkdir(parents=True,exist_ok=True)
    archive=root/'OpenHW-HW1.obig';entries={}
    with archive.open('rb') as source:
        magic,version,count,offset=HEADER.unpack(source.read(HEADER.size))
        if magic!=b'OBIGPK01' or version!=1 or offset!=HEADER.size+count*ENTRY.size:raise ValueError('Invalid source archive')
        for _ in range(count):
            raw,position,size,crc=ENTRY.unpack(source.read(ENTRY.size));name=raw.split(b'\0')[0].decode('ascii')
            if wanted(name):entries[name]=(None,position,size,crc)
        for path in (root/'Data').rglob('*'):
            if path.is_file():
                name=path.relative_to(root/'Data').as_posix().lower()
                if wanted(name):entries[name]=(path,0,path.stat().st_size,0)
        # Current public release overrides for ship definitions / custom meshes.
        for folder in ('asteroids','Subsystems'):
            if (root/folder).exists():
                for path in (root/folder).rglob('*'):
                    if path.is_file():
                        name=path.relative_to(root).as_posix().lower()
                        if wanted(name):entries[name]=(path,0,path.stat().st_size,0)
        ordered=sorted(entries.items());index=[];target=destination/'OpenHW-HW1.obig';temp=target.with_suffix('.tmp')
        data_offset=HEADER.size+len(ordered)*ENTRY.size
        with temp.open('wb') as out:
            out.write(HEADER.pack(magic,1,len(ordered),data_offset));out.seek(data_offset)
            for name,(path,position,size,crc) in ordered:
                at=out.tell()
                if path:
                    data=path.read_bytes();crc=zlib.crc32(data);out.write(data)
                else:
                    source.seek(position);remaining=size
                    while remaining:
                        data=source.read(min(remaining,1024*1024))
                        if not data:raise ValueError('Truncated source payload')
                        out.write(data);remaining-=len(data)
                index.append(ENTRY.pack(name.encode('ascii'),at,size,crc))
            out.seek(HEADER.size)
            for item in index:out.write(item)
        temp.replace(target)
    report={'private_server_content':True,'entries':len(entries),'bytes':target.stat().st_size,
        'excluded':'DDS images, movies, audio, backgrounds. RM hierarchy/animation/gameplay data preserved.'}
    (destination/'server-content.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('installation',type=Path);parser.add_argument('destination',type=Path)
    args=parser.parse_args();build(args.installation,args.destination)
