"""Stage native RM ion bindings without changing HW1 combat parameters."""
import argparse
import json
import re
import shutil
from pathlib import Path
from openhw_weapon_presentation import weapon_configs, matching_weapon_configs, parse_weaponfire
from openhw_fx_presentation import compile_graph, stage_resources, compile_resources, effect_choices


def stage(data, source, output):
    data, source, output = (Path(p).resolve() for p in (data, source, output))
    if output.exists() and any(output.iterdir()):
        raise ValueError('Use an empty staging directory')
    if output.is_relative_to(data) or data.is_relative_to(output):
        raise ValueError('Stage outside live Data')
    rows = []
    families = {'ionbeam', 'kus_ionbeam'}
    for path in sorted(list(data.rglob('*.owshipdef')) + list(data.rglob('*.owsubsystem'))):
        original = path.read_text(encoding='latin1')
        clean = re.sub(r'//[^\n]*|;[^\n]*', '', original)
        ions = [i for i, block in re.findall(r'GUN\s+(\d+)\s*\{([^}]+)\}', clean)
                if re.search(r'GS_\w*Ion', block)]
        if not ions:
            continue
        if 'visual legacy' in clean:
            rows.append({'path': str(path.relative_to(data)), 'guns': ions, 'mode': 'native ion default for legacy hull'})
            continue
        sub = path.suffix == '.owsubsystem'
        snapshot = path.parent / ('component_events.source.subs' if sub else 'hull_events.source.ship')
        configs = weapon_configs(snapshot.read_text(), 'subsystem' if sub else 'ship', source)
        lines = []
        for gun in ions:
            binding = next((line.split() for line in clean.splitlines() if line.startswith('gun '+gun+' ')), None)
            matches = matching_weapon_configs(configs, binding[3:] if binding else [])
            if len(matches) != 1 or not matches[0]['weaponfire_path']:
                raise ValueError(f'Ambiguous ion binding: {path}:{gun}')
            config = matches[0]
            families.add(config['weaponfire'].lower())
            events = path.parent / ('component_events.source.events' if sub else 'hull_events.source.events')
            timelines = set(re.findall(r'"anim"\s*,\s*"([^"]+)"', events.read_text())) if events.exists() else set()
            fields = f'\nWeaponFire {config["weaponfire_path"]}\n'
            if config['animation'] in timelines:
                fields += f'FireAnimation {config["animation"]}\n'
            pattern = r'(GUN\s+'+gun+r'\b[^{}]*\{)([^}]*)(\})'
            def replace(match):
                body = re.sub(r'^\s*(?:WeaponFire|FireAnimation)\s+[^\n]*', '', match[2], flags=re.M)
                return match[1]+fields+body+match[3]
            original, count = re.subn(pattern, replace, original, count=1)
            if count != 1:
                raise ValueError('Missing GUN block')
            lines.append(f'GUN {gun}: '+fields.strip().replace('\n', '; '))
        revised = original
        target = output / path.relative_to(data)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(revised, encoding='latin1')
        rows.append({'path': str(path.relative_to(data)), 'guns': ions, 'bindings': lines})
    previous = json.loads((data/'RemasteredFX/Native/import.json').read_text())
    roots = set(previous['roots'])
    roots.add('ion_beam_charge_combo_kus')
    for family in sorted(families):
        path = source/'scripts/weaponfire'/family/(family+'.wf')
        parsed = parse_weaponfire(path.read_text())
        for slot in parsed['slots']:
            roots.update(effect_choices(slot['fx']))
        target = output/'RemasteredFX/WeaponFire'/(family+'.wf')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    blob, receipt = compile_graph(source/'art/fx', sorted(roots))
    receipt.update(roots=sorted(roots), selection=previous.get('selection', {}))
    receipt['resources'] = stage_resources(source, output, receipt)
    bank = output/'RemasteredFX/Native'
    bank.mkdir(parents=True, exist_ok=True)
    (bank/'effects.fxbank').write_bytes(blob)
    (bank/'effects.fxresources').write_bytes(compile_resources(source, receipt['resources']))
    (bank/'import.json').write_text(json.dumps(receipt, indent=2))
    (output/'ion-update-report.json').write_text(json.dumps(rows, indent=2))
    print(f'Staged {sum(len(r["guns"]) for r in rows)} ion gun definitions in {len(rows)} documents')


if __name__ == '__main__':
    cli = argparse.ArgumentParser(description=__doc__)
    for name in ('data', 'source', 'output'):
        cli.add_argument('--'+name, required=True)
    args = cli.parse_args()
    stage(args.data, args.source, args.output)
