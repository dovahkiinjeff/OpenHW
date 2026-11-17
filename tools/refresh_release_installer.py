"""Refresh a base installer's bootstrap and utilities without changing game data.

The base game version and converter are retained so its supported patch chain
still runs. Release file checksums are regenerated for the changed runtime ZIP.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def refresh(original, bootstrap, launcher, updater, output, work):
    work.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    with original.open('rb') as stream:
        stream.seek(-64, 2)
        magic, offset, size, checksum = struct.unpack('<16sQQ32s', stream.read(64))
        if magic != b'OPENHW1SETUPZIP\0' or offset + size + 64 != original.stat().st_size:
            raise ValueError('Invalid original installer footer')
        stream.seek(offset)
        h = hashlib.sha256()
        left = size
        while left:
            chunk = stream.read(min(left, 4 * 1024 * 1024))
            if not chunk:
                raise ValueError('Truncated original installer')
            h.update(chunk)
            left -= len(chunk)
        if h.digest() != checksum:
            raise ValueError('Original installer checksum failed')
    with zipfile.ZipFile(original) as source:
        manifest = json.loads(source.read('Development/release.json'))
        old_runtime = work / 'runtime-original.zip'
        with source.open('Development/runtime.zip') as incoming, old_runtime.open('wb') as outgoing:
            shutil.copyfileobj(incoming, outgoing)
        new_runtime = work / 'runtime.zip'
        replacements = {'Launcher.exe': launcher, 'Updater.exe': updater}
        with zipfile.ZipFile(old_runtime) as incoming, zipfile.ZipFile(new_runtime, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as outgoing:
            if not replacements.keys() <= set(incoming.namelist()):
                raise ValueError('Base installer has no replaceable Launcher/Updater')
            for item in incoming.infolist():
                if item.filename in replacements:
                    outgoing.write(replacements[item.filename], item.filename)
                else:
                    with incoming.open(item) as data, outgoing.open(item, 'w') as dest:
                        shutil.copyfileobj(data, dest)
        runtime_row = next(row for row in manifest['files'] if row['path'] == 'Development/runtime.zip')
        runtime_row.update(size=new_runtime.stat().st_size, sha256=sha(new_runtime))
        payload = work / 'setup-payload.zip'
        with zipfile.ZipFile(payload, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as outgoing:
            for item in source.infolist():
                if item.filename == 'Development/runtime.zip':
                    outgoing.write(new_runtime, item.filename)
                elif item.filename == 'Development/release.json':
                    outgoing.writestr(item, json.dumps(manifest, indent=2).encode())
                else:
                    with source.open(item) as data, outgoing.open(item, 'w') as dest:
                        shutil.copyfileobj(data, dest)
    with output.open('wb') as target, bootstrap.open('rb') as stub, payload.open('rb') as data:
        shutil.copyfileobj(stub, target)
        offset = target.tell()
        shutil.copyfileobj(data, target)
        target.write(struct.pack('<16sQQ32s', b'OPENHW1SETUPZIP\0', offset, payload.stat().st_size, bytes.fromhex(sha(payload))))
    return dict(base_version=manifest['version'], sha256=sha(output), size=output.stat().st_size,
                launcher_sha256=sha(launcher), updater_sha256=sha(updater), game_data_changed=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'bootstrap', 'launcher', 'updater', 'output', 'work'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    receipt = refresh(**vars(args))
    args.output.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
