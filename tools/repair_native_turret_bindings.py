"""Correct imported weapon bindings when the HOD has a separate Latitude joint.

Run against an explicit loose Data directory. Files are replaced atomically so
staging hardlinks never mutate the converter's source cache.
"""
import argparse
import os
from pathlib import Path
import tempfile
from native_hod_surfaces import NativeSurfaces, load_reader


def repair(data, reader_path):
    data = Path(data).resolve()
    library = data / 'Subsystems'
    if not library.is_dir():
        raise ValueError('Missing Subsystems directory: ' + str(library))
    reader = load_reader(reader_path)
    changed = []
    definitions = sorted(library.glob('*/subsystem.owsubsystem'))
    definitions += sorted(data.glob('**/ship.owshipdef'))
    for definition in definitions:
        original = definition.read_text(encoding='latin1')
        lines = original.splitlines(keepends=True)
        model = next((row.split()[1] for row in lines if row.startswith('hull ')), None)
        if not model or model == 'legacy':
            continue
        hod = data / model[2:] if model.startswith('@/') else definition.parent / model
        if not hod.is_file():
            continue
        with NativeSurfaces(reader, hod) as scene:
            parents = {joint['name']: joint['parent'] for joint in scene.joints()}
        updated = []
        for row in lines:
            fields = row.split()
            if len(fields) == 7 and fields[0] == 'gun' and fields[3] == fields[4] and fields[3].endswith('_Position'):
                latitude = fields[3][:-len('_Position')] + '_Latitude'
                if parents.get(latitude) == fields[3]:
                    fields[4] = latitude
                    if fields[5] == fields[3]:
                        fields[5] = latitude
                    row = ' '.join(fields) + ('\r\n' if row.endswith('\r\n') else '\n')
            updated.append(row)
        revised = ''.join(updated)
        if revised == original:
            continue
        fd, temporary = tempfile.mkstemp(prefix='.turret-', dir=definition.parent)
        try:
            with os.fdopen(fd, 'w', encoding='latin1', newline='') as stream:
                stream.write(revised)
            os.replace(temporary, definition)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        changed.append(definition)
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('reader', type=Path)
    arguments = parser.parse_args()
    files = repair(arguments.data, arguments.reader)
    print('Corrected', len(files), 'subsystem weapon definitions')
    for file in files:
        print(file)
