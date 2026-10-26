"""Convert extracted Remastered DDS cube faces to the existing RTX sky format.

No HOD geometry or mission lighting is imported. Source assets stay local.
The output is a test asset, not a complete reproduction of RM's layered sky.
"""
import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from generate_mission_skies import PARABOLOID_EXTENT, write_hwfloat, srgb_to_linear

FACE_NAMES = ('posx', 'negx', 'posy', 'negy', 'posz', 'negz')


def cube_coordinates(direction):
    x, y, z = direction[..., 0], direction[..., 1], direction[..., 2]
    major = np.argmax(np.abs(direction), axis=-1)
    axis = np.take_along_axis(direction, major[..., None], axis=-1)[..., 0]
    face = major * 2 + (axis < 0)
    divisor = np.maximum(np.abs(axis), 1e-20)
    u = np.select([face == k for k in range(6)], [-z, z, x, x, x, -x]) / divisor
    v = np.select([face == k for k in range(6)], [-y, -y, z, -z, -y, -y]) / divisor
    return face, u, v


def face_directions(face, u, v):
    one = np.ones_like(u)
    values = [(one, -v, -u), (-one, -v, u), (u, one, v),
              (u, -one, -v), (u, -v, one), (-u, -v, -one)]
    return np.stack(values[face], axis=-1)


def pad_faces(faces):
    """One-texel cross-face gutters for bilinear samples at cube boundaries."""
    size = faces[0].shape[0]
    coord = (np.arange(-1, size + 1, dtype=np.float32) + .5) * (2 / size) - 1
    u, v = np.meshgrid(coord, coord)
    result = []
    for k in range(6):
        face, cu, cv = cube_coordinates(face_directions(k, u, v))
        ix = np.clip(((cu + 1) * .5 * size).astype(int), 0, size - 1)
        iy = np.clip(((cv + 1) * .5 * size).astype(int), 0, size - 1)
        padded = np.empty((size + 2, size + 2, 3), np.float32)
        for j in range(6):
            mask = face == j
            padded[mask] = faces[j][iy[mask], ix[mask]]
        padded[1:-1, 1:-1] = faces[k]
        result.append(padded)
    return result


def sample_cube(faces, direction):
    size = faces[0].shape[0] - 2
    face, u, v = cube_coordinates(direction)
    fx = (u + 1) * .5 * size + .5
    fy = (v + 1) * .5 * size + .5
    x, y = np.floor(fx).astype(int), np.floor(fy).astype(int)
    tx, ty = (fx - x)[..., None], (fy - y)[..., None]
    result = np.empty((*face.shape, 3), np.float32)
    for k in range(6):
        mask = face == k
        px, py = x[mask], y[mask]
        a = faces[k][py, px] * (1 - tx[mask]) + faces[k][py, px + 1] * tx[mask]
        b = faces[k][py + 1, px] * (1 - tx[mask]) + faces[k][py + 1, px + 1] * tx[mask]
        result[mask] = a * (1 - ty[mask]) + b * ty[mask]
    return result


def rm_direction(world):
    # Rotate Y-up Remastered into the engine's Z-up frame without mirroring.
    return world[..., [0, 2, 1]] * np.array([1, 1, -1], np.float32)


def convert(faces, side, direction_transform=rm_direction):
    out = np.empty((side, side * 2, 3), np.float32)
    for row in range(0, side, 64):
        end = min(row + 64, side)
        x = ((np.arange(side, dtype=np.float32) + .5) / side * 2 - 1) * PARABOLOID_EXTENT
        y = ((np.arange(row, end, dtype=np.float32) + .5) / side * 2 - 1) * PARABOLOID_EXTENT
        x, y = np.meshgrid(x, y)
        r2 = x * x + y * y
        for hemisphere in range(2):
            d = np.stack([2 * x, 2 * y, (1 - r2) * (1 if hemisphere == 0 else -1)], -1)
            d /= (1 + r2)[..., None]
            out[row:end, hemisphere * side:(hemisphere + 1) * side] = sample_cube(faces, direction_transform(d))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--stem', default='ez02')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--encoder', type=Path, required=True)
    parser.add_argument('--side', type=int, default=2048)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source_images = [Image.open(args.source / f'{args.stem}_hq_{name}.dds').convert('RGB')
                     for name in FACE_NAMES]
    if len({im.size for im in source_images}) != 1 or source_images[0].width != source_images[0].height:
        raise ValueError('all cube faces must be equal square dimensions')
    contact = Image.new('RGB', (768, 560))
    draw = ImageDraw.Draw(contact)
    for k, im in enumerate(source_images):
        col, row = k % 3, k // 3
        contact.paste(im.resize((256, 256)), (col * 256, row * 280 + 24))
        draw.text((col * 256 + 8, row * 280 + 6), FACE_NAMES[k], fill='white')
    contact.save(args.output / 'source_faces.png')
    faces = pad_faces([srgb_to_linear(np.asarray(im, np.float32) / 255) for im in source_images])
    image = convert(faces, args.side)
    if not np.all(np.isfinite(image)) or np.min(image) < 0:
        raise ValueError('invalid radiance')
    write_hwfloat(args.output / 'mission02.hwf6', image)
    subprocess.run([str(args.encoder), str(args.output / 'mission02.hwf6'),
                    str(args.output / 'mission02_sky.dds')], check=True)
    # Readable spherical preview; identical sampling to the runtime asset.
    u, v = np.meshgrid((np.arange(1536) + .5) / 1536, (np.arange(768) + .5) / 768)
    theta, phi = .5 * np.pi - u * 2 * np.pi, v * np.pi
    d = np.stack([np.cos(theta) * np.sin(phi), np.sin(theta) * np.sin(phi), np.cos(phi)], -1)
    preview = sample_cube(faces, rm_direction(d))
    from generate_mission_skies import linear_to_srgb
    Image.fromarray(np.uint8(np.clip(linear_to_srgb(preview), 0, 1) * 255 + .5)).save(
        args.output / 'mission02_panorama.png')
    report = {'source': str(args.source), 'face_size': source_images[0].width,
              'output_size': [args.side * 2, args.side], 'projection': 'dual-paraboloid',
              'lighting_changed': False, 'hod_geometry_imported': False,
              'note': 'Cube textures only; RM HOD foreground layers and flares are not included.'}
    (args.output / 'conversion.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
