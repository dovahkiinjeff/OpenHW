"""Stage supplemental material maps for the locally installed Acolyte and ACV.

Their native HODs omit normals and leave the hull and thruster specular masks
empty. The generated maps are local additions, derived from the installed art.
"""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageFilter

from import_remastered_interceptor import dds_rgba


def page(package, stem, size=None):
    with Image.open(package / f'{stem}.dds') as image:
        if size is not None and image.size != size:
            raise ValueError(f'Texture size mismatch: {package / stem}')
        return image.convert('RGBA')


def unorm(channel):
    return np.asarray(channel, dtype=np.float32) / 255.0


def texture(channels):
    rgba = np.stack(channels, axis=-1)
    return dds_rgba(Image.fromarray(np.uint8(np.rint(np.clip(rgba, 0, 1) * 255))))


def normal_page(luminance, paint_edges, emission):
    # Isolate panel-scale contrast. Suppress paint boundaries and lamps so
    # changing the fleet colors or switching on the engines adds no relief.
    scale = max(luminance.size) / 2048.0
    fine = unorm(luminance.filter(ImageFilter.GaussianBlur(max(0.5, scale))))
    broad = unorm(luminance.filter(ImageFilter.GaussianBlur(max(2.0, 12 * scale))))
    dy, dx = np.gradient(fine - broad)
    strength = 4.5 * (1.0 - paint_edges) * (1.0 - emission)
    x = -dx * strength
    y = -dy * strength
    length = np.hypot(x, y)
    limit = 0.40 / np.maximum(length, 0.40)
    x *= limit
    y *= limit
    z = 1.0 / np.sqrt(1.0 + x*x + y*y)
    return texture((0.5 + 0.5*x*z, 0.5 + 0.5*y*z, 0.5 + 0.5*z, np.ones_like(z)))


def material_page(luminance, paint, emission):
    # Painted panels are rougher, and emissive surfaces reflect less. Alpha
    # uses the native ship shader's [129, 253] environment-mask range.
    coverage = np.clip(3.0 * unorm(luminance), 0, 1) * (1.0 - 0.85 * emission)
    sharpness = (0.30 - 0.16*paint) * coverage
    roughness = 0.41 + 0.18*paint + 0.08*emission
    specular = (0.26 - 0.12*paint) * coverage
    environment = (129.0 + 124.0*0.8*specular) / 255.0
    return texture((sharpness, roughness, specular, environment))


def stage_ship(package, output):
    output.mkdir(parents=True, exist_ok=True)
    for stem in ('c0_m0', 'c0_m1', 'c0_m2'):
        diffuse = page(package, stem)
        luminance = diffuse.convert('L')
        paint_masks = [page(package, f'{stem}_teamEffect{index}', diffuse.size).getchannel('R')
                       for index in (0, 1)]
        paint = 1.0 - (1.0 - unorm(paint_masks[0])) * (1.0 - unorm(paint_masks[1]))
        edges = [ImageChops.subtract(mask.filter(ImageFilter.MaxFilter(5)),
                                    mask.filter(ImageFilter.MinFilter(5)))
                 for mask in paint_masks]
        paint_edges = np.clip(3.0 * unorm(ImageChops.lighter(*edges)), 0, 1)

        variants = (stem, f'{stem}_engineOn') if stem == 'c0_m1' else (stem,)
        emissions = {variant: page(package, f'{variant}_emissive', diffuse.size).convert('L')
                     for variant in variants}
        protected = emissions[stem]
        for variant in variants[1:]:
            protected = ImageChops.lighter(protected, emissions[variant])
        emission = np.clip(5.0 * unorm(protected.filter(ImageFilter.MaxFilter(3))), 0, 1)
        normal = normal_page(luminance, paint_edges, emission)

        for variant in variants:
            normal_name = f'{variant}_normal.dds'
            existing = package / normal_name
            if existing.is_file() and existing.read_bytes() != normal:
                raise ValueError(f'Existing normal map would be overwritten: {existing}')
            (output / normal_name).write_bytes(normal)
            if stem == 'c0_m2':
                continue  # This page already has authored reflection masks.
            material_name = f'{variant}_material.dds'
            material = material_page(luminance, paint, np.clip(5.0 * unorm(emissions[variant]), 0, 1))
            current = page(package, f'{variant}_material', diffuse.size).getextrema()
            if current[0] != (0, 0) or current[2] != (0, 0) or current[3] != (129, 129):
                if (package / material_name).read_bytes() != material:
                    raise ValueError(f'Existing reflection masks would be overwritten: {package / material_name}')
            (output / material_name).write_bytes(material)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True, help='Existing game installation')
    parser.add_argument('--output', type=Path, required=True, help='New staging directory')
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    for ship in ('sAcolyte', 'sACV'):
        relative = Path('r1') / ship / 'RMNative'
        stage_ship(args.root / relative, args.output / relative)
        print(f'Staged Acolyte/ACV material sidecars: {relative}', flush=True)
