"""Package the staged ship document development update; never apply to live game."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from openhw_update import create, digest, rollback


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parent.parent
    installed=args.installed.resolve();output=args.output.resolve()
    if output.exists():raise ValueError('Choose a new output folder')
    version='0.91.3-dev-20261013-shipdocs3'
    output.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix='openhw-ship-update-') as temporary:
        work=Path(temporary);changed=work/'changed';changed.mkdir()
        build=repo/'out/build/deploy-x64-clean/Release'
        for source,target in [('HomeworldModern.exe','OpenHW.exe'),('HomeworldModern.pdb','OpenHW.pdb')]:
            shutil.copy2(build/source,changed/target)
        stage=repo/'out/ship-documents-v2'
        for name in ('Data','Subsystems'):shutil.copytree(stage/name,changed/name)
        docs=changed/'Documentation';docs.mkdir()
        for name in ('OPENHW_SHIP_DEFINITIONS.md','SHIP_AUTHORING.md'):
            shutil.copy2(repo/'docs'/name,docs/name)
        shutil.copy2(stage/'migration-report.json',docs/'ship-document-migration.json')
        package=output/'OpenHW-Ship-Documents.oupdate'
        manifest=create(installed,changed,version,package)
        # The previously shipped updater rejects Data definitions. Carry a
        # private updated tool beside the patch instead of replacing a running
        # updater or changing the user's installed setup utilities.
        shutil.copytree(repo/'out/portable-installer/OpenHW-Setup',output/'Updater')
        test=work/'test-install';test.mkdir()
        shutil.copy2(installed/'version.json',test/'version.json')
        shutil.copy2(installed/'setup-complete.json',test/'setup-complete.json')
        for row in manifest['files']:
            prior=installed/row['path']
            if prior.is_file():
                target=test/row['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(prior,target)
        subprocess.run([str(output/'Updater/OpenHW-Setup.exe'),'--destination',str(test),
                        '--apply-update',str(package)],check=True)
        for row in manifest['files']:
            if digest(test/row['path'])!=row['sha256']:raise ValueError('Applied file verification failed')
        rollback(test)
        for row in manifest['files']:
            prior=installed/row['path'];restored=test/row['path']
            if prior.is_file():
                if digest(prior)!=digest(restored):raise ValueError('Rollback verification failed')
            elif restored.exists():raise ValueError('Rollback left an added file')
        (output/'package-check.json').write_text(json.dumps(dict(version=version,files=len(manifest['files']),
            apply_verified=True,rollback_verified=True,live_install_modified=False),indent=2))
    print(output)


if __name__=='__main__':main()
