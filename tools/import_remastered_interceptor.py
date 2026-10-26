"""Local-only Kushan interceptor visual experiment; never modifies an archive.

Reads the installed HW1Ships.big, validates HOD2 pools, and writes a reversible
HW1 GEO/LOD override. Retains HW1 stats, weapon/docking MEX and save identity.
No source or converted game assets belong in a distributed release.
"""
import argparse
import json
import re
import struct
from pathlib import Path

import numpy as np
from PIL import Image

from extract_remastered_big import extract
from extract_vanilla_ship_textures import read_archive, entry_bytes
from inspect_remastered_scene import inspect_scene
from classic_mesh_layout import translate, HEADER, OBJECT
from translate_remastered_background import light_pools, triangle
from translate_remastered_scene import decode_texture, vertices_from, poses


def remastered_surface_masks(glow, team):
    # sob_ship.frag: mix(teamColor, diffuse, team.r), then
    # mix(stripeColor, paint, team.g). HW1 uses the opposite mask convention.
    # GLOW is packed specular sharpness / emission / specular strength, not RGB.
    channels = team.convert('RGB').split()
    base, stripe = (Image.fromarray(255-np.asarray(c)) for c in channels[:2])
    zero = Image.new('L', team.size)
    converted_team = Image.merge('RGB', (base, stripe, zero))
    emission = glow.convert('RGB').getchannel('G')
    return Image.merge('RGB', (emission, emission, emission)), converted_team


def remastered_paint_shade(diffuse, retention):
    # Preserve the source shader's diffuse-driven paint darkening (3*x-.5).
    # Only painted texels are modulated; unpainted hull stays exactly unchanged.
    rgb = np.asarray(diffuse.convert('RGB'), dtype=np.float32) / 255
    masks = np.asarray(retention.convert('RGB'), dtype=np.float32) / 255
    paint = 1-masks[:, :, 0]*masks[:, :, 1]
    shade = np.clip(rgb*3-.5, 0, 1)
    # Grayscale keeps arbitrary fleet hues, while retaining scratches/panels.
    shade = shade @ np.array([.299, .587, .114], dtype=np.float32)
    shade = 1-paint+paint*shade
    return Image.fromarray(np.uint8(np.rint(np.clip(shade, 0, 1)*255)), 'L')


def dds_rgba(image):
    image = image.convert('RGBA')
    w, h = image.size
    header = bytearray(128)
    header[:4] = b'DDS '
    struct.pack_into('<7I', header, 4, 124, 0x100f, h, w, w*4, 0, 1)
    struct.pack_into('<8I', header, 76, 32, 0x41, 0, 32,
                     0xff, 0xff00, 0xff0000, 0xff000000)
    struct.pack_into('<I', header, 108, 0x1000)
    return bytes(header) + image.tobytes()


def lif_palette(diffuse, glow, team):
    # Legacy raster still needs a LIF; the matching DDS preserves full source
    # diffuse detail. Reserve the first 64 palette entries for authored glow.
    if max(diffuse.size) > 512:
        ratio = 512 / max(diffuse.size)
        size = tuple(max(1, int(v*ratio)) for v in diffuse.size)
        diffuse = diffuse.resize(size, Image.Resampling.LANCZOS)
        glow = glow.resize(size, Image.Resampling.LANCZOS)
        team = team.resize(size, Image.Resampling.LANCZOS)
    rgb = np.asarray(diffuse.convert('RGB'))
    mask = np.asarray(glow.convert('RGB')).max(2) > 12
    bright = rgb.copy(); bright[~mask] = 0
    dark = rgb.copy(); dark[mask] = 0
    bright = Image.fromarray(bright).quantize(colors=64)
    dark = Image.fromarray(dark).quantize(colors=192)
    indexed = np.where(mask, np.asarray(bright), np.asarray(dark).astype(np.uint16)+64).astype('u1')
    colors = ((bright.getpalette()+[0]*192)[:64*3] +
              (dark.getpalette()+[0]*576)[:192*3])
    palette = np.concatenate((np.asarray(colors, 'u1').reshape(256, 3),
                              np.full((256, 1), 255, 'u1')), 1).tobytes()
    effects = []
    team = np.asarray(team.convert('RGB'))
    for channel in (0, 1):
        totals = np.bincount(indexed.ravel(), weights=team[:, :, channel].ravel(), minlength=256)
        counts = np.bincount(indexed.ravel(), minlength=256)
        effects.append(np.rint(totals / np.maximum(counts, 1)).astype('u1').tobytes())
    pixels = indexed.tobytes()
    w, h = diffuse.size
    return (struct.pack('<8s10I', b'Willy 7\0', 0x104, 0x32, w, h, 0, 0,
                        48, 48+len(pixels), 48+len(pixels)+1024, 48+len(pixels)+1280)
            + pixels + palette + effects[0] + effects[1])


def geo(parts, scale):
    # HWRM: X right/Y up/Z forward. HW1 mesh: X up/Y left/Z forward.
    axis = np.array([[0, 1, 0], [-1, 0, 0], [0, 0, 1]], dtype=float)
    vertices, normals, faces, texcoords, materials = [], [], [], [], []
    for material, source in parts:
        p = source[:, :3] @ axis.T * scale
        n = source[:, 3:6] @ axis.T
        lengths = np.linalg.norm(n, axis=1)
        if np.any(lengths < 1e-5):
            raise ValueError('missing source vertex normals')
        n /= lengths[:, None]
        offset = len(vertices)
        vertices.extend(p); normals.extend(n)
        faces.extend(np.arange(len(source)).reshape(-1, 3) + offset)
        texcoords.extend(source[:, 16:18].reshape(-1, 6))
        materials.extend([material] * (len(source)//3))
    vertices, normals, faces = map(np.asarray, (vertices, normals, faces))
    if len(vertices) > 65535:
        raise ValueError('HW1 16-bit vertex limit exceeded')
    fn = np.cross(vertices[faces[:, 1]]-vertices[faces[:, 0]],
                  vertices[faces[:, 2]]-vertices[faces[:, 0]])
    fn /= np.maximum(np.linalg.norm(fn, axis=1)[:, None], 1e-10)
    output = bytearray(68+112+2*32)
    def append(raw):
        start = len(output); output.extend(raw); return start
    def string(value):
        return append(value.encode('ascii')+b'\0')
    name = string('HWRM_Kushan_Interceptor_Test')
    for i, label in enumerate(('HWRM_Hull', 'HWRM_Engine')):
        mn, tn = string(label), string(label)
        struct.pack_into('<4IfIHBBI', output, 180+i*32, mn,
                         0xffffff, 0xffffff, 0, 1.0, tn, 2, 64, 0, 0)
    nv, nf = len(vertices), len(faces)
    v = append(b''.join(struct.pack('<3fi', *p, nf+i) for i, p in enumerate(vertices)))
    n = append(b''.join(struct.pack('<3fI', *p, 0) for p in np.concatenate((fn, normals))))
    t = append(b''.join(struct.pack('<i4H6fH2x', i, *f, materials[i], *texcoords[i], 0)
                        for i, f in enumerate(faces)))
    matrix = np.eye(4).flatten().tolist()
    OBJECT.pack_into(output, 68, name, 1, 0, 0, nv, nf, nv, nf,
                     v, n, t, 0, 0, 0, *matrix)
    HEADER.pack_into(output, 0, b'RMF97ba\0', 0x402, name, len(output)-68,
                     0, 2, 0, 180, 180, 1, b'\0'*24)
    translate(bytes(output))  # Same strict format/index validator as Cata import.
    return bytes(output), vertices, faces, np.asarray(texcoords), np.asarray(materials)


def run(rm_archive, classic_archive, output):
    output = output.resolve()
    extract(rm_archive, output/'source', ('ship/kus_interceptor/',))
    source = output/'source/ship/kus_interceptor/kus_interceptor.hod'
    data = source.read_bytes()
    info, pools = inspect_scene(data), light_pools(data)
    if len(info['meshes']) != 1 or info['meshes'][0]['lod'] != 0:
        raise ValueError('interceptor experiment requires the verified single-LOD HOD')
    transforms, _ = poses(info, {}, 0)
    mesh = info['meshes'][0]
    matrix = transforms[mesh['parent']][0]
    parts = []
    for part in mesh['parts']:
        material = info['materials'][part['material']]
        if material['shader'] not in ('ship', 'thruster', 'badge'):
            raise ValueError('unsupported ship surface '+material['shader'])
        vertices = vertices_from(part, pools[1], pools[2])
        vertices[:, :3] = vertices[:, :3] @ matrix[:3, :3].T + matrix[:3, 3]
        vertices[:, 3:6] = vertices[:, 3:6] @ matrix[:3, :3].T
        # Badge surfaces contain real hull triangles, not just a floating decal.
        # Their UV0 addresses the same hull textures; UV1 is the badge overlay.
        # Preserve the hull even before translating the customizable UV1 decal.
        parts.append((0 if material['shader'] == 'badge' else part['material'], vertices))
    blob, entries = read_archive(classic_archive)
    lookup = {e.name.lower(): e for e in entries}
    classic = entry_bytes(blob, lookup['r1\\heavyinterceptor\\rl0\\lod0\\heavyinterceptor.peo'])
    obj = OBJECT.unpack_from(classic, 68)
    oldp = np.ndarray((obj[4], 3), '<f4', buffer=classic, offset=obj[8], strides=(16, 4))
    rawp = np.concatenate([p[:, :3] for _, p in parts])
    scale = float(np.ptp(oldp[:, 2]) / np.ptp(rawp[:, 2]))
    converted, positions, faces, uv, mats = geo(parts, scale)
    root = output/'runtime/R1'
    target = root/'HeavyInterceptor/RMTest'
    target.mkdir(parents=True, exist_ok=True)
    (target/'HeavyInterceptor.geo').write_bytes(converted)
    # Use the same HWRM mesh at every existing LOD for this first visual test.
    # A distance transition must not change the ship back into its classic mesh.
    lod = entry_bytes(blob, lookup['r1\\heavyinterceptor.lod']).decode('latin1')
    lod = re.sub(r'HeavyInterceptor\\Rl0\\LOD[0-4]\\HeavyInterceptor.geo',
                 lambda _: r'HeavyInterceptor\RMTest\HeavyInterceptor.geo', lod, flags=re.I)
    (root/'HeavyInterceptor.lod').write_text(lod, encoding='ascii')
    images = [Image.frombytes('RGBA', tuple(t['mips'][0]), decode_texture(t, pools[0])) for t in info['textures']]
    rendered = []
    for i, label in enumerate(('HWRM_Hull', 'HWRM_Engine')):
        material = info['materials'][i]
        refs = {p['name']: struct.unpack('<I', bytes.fromhex(p['value_hex']))[0]
                for p in material['parameters'] if p['type'] == 5}
        diffuse = images[refs.get('$diffuse', refs.get('$diffuseOn'))].copy()
        diffuse.putalpha(255)
        glow = images[refs.get('$glow', refs.get('$glowOn'))]
        normal = images[refs['$normal']]
        # No team texture (thruster) means no fleet recoloring, hence white
        # retention masks in Remastered, not black replacement masks.
        team = images[refs['$team']] if '$team' in refs else Image.new('RGB', diffuse.size, 'white')
        paint_shade = remastered_paint_shade(diffuse, team)
        glow, team = remastered_surface_masks(glow, team)
        fallback = lif_palette(diffuse, glow, team)
        (target/(label+'.lif')).write_bytes(fallback)
        fallback_size = struct.unpack_from('<II', fallback, 16)
        (target/(label+'.dds')).write_bytes(dds_rgba(diffuse))
        (target/(label+'_normal.dds')).write_bytes(dds_rgba(normal))
        (target/(label+'_paintShade.dds')).write_bytes(dds_rgba(paint_shade))
        for channel in (0, 1):
            mask = team.getchannel(('R', 'G')[channel]).resize(fallback_size, Image.Resampling.LANCZOS)
            (target/(label+f'_teamEffect{channel}.dds')).write_bytes(dds_rgba(Image.merge('RGBA', (mask, mask, mask, Image.new('L', mask.size, 255)))))
        rendered.append(np.asarray(diffuse, dtype=float)/255)
    # UV-aware software preview: painter ordering is for inspection only.
    eye = np.array([.65, -.8, 1.8]); eye /= np.linalg.norm(eye)
    right = np.cross([1, 0, 0], eye); right /= np.linalg.norm(right)
    up = np.cross(eye, right)
    camera = positions @ np.stack((right, up, -eye), 1)
    camera[:, 2] += np.linalg.norm(positions, axis=1).max()*3
    camera[:, :2] *= 2.4
    preview = np.zeros((768, 768, 3)); preview[:] = [.03, .045, .075]
    for index in np.argsort(camera[faces, 2].mean(1))[::-1]:
        triangle(preview, np.concatenate((camera[faces[index]], uv[index].reshape(3, 2)), 1), rendered[mats[index]])
    Image.fromarray(np.uint8(np.clip(preview*255, 0, 255))).save(output/'preview.png')
    report = dict(source_sha256=info['sha256'], triangles=len(faces), scale=scale,
                  bounds=[positions.min(0).tolist(), positions.max(0).tolist()],
                  source_textures=[dict(name=t['name'], dimensions=t['mips'][0]) for t in info['textures']],
                  collision=info['collisions'], badge_hull_preserved=True, badge_overlay_omitted=True,
                  gameplay='Unchanged: original SHP/MEX, ship identity and weapons',
                  lods='Same source mesh in slots 0–4 for the initial test')
    (output/'report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rm-archive', type=Path, required=True)
    parser.add_argument('--classic-archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.rm_archive, args.classic_archive, args.output)
