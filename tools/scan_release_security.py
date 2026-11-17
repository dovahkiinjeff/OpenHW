"""Fail a Windows release when Defender cannot verify its packages and payloads.

Uses the installed Microsoft scanner. Does not execute package contents, change
Defender settings, add exclusions, restore quarantined files or upload files.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import uuid
import zipfile


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def unpack(path, destination, budget, depth=0):
    if not zipfile.is_zipfile(path):
        return
    if depth > 4:
        raise ValueError('Nested package exceeds the release scan depth limit')
    destination.mkdir(parents=True)
    names = set()
    with zipfile.ZipFile(path) as archive:
        for item in archive.infolist():
            name = item.filename.replace('\\', '/')
            parts = PurePosixPath(name).parts
            if not parts or name.startswith('/') or any(p in ('.', '..') or ':' in p for p in parts):
                raise ValueError('Unsafe release archive member: ' + name)
            key = name.rstrip('/').lower()
            if key in names:
                raise ValueError('Duplicate release archive member: ' + name)
            names.add(key)
            target = (destination / name).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise ValueError('Release archive escapes its scan directory')
            if item.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            budget[0] -= item.file_size
            if budget[0] < 0:
                raise ValueError('Unpacked release exceeds the 8 GiB scan limit')
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(item) as incoming, target.open('xb') as output:
                shutil.copyfileobj(incoming, output)
            if target.suffix.lower() in ('.exe', '.zip'):
                unpack(target, target.with_name(target.name + '.payload'), budget, depth + 1)


def checked(command, log, runner=subprocess.run):
    result = runner(command, capture_output=True, timeout=1800)
    log.write_bytes(result.stdout + b'\n' + result.stderr)
    if result.returncode != 0:
        raise RuntimeError('Defender did not pass this release check. See ' + str(log))


def scan(files, output, runner=subprocess.run):
    if os.name != 'nt':
        raise RuntimeError('Release security verification requires Windows Defender')
    scanner = Path(os.environ['ProgramFiles']) / 'Windows Defender/MpCmdRun.exe'
    if not scanner.is_file():
        raise RuntimeError('Microsoft Defender scanner is unavailable; release is blocked')
    files = [Path(p).resolve(strict=True) for p in files]
    output = Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    folder = output.parent / ('security-scan-' + uuid.uuid4().hex)
    folder.mkdir()
    report = dict(status='blocked', utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  scanner=str(scanner), evidence=str(folder), files=[])
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    try:
        checked([str(scanner), '-SignatureUpdate'], folder / 'signature-update.log', runner)
        budget = [8 * 1024 ** 3]
        for index, source in enumerate(files):
            checksum = digest(source)
            report['files'].append(dict(path=str(source), sha256=checksum, size=source.stat().st_size))
            payload = folder / str(index)
            payload.mkdir()
            copied = payload / source.name
            shutil.copyfile(source, copied)
            unpack(copied, payload / 'unpacked', budget)
        checked([str(scanner), '-Scan', '-ScanType', '3', '-File', str(folder), '-DisableRemediation'],
                folder / 'scan.log', runner)
        for row in report['files']:
            if digest(Path(row['path'])) != row['sha256']:
                raise RuntimeError('Release file changed during security verification')
        report['status'] = 'passed'
        return report
    except BaseException as error:
        report['error'] = str(error)
        raise
    finally:
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--file', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    scan(args.file, args.output)
    print('Defender passed the exact release files and recursively unpacked payloads.')


if __name__ == '__main__':
    main()
