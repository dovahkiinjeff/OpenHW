"""Current-release additions to the local Collection conversion pipeline."""
from pathlib import Path
import json
import os
import shutil
from import_local_assets import run_module

SPECIALS = (
    ('bentusi', 'traders/floatingcity', 'source-root', 'v1'),
    ('ghostship', 'traders/ghostship', 'source-root', 'v2'),
    ('research_station', 'traders/researchstation', 'source-root', 'v2'),
    ('junkyardhq', 'traders/junkyardhq', 'source-root', 'v2'),
    ('miningbase', 'traders/miningbase', 'source-root', 'v2'),
    ('council', 'p3/p3megaship', 'source-root', 'v1'),
)

def pack_migration_input(generated,work,destination):
    # Migration reads ship/gameplay definitions and attachment HOD identities,
    # never surface pixels. Avoid writing an entire texture archive twice.
    from openhw_obig import pack
    source=work/'migration-input';source.mkdir(exist_ok=True)
    for path in generated.rglob('*'):
        if not path.is_file() or path.suffix.lower() not in ('.owshipdef','.shp','.hod'):continue
        target=source/path.relative_to(generated)
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():
            if os.path.samefile(path,target):continue
            target.unlink()
        try:os.link(path,target)
        except OSError:shutil.copyfile(path,target)
    return pack(source,destination)


def stage_special_models(sources, output, tools, work, authored):
    # The reviewed staging tools calibrate against Classic meshes and preserve
    # integrated gameplay. Supply only those inputs, never an external install.
    from portable_convert import overlay
    sandbox=work/'special-game'
    for _, relative, _, _ in SPECIALS:
        folder=output/relative
        for path in folder.rglob('*'):
            if path.is_file() and path.suffix.lower() in ('.peo','.geo','.shp'):
                target=sandbox/'Data'/path.relative_to(output)
                target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        source=authored/relative/'rmnative/ship.owshipdef'
        if not source.is_file():raise ValueError('Missing reviewed special-ship recipe: '+relative)
        target=sandbox/'Data'/relative/'rmnative/ship.owshipdef'
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
    for module, _, source_kind, version in SPECIALS:
        cache_name='researchstation' if module=='research_station' else module
        destination=work/'out'/('hwrm-'+cache_name+'-native')
        print('Importing release native model: '+cache_name,flush=True)
        arguments=['--reader',tools/'remastered_asset_reader.dll',
                   '--game-root',sandbox,'--output',destination]
        arguments+=['--'+source_kind,sources/('Archives/Homeworld2.big' if source_kind=='archive' else 'Remastered')]
        if module in ('bentusi','research_station'):
            arguments+=['--classic-archive',__import__('release_compact').source_locations(sources)[1]/'Homeworld.big']
        receipt=destination/'release-complete.json'
        if not receipt.is_file():
            run_module('stage_'+module+'_native',arguments)
            receipt.write_text(json.dumps({'recipe':1,'model':cache_name}))
        package=destination/'package-cache'/version
        if not package.is_dir():raise ValueError('Special-ship conversion produced no package: '+cache_name)
        for race in package.iterdir():
            if race.is_dir():overlay(race,output/race.name.lower())
        from release_compact import discard
        discard(destination,work)


def apply_authored(root, generated, authored):
    """Apply reviewed text recipes after conversion/migration, never user mods."""
    from portable_convert import overlay
    from release_local_art import restore_cursors
    restore_cursors(generated)
    from release_event_edits import rebuild
    rebuild(root, generated)
    # Loose editable definitions must find their source-generated event guide
    # through the same filesystem path as the previously deployed version.
    for event in generated.rglob('*.owevents'):
        relative=event.relative_to(generated)
        target=root/'Data'/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(event,target)
        if relative.parts[0].lower()=='subsystems':
            target=root/'Subsystems'/Path(*relative.parts[1:])
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(event,target)
    overlay(authored,generated)
    overlay(authored,root/'Data')
    if (authored/'subsystems').is_dir():overlay(authored/'subsystems',root/'Subsystems')
    for definition in authored.rglob('ship.owshipdef'):
        target=root/'Modding/RuntimeDefinitions'/definition.relative_to(authored)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(definition,target)
    # SDL/direct-stream consumers need loose counterparts of these resources.
    for name in ('ui','cursors','planets','asteroids','dustclouds','clouds','missions','misc'):
        if (generated/name).is_dir():overlay(generated/name,root/name)
    # ModernAsteroid's portable SDL loader reads physical Data/Asteroids files;
    # the OBIG entry and root-level Asteroids copy cannot satisfy that lookup.
    if (generated/'asteroids').is_dir():
        overlay(generated/'asteroids',root/'Data'/'Asteroids')
    # Current profile wins over any old conversion-resume checkpoint. These
    # settings must also exist as loose files at the runtime loaders' paths.
    overlay(root/'Development/Content/missions',generated/'missions')
    from release_mission_authoring import publish_mission_defaults
    publish_mission_defaults(generated/'missions',root)
