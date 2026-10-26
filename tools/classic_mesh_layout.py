#!/usr/bin/env python3
"""Validate the Homeworld 1 RMF 0x402 mesh layout and preserve its payload."""
import argparse
import math
import struct
from pathlib import Path

HEADER = struct.Struct('<8s9I24s')
OBJECT = struct.Struct('<IBBH4i6I16f')


def translate(data):
    if len(data) < HEADER.size:
        raise ValueError('truncated header')
    h = HEADER.unpack_from(data)
    ident, version, _, _, _, public, local, material, _, count, _ = h
    if ident not in (b'RMF97ba\0', b'RMF99ba\0') or version != 0x402:
        raise ValueError('unsupported RMF identifier/version')
    stride = 112

    def span(offset, size):
        if offset < 0 or size < 0 or offset + size > len(data):
            raise ValueError(f'payload outside file: {offset}+{size}/{len(data)}')

    def string(offset):
        span(offset, 1)
        if data.find(b'\0', offset) < 0:
            raise ValueError('unterminated string')

    span(68, count * stride)
    span(material, (public + local) * 32)
    objects = []
    for i in range(count):
        start = 68 + i * stride
        raw = data[start:start + stride]
        obj = list(OBJECT.unpack(raw))
        name, _, _, _, nv, nf, nn, nt, vertices, normals, triangles, *rest = obj
        if min(nv, nf, nn, nt) < 0:
            raise ValueError('negative element count')
        string(name)
        span(vertices, nv * 16)
        span(normals, (nf + nn) * 16)
        span(triangles, nt * 40)
        for j in range(nv):
            x, y, z, ni = struct.unpack_from('<3fi', data, vertices + j * 16)
            if not all(math.isfinite(v) for v in (x, y, z)) or not (-1 <= ni < nf + nn):
                raise ValueError(f'object {i}: invalid vertex/normal index')
        for j in range(nt):
            face, a, b, c, mat = struct.unpack_from('<i4H', data, triangles + j * 40)
            if not (0 <= face < nf and max(a, b, c) < nv and mat < public + local):
                raise ValueError(f'object {i}, triangle {j}: invalid index')
        for j in (11, 12, 13):
            link = obj[j]
            if link:
                if link < 68 or (link - 68) % stride or (link - 68) // stride >= count:
                    raise ValueError(f'object {i}: invalid hierarchy link {link}')
                obj[j] = 68 + ((link - 68) // stride) * 112
        if not all(math.isfinite(v) for v in obj[14:]):
            raise ValueError('invalid transform')
        objects.append(obj)
    seen = set()

    def visit(index):
        while index is not None:
            if index in seen:
                raise ValueError('cyclic or multiply referenced hierarchy')
            seen.add(index)
            obj = objects[index]
            if obj[12]:
                visit((obj[12] - 68) // 112)
            index = (obj[13] - 68) // 112 if obj[13] else None

    if count:
        visit(0)
    if len(seen) != count:
        raise ValueError('unreachable hierarchy objects')
    for i in range(public + local):
        texture = struct.unpack_from('<I', data, material + i * 32 + 20)[0]
        if texture:
            string(texture)
    output = bytearray(data)
    struct.pack_into('<I', output, 8, 0x402)
    for i, obj in enumerate(objects):
        OBJECT.pack_into(output, 68 + i * 112, *obj)
    return bytes(output), {'version': hex(version), 'objects': count,
                          'triangles': sum(o[7] for o in objects),
                          'materials': public + local}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('meshes', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, help='single input only; otherwise validate without writes')
    args = parser.parse_args()
    if args.output and len(args.meshes) != 1:
        parser.error('--output requires one input')
    for path in args.meshes:
        converted, report = translate(path.read_bytes())
        translate(converted)  # validate the HW1 output independently
        print(path, report)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(converted)


if __name__ == '__main__':
    main()
