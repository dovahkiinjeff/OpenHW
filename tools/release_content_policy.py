"""Explicit release content inventory. A file extension is not provenance.

Retail game files are imported at setup. Two user-approved exceptions are
supplied concept-art UI plates and restored/upscaled Classic textures. Each
exception needs an inventory row with an output checksum and a source identity.
This is a packaging control, not a claim of public-domain status or a license.
"""
import hashlib
import io
import json
import struct
import re
import shutil
from pathlib import Path, PurePosixPath
import zipfile

TEXT_TYPES = {'.owshipdef', '.owsubsystem', '.rmnative',
              '.rtxmap', '.rtxlight', '.rtxdust', '.rtxcloud'}
UI_PLATES = {
    'concept_bentusi_study.png', 'concept_galactic_plan.png', 'concept_new_dock.png', 'concept_galactic_route.png', 'concept_hangar_sketch.png', 'concept_repair_study.png', 'concept_frigate_grey.png', 'concept_carrier_study.png', 'concept_battle_study.png', 'concept_taiidan_blue.png', 'concept_bentusi_lights.png', 'concept_chamber.png', 'concept_taiidan_dark.png', 'concept_mothership_flight.png', 'concept_strike_study.png',
    'archive_plate.png', 'campaign_plate.png', 'fleet_plate.png',
    'systems_plate.png', 'menu_backdrop.png', 'production_plate.png',
    'startup_splash.png', 'concept_interceptor.png', 'concept_science.png',
    'concept_dock.png', 'concept_corvette.png', 'concept_blue_corvette.png',
    'concept_bridge.png', 'concept_hangar.png', 'concept_industry.png',
    'concept_nebula.png', 'concept_mothership.png', 'concept_engineering.png',
    'concept_cockpit.png', 'concept_drone.png',
    'cata_title.png', 'cata_strikecraft.png', 'cata_patrolcraft.png',
    'cata_kuun_lan.png', 'cata_beast_sphere.png', 'cata_beast_ship.png',
}
CATACLYSM_NATIVE_SHIPS = frozenset({
    'sAcolyte', 'sACV', 'sMothership', 'sMothershipArmour', 'sMothershipDockingBay',
    'sMothershipLowerSpine', 'sMothershipMicro', 'sMothershipOreCanArmour',
    'sMothershipSpecial', 'sMothershipWeapons', 'sProcessor', 'sRammingFrigate',
    'sRecon', 'sWorker',
})
CATACLYSM_NATIVE_ROOT = 'Content/cataclysm-native'
CATACLYSM_ASSET_AUTHOR = 'Herocrafter1987'
GAME_FORMATS = {'.big', '.obig', '.hwt', '.hod', '.mad', '.geo', '.peo',
                '.mex', '.lod', '.shp', '.script', '.mif', '.ebg',
                '.vce', '.wxd', '.bik', '.wav', '.ogg', '.mp3', '.wem',
                '.dds', '.lif', '.png', '.jpg', '.jpeg', '.tga', '.obj', '.owevents', '.wf'}

# User-supplied menu audio, explicitly requested for inclusion; PCM conversion
# of the supplied MP3. Pin the exact output rather than allowing audio broadly.
RUNTIME_SUPPLIED = {'Data/UI/archive_signal.wav':
    '1549ae3d6ce7dd661939bbaa2fdf87fd6169130e6f4558559687173d842955a6'}


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_name(name):
    name = name.replace('\\', '/')
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts or ':' in name or not p.parts:
        raise ValueError('Unsafe release path: ' + name)
    return p.as_posix()


def verify_cataclysm_native(name, row, inventory):
    parts = name.split('/')
    if (len(parts) != 6 or '/'.join(parts[:3]) != CATACLYSM_NATIVE_ROOT + '/r1'
            or parts[3] not in CATACLYSM_NATIVE_SHIPS or parts[4] != 'RMNative'
            or row.get('author') != CATACLYSM_ASSET_AUTHOR):
        raise ValueError('Unapproved Cataclysm native asset: ' + name)
    file = parts[-1]
    if file in ('hull.hod', 'animation.mad'):
        source = safe_name(row.get('source_member', ''))
        if (not source.startswith(('HWCR/ship/', 'HWCR/subsystem/'))
                or PurePosixPath(source).suffix.lower() != PurePosixPath(file).suffix
                or row.get('source_sha256') != row['sha256']):
            raise ValueError('Cataclysm native archive provenance missing: ' + name)
    else:
        if file != 'ship.owshipdef' and not re.fullmatch(
                r'c\d+_m\d+(?:_engineOn)?(?:\.(?:lif|dds)|_(?:emissive|material|normal|paintShade|teamEffect[01])\.dds)', file):
            raise ValueError('Unexpected Cataclysm native sidecar: ' + name)
        hull = inventory.get('/'.join(parts[:-1]) + '/hull.hod')
        if (hull is None or hull.get('category') != 'supplied-cataclysm-native'
                or row.get('source_sha256') != hull['sha256']):
            raise ValueError('Cataclysm native sidecar has no approved hull: ' + name)


def install_cataclysm_native(root):
    # Setup has already checked the file inventory and hashes.
    root = Path(root)
    native = root/'Development'/CATACLYSM_NATIVE_ROOT/'r1'
    if native.is_dir():
        shutil.copytree(native, root/'r1', dirs_exist_ok=True)


def verify_profile(root):
    root = Path(root).resolve()
    from release_identity_policy import verify_public_identity
    # Installed Development directories may also contain private backups.
    # Only these paths are public profile inputs; archive audit separately
    # inspects every configuration actually included in a published bundle.
    for configuration in [root/'defaults.cfg',*(root/'Presets').rglob('*.cfg')]:
        if configuration.is_file():
            verify_public_identity(configuration.read_text(encoding='utf-8-sig'),configuration.relative_to(root))
    inventory = json.loads((root/'content-inventory.json').read_text())
    if inventory.get('schema') != 2:
        raise ValueError('A reviewed release content inventory is required')
    approved = {}
    inventory_by_path = {r['path']: r for r in inventory['files']}
    for row in inventory['files']:
        name = safe_name(row['path'])
        if name.lower().startswith('authored/subsystems/'):
            from release_compact import hw1_subsystem
            if not hw1_subsystem(name.split('/')[2]):raise ValueError('Non-HW1 subsystem in release profile: '+name)
        if name in approved:
            raise ValueError('Duplicate content approval: ' + name)
        path = root/name
        if path.is_symlink() or not path.resolve().is_relative_to(root):
            raise ValueError('Unsafe approved file: ' + name)
        if digest(path) != row['sha256']:
            raise ValueError('Approved content changed: ' + name)
        category = row['category']
        suffix = path.suffix.lower()
        if category == 'authored-text':
            if suffix not in TEXT_TYPES or '\0' in path.read_text(encoding='latin1'):
                raise ValueError('Non-text authored file: ' + name)
        elif category == 'supplied-concept-art':
            if name != 'Content/ui/'+path.name or path.name not in UI_PLATES:
                raise ValueError('Unapproved concept-art path: ' + name)
        elif category == 'supplied-cataclysm-native':
            verify_cataclysm_native(name, row, inventory_by_path)
        elif category == 'upscaled-classic-texture':
            cursor=suffix=='.png' and name.startswith('Content/cursors/')
            if (suffix != '.dds' and not cursor) or not row.get('classic_entry', '').endswith('.lif'):
                raise ValueError('Missing Classic texture lineage: ' + name)
            if not row.get('classic_sha256') or not row.get('upscaled_base'):
                raise ValueError('Incomplete Classic texture lineage: ' + name)
            header=path.open('rb').read(128)
            if cursor:
                if header[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('Invalid restored cursor PNG: '+name)
                width,height=struct.unpack_from('>2I',header,16)
            else:
                if len(header)!=128 or header[:4]!=b'DDS ':raise ValueError('Invalid restored DDS: '+name)
                height,width=struct.unpack_from('<2I',header,12)
            if [width,height]!=row['output_dimensions']:
                raise ValueError('Restored texture dimensions changed: '+name)
        elif category == 'owner-created-asteroid':
            if not re.fullmatch(r'Content/asteroids/asteroid[1-4](?:_lod[1-3]\.obj|\.obj|_(?:basecolor|normal|orm)\.dds|_albedo\.png)',name):
                raise ValueError('Unexpected owner-created asteroid file: '+name)
            if row.get('approval')!='Project owner confirmed these asteroid models and textures are their own work':
                raise ValueError('Asteroid ownership confirmation missing: '+name)
        else:
            raise ValueError('Unknown release content category: ' + category)
        approved[name] = row
    for name,row in approved.items():
        if row['category']!='upscaled-classic-texture':continue
        base=approved.get('Content/'+row['upscaled_base'])
        if base is None or base['category']!='upscaled-classic-texture' or any(
            a<=b for a,b in zip(base['output_dimensions'],base['classic_dimensions'])):
            raise ValueError('Restored texture has no enlarged Classic color source: '+name)
    actual = {p.relative_to(root).as_posix() for category in ('Content', 'Authored')
              for p in (root/category).rglob('*') if p.is_file()}
    if actual != set(approved):
        raise ValueError('Uninventoried release content: '+str(sorted(actual ^ set(approved))[:12]))
    return inventory


def audit_archive(archive):
    """Check the actual outer AND nested runtime ZIP, against approved hashes."""
    with zipfile.ZipFile(archive) as outer:
        inventory = json.loads(outer.read('Development/content-inventory.json'))
        inventory_by_path = {safe_name(row['path']): row for row in inventory['files']}
        approved = {'Development/'+name: row for name,row in inventory_by_path.items()}
        seen = set()
        checked = 0
        def check_zip(z, prefix=''):
            nonlocal checked
            for info in z.infolist():
                name = safe_name(info.filename)
                key = prefix+name
                if key.lower() in seen:
                    raise ValueError('Duplicate release archive path: '+key)
                seen.add(key.lower())
                if info.is_dir():
                    continue
                if name.lower().endswith('.zip'):
                    check_zip(zipfile.ZipFile(io.BytesIO(z.read(info))), key+'!/')
                    continue
                suffix = PurePosixPath(name).suffix.lower()
                if suffix=='.cfg':
                    from release_identity_policy import verify_public_identity
                    verify_public_identity(z.read(info).decode('utf-8-sig'),key)
                if prefix:
                    # Only executable/library/preset/license files in runtime.zip.
                    if prefix == 'Development/runtime.zip!/' and suffix in GAME_FORMATS:
                        if RUNTIME_SUPPLIED.get(name) != hashlib.sha256(z.read(info)).hexdigest():
                            raise ValueError('Retail/art data in nested runtime ZIP: '+key)
                    continue
                if name.startswith(('Development/Content/', 'Development/Authored/')):
                    if name.lower().startswith('development/authored/subsystems/'):
                        from release_compact import hw1_subsystem
                        if not hw1_subsystem(name.split('/')[3]):raise ValueError('Non-HW1 subsystem in installer: '+name)
                    row = approved.get(name)
                    if row is None or hashlib.sha256(z.read(info)).hexdigest() != row['sha256']:
                        raise ValueError('Content missing approval/hash: '+name)
                    category=row.get('category')
                    relative=name.removeprefix('Development/')
                    if category=='authored-text':
                        if suffix not in TEXT_TYPES or b'\0' in z.read(info):
                            raise ValueError('Unapproved authored binary: '+name)
                    elif category=='supplied-concept-art':
                        if relative!='Content/ui/'+PurePosixPath(relative).name or PurePosixPath(relative).name not in UI_PLATES:
                            raise ValueError('Unapproved supplied art: '+name)
                    elif category=='supplied-cataclysm-native':
                        verify_cataclysm_native(relative, row, inventory_by_path)
                    elif category=='upscaled-classic-texture':
                        cursor=suffix=='.png' and name.startswith('Development/Content/cursors/')
                        if (suffix!='.dds' and not cursor) or not row.get('classic_sha256') or not row.get('upscaled_base'):
                            raise ValueError('Classic texture lineage missing: '+name)
                    elif category=='owner-created-asteroid':
                        if not re.fullmatch(r'Content/asteroids/asteroid[1-4](?:_lod[1-3]\.obj|\.obj|_(?:basecolor|normal|orm)\.dds|_albedo\.png)',relative):
                            raise ValueError('Unapproved replacement model: '+name)
                    else:raise ValueError('Unknown content approval: '+name)
                    checked += 1
                elif suffix in GAME_FORMATS and not name.startswith('Development/Installer/'):
                    raise ValueError('Unapproved game content: '+name)
        check_zip(outer)
        if checked != len(approved):
            raise ValueError('Content inventory and archive disagree')
        return {'status': 'passed', 'inventoriedContentFiles': checked,
                'nestedRuntimeChecked': True,
                'exceptions': ['user-approved supplied concept art',
                               'supplied Cataclysm Remastered native packages',
                               'user-approved upscaled Classic textures',
                               'owner-created asteroid models and textures']}
