"""Stage this private OpenHW development distribution from an explicit release.

Imported RM hulls/materials are rebuilt by setup. Active upscaled Classic
artwork is included as requested; this payload is a private development build.
"""
import argparse
import json
from pathlib import Path
import shutil
import struct
from openhw_ship_definitions import reviewed_bindings
from portable_setup import sha256
from plan_kushan_hod_bindings import R1_HULLS
from stage_taiidan_native import R2_HULLS
from stage_raiders_derelicts_native import P1_HULLS, DERELICT_HULLS
from stage_kadeshi_native import P2_HULLS
from authored_mission_defaults import stage as stage_authored_mission_defaults

GAME_ROOTS = {'r1','r2','p1','p2','p3','traders','derelicts','etg','misc','asteroids',
              'cursors','ui','planets','dustclouds','missions'}
DEFERRED = {'catascout','cataacolyte','cataacv','acolyte','sacolyte','sacv','srecon','somtaaw'}
REPLACED = {race:{name.lower() for name in mapping} for race,mapping in
            (('r1',R1_HULLS),('r2',R2_HULLS),('p1',P1_HULLS),('p2',P2_HULLS),('derelicts',DERELICT_HULLS))}


def include_content(relative):
    parts = relative.as_posix().lower().split('/')
    # Their native hulls/materials are rebuilt from RM. Include only the
    # existing gameplay-point overrides from these directories, not dormant
    # older model/texture experiments that happen to remain in the release.
    if len(parts)>2 and parts[1] in REPLACED.get(parts[0],set()) and relative.suffix.lower()!='.mex':
        return False
    return (parts[0] in GAME_ROOTS and 'rmnative' not in parts and
            not any(any(tag in part for tag in DEFERRED) for part in parts) and
            relative.suffix.lower() not in ('.hod','.mad','.bak','.tmp') and
            '.bak-' not in relative.name.lower())


def stage(repo, release, destination):
    repo, release, destination = map(lambda p:Path(p).resolve(), (repo,release,destination))
    if destination==release or destination.is_relative_to(release):
        raise ValueError('Development stage must be separate from the working release')
    destination.mkdir(parents=True, exist_ok=True)
    payload = destination/'DevelopmentPayload'
    payload.mkdir(exist_ok=True)
    inventory = []
    for path in release.rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(release)
        if include_content(relative):
            target = payload/relative.as_posix().lower()
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
    # Approved mission authoring supersedes older data from the source release.
    stage_authored_mission_defaults(repo, payload/'missions')
    # The active texture pack contains many overrides absent as loose files.
    texture_pack=release/'HomeworldTextures.hwt'
    if texture_pack.is_file():
        with texture_pack.open('rb') as stream:
            magic,version,count,base=struct.unpack('<8sIIQ',stream.read(24))
            if magic!=b'HWTPACK1' or version!=1 or count>1000000:
                raise ValueError('Invalid existing texture pack')
            rows=[struct.unpack('<256sQII',stream.read(272)) for _ in range(count)]
            for raw,offset,size,_ in rows:
                relative=Path(raw.split(b'\0',1)[0].decode('utf-8').replace('\\','/'))
                if not include_content(relative):
                    continue
                target=payload/relative
                if target.is_file():
                    continue
                if offset<base or offset+size>texture_pack.stat().st_size:
                    raise ValueError('Existing texture pack bounds invalid')
                stream.seek(offset);data=stream.read(size)
                target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    build=repo/'out/build/deploy-x64-clean/Release'
    for path in build.iterdir():
        if path.suffix.lower()=='.dll' or path.name in ('HomeworldModern.exe','HomeworldModern.pdb'):
            if path.name=='remastered_asset_reader.dll':
                continue
            name='OpenHW'+path.suffix if path.stem=='HomeworldModern' else path.name
            shutil.copyfile(path,destination/name)
    tools=destination/'SetupTools'
    tools.mkdir(exist_ok=True)
    old_tools=repo/'out/import-tools'
    for name in ('bc6h_encode.exe','ship_texture_dds.exe','DirectXTex.dll','msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll'):
        shutil.copyfile(old_tools/name,tools/name)
    for name in ('remastered_asset_reader.dll','OpenHWAssetCheck.exe'):
        shutil.copyfile(build/name,tools/name)
    (tools/'native-bindings.json').write_text(json.dumps(reviewed_bindings(repo/'src/Game/NativeVisualEvents.c'),indent=2))
    for path in release.glob('*.cfg'):
        shutil.copyfile(path,destination/path.name)
    import os
    for candidate in (Path(os.environ.get('APPDATA',''))/'Homeworld.cfg',
                      Path(os.environ.get('APPDATA',''))/'.homeworld'/'Homeworld.cfg'):
        if candidate.is_file() and not (destination/'Homeworld.cfg').exists():
            shutil.copyfile(candidate,destination/'Homeworld.cfg')
    launchers=repo/'assets/Portable'
    for path in launchers.iterdir():
        if path.is_file():shutil.copyfile(path,destination/path.name)
    (destination/'version.json').write_text(json.dumps(dict(version='0.91.3-dev-20261013-'+sha256(destination/'OpenHW.exe')[:12]),indent=2))
    for name in ('UI','Cursors','Planets','Asteroids','DustClouds','Missions','misc'):
        folder=payload/name.lower()
        if folder.is_dir():
            shutil.copytree(folder,destination/name,dirs_exist_ok=True)
    if (release/'Modding').is_dir():
        shutil.copytree(release/'Modding',destination/'Modding',dirs_exist_ok=True)
    for name in ('licenses',):
        if (release/name).is_dir():
            shutil.copytree(release/name,destination/name,dirs_exist_ok=True)
    if (repo/'out/portable-third-party-notices').is_dir():
        shutil.copytree(repo/'out/portable-third-party-notices',destination/'licenses'/'SetupPython',dirs_exist_ok=True)
    shutil.copytree(repo/'docs',destination/'Documentation',dirs_exist_ok=True)
    for path in sorted(payload.rglob('*')):
        if path.is_file():
            inventory.append(dict(path=path.relative_to(payload).as_posix(),size=path.stat().st_size,sha256=sha256(path)))
    (destination/'development-payload.json').write_text(json.dumps(dict(private_development=True,files=inventory),indent=2))
    (destination/'openhw.portable').write_text('OpenHW portable development release\n')
    mods=destination/'mods';mods.mkdir(exist_ok=True)
    if not (mods/'load-order.txt').exists():
        (mods/'load-order.txt').write_text('# Folder names, one per line; later entries override earlier ones.\n# mod1\n# mod2\n')
    print('Staged',len(inventory),'upscaled/development files at',destination)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('repository','release','destination'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    stage(args.repository,args.release,args.destination)
