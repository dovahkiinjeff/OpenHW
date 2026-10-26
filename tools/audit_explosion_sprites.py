"""Inspect shared RM death-effect descendants without modifying game data."""
import sys
from pathlib import Path
from openhw_fx_presentation import parse_fx, dependencies

root = Path(sys.argv[1])
pending = sys.argv[2:] or [p.stem for p in root.glob('death*.lua')]
seen = set()
while pending:
    name = pending.pop().lower()
    if name in seen:
        continue
    seen.add(name)
    path = root / (name + '.lua')
    if not path.is_file():
        print('MISSING', name)
        continue
    effect = parse_fx(path.read_bytes())
    pending.extend(dependencies(effect))
    props = {p['name']: p['string'] or p['values'] for p in effect['properties']}
    if props.get('texture'):
        print(name, {key: props.get(key) for key in ('texture', 'blending', 'usemesh')})
