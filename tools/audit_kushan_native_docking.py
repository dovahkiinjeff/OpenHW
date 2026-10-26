"""Compare native docking path samples with classic MEX points, read-only.

No nearest-point result is a binding: multiple paths share endpoints and their
travel direction/flags require explicit review before gameplay integration.
"""
import argparse
import json
import math
from pathlib import Path
from native_hod_surfaces import NativeSurfaces, load_reader
from plan_kushan_hod_bindings import R1_HULLS


def hw_position(position, scale):
    if len(position)!=3 or not math.isfinite(scale) or scale<=0:
        raise ValueError('Invalid native docking position/scale')
    result=[position[1]*scale,-position[0]*scale,position[2]*scale]
    if not all(math.isfinite(v) for v in result):
        raise ValueError('Nonfinite native docking position')
    return result


def compare(paths, markers, scale):
    result=[]
    for path in paths:
        record=dict(path)
        # RM uses "world" to mean the ship's source-model frame here. Preserve
        # non-world parented paths without guessing their hierarchy transform.
        if path['parent']!='world':
            record['status']='parent-transform-review-required'
            result.append(record)
            continue
        samples=[]
        for i,point in enumerate(path['points']):
            position=hw_position(point['position'],scale)
            candidates=sorted((dict(semantic=m.get('semantic_name',m['name']),
                distance=math.dist(position,m['position'])) for m in markers),
                key=lambda m:(m['distance'],m['semantic']))
            samples.append(dict(index=i,hw_position=position,
                nearest_classic=candidates[:3]))
        record['status']='unbound-audit-only'
        record['samples']=samples
        result.append(record)
    return result


def audit(reader, source, classic, packages):
    old={s['classic_type']:s for s in classic['ships']}
    output=dict(schema=1,runtime_bindings_changed=False,ships=[])
    for package in packages['ships']:
        name=package['classic_type'];model=R1_HULLS[name]
        with NativeSurfaces(reader,source/model/(model+'.hod')) as scene:
            paths=scene.dock_paths()
        markers=[m for m in old[name]['markers'] if m['type']=='Dok']
        output['ships'].append(dict(classic_type=name,scale=package['scale'],
            classic_docking_markers=markers,
            paths=compare(paths,markers,package['scale'])))
    return output


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('reader','source','classic','packages','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    result=audit(load_reader(args.reader),args.source,
        json.loads(args.classic.read_text()),json.loads(args.packages.read_text()))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    count=sum(len(s['paths']) for s in result['ships'])
    print(f'Audited {count} native docking paths across {len(result["ships"])} R1 identities; no runtime changes.')
