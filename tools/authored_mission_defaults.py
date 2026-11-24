"""Carry approved mission authoring into portable release payloads."""
import hashlib
import json
from pathlib import Path
import re


def stage(repository, destination, include_manifest=True):
    source = Path(repository) / 'assets' / 'Missions'
    manifest_path = source / 'authored-defaults.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    files = []
    seen = set()
    for entry in manifest['files']:
        name = entry['path']
        if not re.fullmatch(r'Mission(?:0[1-9]|1[0-6])\.rtx(?:map|light)', name) or name.lower() in seen:
            raise ValueError('Invalid authored mission default: ' + name)
        seen.add(name.lower())
        data = (source / name).read_bytes()
        if hashlib.sha256(data).hexdigest().lower() != entry['sha256'].lower():
            raise ValueError('Authored mission default changed without promotion: ' + name)
        files.append((name, data))
    if include_manifest:
        files.append((manifest_path.name, manifest_path.read_bytes()))
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in files:
        target = destination / name.lower()
        temporary = target.with_name(target.name + '.authored-pending')
        temporary.write_bytes(data)
        temporary.replace(target)  # Never write through a converted-data hard link.
    return len(manifest['files'])


def stage_release_content(repository, development, inventory):
    """Promote current approved authoring into a staged copy, never its input profile."""
    development = Path(development)
    source = Path(repository)/'assets/Missions'
    count = stage(repository, development/'Content/missions', include_manifest=False)
    manifest = json.loads((source/'authored-defaults.json').read_text(encoding='utf-8'))
    records = {row['path']: dict(row) for row in inventory['files']}
    for entry in manifest['files']:
        name = 'Content/missions/'+entry['path'].lower()
        records[name] = dict(path=name, category='authored-text', sha256=entry['sha256'])
    staged = dict(inventory, files=sorted(records.values(), key=lambda row: row['path']))
    (development/'content-inventory.json').write_text(json.dumps(staged, indent=2)+'\n', encoding='utf-8')
    (development/'mission-authoring.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return count
