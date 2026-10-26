"""Validate every HOD2 background layer without discarding unsupported content.

Inspection only: do not enable a mission from this report. In particular, an
animated planet cannot be replaced with its diffuse texture on the sky sphere.
Uses local, legally installed assets; no copyrighted inputs are distributed.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from translate_remastered_background import chunks, light_pools


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def take(self, count):
        if count < 0 or self.offset+count > len(self.data):
            raise ValueError('truncated scene field')
        value = self.data[self.offset:self.offset+count]
        self.offset += count
        return value

    def unpack(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))

    def string(self):
        size, = self.unpack('<I')
        if size > 4096:
            raise ValueError('oversized scene string')
        return self.take(size).decode('ascii').rstrip('\0')

    def done(self):
        if self.offset != len(self.data):
            raise ValueError(f'unconsumed scene bytes: {len(self.data)-self.offset}')


def inspect_scene(data, streams=None):
    pools = [Reader(p) for p in (streams if streams is not None else light_pools(data))]
    top = list(chunks(data))
    blocks = list(chunks(next(p for k, p in top if k == b'HVMD')))
    textures, materials, meshes = [], [], []
    for kind, payload in blocks:
        if kind == b'LMIP':
            r = Reader(payload)
            name, fmt = r.string(), r.take(4).decode('ascii')
            count, = r.unpack('<I')
            if not 1 <= count <= 16:
                raise ValueError('bad mip count')
            dimensions = [r.unpack('<II') for _ in range(count)]
            r.done()
            sizes = []
            for w, h in dimensions:
                if not 1 <= w <= 16384 or not 1 <= h <= 16384:
                    raise ValueError('bad texture extent')
                if fmt == '8888':
                    sizes.append(w*h*4)
                elif fmt in ('DXT1', 'DXT3', 'DXT5'):
                    sizes.append(((w+3)//4)*((h+3)//4)*(8 if fmt == 'DXT1' else 16))
                else:
                    raise ValueError(f'unsupported texture format: {fmt}')
            start = pools[0].offset
            pixels = pools[0].take(sum(sizes))
            textures.append(dict(name=name, format=fmt, mips=dimensions,
                                 pool_offset=start, bytes=len(pixels)))
        elif kind == b'STAT':
            r = Reader(payload)
            version, = r.unpack('>I')
            if version not in (1000, 1001):
                raise ValueError('unsupported material version')
            name, shader = r.string(), r.string()
            count, = r.unpack('<I')
            if count > 1024:
                raise ValueError('oversized material')
            params = []
            for _ in range(count):
                typ, length = r.unpack('<II')
                value = r.take(length)
                parameter = r.string()
                params.append(dict(name=parameter, type=typ, value_hex=value.hex()))
                if typ == 5:
                    if length != 4 or struct.unpack('<I', value)[0] >= len(textures):
                        raise ValueError('invalid material texture reference')
            r.done()
            materials.append(dict(name=name, shader=shader, parameters=params))
        elif kind == b'MULT':
            r = Reader(payload)
            version, = r.unpack('>I')
            if version not in (1400, 1401):
                raise ValueError('unsupported MULT version')
            name, parent = r.string(), r.string()
            lod_count, = r.unpack('<I')
            children = list(chunks(r.take(len(r.data)-r.offset)))
            for tag, body in children:
                if tag == b'TAGS':
                    tags = Reader(body)
                    tags.string()
                    tags.done()
                elif tag != b'BMSH':
                    raise ValueError('unsupported MULT child')
            mesh_blocks = [(tag, body) for tag, body in children if tag == b'BMSH']
            if len(mesh_blocks) != lod_count:
                raise ValueError('LOD count mismatch')
            for tag, body in mesh_blocks:
                if tag != b'BMSH':
                    raise ValueError('unsupported MULT child')
                b = Reader(body)
                mesh_version, = b.unpack('>I')
                lod, count = b.unpack('<II')
                if mesh_version not in (1400, 1401) or count > 1024:
                    raise ValueError('unsupported BMSH layout')
                parts = []
                for _ in range(count):
                    material, mask, n = b.unpack('<III')
                    marker, index_count = b.unpack('<hI')
                    if marker != -1 or mask & ~0x603f or not mask & 1:
                        raise ValueError(f'unsupported pooled vertex mask {mask:#x}')
                    if material >= len(materials) or not 0 < n <= 65536 or index_count % 3:
                        raise ValueError('invalid pooled mesh counts/material')
                    fields = [(1, 16), (2, 16), (4, 4), (8, 8), (0x2000, 12), (0x4000, 12)]
                    if mesh_version == 1401:
                        fields += [(16, 8), (32, 8)]
                    elif mask & 48:
                        raise ValueError('UV1/UV2 require mesh version 1401')
                    stride = sum(size for bit, size in fields if mask & bit)
                    start, face_start = pools[1].offset, pools[2].offset
                    raw = pools[1].take(n*stride)
                    # Packed vertex color is not a float; check only positions.
                    positions = np.ndarray((n, 3), '<f4', buffer=raw, strides=(stride, 4))
                    indices = np.frombuffer(pools[2].take(index_count*2), '<u2')
                    if not np.isfinite(positions).all() or np.any(indices >= n):
                        raise ValueError('invalid pooled positions/indices')
                    parts.append(dict(material=material, mask=mask, stride=stride,
                                      vertices=n, triangles=index_count//3,
                                      vertex_pool_offset=start, face_pool_offset=face_start,
                                      bounds=[positions.min(0).tolist(), positions.max(0).tolist()]))
                b.done()
                meshes.append(dict(name=name, parent=parent, lod=lod, parts=parts))
    transform = next(p for k, p in top if k == b'DTRM')
    # Ship HODs append collision vertices/indices to the same pools. These are
    # not render geometry; validate and consume them rather than guessing that
    # the unused bytes are another visual LOD.
    collisions = []
    for kind, body in chunks(transform):
        if kind != b'COLD':
            continue
        c = Reader(body)
        parent = c.string()
        for tag, payload in chunks(c.take(len(c.data)-c.offset)):
            if tag == b'TRIS':
                if len(payload) != 8:
                    raise ValueError('invalid collision triangle descriptor')
                # HOD counts are little endian despite the IFF chunk envelope.
                vertices, indices = struct.unpack('<II', payload)
                if not 0 < vertices <= 65536 or indices > 1000000 or indices % 3:
                    raise ValueError('invalid collision geometry counts')
                positions = np.frombuffer(pools[1].take(vertices*12), '<f4')
                faces = np.frombuffer(pools[2].take(indices*2), '<u2')
                if not np.isfinite(positions).all() or np.any(faces >= vertices):
                    raise ValueError('invalid collision geometry')
                collisions.append(dict(parent=parent, vertices=vertices, triangles=indices//3))
            elif tag in (b'BBOX', b'BSPH'):
                if len(payload) != (24 if tag == b'BBOX' else 16) or not np.isfinite(np.frombuffer(payload, '<f4')).all():
                    raise ValueError('invalid collision bounds')
            else:
                raise ValueError('unsupported collision shape')
    for index, pool in enumerate(pools):
        if pool.offset != len(pool.data):
            raise ValueError(f'unconsumed pool {index} bytes: {len(pool.data)-pool.offset}')
    hierarchy = next(p for k, p in chunks(transform) if k == b'HIER')
    r = Reader(hierarchy)
    count, = r.unpack('<i')
    if not -10000 < count < 0:
        raise ValueError('expected HOD2 hierarchy')
    joints = []
    for _ in range(-count):
        name, parent = r.string(), r.string()
        pose = r.unpack('<9f')
        if not np.isfinite(pose).all():
            raise ValueError('non-finite joint pose')
        joints.append(dict(name=name, parent=parent, position=pose[:3],
                           rotation=pose[3:6], gimbal_limits=pose[6:]))
    r.done()
    names = {j['name'] for j in joints}
    if len(names) != len(joints) or any(j['parent'] and j['parent'] not in names for j in joints):
        raise ValueError('invalid hierarchy parents')
    for joint in joints:
        chain = set()
        cursor = joint
        while cursor['parent']:
            if cursor['name'] in chain:
                raise ValueError('cyclic hierarchy')
            chain.add(cursor['name'])
            cursor = next(j for j in joints if j['name'] == cursor['parent'])
    if any(m['parent'] not in names for m in meshes):
        raise ValueError('mesh has no transform')
    return dict(sha256=hashlib.sha256(data).hexdigest(), textures=textures,
                materials=materials, meshes=meshes, joints=joints, collisions=collisions)


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument('--root', type=Path, required=True)
    cli.add_argument('--output', type=Path, required=True)
    args = cli.parse_args()
    reports = {}
    for mission in range(1, 17):
        stem = f'ez{mission:02d}'
        root = args.root/stem
        report = inspect_scene((root/(stem+'_light.hod')).read_bytes())
        report['scripts'] = {p.name: p.read_text() for p in root.glob('*.lua')}
        report['animation_bytes'] = (root/(stem+'_light.mad')).stat().st_size if (root/(stem+'_light.mad')).exists() else 0
        reports[stem] = report
        print(f'{stem}: {len(report["textures"])} textures, {len(report["meshes"])} meshes, {len(report["joints"])} joints', flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(reports, indent=2))


if __name__ == '__main__':
    main()
