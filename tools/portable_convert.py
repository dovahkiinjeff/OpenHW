"""Rebuild OpenHW's local HW1 package from the imported Collection sources."""
import json
import os
from pathlib import Path
import shutil
import subprocess
from import_local_assets import run_module, import_backgrounds
from extract_remastered_big import safe_path
from extract_vanilla_ship_textures import read_archive, entry_bytes
from openhw_ship_definitions import generate
from openhw_obig import pack, validate

NATIVE_PRESENTATION_RECIPE=4


def conversion_steps(state):
    """Resume unchanged work, rebuild stale derived native presentation only."""
    recipe=state.get('native_presentation_recipe',0)
    if isinstance(recipe,bool) or not isinstance(recipe,int) or not 0<=recipe<=NATIVE_PRESENTATION_RECIPE:
        raise ValueError('Unsupported setup conversion checkpoint recipe')
    completed=set(state.get('completed',[]))
    if recipe!=NATIVE_PRESENTATION_RECIPE:
        completed.difference_update(('r1-material','r1-package','r2','p1','p2','native-overlay','generated','subsystems'))
    return completed


def overlay(source, destination):
    for path in Path(source).rglob('*'):
        if path.is_file():
            relative = path.relative_to(source)
            if path.is_symlink() or not path.resolve().is_relative_to(Path(source).resolve()):
                raise ValueError('Unsafe converter output')
            target = destination/relative
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary=target.with_name(target.name+'.conversion-pending')
            shutil.copyfile(path,temporary);temporary.replace(target)


def install_streaming_audio(sources, root):
    # Both pre-init and soundstreamopenfile use the loose override root (flags
    # zero), not FF_HomeworldDataPath. Keep these streams beside the executable.
    sources, root = Path(sources), Path(root)
    from release_compact import source_locations
    _,classic_dir=source_locations(sources)
    names = ('HW_Comp.vce', 'HW_Music.wxd')
    for name in names:
        if not (classic_dir/name).is_file():
            raise ValueError('Required Classic audio stream missing: '+name)
    for name in names:
        shutil.copyfile(classic_dir/name, root/name)


def convert(root, tools, sky_size=1024):
    root, tools = Path(root).resolve(), Path(tools).resolve()
    release=(root/'Development/release.json').is_file()
    development=root/'Development' if release else root
    sources = development/'SetupSources'
    from release_compact import source_locations, discard, link_or_copy
    archives,classic_dir=source_locations(sources)
    classic = classic_dir/'Homeworld.big'
    work = development/'SetupWork'
    output = work/'generated'
    output.mkdir(parents=True, exist_ok=True)
    checkpoint=work/'pipeline.json'
    completed=conversion_steps(json.loads(checkpoint.read_text())) if checkpoint.is_file() else set()
    def finish(step):
        completed.add(step);temporary=checkpoint.with_suffix('.pending')
        temporary.write_text(json.dumps(dict(completed=sorted(completed),
            native_presentation_recipe=NATIVE_PRESENTATION_RECIPE),indent=2));os.replace(temporary,checkpoint)
    # Complete classic game content supplies the HW1 simulation/campaign data.
    # Updated archives, when supplied by the Collection, take precedence.
    for archive in (classic, classic_dir/'Update.big'):
        if 'classic' in completed:break
        if not archive.is_file():
            continue
        blob, entries = read_archive(archive)
        for entry in entries:
            target = output/safe_path(entry.name.lower())
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(entry_bytes(blob, entry))
    finish('classic')
    # One canonical working tree.
    payload=development/'Content' if release else root/'DevelopmentPayload'
    if payload.is_dir() and 'payload' not in completed:
        overlay(payload, output)
    finish('payload')
    reader = tools/'remastered_asset_reader.dll'
    out = work/'out'
    r1 = out/'hwrm-kushan-native'
    from extract_remastered_big import extract
    if 'r1-plan' not in completed:
        extract(archives/'HW1Ships.big', r1/'source', ('ship/kus_', 'subsystem/kus_'))
        extract(archives/'Homeworld2.big', r1/'source', ('subsystem/kus_', 'subsystem/hw1_researchmodule/'))
        run_module('plan_kushan_hod_bindings', ['--root', r1/'source', '--output', r1/'plan.json']);finish('r1-plan')
    if 'r1-material' not in completed and 'r1-merged' not in completed:
        run_module('stage_kushan_native_materials', ['--reader', reader, '--source', r1/'source',
            '--plan', r1/'plan.json', '--output', r1/'material-cache/v1']);finish('r1-material')
    if 'r1-package' not in completed and 'r1-merged' not in completed:
        from stage_kushan_native_packages import stage as stage_packages
        from native_hod_surfaces import load_reader
        stage_packages(load_reader(reader),r1/'source',json.loads((r1/'plan.json').read_text()),
            r1/'material-cache/v1',classic,r1/'package-cache/v1');finish('r1-package')
    if 'r1-merged' not in completed:
        for race in ('R1','R2','P1','P2','Derelicts'):
            folder=r1/'package-cache/v1'/race
            if folder.is_dir():overlay(folder,output/race.lower())
        finish('r1-merged')
    if release:
        discard(r1/'material-cache',work)
        discard(r1/'package-cache',work)
    r2, p1, p2 = out/'hwrm-taiidan-native', out/'hwrm-raiders-derelicts-native', out/'hwrm-kadeshi-native'
    if 'r2' not in completed:
        run_module('stage_taiidan_native', ['--reader', reader, '--rm-data', archives,
            '--classic-archive', classic, '--output', r2]);finish('r2')
    if 'r2-merged' not in completed:
        for race in ('R1','R2','P1','P2','Derelicts'):
            folder=r2/'package-cache/v1'/race
            if folder.is_dir():overlay(folder,output/race.lower())
        finish('r2-merged')
    if release:
        discard(r2/'material-cache',work)
        discard(r2/'package-cache',work)
    if 'p1' not in completed:
        from stage_raiders_derelicts_native import stage as stage_raiders
        stage_raiders(reader,archives/'Homeworld2.big',classic,p1);finish('p1')
    if 'p1-merged' not in completed:
        for race in ('R1','R2','P1','P2','Derelicts'):
            folder=p1/'package-cache/v1'/race
            if folder.is_dir():overlay(folder,output/race.lower())
        finish('p1-merged')
    if release:
        discard(p1/'material-cache',work)
        discard(p1/'package-cache',work)
    if 'p2' not in completed:
        run_module('stage_kadeshi_native', ['--reader', reader, '--archive', archives/'Homeworld2.big',
            '--classic-archive', classic, '--output', p2]);finish('p2')
    if 'p2-merged' not in completed:
        for race in ('R1','R2','P1','P2','Derelicts'):
            folder=p2/'package-cache/v1'/race
            if folder.is_dir():overlay(folder,output/race.lower())
        finish('p2-merged')
    if release:
        discard(p2/'material-cache',work)
        discard(p2/'package-cache',work)
    if release and 'release-specials' not in completed:
        from release_import import stage_special_models
        stage_special_models(sources,output,tools,work,development/'Authored')
        finish('release-specials')
    bindings = json.loads((tools/'native-bindings.json').read_text(encoding='utf-8'))
    generated = output
    # Generate definitions using the original case-sensitive type spelling from
    # each visual manifest, while finding gameplay in the canonical namespace.
    from openhw_ship_definitions import generate as definitions
    definition_count = definitions(generated, bindings,sources/'Remastered')
    from openhw_weapon_presentation import stage_weaponfire,stage_projectiles,weapon_configs
    stage_weaponfire(sources/'Remastered'/'scripts'/'weaponfire',generated)
    projectile_families=set()
    for extension,kind in (('ship','ship'),('subs','subsystem')):
        for snapshot in generated.rglob('*.source.'+extension):
            for config in weapon_configs(snapshot.read_text(encoding='utf-8-sig'),kind,sources/'Remastered'):
                if config['projectile_source']:
                    projectile_families.add(Path(config['projectile_source']).stem)
    stage_projectiles(sources/'Remastered',generated,projectile_families)
    from openhw_fx_presentation import stage_selected as stage_native_fx
    native_fx=stage_native_fx(sources/'Remastered',generated,generated)
    print('Native FX:',native_fx['nodes'],'source graph nodes;',len(native_fx['resources']),'original resources',flush=True)
    print('Generated', definition_count, 'OpenHW ship/derelict definitions', flush=True)
    if 'fx' not in completed:
        from import_kushan_weapon_fx import build as build_fx
        from native_hod_surfaces import load_reader
        build_fx(sources/'Remastered',r1/'source',generated/'RemasteredFX',p1/'source',r2/'source',p2/'source',True,True,load_reader(reader));finish('fx')
    if 'backgrounds' not in completed:
        import_backgrounds(archives, work/'background-work', generated, tools, sky_size, compact=release)
        finish('backgrounds')
    # Direct streaming/SDL image readers retain these files beside the archive.
    # They are generated here and included in the setup receipt too.
    for name in ('RemasteredBackgrounds',):
        shutil.copytree(generated/name,root/name,copy_function=link_or_copy,dirs_exist_ok=True)
    data = root/'Data'
    data.mkdir(exist_ok=True)
    install_streaming_audio(sources, root)
    # Music is now decoded from RM PCM rather than the Classic WXD stream.
    # The old stream remains a migration fallback for unmapped credits/logos;
    # speech and compressed event audio are migrated separately.
    from import_remastered_audio import stage as stage_music, stage_effects
    stage_music(sources/'Remastered'/'sound'/'music', generated)
    stage_effects(sources/'Remastered'/'sound'/'sfx'/'etg', generated)
    from stage_openhw_subsystems import build as build_subsystems
    from native_hod_surfaces import load_reader
    if 'subsystems' not in completed:
        build_subsystems(sources/'Remastered',load_reader(reader),root/'Subsystems',discard_cache=release);finish('subsystems')
    if (classic_dir/'Movies').is_dir():
        overlay(classic_dir/'Movies', data/'Movies')
    print('Packing OpenHW-HW1.obig', flush=True)
    if release:
        from release_import import pack_migration_input
        count=pack_migration_input(generated,work,root/'OpenHW-HW1.obig')
    else:count = pack(generated, root/'OpenHW-HW1.obig')
    from migrate_openhw_ship_documents import migrate
    document_stage=work/'ship-documents-v2'
    migrate(root,document_stage)
    # Authoring templates must match the installed integrated definition, not
    # the intermediate legacy rig. Derelicts retain their compatible v1 form.
    for definition in generated.rglob('ship.owshipdef'):
        relative=definition.relative_to(generated)
        folder=root/'Modding'/'RuntimeDefinitions'/relative.parent
        folder.mkdir(parents=True,exist_ok=True)
        integrated=document_stage/'Data'/relative
        source=integrated if integrated.is_file() else definition
        target=folder/'ship.owshipdef'
        if not target.exists():shutil.copyfile(source,target)
        if not integrated.is_file():
            gameplay=definition.parent/'gameplay.shp'
            if gameplay.is_file() and not (folder/'gameplay.shp').exists():
                shutil.copyfile(gameplay,folder/'gameplay.shp')
    # Re-running setup must not overwrite a user's edited loose definition.
    for definition in (document_stage/'Data').rglob('ship.owshipdef'):
        target=root/'Modding'/'RuntimeDefinitions'/definition.relative_to(document_stage/'Data')
        if not target.exists():
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(definition,target)
    # Build the candidates separately, then install missing baseline files only.
    for category in ('Data','Subsystems'):
        for source in (document_stage/category).rglob('*'):
            if not source.is_file():continue
            target=root/category/source.relative_to(document_stage/category)
            if target.exists():continue
            target.parent.mkdir(parents=True,exist_ok=True)
            link_or_copy(source,target)
    if release:
        from release_import import apply_authored
        # Keep the pre-migration checkpoint immutable so interrupted setup can
        # resume without mixing v1 rig inputs and integrated v2 documents.
        final=work/'release-runtime'
        shutil.copytree(generated,final,copy_function=link_or_copy,dirs_exist_ok=True)
        apply_authored(root,final,development/'Authored')
        count=pack(final,root/'OpenHW-HW1.obig')
    # Validation uses the engine's own HOD/package reader, across all native
    # definitions, including gun/animation mappings and LODs 0 through 4.
    subprocess.run([str(tools/'OpenHWAssetCheck.exe'), str(root/'OpenHW-HW1.obig'),str(root),str(root)], check=True,
                   cwd=root, env={**os.environ, 'HW_PORTABLE_ROOT': str(root)})
    if release:
        from release_compact import audit_factions
        audit_factions(root)
    return dict(resources=count, definitions=definition_count, sky_size=sky_size)
