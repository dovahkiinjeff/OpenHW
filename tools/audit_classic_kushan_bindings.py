"""Read HW1 R1 binding contracts before adapting RM visual joints.

No gameplay stats, model files, archives or deployment are modified.
"""
import argparse
import json
import math
import re
import struct
from pathlib import Path, PureWindowsPath
from extract_vanilla_ship_textures import read_archive, entry_bytes
from inspect_mad import inspect_mad
from plan_kushan_hod_bindings import R1_HULLS


def mex_markers(data):
    if len(data)<12:
        raise ValueError('Truncated MEX header')
    identifier, version, count=struct.unpack_from('<8sHH',data)
    if identifier!=b'mannngo\0' or version!=0x102:
        raise ValueError('Invalid MEX signature')
    offset=12;markers=[]
    for _ in range(count):
        if offset+16>len(data):
            raise ValueError('Truncated MEX chunk')
        raw_type, raw_name, size=struct.unpack_from('<4s8sI',data,offset)
        kind=raw_type.split(b'\0')[0].decode('ascii')
        name=raw_name.split(b'\0')[0].decode('ascii')
        start=offset+16;end=start+size
        if end>len(data):
            raise ValueError('MEX payload exceeds file')
        if kind in ('Gun','Eng','Dok','Sal','Nav'):
            if size<12 or (kind!='Nav' and size<32):
                raise ValueError('Truncated MEX marker')
            position=struct.unpack_from('<3f',data,start)
            normal=None if kind=='Nav' else struct.unpack_from('<3f',data,start+12)
            if not all(math.isfinite(v) for v in position+(normal or ())):
                raise ValueError('Nonfinite MEX marker')
            record=dict(type=kind,name=name,position=list(position),normal=normal)
            if kind in ('Dok','Sal'):
                if size<52:
                    raise ValueError('Truncated named MEX marker')
                record['semantic_name']=data[start+32:start+52].split(b'\0')[0].decode('ascii')
            markers.append(record)
        offset=end
    if offset!=len(data):
        raise ValueError('Trailing MEX bytes')
    return markers


def binding_fields(text):
    result=[]
    for line in text.splitlines():
        line=line.split(';',1)[0].strip()
        match=re.match(r'(BindGun|BindAnimations|Mad\w+)\s+(.+)',line)
        if match:
            result.append(dict(field=match[1],tokens=[t for t in re.split(r'[\s,]+',match[2]) if t]))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    blob,entries=read_archive(args.archive)
    records={entry.name.lower():entry for entry in entries}
    result=dict(schema=1,gameplay_stats_imported=False,ships=[])
    for classic,source in R1_HULLS.items():
        record=dict(classic_type=classic,source=source,bindings=[],markers=[],animations=[])
        result['ships'].append(record)
        for extension in ('shp','mex','mad'):
            key=str(PureWindowsPath('R1',classic,'rl0','lod0',classic+'.mex') if extension=='mex'
                    else PureWindowsPath('R1',classic+'.'+extension)).lower()
            entry=records.get(key)
            if not entry:
                record[extension+'_present']=False;continue
            data=entry_bytes(blob,entry);record[extension+'_present']=True
            if extension=='shp':record['bindings']=binding_fields(data.decode('latin-1'))
            elif extension=='mex':record['markers']=mex_markers(data)
            else:record['animations']=inspect_mad(data)['animations']
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(f'Read-only binding audit: {len(result["ships"])} R1 identities')


if __name__=='__main__':
    main()
