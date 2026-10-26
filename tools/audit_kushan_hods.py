"""Audit local Kushan HOD layouts without modifying installed assets."""
import json
from pathlib import Path
from inspect_remastered_scene import inspect_scene

root = Path('out/hwrm-kushan-native/source/ship')
rows = []
for path in sorted(root.glob('kus_*/*.hod')):
    try:
        info = inspect_scene(path.read_bytes())
        row = dict(ship=path.stem, shaders=sorted({m['shader'] for m in info['materials']}),
                   materials=len(info['materials']), textures=len(info['textures']),
                   meshes=[dict(name=m['name'],parent=m['parent'],lod=m['lod'],
                                vertices=sum(p['vertices'] for p in m['parts']),
                                triangles=sum(p['triangles'] for p in m['parts']),
                                masks=sorted({p['mask'] for p in m['parts']})) for m in info['meshes']],
                   joints=info['joints'])
        print(row['ship'],row['materials'],row['shaders'],[(m['name'],m['vertices'],m['triangles'],m['masks']) for m in row['meshes']])
    except Exception as error:
        row = dict(ship=path.stem,error=str(error))
        print(path.stem,'ERROR',error)
    rows.append(row)
Path('out/hwrm-kushan-native/audit.json').write_text(json.dumps(rows,indent=2))
