"""Repair installed native weapon presentation bindings; stage, never edit live data."""
import argparse
import json
import re
import shutil
from pathlib import Path
from openhw_weapon_presentation import weapon_configs, matching_weapon_configs, parse_weaponfire
from openhw_fx_presentation import compile_graph, stage_resources, compile_resources, effect_choices


def stage(install, source, output):
    install, source, output = map(lambda p: Path(p).resolve(), (install, source, output))
    if output.exists() and any(output.iterdir()):
        raise ValueError('Staging directory must be empty')
    if output.is_relative_to(install):
        raise ValueError('Stage outside installation')
    data = install/'Data'
    roots_to_scan = [('Data', data)]
    order = install/'mods/load-order.txt'
    for name in order.read_text().splitlines():
        name = name.strip()
        if not name or name.startswith('#'):
            continue
        if Path(name).name != name:
            raise ValueError('Invalid mod folder')
        roots_to_scan.append(('mods/'+name, install/'mods'/name))
    report = []; families = set()
    for prefix, root in roots_to_scan:
        for path in sorted(list(root.rglob('*.owshipdef')) + list(root.rglob('*.owsubsystem'))):
            original = path.read_text(encoding='latin1'); revised = original
            clean = re.sub(r'//[^\n]*|;[^\n]*', '', original)
            guns = re.findall(r'GUN\s+(\d+)\s*\{([^}]+)\}', clean)
            if not guns:
                continue
            rel = path.relative_to(root); sub = path.suffix == '.owsubsystem'
            snapshot_name = 'component_events.source.subs' if sub else 'hull_events.source.ship'
            snapshot = path.parent/snapshot_name
            if not snapshot.exists():
                snapshot = data/rel.parent/snapshot_name
            if not snapshot.exists() and rel.parent.name.lower() == 'kus_ltcorvette_armed':
                snapshot = source/'subsystem/kus_ltcorvette/kus_ltcorvette.subs'
            if not snapshot.exists():
                report.append(dict(path=prefix+'/'+str(rel), status='no RM source snapshot', guns=[i for i,b in guns]))
                continue
            configs = weapon_configs(snapshot.read_text(), 'subsystem' if sub else 'ship', source)
            for gun, block in guns:
                existing = re.search(r'^\s*WeaponFire\s+(\S+)', block, re.M)
                if existing:
                    families.add(Path(existing[1]).stem.lower()); continue
                binding = next((line.split() for line in clean.splitlines() if line.startswith('gun '+gun+' ')), [])
                if not binding:
                    report.append(dict(path=prefix+'/'+str(rel), gun=gun, status='no native gun rig'))
                    continue
                candidates = [c for c in configs if c['activation'] in ('Normal', 'Dropped') and c['weaponfire_path']]
                matches = matching_weapon_configs(candidates, binding[3:])
                # HW1 can have more barrels than RM. If the entire source ship
                # has exactly one family, it is unambiguous for those barrels.
                if not matches and len({c['weaponfire_path'] for c in candidates}) == 1:
                    matches = candidates
                unique = {c['weaponfire_path'] for c in matches}
                if len(unique) != 1:
                    report.append(dict(path=prefix+'/'+str(rel), gun=gun, status='unresolved mount', markers=binding[3:]))
                    continue
                config = matches[0];families.add(config['weaponfire'].lower())
                field = '\nWeaponFire '+config['weaponfire_path']+'\n'
                revised, count = re.subn(r'(GUN\s+'+gun+r'\b[^{}]*\{)', lambda m:m[1]+field, revised, count=1)
                if count != 1:
                    raise ValueError('Missing gun block')
                report.append(dict(path=prefix+'/'+str(rel), gun=gun, status='bound', family=config['weaponfire']))
            if revised != original:
                # Only WeaponFire entries may differ: preserve user geometry,
                # events, formatting and every gameplay value.
                strip = lambda t: re.sub(r'\s+', ' ', re.sub(r'^\s*WeaponFire\s+[^\n]*', '', t, flags=re.M)).strip()
                if strip(original) != strip(revised):
                    raise ValueError('Non-presentation change')
                target = output/prefix/rel;target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(revised, encoding='latin1')
    previous=json.loads((data/'RemasteredFX/Native/import.json').read_text())
    roots=set(previous['roots'])
    for family in families:
        path=source/'scripts/weaponfire'/family/(family+'.wf')
        parsed=parse_weaponfire(path.read_text())
        for slot in parsed['slots']:roots.update(effect_choices(slot['fx']))
        target=output/'Data/RemasteredFX/WeaponFire'/(family+'.wf');target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(path,target)
    blob,receipt=compile_graph(source/'art/fx',sorted(roots))
    receipt.update(roots=sorted(roots),selection=previous.get('selection',{}))
    receipt['resources']=stage_resources(source,output/'Data',receipt)
    bank=output/'Data/RemasteredFX/Native';bank.mkdir(parents=True,exist_ok=True)
    (bank/'effects.fxbank').write_bytes(blob)
    (bank/'effects.fxresources').write_bytes(compile_resources(source,receipt['resources']))
    (bank/'import.json').write_text(json.dumps(receipt,indent=2))
    target=output/'Dev/weapon-bindings-report.json';target.parent.mkdir(exist_ok=True)
    target.write_text(json.dumps(report,indent=2))
    print(json.dumps({status:sum(r['status']==status for r in report) for status in {r['status'] for r in report}}))


if __name__=='__main__':
    cli=argparse.ArgumentParser(description=__doc__)
    for key in ('install','source','output'):cli.add_argument('--'+key,required=True)
    args=cli.parse_args();stage(args.install,args.source,args.output)
