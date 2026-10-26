"""Reproducible local Taiidan visual import from owned RM archives; no gameplay import."""
import argparse
import json
import re
from pathlib import Path
from extract_remastered_big import extract
from extract_vanilla_ship_textures import read_archive, entry_bytes
from native_hod_surfaces import NativeSurfaces, load_reader, animation_clips
from plan_kushan_hod_bindings import plan, hierarchy_audit
from audit_classic_kushan_bindings import binding_fields, mex_markers

R2_HULLS = {
    'AdvanceSupportFrigate':'tai_supportfrigate', 'AttackBomber':'tai_attackbomber',
    'Carrier':'tai_carrier', 'CloakGenerator':'tai_cloakgenerator',
    'DefenseFighter':'tai_defensefighter', 'DFGFrigate':'tai_fieldfrigate',
    'GravWellGenerator':'tai_gravwellgenerator', 'HeavyCorvette':'tai_heavycorvette',
    'HeavyCruiser':'tai_heavycruiser', 'HeavyDefender':'tai_defender',
    'HeavyInterceptor':'tai_interceptor', 'IonCannonFrigate':'tai_ioncannonfrigate',
    'LightCorvette':'tai_lightcorvette', 'LightInterceptor':'tai_scout',
    'MinelayerCorvette':'tai_minelayercorvette', 'MissileDestroyer':'tai_missiledestroyer',
    'Mothership':'tai_mothership', 'MultiGunCorvette':'tai_multiguncorvette',
    'Probe':'tai_probe', 'ProximitySensor':'tai_proximitysensor',
    'RepairCorvette':'tai_repaircorvette', 'ResearchShip':'tai_researchship',
    'ResourceCollector':'tai_resourcecollector', 'ResourceController':'tai_resourcecontroller',
    'SalCapCorvette':'tai_salvagecorvette', 'SensorArray':'tai_sensorarray',
    'StandardDestroyer':'tai_destroyer', 'StandardFrigate':'tai_assaultfrigate',
    'TargetDrone':'tai_targetdrone',
}
# No RM equivalents in the owned archive. Never substitute a different ship.
RETAIN_CLASSIC = ('CryoTray', 'LightDefender')

def tai_name(name):
    value=name.lower()
    if value!='hw1_researchmodule' and not re.fullmatch(r'tai_[a-z0-9_]+',value):
        raise ValueError('Dependency is not a reviewed Taiidan model: '+name)
    return value

def audit(reader, source, classic_archive, result):
    blob,entries=read_archive(classic_archive)
    classic={e.name.lower():e for e in entries}
    records=[]
    for ship in result['ships']:
        name=ship['classic_type']
        record=dict(type=name,components=[])
        models=[('Hull',ship['hull'])]+[(a['visual_instance'],a['model']) for a in ship['attachments'] if a['model'] and a.get('source_visible')]
        for slot,model in models:
            with NativeSurfaces(reader,source/model) as native:
                meshes=[]
                for mi in range(reader.hwRmHodSceneMeshCount(native.scene)):
                    m=reader.hwRmHodSceneMesh(native.scene,mi).contents
                    meshes.append(dict(name=m.name.decode(),parent=m.parent.decode(),lod=m.lod,
                        triangles=sum(m.parts[i].indexCount//3 for i in range(m.partCount))))
                record['components'].append(dict(slot=slot,model=model,joints=native.joints(),meshes=meshes))
        mad=(source/ship['hull']).with_suffix('.mad')
        record['animations']=animation_clips(reader,mad) if mad.is_file() else []
        script=entry_bytes(blob,classic[f'r2\\{name}.shp'.lower()]).decode('latin1')
        record['classic_bindings']=binding_fields(script)
        record['gameplay_guns']=[int(i) for i in re.findall(r'^GUN\s+(\d+)',script,re.M)]
        key=f'r2\\{name}\\rl0\\lod0\\{name}.mex'.lower()
        record['classic_markers']=mex_markers(entry_bytes(blob,classic[key])) if key in classic else []
        records.append(record)
    return dict(ships=records,retained_classic=list(RETAIN_CLASSIC),gameplay_stats_imported=False)

def main():
    cli=argparse.ArgumentParser(description=__doc__)
    for key in ('reader','rm-data','classic-archive','output'):
        cli.add_argument('--'+key,type=Path,required=True)
    cli.add_argument('--audit-only',action='store_true')
    args=cli.parse_args()
    root=args.output.resolve()
    if root.name!='hwrm-taiidan-native' or root.parent.name!='out':
        raise ValueError('Use dedicated ignored local cache')
    source=root/'source'
    extract(args.rm_data/'HW1Ships.big',source,('ship/tai_','subsystem/tai_'))
    extract(args.rm_data/'Homeworld2.big',source,('subsystem/tai_','subsystem/hw1_researchmodule/'))
    reader=load_reader(args.reader)
    def names(path):
        from collections import Counter
        with NativeSurfaces(reader,path) as native:
            return Counter(j['name'] for j in native.joints())
    result=plan(source,R2_HULLS,'R2',tai_name,names)
    (root/'plan.json').write_text(json.dumps(result,indent=2)+'\n')
    gun_result={'ships':[{**s,'classic_type':'Taiidan_'+s['classic_type']} for s in result['ships']]}
    (root/'gun-audit.tsv').write_text(hierarchy_audit(gun_result))
    (root/'hierarchy-audit.tsv').write_text(hierarchy_audit(result))
    (root/'binding-audit.json').write_text(json.dumps(audit(reader,source,args.classic_archive,result),indent=2)+'\n')
    if not args.audit_only:
        from stage_kushan_native_materials import stage as materials
        from stage_kushan_native_packages import stage as packages
        material_root=root/'material-cache/v1'
        materials(reader,source,result,material_root,allowed_races=('R2',))
        packages(reader,source,result,material_root,args.classic_archive,root/'package-cache/v1',
                 hulls=R2_HULLS,race='R2',identity_prefix='Taiidan_',engines=ENGINES)

ENGINES = {
    **{name:('EngineMainDrive',) for name in (
        'AdvanceSupportFrigate','Carrier','DFGFrigate','GravWellGenerator',
        'HeavyCruiser','IonCannonFrigate','MissileDestroyer','ResourceController',
        'SensorArray','StandardDestroyer','StandardFrigate')},
    **{name:('EngineNozzle1',) for name in (
        'AttackBomber','DefenseFighter','HeavyCorvette','HeavyDefender','HeavyInterceptor',
        'LightCorvette','LightInterceptor','MinelayerCorvette','MultiGunCorvette',
        'Probe','ProximitySensor','RepairCorvette','SalCapCorvette','TargetDrone')},
    'ResourceCollector':('EngineMainDrive','EngineMainDrive2'),
    # Mothership/cloak/research have unlike classic nozzle counts. Preserve
    # their original trails rather than duplicate an RM marker arbitrarily.
}

if __name__=='__main__':main()
