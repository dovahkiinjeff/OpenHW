"""Stage RM event bindings against one installed OpenHW baseline, without writing it.

This is an update-preparation tool, not an alternate runtime asset format.
The output is suitable for the existing OBIG/update packers after validation.
"""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path

from openhw_obig import validate
from openhw_presentation_events import stage_events


HARDPOINT = re.compile(
    r'^StartShipHardPointConfig\(NewShipType, "([^"]+)", "[^"]+", '
    r'"[^"]+", "Innate", "Ship", "([^"]+)"\);\r?$', re.M)


def append_model_line(text, line):
    if line in text.splitlines():
        return text
    if '\n[Ship]' in text:
        return text.replace('\n[Ship]', '\n' + line + '\n[Ship]', 1)
    if '\n[Weapons]' in text:
        return text.replace('\n[Weapons]', '\n' + line + '\n[Weapons]', 1)
    return text.rstrip('\r\n') + '\n' + line + '\n'


def source_plan(repo, installed, name):
    local = repo / 'out' / name
    plan = local / 'plan.json'
    if not plan.is_file():
        plan = installed / 'SetupWork' / 'out' / name / 'plan.json'
    if not plan.is_file() or not (local / 'source').is_dir():
        raise ValueError('Missing reviewed source cache or plan: ' + name)
    return local / 'source', json.loads(plan.read_text())['ships']


def stage(repo, installed, output):
    repo, installed, output = map(lambda p: Path(p).resolve(), (repo, installed, output))
    if output.exists() and any(output.iterdir()):
        raise ValueError('Use an empty staging directory, never an existing install')
    if output == installed or output.is_relative_to(installed):
        raise ValueError('Staging must not modify the installed game')
    runtime, update = output / 'runtime', output / 'update'
    runtime.mkdir(parents=True)
    update.mkdir()
    base = validate(installed / 'OpenHW-HW1.obig')
    sources = ('hwrm-kushan-native', 'hwrm-taiidan-native',
               'hwrm-raiders-derelicts-native', 'hwrm-kadeshi-native')
    summary = {'ships': 0, 'hull_events': 0, 'component_events': 0,
               'subsystem_variants': 0, 'untouched': [], 'source_hashes': {}}
    variant_sources = {}
    with (installed / 'OpenHW-HW1.obig').open('rb') as archive:
        for name in sources:
            source, ships = source_plan(repo, installed, name)
            for ship in ships:
                race, kind = ship['classic_race'], ship['classic_type']
                relative = f'{race}/{kind}/RMNative/ship.owshipdef'
                key = relative.lower()
                if key not in base:
                    summary['untouched'].append(relative + ': no installed native definition')
                    continue
                loose = installed / 'Data' / relative
                if loose.is_file():
                    original = loose.read_bytes()
                    target = update / 'Data' / relative
                    if not original.startswith(b'OPENHW_SHIP_2'):
                        raise ValueError('Unexpected loose ship document: ' + relative)
                else:
                    offset, size, _ = base[key]
                    archive.seek(offset)
                    original = archive.read(size)
                    target = runtime / relative
                    if not original.startswith(b'OPENHW_SHIP_1'):
                        raise ValueError('Unexpected archived ship document: ' + relative)
                text = original.decode('latin1')
                hull_events = (source / ship['hull']).with_suffix('.events')
                if hull_events.is_file():
                    event_dir = runtime / race / kind / 'RMNative'
                    filename = stage_events(hull_events, event_dir, 'hull_events')
                    text = append_model_line(text, 'presentation Hull ' + filename)
                    summary['hull_events'] += 1
                    summary['source_hashes'][str(hull_events.relative_to(source))] = hashlib.sha256(hull_events.read_bytes()).hexdigest()
                hardpoints = dict(HARDPOINT.findall(text))
                for attachment in ship['attachments']:
                    if not attachment.get('source_visible') or not attachment.get('model'):
                        continue
                    event_source = (source / attachment['model']).with_suffix('.events')
                    if not event_source.is_file():
                        continue
                    slot = attachment.get('visual_instance') or attachment['slot']
                    variant = hardpoints.get(slot)
                    if variant:
                        previous = variant_sources.setdefault(variant, event_source)
                        if hashlib.sha256(previous.read_bytes()).digest() != hashlib.sha256(event_source.read_bytes()).digest():
                            raise ValueError('Variant has conflicting event sources: ' + variant)
                    elif original.startswith(b'OPENHW_SHIP_1'):
                        component = next((i for i, row in enumerate(
                            (line.split() for line in text.splitlines() if line.startswith('attach ')), 1)
                            if len(row) == 5 and row[1] == slot), None)
                        if component is None:
                            raise ValueError('Missing archived component: ' + relative + '/' + slot)
                        filename = stage_events(event_source, runtime / race / kind / 'RMNative',
                                                f'component{component}_events')
                        text = append_model_line(text, f'presentation {slot} {filename}')
                        summary['component_events'] += 1
                    else:
                        raise ValueError('Missing v2 subsystem variant: ' + relative + '/' + slot)
                if text.encode('latin1') != original:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(text.encode('latin1'))
                summary['ships'] += 1
    for variant, event_source in variant_sources.items():
        original_path = installed / 'Subsystems' / variant / 'subsystem.owsubsystem'
        if not original_path.is_file():
            raise ValueError('Missing installed subsystem variant: ' + variant)
        original = original_path.read_bytes()
        if not original.startswith(b'OPENHW_SUBSYSTEM_2'):
            raise ValueError('Unexpected subsystem version: ' + variant)
        event_dir = runtime / 'Subsystems' / variant
        filename = stage_events(event_source, event_dir, 'component_events')
        text = append_model_line(original.decode('latin1'), 'presentation Hull ' + filename)
        if text.encode('latin1') != original:
            target = update / 'Subsystems' / variant / 'subsystem.owsubsystem'
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(text.encode('latin1'))
        summary['component_events'] += 1
        summary['subsystem_variants'] += 1
    for part in (repo / 'out/native-fx-fleet-stage/runtime', repo / 'out/presentation-audio-stage'):
        if not part.is_dir():
            raise ValueError('Missing staged native presentation payload: ' + str(part))
        shutil.copytree(part, runtime, dirs_exist_ok=True)
    verification = output / 'verification-overlay'
    shutil.copytree(runtime, verification)
    shutil.copytree(update, verification, dirs_exist_ok=True)
    (output / 'receipt.json').write_text(json.dumps(summary, indent=2))
    return summary


if __name__ == '__main__':
    cli = argparse.ArgumentParser(description=__doc__)
    for key in ('repo', 'installed', 'output'):
        cli.add_argument('--' + key, type=Path, required=True)
    args = cli.parse_args()
    print(json.dumps(stage(args.repo, args.installed, args.output), indent=2))
