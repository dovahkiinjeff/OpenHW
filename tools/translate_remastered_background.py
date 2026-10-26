"""Strict Remastered background translator; never guess a vertex stride.

The HOD2 pooled PRE_Space mesh and RGBA sky are the visible galaxy source.
BGSG sprites and STRF stars come from the companion background HOD.
Foreground meshes and animated Lua flares are not implemented yet.
Original game assets are local inputs and must not be redistributed.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image
from generate_mission_skies import srgb_to_linear, linear_to_srgb, write_hwfloat
from import_remastered_sky import convert, pad_faces, sample_cube, rm_direction


def chunks(data):
    offset = 0
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError('truncated IFF header')
        outer, size, tag = struct.unpack_from('>4sI4s', data, offset)
        if size < 4 or offset + 8 + size > len(data):
            raise ValueError('invalid IFF extent')
        yield (outer, data[offset + 8:offset + 8 + size]) if outer not in (b'FORM', b'NRML') else (tag, data[offset + 12:offset + 8 + size])
        offset += 8 + size


def mesh(data):
    version, = struct.unpack_from('>I', data)
    lod, count = struct.unpack_from('<II', data, 4)
    if version != 1400 or lod != 0 or count > 1000:
        raise ValueError('unsupported background mesh version/LOD')
    offset = 12
    parts = []
    for _ in range(count):
        material, mask, n = struct.unpack_from('<III', data, offset)
        offset += 12
        # Only the verified colored sky format. Reject other masks rather than
        # drawing corrupt triangles from an assumed layout.
        if mask != 5 or not 0 < n <= 65536:
            raise ValueError(f'unsupported sky vertex mask {mask:#x}')
        size = n * 20
        if offset + size + 2 > len(data):
            raise ValueError('truncated vertex stream')
        dtype = np.dtype([('position', '<f4', (4,)), ('color', '<u4')])
        vertices = np.frombuffer(data, dtype=dtype, count=n, offset=offset)
        offset += size
        pos = vertices['position'][:, :3].copy()
        packed = vertices['color']
        # HOD stores RRGGBBAA, with the integer itself little-endian.
        rgba = np.stack([(packed >> shift) & 255 for shift in (24, 16, 8, 0)], -1) / 255.
        groups, = struct.unpack_from('<h', data, offset)
        offset += 2
        faces = []
        for _ in range(1 if groups == -1 else groups):
            if groups == -1:
                primitive = 514
                length, = struct.unpack_from('<I', data, offset)
                offset += 4
            else:
                primitive, length = struct.unpack_from('<II', data, offset)
                offset += 8
            if length > 10000000 or offset + length * 2 > len(data):
                raise ValueError('truncated index stream')
            indices = np.frombuffer(data, '<u2', length, offset).astype(np.int32)
            offset += length * 2
            if np.any(indices >= n):
                raise ValueError('index outside vertex stream')
            if primitive == 514 and length % 3 == 0:
                faces.extend(indices.reshape(-1, 3))
            elif primitive == 518 and length >= 3:
                faces.extend((indices[i], indices[i+1], indices[i+2]) for i in range(length-2))
            else:
                raise ValueError('unsupported primitive')
        if not np.all(np.isfinite(pos)) or np.any(np.linalg.norm(pos, axis=1) < 1e-4):
            raise ValueError('invalid background positions')
        parts.append((pos, rgba, np.asarray(faces)))
    if offset != len(data):
        raise ValueError(f'unconsumed mesh bytes: {len(data)-offset}')
    return parts


def parse(data):
    parts, sprites, stars = [], [], np.zeros((0, 7), np.float32)
    for tag, payload in chunks(data):
        if tag == b'VERS' and payload != struct.pack('<I', 1000):
            raise ValueError('only background HOD v1000 supported')
        if tag == b'BGMS':
            for inner, block in chunks(payload):
                if inner != b'BMSH':
                    raise ValueError('unknown BGMS child')
                parts.extend(mesh(block))
        elif tag == b'BGSG':
            length, = struct.unpack_from('<I', payload)
            name = payload[4:4+length].decode('ascii')
            if Path(name).name != name:
                raise ValueError('invalid sprite path')
            count, = struct.unpack_from('<I', payload, 4+length)
            if len(payload) != 8 + length + count * 32:
                raise ValueError('invalid BGSG size')
            sprites.append((name, np.frombuffer(payload, '<f4', count*8, 8+length).reshape(-1, 8)))
        elif tag == b'STRF':
            version, = struct.unpack_from('>I', payload)
            count, = struct.unpack_from('<I', payload, 4)
            if version != 1000 or len(payload) != 8 + count * 28:
                raise ValueError('invalid star field')
            stars = np.frombuffer(payload, '<f4', count*7, 8).reshape(-1, 7)
    if not parts:
        raise ValueError('no visible background meshes')
    return parts, sprites, stars


def decompress_pool(data, expected):
    """HODOR pool LZ77, matching the published HODEditorJS match tables.

    Unlike permissive viewers, reject truncated streams and invalid backrefs.
    """
    tables = ((255, 2, 0, 0, 1), (65535, 2, 0, 0, 2),
              (65535, 6, 15, 2, 2), (16777215, 8, 31, 3, 3),
              (255, 2, 0, 0, 1), (65535, 2, 0, 0, 2),
              (65535, 6, 15, 2, 2), (4294967295, 11, 255, 3, 4))
    out = bytearray()
    cursor, flags = 0, 1
    while len(out) < expected:
        if flags == 1:
            if cursor+4 > len(data):
                raise ValueError('truncated pool flags')
            flags, = struct.unpack_from('<I', data, cursor)
            cursor += 4
            if not flags & 0x80000000:
                raise ValueError('missing pool sentinel')
        if not flags & 1:
            if cursor >= len(data):
                raise ValueError('truncated pool literal')
            out.append(data[cursor]); cursor += 1
        else:
            word = int.from_bytes(data[cursor:cursor+4], 'little')
            mask, shift, lmask, lshift, consumed = tables[word & 7]
            offset, length = (word & mask) >> shift, ((word >> lshift) & lmask)+3
            if cursor+consumed > len(data) or len(out)+length > expected:
                raise ValueError('invalid pool match extent')
            cursor += consumed
            if offset > len(out):
                raise ValueError('invalid pool backreference')
            start = len(out)-offset
            for i in range(length):
                if offset == 0:
                    out.append(0)
                else:
                    source = start+i if offset >= 4 or i < 3 else start+i-(2+(offset & 1))
                    if not 0 <= source < len(out):
                        raise ValueError('invalid short pool backreference')
                    out.append(out[source])
        flags >>= 1
    return bytes(out)


def light_pools(data):
    pool = next(p for k, p in chunks(data) if k == b'POOL')
    offset, streams = 4, []
    for _ in range(3):
        compressed, expanded = struct.unpack_from('<II', pool, offset)
        offset += 8
        if expanded > 512*1024*1024 or offset+compressed > len(pool):
            raise ValueError('invalid pool size')
        block = pool[offset:offset+compressed]
        streams.append(block if compressed == expanded else decompress_pool(block, expanded))
        offset += compressed
    if offset != len(pool):
        raise ValueError('unconsumed pool data')
    return streams


def face_space(p, face):
    x, y, z = p.T
    return np.stack([(-z, -y, x), (z, -y, -x), (x, z, y),
                     (x, -z, -y), (x, -y, z), (-x, -y, -z)][face], -1)


def clip(poly):
    # Clip BEFORE perspective division, including face borders. This avoids
    # the giant inverted triangles produced by the earlier guessed importer.
    for plane in ((0, 0, 1, -1e-5), (1, 0, 1, 0), (-1, 0, 1, 0),
                  (0, 1, 1, 0), (0, -1, 1, 0)):
        out = []
        for a, b in zip(poly, np.roll(poly, -1, axis=0)):
            da, db = np.dot(a[:3], plane[:3])+plane[3], np.dot(b[:3], plane[:3])+plane[3]
            if da >= 0:
                out.append(a)
            if (da >= 0) != (db >= 0):
                out.append(a + (b-a) * (da/(da-db)))
        poly = np.asarray(out)
        if len(poly) < 3:
            return []
    return poly


def triangle(image, v, texture=None):
    side = image.shape[0]
    p = (v[:, :2] / v[:, 2:3] + 1) * (.5 * side)
    lo = np.maximum(np.floor(p.min(0)).astype(int), 0)
    hi = np.minimum(np.ceil(p.max(0)).astype(int), side)
    if np.any(lo >= hi):
        return
    a, b, c = p
    denom = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    if abs(denom) < 1e-8:
        return
    x, y = np.meshgrid(np.arange(lo[0], hi[0])+.5, np.arange(lo[1], hi[1])+.5)
    w1 = ((x-a[0])*(c[1]-a[1])-(y-a[1])*(c[0]-a[0])) / denom
    w2 = ((b[0]-a[0])*(y-a[1])-(b[1]-a[1])*(x-a[0])) / denom
    weights = np.stack([1-w1-w2, w1, w2], -1)
    inside = np.all(weights >= -1e-7, -1)
    q = weights / v[:, 2]
    rgba = (q @ v[:, 3:]) / q.sum(-1)[..., None]
    if texture is not None:
        uv = rgba
        px = (uv[..., 0] % 1) * texture.shape[1]-.5
        py = np.clip(uv[..., 1], 0, 1) * texture.shape[0]-.5
        ix, iy = np.floor(px).astype(int), np.floor(py).astype(int)
        fx, fy = (px-ix)[..., None], (py-iy)[..., None]
        xa, xb = ix % texture.shape[1], (ix+1) % texture.shape[1]
        ya, yb = np.clip(iy, 0, texture.shape[0]-1), np.clip(iy+1, 0, texture.shape[0]-1)
        rgba = ((texture[ya, xa]*(1-fx)+texture[ya, xb]*fx)*(1-fy) +
                (texture[yb, xa]*(1-fx)+texture[yb, xb]*fx)*fy)
    target = image[lo[1]:hi[1], lo[0]:hi[0]]
    alpha = np.clip(rgba[..., 3:4], 0, 1)
    target[inside] = (rgba[..., :3]*alpha + target*(1-alpha))[inside]


def render(parts, sprites, stars, texture_root, side, faces=None):
    if faces is None:
        faces = [np.zeros((side, side, 3), np.float32) for _ in range(6)]
    for face, image in enumerate(faces):
        for pos, color, indices in parts:
            vertices = np.column_stack([face_space(pos, face), color])
            for indices3 in indices:
                poly = clip(vertices[indices3])
                for k in range(1, len(poly)-1):
                    triangle(image, poly[[0, k, k+1]])
    # Sprite billboards are centered on their authored sky direction, not the
    # game camera. Evaluate every cube face to preserve cross-boundary sprites.
    coords = (np.arange(side)+.5)*(2/side)-1
    from import_remastered_sky import face_directions
    u, v = np.meshgrid(coords, coords)
    directions = [face_directions(f, u, v) for f in range(6)]
    directions = [d / np.linalg.norm(d, axis=-1)[..., None] for d in directions]
    for name, rows in sprites:
        matches = [p for p in texture_root.iterdir() if p.name.lower() == name.lower()]
        if len(matches) != 1:
            raise ValueError(f'missing sprite {name}')
        texture = np.asarray(Image.open(matches[0]).convert('RGBA'), np.float32)/255
        for row in rows:
            center = row[:3]; radius = np.linalg.norm(center)
            forward = center / radius
            right = np.cross(forward, [0, 1, 0])
            if np.linalg.norm(right) < 1e-5:
                right = np.cross(forward, [1, 0, 0])
            right /= np.linalg.norm(right)
            up = np.cross(right, forward)
            for image, d in zip(faces, directions):
                facing = d @ forward
                tx = (d @ right) / np.maximum(facing, 1e-8) * radius / max(row[3], 1e-6) + .5
                ty = -(d @ up) / np.maximum(facing, 1e-8) * radius / max(row[3], 1e-6) + .5
                mask = (facing > 0) & (tx >= 0) & (tx < 1) & (ty >= 0) & (ty < 1)
                if not mask.any():
                    continue
                texel = texture[(ty[mask]*texture.shape[0]).astype(int), (tx[mask]*texture.shape[1]).astype(int)]
                image[mask] += texel[:, :3]*texel[:, 3:4]*row[4:7]*row[7]
    # Point stars remain in a separate catalog for the native batched star pass.
    # Do not invent replacement stars or reuse the unrelated HW1 BTG catalog.
    return [srgb_to_linear(np.clip(f, 0, None)) for f in faces]


def remastered_sky(data, side):
    """Decode the actual visible RM sky: POOL + LMIP + PRE_Space MULT.

    The older ezNN.hod colored mesh is deliberately not used for this layer.
    Spherical UVs come from the source mesh, so no guessed face orientation.
    """
    textures, mesh_pool, face_pool = light_pools(data)
    hvmd = next(p for k, p in chunks(data) if k == b'HVMD')
    blocks = list(chunks(hvmd))
    lmip = next(p for k, p in blocks if k == b'LMIP')
    n, = struct.unpack_from('<I', lmip)
    if lmip[n+4:n+8] != b'8888':
        raise ValueError('sky texture must be RGBA8888')
    mips, width, height = struct.unpack_from('<III', lmip, n+8)
    if not 1 <= mips <= 16 or not 1 <= width <= 16384 or not 1 <= height <= 16384:
        raise ValueError('invalid sky texture dimensions')
    texture = np.frombuffer(textures, np.uint8, width*height*4).reshape(height, width, 4).astype(np.float32)/255
    mult = next(p for k, p in blocks if k == b'MULT')
    n, = struct.unpack_from('<I', mult, 4)
    if mult[8:8+n] != b'PRE_Space':
        raise ValueError('first mesh is not the sky dome')
    offset = 8+n
    n, = struct.unpack_from('<I', mult, offset)
    offset += 4+n
    count, = struct.unpack_from('<I', mult, offset)
    if count != 1:
        raise ValueError('unexpected sky LOD count')
    offset += 4
    bmsh = next(p for k, p in chunks(mult[offset:]) if k == b'BMSH')
    version, = struct.unpack_from('>I', bmsh)
    lod, parts, material, mask, vertices, marker, indices = struct.unpack_from('<IIIIIhI', bmsh, 4)
    if version not in (1400, 1401) or lod != 0 or parts != 1 or material != 0 or marker != -1 or mask not in (11, 27):
        raise ValueError('unsupported Remastered sky mesh layout')
    stride = 12 if mask == 27 else 10
    if vertices*stride*4 > len(mesh_pool) or indices*2 > len(face_pool) or indices % 3:
        raise ValueError('truncated pooled sky mesh')
    vertex = np.frombuffer(mesh_pool, '<f4', vertices*stride).reshape(-1, stride)
    index = np.frombuffer(face_pool, '<u2', indices).reshape(-1, 3)
    if not np.all(np.isfinite(vertex)) or np.any(index >= vertices):
        raise ValueError('invalid pooled mesh values')
    faces = [np.zeros((side, side, 3), np.float32) for _ in range(6)]
    for face, image in enumerate(faces):
        data = np.column_stack([face_space(vertex[:, :3], face), vertex[:, 8:10]])
        for tri in index:
            poly = clip(data[tri])
            for k in range(1, len(poly)-1):
                triangle(image, poly[[0, k, k+1]], texture)
    return faces, dict(texture_size=[width, height], sky_vertices=vertices,
                       sky_triangles=indices//3, foreground_meshes=sum(k == b'MULT' for k, _ in blocks)-1)


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument('--source', type=Path, required=True)
    cli.add_argument('--light-source', type=Path, required=True)
    cli.add_argument('--mission', type=int, default=2)
    cli.add_argument('--output', type=Path, required=True)
    cli.add_argument('--encoder', type=Path, required=True)
    cli.add_argument('--side', type=int, default=1024)
    cli.add_argument('--foreground-scene', type=Path)
    args = cli.parse_args()
    if not 64 <= args.side <= 4096:
        raise ValueError('side must be 64..4096')
    if not 1 <= args.mission <= 16:
        raise ValueError('mission must be 1..16')
    data = args.source.read_bytes()
    parts, sprites, stars = parse(data)
    print(f'Validated {len(parts)} mesh parts, {sum(len(p[2]) for p in parts)} triangles, '
          f'{sum(len(s[1]) for s in sprites)} sprites, {len(stars)} point stars', flush=True)
    args.output.mkdir(parents=True, exist_ok=True)
    faces, light_info = remastered_sky(args.light_source.read_bytes(), args.side)
    if light_info['foreground_meshes'] and (not args.foreground_scene or
            not args.foreground_scene.is_file() or args.foreground_scene.read_bytes()[:8] != b'RMSC3\0\0\0'):
        raise ValueError('Mission has foreground meshes: full scene translation is required; '
                         'refusing to publish a sky with missing planets/nebula layers')
    faces = pad_faces(render([], sprites, stars, args.source.parent.parent/'stars', args.side, faces))
    atlas = convert(faces, args.side*2, lambda d: d)
    stem = f'mission{args.mission:02d}'
    write_hwfloat(args.output/(stem+'.hwf6'), atlas)
    subprocess.run([str(args.encoder.resolve()), str((args.output/(stem+'.hwf6')).resolve()),
                    str((args.output/(stem+'_sky.dds')).resolve())], check=True)
    # Encoder scratch, not a runtime asset; do not fill each user cache with
    # sixteen uncompressed float atlases after the validated DDS is available.
    (args.output/(stem+'.hwf6')).unlink()
    u, v = np.meshgrid((np.arange(1536)+.5)/1536, (np.arange(768)+.5)/768)
    theta, phi = .5*np.pi-u*2*np.pi, v*np.pi
    d = np.stack([np.cos(theta)*np.sin(phi), np.sin(theta)*np.sin(phi), np.cos(phi)], -1)
    preview = sample_cube(faces, d)
    Image.fromarray(np.uint8(np.clip(linear_to_srgb(preview), 0, 1)*255+.5)).save(args.output/(stem+'_preview.png'))
    if not np.all(np.isfinite(stars)):
        raise ValueError('invalid star catalog')
    with (args.output/(stem+'_stars.bin')).open('wb') as stream:
        stream.write(b'RMS1'+struct.pack('<I', len(stars)))
        stream.write(stars.astype('<f4').tobytes())
    report = dict(source=str(args.source), sha256=hashlib.sha256(data).hexdigest(),
                  mesh_parts=len(parts), triangles=sum(len(p[2]) for p in parts),
                  sprites=sum(len(s[1]) for s in sprites), point_stars=len(stars),
                  remastered=light_info,
                  limitations=['Remastered bloom/washout timing uses the host engine'], lighting_changed=False)
    (args.output/(stem+'_translation.json')).write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
