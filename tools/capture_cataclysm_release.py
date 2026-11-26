"""Stage the Cataclysm packages checked against the supplied HWCR archive."""
import argparse
import json
import shutil
from collections import defaultdict
from pathlib import Path
from zipfile import ZipFile
import hashlib
import zlib

from release_content_policy import (
    CATACLYSM_ASSET_AUTHOR, CATACLYSM_NATIVE_ROOT, CATACLYSM_NATIVE_SHIPS,
    UI_PLATES, digest, verify_profile,
)


def capture(source, installed, supplied, profile):
    inventory_path = profile/'content-inventory.json'
    inventory = verify_profile(profile)
    records = {row['path']: row for row in inventory['files']}

    def add(path, relative, category, **provenance):
        target = profile/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
        records[relative] = dict(path=relative, category=category, sha256=digest(path), **provenance)

    with ZipFile(supplied) as archive:
        candidates = defaultdict(list)
        for info in archive.infolist():
            if (info.filename.startswith(('HWCR/ship/', 'HWCR/subsystem/')) and
                    Path(info.filename).suffix.lower() in ('.hod', '.mad')):
                candidates[(info.file_size, info.CRC)].append(info)

        for ship in sorted(CATACLYSM_NATIVE_SHIPS):
            relative = f'{CATACLYSM_NATIVE_ROOT}/r1/{ship}/RMNative'
            native = installed/'r1'/ship/'RMNative'
            hull_hash = digest(native/'hull.hod')
            for file in sorted(native.iterdir()):
                if not file.is_file() or file.is_symlink():
                    raise ValueError(f'Unexpected native package entry: {file}')
                provenance = dict(author=CATACLYSM_ASSET_AUTHOR, source_sha256=hull_hash)
                if file.name in ('hull.hod', 'animation.mad'):
                    data = file.read_bytes()
                    matches = candidates[(len(data), zlib.crc32(data))]
                    match = next((m for m in matches if archive.read(m) == data), None)
                    if match is None:
                        raise ValueError(f'{ship}/{file.name} differs from the supplied HWCR archive')
                    provenance.update(source_member=match.filename, source_sha256=hashlib.sha256(data).hexdigest())
                add(file, f'{relative}/{file.name}', 'supplied-cataclysm-native', **provenance)

    for art in sorted(name for name in UI_PLATES if name.startswith('cata_')):
        add(source/'assets/UI'/art, f'Content/ui/{art}', 'supplied-concept-art')

    # Mission 2 is context 20 in the map loader. Use the author's effective files.
    name = 'CataclysmMission02'
    map_file = installed/'RTXExports/Maps'/f'{name}.rtxmap'
    light_file = installed/'RTXLighting'/f'{name}.rtxlight'
    map_text = map_file.read_text(encoding='utf-8-sig')
    if not map_text.startswith('RTXMAP ') or 'MISSION 20' not in map_text.splitlines():
        raise ValueError('Invalid Cataclysm Mission 2 map')
    if not light_file.read_text(encoding='utf-8-sig').startswith('RTXMISSIONLIGHTING '):
        raise ValueError('Invalid Cataclysm Mission 2 lighting')
    for file in (map_file, light_file):
        add(file, f'Content/missions/{file.name}', 'authored-text')

    inventory['files'] = sorted(records.values(), key=lambda row: row['path'])
    inventory_path.write_text(json.dumps(inventory, indent=2), encoding='utf-8')
    verify_profile(profile)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('source', 'installed', 'supplied', 'profile'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    capture(args.source, args.installed, args.supplied, args.profile)
