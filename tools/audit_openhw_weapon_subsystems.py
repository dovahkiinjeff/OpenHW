"""Audit every installed native ship against its actual RM subsystem models."""
import argparse
import hashlib
import json
from pathlib import Path
from openhw_obig import validate


def audit(root, archive_only=False):
    root = Path(root)
    archive = root / 'OpenHW-HW1.obig'
    index = validate(archive)
    models = {}
    for path in sorted((root / 'Subsystems').glob('*/model.hod')):
        models.setdefault(hashlib.sha256(path.read_bytes()).hexdigest(), []).append(path.parent.name)
    rows = []
    with archive.open('rb') as stream:
        def read(name):
            name = name.replace('\\', '/').lower()
            loose = root / 'Data' / name
            if not archive_only and loose.is_file():
                return loose.read_bytes()
            offset, size, _ = index[name]
            stream.seek(offset)
            return stream.read(size)
        for name in sorted(index):
            if not name.endswith('/ship.owshipdef') or name.startswith('derelicts/'):
                continue
            text = read(name).decode('ascii')
            fields = [line.split() for line in text.splitlines() if line.split()]
            attachments, guns = [], []
            for f in fields:
                if f[0] == 'attach':
                    raw = read(name.rsplit('/', 1)[0] + '/' + f[4])
                    matches = models.get(hashlib.sha256(raw).hexdigest(), [])
                    attachments.append(dict(slot=f[1], anchor=f[2], role=int(f[3]), model=f[4], library_matches=matches))
                elif f[0] == 'gun':
                    guns.append(dict(index=int(f[1]), slot=f[2], joints=f[3:]))
            rows.append(dict(path=name, identity=next(f[1] for f in fields if f[0]=='type'),
                             attachments=attachments, guns=guns,
                             hull_guns=[g['index'] for g in guns if g['slot']=='Hull'],
                             unmapped_slots=[a['slot'] for a in attachments if a['role']==1 and not any(g['slot']==a['slot'] for g in guns)]))
    return dict(ships=rows, ship_count=len(rows), attachment_count=sum(len(s['attachments']) for s in rows),
                unresolved_models=[dict(ship=s['identity'], **a) for s in rows for a in s['attachments'] if not a['library_matches']])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='ships'}, indent=2))
