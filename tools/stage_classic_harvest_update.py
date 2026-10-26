"""Stage collector event files with HW1 resource visuals and original RM audio.

The RM .source.events files are immutable provenance. Only OpenHW's compiled
.owevents copies change; this is the same canonical event format as setup.
"""
import argparse
import hashlib
import json
from pathlib import Path

from openhw_presentation_events import compile_events, parse_source_events


def stage(installed: Path, output: Path) -> dict:
    installed, output = installed.resolve(), output.resolve()
    if output == installed or output.is_relative_to(installed):
        raise ValueError('Staging cannot modify the installed game')
    if output.exists() and any(output.iterdir()):
        raise ValueError('Use an empty staging directory')
    receipt = {}
    for race in ('r1', 'r2'):
        relative = Path('Data') / race / 'resourcecollector' / 'rmnative'
        source = installed / relative / 'hull_events.source.events'
        if not source.is_file():
            raise FileNotFoundError(source)
        raw = source.read_bytes()
        blob, diagnostics = compile_events(parse_source_events(raw), classic_harvest=True)
        if not any('HW1 resource ETG replaces RM' in row for row in diagnostics):
            raise ValueError('Expected resourcing visuals were not mapped: ' + str(source))
        target = output / relative / 'hull_events.owevents'
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(blob)
        previous_receipt = installed / relative / 'hull_events.import.json'
        imported = json.loads(previous_receipt.read_text(encoding='utf-8'))
        if imported.get('source_sha256') != hashlib.sha256(raw).hexdigest():
            raise ValueError('Collector source changed since its import: ' + str(source))
        imported['runtime_sha256'] = hashlib.sha256(blob).hexdigest()
        imported['visual_mappings'] = diagnostics
        (target.parent / 'hull_events.import.json').write_text(
            json.dumps(imported, indent=2), encoding='utf-8')
        receipt[race] = {
            'source_sha256': hashlib.sha256(raw).hexdigest(),
            'runtime_sha256': hashlib.sha256(blob).hexdigest(),
            'visual_mappings': diagnostics,
        }
    (output / 'harvest_event_mapping.json').write_text(
        json.dumps(receipt, indent=2), encoding='utf-8')
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.installed, args.output), indent=2))
