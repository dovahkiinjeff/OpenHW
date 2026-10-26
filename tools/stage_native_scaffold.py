"""Stage only the intact Mission 1 scaffold, with no gameplay/stat imports."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

from extract_remastered_big import extract
from extract_vanilla_ship_textures import read_archive, entry_bytes
from native_hod_surfaces import NativeSurfaces, load_reader
from stage_kushan_native_materials import stage as stage_materials
from stage_kushan_native_packages import classic_bounds, calibrated_scale


def stage(reader_path, rm_archive, classic_archive, output):
    root = Path(output).resolve()
    if root.name != 'hwrm-scaffold-native' or root.parent.name != 'out':
        raise ValueError('Scaffold output must be its dedicated local out cache')
    source = root/'source'
    extract(rm_archive, source, ('ship/der_scaffold/',))
    relative = 'ship/der_scaffold/der_scaffold.hod'
    hull = source/relative
    reader = load_reader(reader_path)
    # Reuse the surface translator; this is not an R1 unit/roster import.
    plan = {'ships': [{'classic_race': 'R1', 'hull': relative, 'attachments': []}]}
    materials = root/'material-cache/v1'
    manifest = stage_materials(reader, source, plan, materials)
    blob, entries = read_archive(Path(classic_archive))
    classic = next(e for e in entries if e.name.lower() ==
                   'derelicts\\scaffold\\rl0\\lod0\\scaffold.peo')
    old = classic_bounds(entry_bytes(blob, classic))
    with NativeSurfaces(reader, hull) as native:
        bounds = native.hull_bounds()
    scale, axis = calibrated_scale(old, bounds)
    folder = root/'package-cache/v1/Derelicts/Scaffold/RMNative'
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copy2(hull, folder/'hull.hod')
    lines = ['HW_RM_SCAFFOLD_VISUAL_1', 'type Scaffold', f'scale {scale:.9g}',
             'hull hull.hod', 'mad -']
    for material in manifest['materials']:
        if not material['variants']:
            if material['shader'] != 'innateSS':
                raise ValueError('Unmapped scaffold material')
            continue
        if len(material['variants']) != 1:
            raise ValueError('Scaffold must not require animated engine variants')
        label = f'Hull_m{material["index"]}'
        lines.append(f'surface 0 {material["index"]} {label}')
        for key, record in material['variants'][0]['files'].items():
            file = materials/record['path']
            if hashlib.sha256(file.read_bytes()).hexdigest() != record['sha256']:
                raise ValueError('Stale scaffold material output')
            suffix = '.lif' if key == 'bootstrap' else '.dds' if key == 'diffuse' else '_'+key+'.dds'
            shutil.copy2(file, folder/(label+suffix))
    (folder/'visual.rmnative').write_text('\n'.join(lines)+'\n', encoding='ascii')
    report = {'type': 'Scaffold', 'source_hod': relative, 'scale': scale,
              'calibration_axis': axis, 'classic_bounds': [x.tolist() for x in old],
              'native_bounds': [x.tolist() for x in bounds],
              'gameplay_imported': False, 'source_sha256': hashlib.sha256(hull.read_bytes()).hexdigest()}
    (root/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('reader', 'rm-archive', 'classic-archive', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    stage(args.reader, args.rm_archive, args.classic_archive, args.output)
