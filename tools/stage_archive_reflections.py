"""Upgrade packed installs' native material pages using their own HOD maps."""
import argparse
import io
import os
import re
from collections import defaultdict
import struct
import tempfile
import zlib
from pathlib import Path, PurePosixPath

import numpy as np
from PIL import Image

from import_remastered_interceptor import dds_rgba
from native_hod_surfaces import NativeSurfaces, load_reader


def reflection_source(material, suffix):
    shader = material['shader'].lower()
    rgb_glow = shader in ('shipglow', 'shipglow_ns', 'badgeglow')
    parameter = '$spec' if rgb_glow else '$glow'
    if shader == 'thruster':
        parameter += 'On' if suffix else 'Off'
    return shader == 'bay', rgb_glow, material['parameters'].get(parameter)


def stage(archive, output, reader, resume=False, writer=None):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with archive.open('rb') as f, tempfile.TemporaryDirectory() as temp:
        magic, version, count, end = struct.unpack('<8sIIQ', f.read(24))
        if magic != b'OBIGPK01' or version != 1 or not 0 < count <= 1_000_000 or end != 24 + count * 272:
            raise ValueError('Invalid OBIG directory')
        entries = {name.rstrip(b'\0').decode(): (offset, size, crc)
                   for name, offset, size, crc in struct.iter_unpack('<256sQII', f.read(end-24))}
        archive_size = archive.stat().st_size
        for key,(offset,size,crc) in entries.items():
            path = PurePosixPath(key)
            if path.is_absolute() or '..' in path.parts or any(c in key for c in (':', chr(92))):
                raise ValueError('Invalid archive path: '+key)
            if offset < end or offset+size > archive_size:
                raise ValueError('Invalid archive extent: '+key)
        def read(key):
            offset, size, crc = entries[key]
            f.seek(offset)
            data = f.read(size)
            if len(data) != size or zlib.crc32(data) != crc:
                raise ValueError('Invalid archive data: ' + key)
            return data
        def asset(folder, token):
            path = PurePosixPath(token[2:]) if token.startswith('@/') else folder/token
            if path.is_absolute() or '..' in path.parts or ':' in str(path):
                raise ValueError('Invalid asset name: '+token)
            return path.as_posix().lower()
        by_model = defaultdict(list)
        owners = {}
        aliases = defaultdict(list)

        def add(hull, index, suffix, key):
            owner = (hull, index, suffix)
            if key not in owners:
                owners[key] = owner
                by_model[hull].append((index, suffix, key))
            elif owners[key] != owner:
                if hull != owners[key][0]:
                    raise ValueError('Conflicting source models for '+key)
                aliases[key].append((index, suffix))

        for name in entries:
            if not name.endswith(('.owshipdef', '.owsubsystem')):
                continue
            folder = PurePosixPath(name).parent
            rows = [line.split() for line in read(name).decode('utf-8-sig').splitlines()]
            hull = next((asset(folder,r[1]) for r in rows if len(r)==2 and r[0]=='hull'), None)
            if not hull:
                continue
            for row in rows:
                if len(row)!=4 or row[:2]!=['surface','0']:
                    continue
                base = asset(folder,row[3])
                for suffix in ('','_engineon'):
                    key = base+suffix+'_material.dds'
                    if key in entries:
                        add(hull, int(row[2]), suffix, key)

        # Older packages also retain the original component-local surfaces
        # alongside the newer shared subsystem definitions.
        all_pages = {key for key in entries if key.endswith('_material.dds')}
        for key in sorted(all_pages - owners.keys()):
            match = re.fullmatch(r'(.+/rmnative)/c([1-9][0-9]*)_m([0-9]+)(_engineon)?_material[.]dds', key)
            if match:
                folder, component, index, suffix = match.groups()
                hull = f'{folder}/component{component}.hod'
                if hull in entries:
                    add(hull, int(index), suffix or '', key)

        done = set()
        if resume:
            # Only completed writes are renamed from .part into place.
            done.update(k for k in owners if (output/k).is_file())
        for hull, pending in by_model.items():
            jobs = [job for job in pending if job[2] not in done]
            if not jobs:
                continue
            print('Reading', hull, flush=True)
            model = Path(temp)/'source.hod'
            model.write_bytes(read(hull))
            with NativeSurfaces(reader,model) as native:
                materials = {m['index']:m for m in native.materials()}
                for index,suffix,key in jobs:
                    if key in done:
                        continue
                    dst = output/key
                    dst.parent.mkdir(parents=True,exist_ok=True)
                    source = reflection_source(materials[index], suffix)
                    shared = [reflection_source(materials[i], state) for i,state in aliases[key]]
                    if any(other[1:] != source[1:] for other in shared):
                        raise ValueError('Conflicting reflection maps for '+key)
                    bay, rgb_glow, packed = source
                    # A shared hull/bay texture keeps the hull mask. The mesh
                    # bridge applies the HOD bay policy per material instance.
                    bay = bay and all(other[0] for other in shared)
                    data = read(key)
                    if data[:4] == b'DDS ' and struct.unpack_from('<6I',data,88) == (32,255,65280,16711680,4278190080,4096):
                        height,width = struct.unpack_from('<II',data,12)
                        controls = np.frombuffer(data,dtype=np.uint8,offset=128,count=width*height*4).reshape(height,width,4).copy()
                        size = (width,height)
                    else:
                        with Image.open(io.BytesIO(data)) as page:
                            controls = np.array(page.convert('RGBA'))
                            size = page.size
                    if packed is None:
                        sharp = mask = 0
                    else:
                        im = native.image(packed).convert('RGB')
                        if im.size != size:
                            im = im.resize(size,Image.Resampling.BILINEAR)
                        rgb = np.asarray(im)
                        sharp = rgb[:,:,0]
                        mask = 0 if rgb_glow else np.maximum(0,rgb[:,:,2].astype(np.int16)-rgb[:,:,1])
                    controls[:,:,0] = sharp
                    controls[:,:,3] = 128 if bay else 129+np.rint(mask*(124.0/255.0))
                    payload=dds_rgba(Image.fromarray(controls))
                    if writer is None:
                        partial=dst.with_suffix('.part')
                        partial.write_bytes(payload)
                        os.replace(partial,dst)
                    else:
                        writer(key,payload)
                    done.add(key)
        missing = all_pages - done
        print(f'{len(done)} material pages staged; {len(missing)} unreferenced pages',flush=True)
        if missing:
            raise ValueError('Unresolved material pages: '+'; '.join(sorted(missing)))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for field in ('archive','output','reader'):
        parser.add_argument('--'+field,type=Path,required=True)
    parser.add_argument('--resume', action='store_true',
                        help='reuse completed pages from this same archive and staging run')
    args=parser.parse_args()
    stage(args.archive,args.output,load_reader(args.reader.resolve()), args.resume)
