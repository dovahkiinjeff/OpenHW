"""Build a reusable native subsystem library from locally imported RM assets.

This is a targeted asset step, not a campaign/core reimport. Source files are
never modified; unsupported model/material contracts are reported, not guessed.
"""
import argparse
import json
from pathlib import Path
import shutil
from native_hod_surfaces import load_reader
from stage_kushan_native_materials import stage, select_static_material_variant, stage_active_engine_variant


def build(source, reader, output, *, discard_cache=False):
    source, output = Path(source).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    records, rejected = [], []
    for model in sorted((source/'subsystem').glob('*/*.hod')):
        name=model.parent.name
        from release_compact import hw1_subsystem
        if not hw1_subsystem(name):continue
        if not name.isascii() or not all(c.isalnum() or c=='_' for c in name):continue
        relative=model.relative_to(source).as_posix()
        try:
            cache=source.parent/'material-cache'/'subsystems'/name
            report=stage(reader,source,{'ships':[{'classic_race':'R1','hull':relative,'attachments':[]}]},cache)
            rows=['OPENHW_SUBSYSTEM_1', 'type Subsystem_'+name, 'scale 1', 'hull model.hod', 'mad -']
            folder=output/name;folder.mkdir(exist_ok=True)
            surfaces=0
            for material in report['materials']:
                if material['policy']=='effect-proxy-no-default-opaque-surface':continue
                if material['policy']=='unused-source-material':continue
                variants=material.get('variants',[])
                if not variants:raise ValueError('Unsupported visible material: '+material['name'])
                variant=select_static_material_variant(variants)
                label='m'+str(material['index'])
                rows.append(f"surface 0 {material['index']} {label}")
                for kind,info in variant['files'].items():
                    suffix='.lif' if kind=='bootstrap' else ('.dds' if kind=='diffuse' else '_'+kind+'.dds')
                    shutil.copyfile(cache/info['path'],folder/(label+suffix))
                stage_active_engine_variant(material, cache, folder, label)
                surfaces+=1
            # Innate engine/resource components can be marker/effect-only.
            # Their hierarchy is still a valid subsystem; no fake hull is added.
            shutil.copyfile(model,folder/'model.hod')
            subs=model.with_suffix('.subs')
            if subs.is_file():shutil.copyfile(subs,folder/'source.subs')
            from openhw_presentation_events import stage_events
            events=stage_events(model.with_suffix('.events'),folder)
            if events:rows.append('presentation Hull '+events)
            # Publish only after every required material has translated.
            (folder/'subsystem.owsubsystem').write_text('\n'.join(rows)+'\n',encoding='ascii')
            records.append(name)
            print('Subsystem ready:',name,flush=True)
        except Exception as error:
            rejected.append(dict(type=name,reason=str(error)))
            print('Subsystem unavailable:',name,str(error),flush=True)
        finally:
            if discard_cache:
                from release_compact import discard
                discard(source.parent/'material-cache'/'subsystems'/name,source.parent)
    (output/'catalog.txt').write_text('\n'.join(records)+'\n',encoding='ascii')
    (output/'import-report.json').write_text(json.dumps(dict(available=records,rejected=rejected),indent=2))
    return records


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--reader',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();build(args.source,load_reader(args.reader),args.output)
