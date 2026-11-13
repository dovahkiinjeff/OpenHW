"""Read installed native hull markers and classic emitter settings, without deployment."""
import argparse
import json
from pathlib import Path
from openhw_obig import validate
from native_hod_surfaces import NativeSurfaces, load_reader
from audit_classic_kushan_bindings import mex_markers


def audit(archive, reader_path, output):
    rows = validate(archive)
    output.mkdir(parents=True, exist_ok=True)
    reader = load_reader(reader_path)
    result = []
    with archive.open('rb') as source:
        def read(name):
            offset, size, _ = rows[name]
            source.seek(offset)
            return source.read(size)
        for race, hull in (('r1', 'resourcecollector'), ('r1', 'lightinterceptor'), ('r2', 'lightinterceptor')):
            prefix = f'{race}/{hull}'
            path = output/f'{race}-{hull}.hod'
            path.write_bytes(read(f'{prefix}/rmnative/hull.hod'))
            manifest = read(f'{prefix}/rmnative/visual.rmnative').decode()
            with NativeSurfaces(reader, path) as native:
                joints = [j for j in native.joints() if 'engine' in j['name'].lower() or 'navlight' in j['name'].lower()]
                posed = native.marker_transforms([j['name'] for j in joints])
                bounds = [v.tolist() for v in native.hull_bounds()]
            settings = {}
            for name in (f'{prefix}.shp', f'{prefix}/{hull}.ship'):
                if name in rows:
                    text = read(name).decode('latin1')
                    settings[name] = [line for line in text.splitlines() if any(k in line.lower() for k in ('trail', 'navlight', 'enginenozzle'))]
            record = dict(race=race, hull=hull, bounds=bounds, joints=joints,
                transforms=posed, classic=mex_markers(read(f'{prefix}/rl0/lod0/{hull}.mex')),
                settings=settings, manifest=[line for line in manifest.splitlines() if line.startswith(('scale ', 'marker '))])
            result.append(record)
            print(json.dumps(record, indent=2))
    (output/'emitters.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('reader', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    audit(args.archive, args.reader, args.output)
