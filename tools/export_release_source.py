"""Copy the complete current build source, without retail data or local caches."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from release_content_policy import verify_profile, digest, UI_PLATES

CODE_EXTENSIONS={'.c','.cpp','.h','.hlsl','.inc','.json','.md','.txt','.kas',
                 '.kash','.l','.y','.ps1','.manifest','.in','.cmake','.py','.omwfx',
                 '.yml','.yaml','.cfg'}


def export(repo, output, profile=None):
    repo,output=Path(repo).resolve(),Path(output).resolve()
    if output.exists() and any(output.iterdir()):raise ValueError('Source destination must be empty')
    if output==repo or output.is_relative_to(repo):raise ValueError('Use a separate source destination')
    profile=Path(profile).resolve() if profile else repo/'assets/Release'
    verify_profile(profile)
    output.mkdir(parents=True,exist_ok=True)
    def copy(path):
        if path.is_symlink():raise ValueError('Source export refuses symlinks: '+str(path))
        target=output/path.relative_to(repo);target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(path,target)
    for directory in ('src','tools','tests','cmake','Windows','docs','documentation','deferred','.github'):
        for path in (repo/directory).rglob('*'):
            if not path.is_file() or '__pycache__' in path.parts or '.texture-tools' in path.parts:continue
            if path.suffix.lower() in CODE_EXTENSIONS:copy(path)
    for path in repo.iterdir():
        if path.is_file() and path.name not in ('.git','CMakeUserPresets.json') and (
            path.suffix.lower() in {'.md','.txt','.json'} or path.name in {
                'CMakeLists.txt','CHANGELOG','.clang-format','.gitattributes','.gitignore'}):copy(path)
    shutil.copytree(profile,output/'assets/Release',dirs_exist_ok=True)
    if (profile/'Content/cursors').is_dir():
        shutil.copytree(profile/'Content/cursors',output/'assets/Cursors',dirs_exist_ok=True)
    for part in ('Missions','DustClouds','Parades'):
        for path in (repo/'assets'/part).rglob('*'):
            if path.is_file() and path.suffix.lower() in {'.rtxmap','.rtxlight','.rtxdust','.cfg','.md','.json'}:copy(path)
    for path in (repo/'assets/Asteroids').rglob('*'):
        if path.is_file():copy(path)
    for name in UI_PLATES:
        path=repo/'assets/UI'/name
        if path.is_file():copy(path)
    for name in ('ART_PROVENANCE.md','CONCEPT_ART_MANIFEST.json'):
        copy(repo/'assets/UI'/name)
    # These are upstream shader noise inputs, not Homeworld retail images.
    # Preserve upstream provenance and its explicitly unresolved permission note.
    for path in (repo/'src/ThirdParty/OpenMWVolumetricClouds/textures').glob('*.png'):copy(path)
    copy(repo/'tools/sky_materials/btg_microstructure.png')
    # CMake's asset-directory copies must work on a fresh source checkout.
    for name in ('UI','Missions','Planets','Cursors','Asteroids','R1'):
        folder=output/'assets'/name;folder.mkdir(parents=True,exist_ok=True)
        (folder/'LOCAL_IMPORT.txt').write_text('Retail resources are generated from the selected Collection during setup.\n')
    files=[{'path':p.relative_to(output).as_posix(),'size':p.stat().st_size,'sha256':digest(p)}
           for p in sorted(output.rglob('*')) if p.is_file()]
    (output/'SOURCE_MANIFEST.json').write_text(json.dumps({'schema':1,'version':'1.0',
       'snapshot':'Full current engine, shaders, tests, importer, packaging tools and release profile',
       'excluded':['retail archives/models/audio','unchanged retail textures','build outputs/dependency caches',
                   'personal logs/saves','Git worktree pointer/history','unused Kharak textures'],
       'files':files},indent=2))
    print(json.dumps({'source':str(output),'files':len(files),'bytes':sum(r['size'] for r in files)},indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();export(a.repo,a.output)
