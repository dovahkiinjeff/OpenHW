"""Carry approved mission authoring into portable release payloads."""
import hashlib
import json
from pathlib import Path
import re


def stage(repository, destination):
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
    files.append((manifest_path.name, manifest_path.read_bytes()))
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in files:
        target = destination / name.lower()
        temporary = target.with_name(target.name + '.authored-pending')
        temporary.write_bytes(data)
        temporary.replace(target)  # Never write through a converted-data hard link.
    return len(files) - 1
