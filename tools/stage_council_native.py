"""Stage the owned T-Mat Council megaship visual without changing gameplay."""
import argparse
import hashlib
import json
import shutil
import struct
from pathlib import Path
import numpy as np
from PIL import Image
from import_remastered_interceptor import dds_rgba
from native_hod_surfaces import NativeSurfaces, load_reader
from stage_kushan_native_materials import stage as materials, select_static_material_variant
from stage_kushan_native_packages import classic_bounds, calibrated_scale


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    for key in ('reader','source-root','game-root','output'):
        cli.add_argument('--'+key,type=Path,required=True)
    cli.add_argument('--package-only',action='store_true')
    args=cli.parse_args();root=args.output.resolve()
    if root.name!='hwrm-council-native' or root.parent.name!='out':
        raise ValueError('Use the dedicated local Council staging cache')
    model='ship/misc_t-matmegaship/misc_t-matmegaship.hod'
    source=root/'source';(source/model).parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(args.source_root/model,source/model)
    reader=load_reader(args.reader)
    with NativeSurfaces(reader,source/model) as native:
        bounds=native.hull_bounds();joints=native.joints()
    old=classic_bounds((args.game_root/'Data/p3/p3megaship/rl0/lod0/p3megaship.peo').read_bytes())
    scale,axis=calibrated_scale(old,bounds)
    plan={'ships':[dict(classic_race='P3',classic_type='P3Megaship',source='misc_t-matmegaship',hull=model,attachments=[])]}
    cache=root/'material-cache/v1'
    report=json.loads((cache/'manifest.json').read_text()) if args.package_only else materials(reader,source,plan,cache,allowed_races=('P3',))
    folder=root/'package-cache/v1/P3/P3Megaship/RMNative';folder.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source/model,folder/'hull.hod')
    lines=['type P3Megaship',f'scale {scale:.9g}','hull hull.hod','mad -']
    for material in report['materials']:
        if not material['variants']:
            if material['policy']=='unused-source-material' or material['shader'].lower()=='innatess':continue
            raise ValueError('Unmapped Council material')
        variant=select_static_material_variant(material['variants']);label=f'c0_m{material["index"]}'
        lines.append(f'surface 0 {material["index"]} {label}')
        for kind,record in variant['files'].items():
            path=(cache/record['path']).resolve()
            if not path.is_relative_to(cache.resolve()) or hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:
                raise ValueError('Stale or unsafe material input')
            suffix='.lif' if kind=='bootstrap' else '.dds' if kind=='diffuse' else '_'+kind+'.dds'
            target=folder/(label+suffix)
            if kind=='bootstrap':shutil.copy2(path,target);continue
            data=path.read_bytes();height,width=struct.unpack_from('<II',data,12)
            if data[:4]!=b'DDS ' or len(data)!=128+width*height*4:raise ValueError('Expected translated RGBA8 DDS')
            pixels=np.frombuffer(data,dtype='<u4',offset=128)
            if np.all(pixels==pixels[0]):target.write_bytes(dds_rgba(Image.new('RGBA',(1,1),tuple(data[128:132]))))
            else:shutil.copy2(path,target)
    original=(args.game_root/'Data/p3/p3megaship/rmnative/ship.owshipdef').read_bytes()
    if not original.startswith(b'OPENHW_SHIP_2') or b'[Ship]' not in original:raise ValueError('Unrecognized installed ship definition')
    gameplay=b'[Ship]'+original.split(b'[Ship]',1)[1]
    (folder/'ship.owshipdef').write_bytes(('OPENHW_SHIP_2\n'+'\n'.join(lines)+'\n\n').encode('ascii')+gameplay)
    (folder/'visual.rmnative').write_text('HW_RM_P3_VISUAL_1\n'+'\n'.join(lines)+'\n')
    audit=dict(source=model,source_sha256=hashlib.sha256((source/model).read_bytes()).hexdigest(),scale=scale,axis=axis,
               original_bounds=[v.tolist() for v in old],native_bounds=[v.tolist() for v in bounds],joints=joints,
               gameplay_sha256=hashlib.sha256(gameplay).hexdigest())
    (root/'manifest.json').write_text(json.dumps(audit,indent=2))
    print(f'Council native hull staged: scale {scale:.9g}, {len(report["materials"])} materials; gameplay bytes unchanged')


if __name__=='__main__':main()
