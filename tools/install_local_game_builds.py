"""Install a matching normal/diagnostic development pair without publishing."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_build(folder, diagnostic, source):
    folder = folder.resolve()
    cache_path = folder.parent / 'CMakeCache.txt'
    cache = {}
    for line in cache_path.read_text(encoding='utf-8').splitlines():
        if not line.startswith(('#', '//')) and '=' in line and ':' in line:
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    if Path(cache['CMAKE_HOME_DIRECTORY']).resolve() != source:
        raise ValueError('Both builds must come from this source directory')
    if cache.get('HW_ENABLE_DIAGNOSTICS') != ('ON' if diagnostic else 'OFF'):
        raise ValueError('Incorrect diagnostics setting in ' + str(cache_path))
    executable = folder / 'HomeworldModern.exe'
    if not executable.is_file():
        raise ValueError('Missing build: ' + str(executable))
    latest = max(path.stat().st_mtime_ns for path in (source / 'src').rglob('*')
                 if path.is_file() and path.suffix.lower() in ('.c', '.cpp', '.h', '.inc', '.def'))
    latest = max(latest, (source / 'CMakeLists.txt').stat().st_mtime_ns)
    if executable.stat().st_mtime_ns < latest:
        raise ValueError('Build predates source changes; rebuild ' + str(folder))
    return executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--normal', type=Path, required=True)
    parser.add_argument('--diagnostic', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    root = args.root.resolve(strict=True)
    if not (root / 'OpenHW.exe').is_file():
        raise ValueError('Destination must be an existing OpenHW installation')
    normal = validate_build(args.normal, False, source)
    diagnostic = validate_build(args.diagnostic, True, source)
    if digest(normal) == digest(diagnostic):
        raise ValueError('Normal and diagnostic executables must be distinct builds')
    builds = {'OpenHW.exe': normal, 'OpenHW-Daignostics.exe': diagnostic}
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup = root / 'Development/Backups/LocalGameBuilds' / stamp
    backup.mkdir(parents=True)
    staged = {}
    try:
        for name, build in builds.items():
            target = root / name
            if target.is_symlink():
                raise ValueError('Linked executable refused: ' + str(target))
            if target.exists():
                shutil.copy2(target, backup / name)
            pending = root / (name + '.local-pending')
            if pending.exists():
                raise ValueError('Pending deployment already exists: ' + str(pending))
            staged[name] = pending
            shutil.copy2(build, pending)
            if digest(pending) != digest(build):
                raise ValueError('Staged executable verification failed')
        replaced = []
        try:
            for name, pending in staged.items():
                os.replace(pending, root / name)
                replaced.append(name)
        except OSError:
            for name in replaced:
                shutil.copy2(backup / name, root / name)
            raise
        rows = []
        for name, build in builds.items():
            installed = digest(root / name)
            if installed != digest(build):
                raise ValueError('Installed executable verification failed: ' + name)
            rows.append(dict(file=name, build=str(build), sha256=installed))
        receipt = dict(source=str(source), backup=str(backup), files=rows, published=False)
        (backup / 'deployment.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(receipt, indent=2))
    finally:
        for pending in staged.values():
            if pending.exists():
                pending.unlink()


if __name__ == '__main__':
    main()
