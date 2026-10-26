"""Stage only missing RM material controls; never rewrite ships, HODs or mods.

Uses the installed, unmodified native HOD and its declared surface mapping.
Output mirrors the install root and can be deployed independently of gameplay.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image
from native_hod_surfaces import NativeSurfaces, load_reader
from stage_kushan_native_materials import native_material_controls
from import_remastered_interceptor import dds_rgba


def stage(install, output, reader):
    install, output = Path(install).resolve(), Path(output).resolve()
    if output == install or output.is_relative_to(install / 'Data'):
        raise ValueError('Stage outside live Data')
    roots = [install/'Data'] + sorted(p for p in (install/'mods').glob('*') if p.is_dir())
    records, seen, errors = [], {}, []
    for root in roots:
        def resolve(folder, token, texture=False):
            if token.startswith('@/'):
                relative = token[2:]
                candidate = root/relative
                if not candidate.exists() and not (texture and Path(str(candidate)+'.dds').exists()): candidate = install/'Data'/relative
            else:
                candidate = folder/token
            candidate = candidate.resolve()
            if not candidate.is_relative_to(install): raise ValueError('Path escapes install')
            return candidate
        definitions = sorted(list(root.rglob('*.owshipdef'))+list(root.rglob('*.owsubsystem')))
        for definition in definitions:
            try:
                lines = [line.split() for line in definition.read_text(encoding='utf-8-sig').splitlines()]
                hull = next((r[1] for r in lines if len(r)==2 and r[0]=='hull'), None)
                if not hull: continue
                model = resolve(definition.parent,hull)
                rows = [r for r in lines if len(r)==4 and r[0]=='surface']
                if not rows: continue
                with NativeSurfaces(reader,model) as native:
                    materials = {m['index']:m for m in native.materials()}
                    for row in rows:
                        if row[1]!='0': raise ValueError('Unexpected non-hull component surface')
                        material = materials[int(row[2])]
                        shader = material['shader']
                        if shader=='Ship': shader='ship'
                        if shader not in ('ship','badge','bay','shipglow','badgeglow','thruster','matte','matteAlpha','matteScissor','default'):
                            raise ValueError('Unsupported surface shader '+shader)
                        label = resolve(definition.parent,row[3],texture=True)
                        variants = [('', 'Off'), ('_engineOn', 'On')] if shader=='thruster' else [('', '')]
                        for suffix,state in variants:
                            target = Path(str(label)+suffix+'_material.dds')
                            signature = (str(model).lower(),int(row[2]),state)
                            if target in seen:
                                if seen[target]!=signature: raise ValueError('Conflicting material mapping '+str(target))
                                continue
                            seen[target]=signature
                            diffuse = Path(str(label)+suffix+'.dds')
                            if not diffuse.is_file(): raise ValueError('Missing staged diffuse '+str(diffuse))
                            with Image.open(diffuse) as image: size=image.size
                            parameters=material['parameters']
                            def image(name):
                                index=parameters.get(name)
                                return native.image(index) if index is not None else None
                            controls=native_material_controls(shader,size,image('$glow'+state),image('$team'),image('$spec'))
                            data=dds_rgba(controls)
                            relative=target.relative_to(install)
                            staged=output/relative
                            staged.parent.mkdir(parents=True,exist_ok=True)
                            staged.write_bytes(data)
                            records.append(dict(path=relative.as_posix(),sha256=hashlib.sha256(data).hexdigest(),
                                                model=str(model),material=int(row[2]),shader=shader))
                print('Staged',definition.relative_to(install),flush=True)
            except Exception as error:
                errors.append(dict(definition=str(definition),error=str(error)))
                print('ERROR',definition,error,flush=True)
    output.mkdir(parents=True,exist_ok=True)
    (output/'material-controls-report.json').write_text(json.dumps(dict(files=records,errors=errors),indent=2))
    print('Material maps:',len(records),'errors:',len(errors),flush=True)
    return bool(errors)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--reader',type=Path,required=True)
    args=parser.parse_args()
    raise SystemExit(stage(args.install,args.output,load_reader(args.reader)))
