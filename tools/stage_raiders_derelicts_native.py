"""Stage audited P1 and derelict HOD visuals; no gameplay/Lua/archive writes."""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path
from extract_remastered_big import extract
from extract_vanilla_ship_textures import read_archive, entry_bytes
from native_hod_surfaces import NativeSurfaces, load_reader, animation_clips
from plan_kushan_hod_bindings import literal_calls, hierarchy_audit
from stage_kushan_native_materials import stage as stage_materials, select_static_material_variant, stage_active_engine_variant
from stage_kushan_native_packages import classic_bounds, calibrated_scale
from audit_classic_kushan_bindings import binding_fields, mex_markers

P1_HULLS = {
    'P1Fighter':'tur_fighter', 'P1StandardCorvette':'tur_standardcorvette',
    'P1MissileCorvette':'tur_missilecorvette', 'P1IonArrayFrigate':'tur_ionarrayfrigate',
    'P1Mothership':'tur_p1mothership',
}
DERELICT_HULLS = {
    **{name:'der_'+name.lower() for name in (
        'Crate','FragmentPanel0a','FragmentPanel0b','FragmentPanel0c','FragmentPanel1',
        'FragmentPanel2','FragmentPanel3','FragmentStrut','PrisonShipOld','PrisonShipNew',
        'Scaffold','Scaffold_scarred','ScaffoldFingerA_scarred','ScaffoldFingerB_scarred','Shipwreck',
        'Carrier','DDDFrigate','HeavyCorvette','HeavyCruiser','LightCorvette',
        'MinelayerCorvette','MultiGunCorvette','RepairCorvette',
        'Junk0_antenna','Junk0_fin1','Junk0_fin2','Junk0_GunAmmo','Junk0_panel','Junk0_sensors',
        'Junk1_partA','Junk1_partB','Junk1_shell','Junk1_strut',
        'Junk2_panelA','Junk2_panelB','Junk2_panelC','Junk2_panelD','Junk2_shipwreck',
        'Junk3_Boiler','Junk3_BoilerCasing')},
    'AdvanceSupportFrigate':'der_supportfrigate',
}
# These are not the same object as an intact fleet hull. Keep originals instead
# of silently substituting an unrelated wreck. Planet/backdrop bodies and the
# invisible hyperspace-gate dummy are intentionally outside mesh replacement.
UNMATCHED = ('IonCannonFrigate','ResourceController','SalCapCorvette','StandardFrigate',
             'M13PanelA','M13PanelB','M13PanelC','LifeBoat')
ENGINES = {
    'P1Fighter':('EngineNozzle1','EngineNozzle2'),
    'P1StandardCorvette':('EngineNozzle1','EngineNozzle2'),
    'P1MissileCorvette':('EngineNozzle1','EngineNozzle2'),
    'P1IonArrayFrigate':('EngineMainDrive','EngineMainDrive2'),
    'P1Mothership':('EngineMainDrive','EngineMainDrive2'),
}

def hull_plan(source, race, name, model, reader):
    relative=f'ship/{model}/{model}.hod'
    with NativeSurfaces(reader,source/relative) as native:
        names=[j['name'] for j in native.joints()]
    attachments=[]
    if name=='P1Mothership':
        script=(source/f'ship/{model}/{model}.ship').read_text()
        script=re.sub(r'(StartShipHardPointConfig\([^)]*\))',
                      lambda m:m[0].replace('\n',' ').replace('\r',' '),script)
        calls=literal_calls(script,'StartShipHardPointConfig','NewShipType')
        if len(calls)!=1 or calls[0][:6]!=['Engine','Hardpoint_Engine','System','Innate','Damageable','Tur_CarrierEng']:
            raise ValueError('Unexpected carrier engine dependency')
        anchor='Hardpoint_Engine_Position'
        if names.count(anchor)!=1:raise ValueError('Ambiguous carrier anchor')
        attachments.append(dict(slot='Engine',visual_instance='Engine',anchor=anchor,role='System',
            model='subsystem/tur_carriereng/tur_carriereng.hod',source_visible=True))
    return dict(classic_race=race,classic_type=name,source=model,hull=relative,attachments=attachments)

def stage(reader_path, archive, classic_archive, output, package_only=False):
    root=Path(output).resolve()
    if root.name!='hwrm-raiders-derelicts-native' or root.parent.name!='out':
        raise ValueError('Use the dedicated local staging cache')
    source=root/'source'
    extract(archive,source,('ship/tur_','subsystem/tur_','ship/der_'))
    reader=load_reader(reader_path)
    plan={'ships':[hull_plan(source,race,name,model,reader)
                  for race,mapping in [('P1',P1_HULLS),('Derelicts',DERELICT_HULLS)]
                  for name,model in mapping.items()]}
    (root/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    (root/'hierarchy-audit.tsv').write_text(hierarchy_audit({'ships':plan['ships'][:5]}))
    (root/'hierarchy-audit-all.tsv').write_text(hierarchy_audit(plan))
    materials=root/'material-cache/v1'
    mat=json.loads((materials/'manifest.json').read_text()) if package_only else stage_materials(
        reader,source,plan,materials,allowed_races=('P1','Derelicts'))
    by_model={}
    for m in mat['materials']:by_model.setdefault(m['model'],[]).append(m)
    blob,entries=read_archive(Path(classic_archive));classic={e.name.lower():e for e in entries}
    report={'gameplay_stats_imported':False,'source_format':'native HOD','packages':[],
            'retained_classic_missing_rm':list(UNMATCHED),
            'excluded':'Existing planet/backdrop replacements and invisible hyperspace gate'}
    packages=root/'package-cache/v1'
    for ship in plan['ships']:
        race,name=ship['classic_race'],ship['classic_type']
        key=f'{race}\\{name}\\rl0\\lod0\\{name}'.lower()
        old_entry=classic.get(key+'.peo') or classic.get(key+'.geo')
        if old_entry is None:raise ValueError('Missing classic reference: '+key)
        old=classic_bounds(entry_bytes(blob,old_entry))
        hull=source/ship['hull']
        with NativeSurfaces(reader,hull) as native:
            bounds=native.hull_bounds();joints=native.joints()
        scale,axis=calibrated_scale(old,bounds)
        folder=packages/race/name/'RMNative';folder.mkdir(parents=True,exist_ok=True)
        identity=name if race=='P1' else 'Derelict_'+name
        header='HW_RM_P1_VISUAL_1' if race=='P1' else 'HW_RM_DERELICT_VISUAL_1'
        mad=hull.with_suffix('.mad') if race=='P1' else None
        has_mad=mad is not None and mad.is_file()
        lines=[header,'type '+identity,f'scale {scale:.9g}','hull hull.hod',
               'mad '+('animation.mad' if has_mad else '-')]
        from openhw_presentation_events import stage_events
        hull_events=stage_events(hull.with_suffix('.events'),folder,'hull_events')
        if hull_events:lines.append('presentation Hull '+hull_events)
        shutil.copy2(hull,folder/'hull.hod')
        if has_mad:shutil.copy2(mad,folder/'animation.mad')
        names=[j['name'] for j in joints]
        for i,joint in enumerate(ENGINES.get(name,())):
            if names.count(joint)!=1:raise ValueError('Missing engine '+joint)
            lines.append(f'marker engine_{i} Hull {joint}')
        models=[ship['hull']]
        for a in ship['attachments']:
            ci=len(models);models.append(a['model']);file=f'component{ci}.hod'
            shutil.copy2(source/a['model'],folder/file)
            lines.append(f'attach {a["slot"]} {a["anchor"]} 2 {file}')
            events=stage_events((source/a['model']).with_suffix('.events'),folder,'component'+str(ci)+'_events')
            if events:lines.append('presentation '+a['slot']+' '+events)
        for ci,model in enumerate(models):
            source_hash=hashlib.sha256((source/model).read_bytes()).hexdigest()
            for material in by_model[model]:
                if material['source_sha256']!=source_hash:raise ValueError('Stale material source')
                variants=material['variants']
                if not variants:
                    if material['shader'] in ('innateSS','innatess') or material['policy']=='unused-source-material':continue
                    raise ValueError(f'Unmapped material {model}: {material["shader"]}')
                # Keep imported hulls idle; motion-controlled engine FX are separate.
                v=select_static_material_variant(variants)
                label=f'c{ci}_m{material["index"]}'
                lines.append(f'surface {ci} {material["index"]} {label}')
                for kind,record in v['files'].items():
                    file=(materials/record['path']).resolve()
                    if not file.is_relative_to(materials) or hashlib.sha256(file.read_bytes()).hexdigest()!=record['sha256']:
                        raise ValueError('Unsafe/stale material output')
                    suffix='.lif' if kind=='bootstrap' else '.dds' if kind=='diffuse' else '_'+kind+'.dds'
                    shutil.copy2(file,folder/(label+suffix))
                stage_active_engine_variant(material, materials, folder, label)
        (folder/'visual.rmnative').write_text('\n'.join(lines)+'\n',encoding='ascii')
        audit=dict(race=race,type=name,identity=identity,source=ship['hull'],
            source_sha256=hashlib.sha256(hull.read_bytes()).hexdigest(),scale=scale,axis=axis,
            original_bounds=[b.tolist() for b in old],native_bounds=[b.tolist() for b in bounds],
            joints=joints,animations=animation_clips(reader,mad) if has_mad else [])
        if race=='P1':
            shp=classic[f'p1\\{name}.shp'.lower()]
            audit['classic_bindings']=binding_fields(entry_bytes(blob,shp).decode('latin1'))
            mex=classic.get(key+'.mex')
            audit['classic_markers']=mex_markers(entry_bytes(blob,mex)) if mex else []
        report['packages'].append(audit)
        print(race+'/'+name+' native package staged',flush=True)
    (root/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('reader','rm-archive','classic-archive','output'):parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--audit-only',action='store_true')
    parser.add_argument('--package-only',action='store_true')
    a=parser.parse_args()
    if a.audit_only:
        root=a.output.resolve()
        if root.name!='hwrm-raiders-derelicts-native' or root.parent.name!='out':raise ValueError('Invalid audit cache')
        plan=json.loads((root/'plan.json').read_text())
        (root/'hierarchy-audit-all.tsv').write_text(hierarchy_audit(plan))
    else:stage(a.reader,a.rm_archive,a.classic_archive,a.output,a.package_only)
