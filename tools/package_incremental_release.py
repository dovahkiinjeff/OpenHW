"""Package a verified incremental release for the OpenHW updater."""
import argparse
import json
import shutil
import zipfile
from pathlib import Path
from tempfile import TemporaryDirectory

from openhw_updater import digest, FOOTER, unpack_payload, writable_member
from release_content_policy import CATACLYSM_NATIVE_ROOT, verify_profile


def package(stage, bootstrap, output):
    dev = stage/'Development'
    manifest = json.loads((dev/'release.json').read_text(encoding='utf-8'))
    version = manifest['version']
    profile = verify_profile(dev)
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=output.parent) as temp:
        work = Path(temp)
        with zipfile.ZipFile(dev/'runtime.zip') as runtime:
            for info in runtime.infolist():
                if not info.is_dir() and writable_member(info.filename):
                    runtime.extract(info, work/'files')
        files = {p.relative_to(work/'files').as_posix(): p for p in (work/'files').rglob('*') if p.is_file()}
        for row in profile['files']:
            name = row['path']
            if name.startswith(CATACLYSM_NATIVE_ROOT+'/'):
                files[name.removeprefix(CATACLYSM_NATIVE_ROOT+'/')] = dev/name
            elif name.startswith('Content/ui/cata_'):
                files['Data/UI/'+Path(name).name] = dev/name
            elif name.startswith('Content/missions/CataclysmMission'):
                files['Data/Missions/'+Path(name).name] = dev/name

        for folder in ('Symbols','SetupTools'):
            for file in sorted((dev/folder).iterdir()):
                files[f'Development/{folder}/{file.name}'] = file
        if any(not writable_member(name) for name in files):
            raise ValueError('Patch includes a protected path')
        if any(name not in files for name in ('OpenHW.exe','OpenHW-Daignostics.exe','Launcher.exe')):
            raise ValueError('Patch is missing required game or Launcher binaries')

        rows = [dict(path=name, size=path.stat().st_size, sha256=digest(path))
                for name, path in sorted(files.items())]
        payload = work/'patch.zip'
        with zipfile.ZipFile(payload, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for row in rows:
                archive.write(files[row['path']], row['path'])
            archive.writestr(f'Development/Patches/{version}/patch.json',
                             json.dumps(dict(version=version, files=rows), indent=2))
        candidate = work/output.name
        with candidate.open('wb') as out, bootstrap.open('rb') as stub, payload.open('rb') as archive:
            shutil.copyfileobj(stub, out)
            offset = out.tell()
            shutil.copyfileobj(archive, out)
            out.write(FOOTER.pack(b'OPENHW1SETUPZIP\0', offset, payload.stat().st_size, bytes.fromhex(digest(payload))))
        unpack_payload(candidate, work/'checked', {'to': version})
        candidate.replace(output)
    print(f'Verified {version} patch with {len(rows)} files')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('stage','bootstrap','output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    package(args.stage, args.bootstrap, args.output)
