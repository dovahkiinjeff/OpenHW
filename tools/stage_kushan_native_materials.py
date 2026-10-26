"""Stage R1-only native HOD surfaces; never activates ships or writes deployment.

Policies translate source texture semantics, not the complete RM lighting model.
Opaque neutral vertex-colored surfaces retain their authored tint; unsupported
untextured surfaces remain explicit blockers, never opaque fallbacks.
"""
import argparse
import hashlib
import json
import struct
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from native_hod_surfaces import NativeSurfaces, load_reader
from import_remastered_interceptor import (dds_rgba, remastered_paint_shade,
                                         remastered_surface_masks)


def stage_active_engine_variant(material, cache, folder, label):
    """Preserve native thruster On pages beside the default Off material."""
    if material['shader'] != 'thruster':
        return
    variants = {v['state']: v for v in material['variants']}
    if 'On' not in variants or 'Off' not in variants:
        raise ValueError('Thruster requires both On and Off variants')
    for kind, record in variants['On']['files'].items():
        source = (cache / record['path']).resolve()
        if not source.is_relative_to(cache.resolve()) or hashlib.sha256(source.read_bytes()).hexdigest() != record['sha256']:
            raise ValueError('Unsafe/stale active thruster texture: ' + str(source))
        suffix = '.lif' if kind == 'bootstrap' else '.dds' if kind == 'diffuse' else '_' + kind + '.dds'
        shutil.copy2(source, folder / (label + '_engineOn' + suffix))


def native_material_controls(shader, size, glow=None, team=None, spec=None):
    """Linear AO/roughness/dielectric-strength/validity, never metallic paint.

    Translate sob_ship.frag and sob_share_materials.imp's default gloss lobe
    into roughness. GLOW R is sharpness; max(B-G,0) is specular strength.
    TEAM also changes gloss: painted panels are not polished bare metal.
    """
    def rgb(image, fallback):
        image = image if image is not None else Image.new('RGB', size, fallback)
        return np.asarray(image.convert('RGB').resize(size, Image.Resampling.BILINEAR), dtype=np.float32)/255.0
    packed = rgb(spec if shader in ('shipglow', 'badgeglow') else glow, (0,0,0))
    retention = rgb(team, (255,255,255))
    painted = 1.0-retention[:,:,0]*retention[:,:,1]*(1.0-retention[:,:,2])
    if shader in ('matte', 'matteAlpha', 'matteScissor'):
        painted = retention[:,:,2]
    gloss = np.maximum(1.0, np.power(packed[:,:,0], .1+1.8*painted)*(75-115*painted))+30-25*painted
    roughness = np.sqrt(2.0/(gloss+2.0))
    # SOB_GLOWRGB replaces G with 1 before subtracting it from B.
    glow_mask = 1.0 if shader in ('shipglow', 'badgeglow') else packed[:,:,1]
    strength = np.maximum(0.0, packed[:,:,2]-glow_mask)*(1.0-.12*painted)
    result = np.ones((*strength.shape,4), dtype=np.float32)
    result[:,:,1] = roughness
    result[:,:,2] = strength
    return Image.fromarray(np.uint8(np.rint(np.clip(result,0,1)*255)), 'RGBA')


def translate_surface(shader, diffuse, glow=None, team=None, spec=None):
    """Return full-resolution diffuse, emission and HW replacement paint masks."""
    if shader == 'Ship': shader = 'ship'  # Older authored derelict material spelling.
    if shader == 'shipglow_ns': shader = 'shipglow'  # Same separate RGB glow/spec texture channels.
    supported = {'ship', 'badge', 'bay', 'shipglow', 'badgeglow', 'thruster', 'matte', 'matteAlpha', 'matteScissor'}
    if shader not in supported:
        raise ValueError(f'Unsupported surface shader: {shader}')
    size = diffuse.size
    def aligned(image, fallback):
        if image is None:
            return Image.new('RGB', size, fallback)
        return image.convert('RGB').resize(size, Image.Resampling.BILINEAR)
    retention = aligned(team, (255, 255, 255))
    packed_glow = aligned(glow, (0, 0, 0))
    mask, paint = remastered_surface_masks(packed_glow, retention)
    diffuse = diffuse.convert('RGBA').copy()
    if shader == 'matteScissor':
        if team is None:
            raise ValueError('Scissor requires source TEAM R coverage')
        # sob_ship.frag discards TEAM R < .5; this is NOT a paint channel.
        coverage = np.asarray(retention.getchannel('R')) >= 128
        diffuse.putalpha(Image.fromarray(np.uint8(coverage)*255))
        paint = Image.new('RGB', size)
        shade = Image.new('L', size, 255)
    else:
        if shader!='matteAlpha': diffuse.putalpha(255)  # Badge alpha is decal coverage, not hull holes.
        shade = remastered_paint_shade(diffuse, retention)
        if shader in ('matte','matteAlpha'):
            # SHIP_Matte uses the same packed G glow but no SOB_TEAM paint.
            paint = Image.new('RGB', size)
            shade = Image.new('L', size, 255)
    if shader in {'shipglow', 'badgeglow'}:
        emission = packed_glow  # SOB_GLOWRGB: separate RGB glow, spec is separate.
    else:
        strength = np.asarray(mask.getchannel('R'), dtype=np.float32)/255
        rgb = np.asarray(diffuse.convert('RGB'), dtype=np.float32)
        emission = Image.fromarray(np.uint8(np.rint(rgb*strength[:, :, None])))
    emission = emission.convert('RGBA')
    # Alpha is the lighting-protection mask, not opaque hull coverage.
    lit = np.any(np.asarray(emission)[:, :, :3] != 0, axis=2)
    emission.putalpha(Image.fromarray(np.where(lit,
        np.asarray(diffuse.getchannel('A')), 0).astype(np.uint8)))
    return {'diffuse': diffuse, 'emissive': emission, 'paintShade': shade,
            'material': native_material_controls(shader, size, glow, team, spec),
            'teamEffect0': paint.getchannel('R'), 'teamEffect1': paint.getchannel('G')}


def select_static_material_variant(variants):
    """Base material is idle; native thrusters also stage an _engineOn sibling."""
    return next((v for v in variants if v['state'] == 'Off'), variants[0])


def lif_rgba(surface):
    """Legacy bootstrap texture with per-pixel paint and real cutout alpha.

    No fake full-ambient palette entries: modern emission comes from its own DDS.
    Full source resolution stays in DDS; this bootstrap stays at most 512px.
    """
    diffuse = surface['diffuse']
    ratio = min(1, 512/max(diffuse.size))
    size = tuple(max(1, int(d*ratio)) for d in diffuse.size)
    pixels = diffuse.resize(size, Image.Resampling.LANCZOS).convert('RGBA').tobytes()
    effects = [surface[f'teamEffect{i}'].resize(size, Image.Resampling.BILINEAR).tobytes()
               for i in range(2)]
    flags = 0x30 | (8 if diffuse.getchannel('A').getextrema()[0] < 255 else 0)
    return (struct.pack('<8s10I', b'Willy 7\0', 0x104, flags, *size, 0, 0,
                        48, 0, 48+len(pixels), 48+len(pixels)+len(effects[0]))
            + pixels + effects[0] + effects[1])

def neutral_vertex_surface(colors):
    if not colors or any((v&255)!=255 or (v>>24)!=((v>>16)&255) or (v>>24)!=((v>>8)&255) for v in colors):
        raise ValueError('Unsupported non-neutral/transparent untextured source color')
    ramp=Image.new('RGBA',(256,1));ramp.putdata([(v,v,v,255) for v in range(256)])
    return translate_surface('ship',ramp)


def stage(reader, source, plan, destination, allowed_races=('R1',)):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if not destination.is_relative_to(source.parent / 'material-cache'):
        raise ValueError('Output must be inside the staging material-cache sibling')
    paths = set()
    for ship in plan['ships']:
        if ship['classic_race'] not in allowed_races:
            raise ValueError('Material plan race is outside explicitly selected scope')
        paths.add(ship['hull'])
        paths.update(a['model'] for a in ship['attachments'] if a.get('source_visible'))
    manifest = {'schema': 1, 'runtime_enabled': False, 'materials': [],
                'requires_explicit_emission_and_cutout_runtime_binding': True}
    for relative in sorted(paths):
        path = (source / relative).resolve()
        if not path.is_relative_to(source):
            raise ValueError('Model path escapes staging source')
        source_hash = hashlib.sha256(path.read_bytes()).hexdigest()
        with NativeSurfaces(reader, path) as surfaces:
            for material in surfaces.materials():
                shader, parameters = material['shader'], material['parameters']
                record = dict(model=relative, source_sha256=source_hash,
                              **material, variants=[])
                manifest['materials'].append(record)
                if shader in ('innateSS','innatess'):
                    record['policy'] = 'effect-proxy-no-default-opaque-surface'
                    continue
                neutral = shader=='default' and not parameters
                if neutral:
                    colors=surfaces.vertex_tints(material['index'])
                    if not colors:
                        record['policy']='unused-source-material';continue
                    if len(set(colors))==1 and (colors[0]&255)==255:
                        v=colors[0]
                        translated=translate_surface('ship',Image.new('RGBA',(256,1),
                            (v>>24,(v>>16)&255,(v>>8)&255,255)))
                    else:
                        translated=neutral_vertex_surface(colors)
                elif not parameters:
                    record['policy'] = 'blocked-untextured-source-material'
                    continue
                record['policy'] = ('authored-neutral-vertex-tint-not-activated'
                                    if neutral else 'staged-not-activated')
                variants = ['On', 'Off'] if shader == 'thruster' else ['']
                for variant in variants:
                    def image_parameter(name, required=False):
                        index = parameters.get(name)
                        if index is None:
                            if required:
                                raise ValueError(f'{relative}:{material["name"]} missing {name}')
                            return None
                        return surfaces.image(index)
                    if not neutral:
                        diffuse = image_parameter('$diffuse'+variant, True)
                        glow = image_parameter('$glow'+variant)
                        team = image_parameter('$team')
                        translated = translate_surface(shader, diffuse, glow, team, image_parameter('$spec'))
                    for name in ('$normal', '$spec'):
                        image = image_parameter(name)
                        if image is not None:
                            translated[name[1:]] = image
                    # Stable model-relative namespace; materials with identical names do not collide.
                    prefix = Path(relative).with_suffix('') / f'm{material["index"]}{variant}'
                    files = {}
                    for name, image in translated.items():
                        suffix = '' if name == 'diffuse' else '_'+name
                        output = destination / f'{prefix}{suffix}.dds'
                        output.parent.mkdir(parents=True, exist_ok=True)
                        data = dds_rgba(image)
                        output.write_bytes(data)
                        files[name] = dict(path=output.relative_to(destination).as_posix(),
                                           sha256=hashlib.sha256(data).hexdigest(), size=list(image.size))
                    bootstrap = lif_rgba(translated)
                    lif_path = destination / f'{prefix}.lif'
                    lif_path.write_bytes(bootstrap)
                    files['bootstrap'] = dict(path=lif_path.relative_to(destination).as_posix(),
                                              sha256=hashlib.sha256(bootstrap).hexdigest())
                    record['variants'].append(dict(state=variant or 'Hull', files=files))
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = stage(load_reader(args.reader), args.source,
                   json.loads(args.plan.read_text()), args.output)
    print(f'Staged {len(result["materials"])} material records; fleet activation remains disabled.')


if __name__ == '__main__':
    main()
