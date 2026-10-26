"""Stage a private, loose-data OpenHW WIP beside an existing installation.

Immutable converter output and setup sources use NTFS hard links to avoid
duplicating tens of gigabytes. Replacing a file in the WIP tree is independent;
editing a linked file in place also edits its converter-cache counterpart.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

from openhw_obig import validate


def linked_tree(source, destination):
    count = 0
    for item in source.rglob('*'):
        if not item.is_file():
            continue
        target = destination / item.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        os.link(item, target)
        count += 1
    return count


def overlay(source, destination):
    if not source.is_dir():
        return 0
    count = 0
    for item in source.rglob('*'):
        if not item.is_file():
            continue
        target = destination / item.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        # Replace the directory entry; never write through a base-data hard link.
        temporary = target.with_name(target.name + '.wip-pending')
        shutil.copyfile(item, temporary)
        temporary.replace(target)
        count += 1
    return count


def overlay_obig(archive, destination):
    rows = validate(archive)
    with archive.open('rb') as stream:
        for name, (offset, size, _) in rows.items():
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + '.wip-pending')
            stream.seek(offset)
            with temporary.open('wb') as output:
                remaining = size
                while remaining:
                    chunk = stream.read(min(remaining, 8 * 1024 * 1024))
                    if not chunk:
                        raise ValueError('Truncated presentation archive: ' + name)
                    output.write(chunk)
                    remaining -= len(chunk)
            temporary.replace(target)
    return len(rows)


def stage(installed, destination, executable, symbols):
    installed, destination = installed.resolve(), destination.resolve()
    if installed == destination or destination.is_relative_to(installed) or installed.is_relative_to(destination):
        raise ValueError('WIP destination must be separate from the installed game')
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('WIP destination is not empty; refusing to overwrite it')
    if installed.drive.lower() != destination.drive.lower():
        raise ValueError('Loose WIP hard links require the same NTFS volume')
    generated = installed / 'SetupWork' / 'generated'
    if not (installed / 'setup-complete.json').is_file() or not generated.is_dir():
        raise ValueError('Existing installation or converted loose source is incomplete')
    archive = validate(installed / 'OpenHW-HW1.obig')
    if len(archive) != sum(p.is_file() for p in generated.rglob('*')) or any(
            not (generated / name).is_file() or (generated / name).stat().st_size != row[1]
            for name, row in archive.items()):
        raise ValueError('Converted loose source does not match the installed base archive')
    destination.mkdir(parents=True, exist_ok=True)
    data = destination / 'Data'
    dev = destination / 'Dev'
    base_count = linked_tree(generated, data)
    presentation_count = overlay_obig(installed / 'OpenHW-Presentation.obig', data)
    for name in ('Data', 'Subsystems', 'RemasteredBackgrounds', 'UI', 'Cursors',
                 'Asteroids', 'Missions', 'Planets', 'misc'):
        source = installed / name
        target = data if name == 'Data' else data / name
        overlay(source, target)
    for name in ('Modding', 'mods', 'SavedGames'):
        overlay(installed / name, destination / name)
    for name in ('SetupSources', 'DevelopmentPayload'):
        source = installed / name
        if source.is_dir():
            linked_tree(source, dev / name)
    for name in ('SetupTools', 'Documentation', 'licenses', 'Updater'):
        overlay(installed / name, dev / name)
    for name in ('Logs', 'Diagnostics', 'ShaderCache', 'RTXExports'):
        (dev / name).mkdir(parents=True, exist_ok=True)
    for item in installed.iterdir():
        if not item.is_file():
            continue
        if item.suffix.lower() == '.dll' or item.name in (
                'openhw.portable', 'setup-complete.json', 'Homeworld.cfg',
                'installation.ini', 'RMHarvestableDust.cfg', 'Parade-Kushan.cfg'):
            shutil.copyfile(item, destination / item.name)
        elif item.name in ('OpenHW-Setup.exe', 'OpenHW.pdb', 'README.txt'):
            shutil.copyfile(item, dev / item.name)
    shutil.copyfile(executable, destination / 'OpenHW.exe')
    shutil.copyfile(symbols, dev / 'OpenHW.pdb')
    (destination / 'openhw.loose').write_text('Data is the authoritative loose asset tree. No OBIG is mounted.\n')
    (destination / 'version.json').write_text(json.dumps({
        'version': '0.91.8-dev-20261014-loose-wip',
        'source': str(installed), 'base_files': base_count,
        'presentation_files': presentation_count}, indent=2))
    print(json.dumps({'destination': str(destination), 'base_files': base_count,
                      'presentation_files': presentation_count,
                      'obig_present': any(destination.glob('*.obig'))}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--symbols', type=Path, required=True)
    args = parser.parse_args()
    stage(args.installed, args.destination, args.executable, args.symbols)
