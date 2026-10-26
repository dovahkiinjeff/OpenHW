"""Print original RM mesh/texture declarations reachable from impact roots."""
import sys
from pathlib import Path
from openhw_fx_presentation import parse_fx, dependencies

root = Path(sys.argv[1])
pending = sys.argv[2:]
seen = set()
while pending:
    name = pending.pop().lower()
    if name in seen:
        continue
    seen.add(name)
    effect = parse_fx((root / (name + '.lua')).read_bytes())
    pending.extend(dependencies(effect))
    props = {p['name']: p['string'] or p['values'] for p in effect['properties']}
    if props.get('usemesh') == [1]:
        print(name, {key: props.get(key) for key in ('mesh', 'texture', 'blending')})
