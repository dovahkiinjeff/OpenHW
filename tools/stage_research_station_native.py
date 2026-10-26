"""Stage the owned Remastered Taiidan research station visual; preserve gameplay verbatim."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
from PIL import Image
import numpy as np
import struct
from import_remastered_interceptor import dds_rgba
from extract_remastered_big import extract
from extract_vanilla_ship_textures import read_archive, entry_bytes
from native_hod_surfaces import NativeSurfaces, load_reader
from stage_kushan_native_materials import stage as materials, select_static_material_variant
from stage_kushan_native_packages import classic_bounds, calibrated_scale


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('reader','classic-archive','game-root','output'):
        parser.add_argument('--'+key,type=Path,required=True)
    inputs=parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--archive',type=Path)
    inputs.add_argument('--source-root',type=Path)
    parser.add_argument('--package-only',action='store_true')
    args=parser.parse_args();root=args.output.resolve()
    if root.name!='hwrm-researchstation-native' or root.parent.name!='out':
        raise ValueError('Use the dedicated local Research station cache')
    source=root/'source';model='ship/tai_researchstation/tai_researchstation.hod'
    if args.source_root:
        shutil.copytree(args.source_root/'ship/tai_researchstation',source/'ship/tai_researchstation',dirs_exist_ok=True)
    else:extract(args.archive,source,('ship/tai_researchstation/',))
    reader=load_reader(args.reader)
    with NativeSurfaces(reader,source/model) as native:
        bounds=native.hull_bounds();joints=native.joints()
    blob,entries=read_archive(args.classic_archive)
    lookup={e.name.lower():e for e in entries}
    key='traders\\researchstation\\rl0\\lod0\\researchstation'
    original=lookup.get(key+'.peo') or lookup.get(key+'.geo')
    if original is None:raise ValueError('Classic Research station calibration mesh missing')
    old=classic_bounds(entry_bytes(blob,original));scale,axis=calibrated_scale(old,bounds)
    plan={'ships':[dict(classic_race='Traders',classic_type='ResearchStation',
        source='tai_researchstation',hull=model,attachments=[])]}
    cache=root/'material-cache/v1'
    report=json.loads((cache/'manifest.json').read_text()) if args.package_only else materials(reader,source,plan,cache,allowed_races=('Traders',))
    folder=root/'package-cache/v2/Traders/ResearchStation/RMNative';folder.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source/model,folder/'hull.hod')
    lines=['type ResearchStation',f'scale {scale:.9g}','hull hull.hod','mad -']
    # Keep the classic weapon/engine markers until separately reviewed; this
    # package changes geometry/materials only, never Lua gameplay settings.
    shared_materials={}
    for material in report['materials']:
        variants=material['variants']
        if not variants:
            if material['policy']=='unused-source-material' or material['shader'].lower()=='innatess':continue
            raise ValueError('Unmapped Research station material')
        variant=select_static_material_variant(variants);label=f'c0_m{material["index"]}'
        # Bay and hull can refer to exactly the same translated source maps.
        # Share their texture identity as well as bytes; retain nonidentical maps.
        signature=tuple(sorted((kind,record['sha256']) for kind,record in variant['files'].items()))
        if signature in shared_materials:
            lines.append(f'surface 0 {material["index"]} {shared_materials[signature]}')
            continue
        shared_materials[signature]=label
        lines.append(f'surface 0 {material["index"]} {label}')
        for kind,record in variant['files'].items():
            path=(cache/record['path']).resolve()
            if not path.is_relative_to(cache.resolve()) or hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:
                raise ValueError('Unsafe or stale material file')
            suffix='.lif' if kind=='bootstrap' else '.dds' if kind=='diffuse' else '_'+kind+'.dds'
            target=folder/(label+suffix)
            if kind!='bootstrap':
                data=path.read_bytes();height,width=struct.unpack_from('<II',data,12)
                if data[:4]!=b'DDS ' or len(data)!=128+width*height*4:
                    raise ValueError('Expected staging RGBA8 DDS layout')
                pixels=np.frombuffer(data,dtype='<u4',offset=128)
                # Compare losslessly in the pipeline's RGBA8 layout, avoiding
                # a costly image-codec decode just to recognize a constant mask.
                if np.all(pixels==pixels[0]):
                    target.write_bytes(dds_rgba(Image.new('RGBA',(1,1),tuple(data[128:132]))))
                else:shutil.copy2(path,target)
            else:shutil.copy2(path,target)
    current=args.game_root/'Data/traders/researchstation/rmnative/ship.owshipdef'
    text=current.read_text(encoding='latin1')
    if not text.startswith('OPENHW_SHIP_2') or '[Ship]' not in text:raise ValueError('Unrecognized installed definition')
    gameplay='[Ship]'+text.split('[Ship]',1)[1]
    (folder/'ship.owshipdef').write_text('OPENHW_SHIP_2\n'+'\n'.join(lines)+'\n\n'+gameplay,encoding='latin1')
    (folder/'visual.rmnative').write_text('HW_RM_TRADERS_VISUAL_1\n'+'\n'.join(lines)+'\n')
    audit=dict(source=model,source_sha256=hashlib.sha256((source/model).read_bytes()).hexdigest(),
        scale=scale,axis=axis,original_bounds=[v.tolist() for v in old],native_bounds=[v.tolist() for v in bounds],
        joints=joints,gameplay_sha256=hashlib.sha256(gameplay.encode('latin1')).hexdigest())
    (root/'manifest.json').write_text(json.dumps(audit,indent=2))
    print(f'Research station native package staged: scale {scale:.9g}; {len(report["materials"])} materials; gameplay retained')


if __name__=='__main__':main()
