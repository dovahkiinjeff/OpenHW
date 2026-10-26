"""Owned Kadeshi HOD staging; preserves HW1 gameplay, source files and identity."""
import argparse
import json
from pathlib import Path
from extract_remastered_big import extract
from extract_vanilla_ship_textures import read_archive, entry_bytes
from native_hod_surfaces import NativeSurfaces, load_reader
from plan_kushan_hod_bindings import plan, hierarchy_audit
from stage_kushan_native_materials import stage as materials
from stage_kushan_native_packages import stage as packages

P2_HULLS={'P2Swarmer':'kad_swarmer','P2AdvanceSwarmer':'kad_advancedswarmer',
          'P2FuelPod':'kad_fuelpod','P2MultiBeamFrigate':'kad_multibeamfrigate',
          'P2Mothership':'kad_p2mothership'}

def kad_name(value):
    import re
    value=value.lower()
    if value!='hw1_fightertank' and not re.fullmatch('kad_[a-z0-9_]+',value): raise ValueError('Not a Kadeshi dependency')
    return value

def main():
    p=argparse.ArgumentParser()
    for name in ('reader','archive','classic-archive','output'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--audit-only',action='store_true')
    a=p.parse_args();root=a.output.resolve()
    if root.name!='hwrm-kadeshi-native' or root.parent.name!='out': raise ValueError('Use dedicated ignored cache')
    source=root/'source';extract(a.archive,source,('ship/kad_','subsystem/kad_','subsystem/hw1_fightertank/'))
    reader=load_reader(a.reader)
    from collections import Counter
    def names(path):
        with NativeSurfaces(reader,path) as n: return Counter(j['name'] for j in n.joints())
    result=plan(source,P2_HULLS,'P2',kad_name,names)
    root.mkdir(parents=True,exist_ok=True)
    (root/'plan.json').write_text(json.dumps(result,indent=2)+'\n')
    (root/'hierarchy-audit.tsv').write_text(hierarchy_audit(result))
    blob,entries=read_archive(a.classic_archive);classic={e.name.lower():e for e in entries}
    audit=[]
    for s in result['ships']:
        name=s['classic_type']
        with NativeSurfaces(reader,source/s['hull']) as n: joints=n.joints()
        script=entry_bytes(blob,classic['p2\\'+name.lower()+'.shp']).decode('latin1')
        audit.append(dict(type=name,joints=joints,classic_script=script))
        print(name,[j['name'] for j in joints if 'Engine' in j['name'] or 'Weapon' in j['name']],flush=True)
    (root/'binding-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    if a.audit_only:return
    engines={s['type']:tuple(j['name'] for j in s['joints'] if j['name'].startswith(('EngineNozzle','EngineMainDrive')) and
                            sum(k['name']==j['name'] for k in s['joints'])==1) for s in audit}
    mat=root/'material-cache/v1';materials(reader,source,result,mat,allowed_races=('P2',))
    packages(reader,source,result,mat,a.classic_archive,root/'package-cache/v1',hulls=P2_HULLS,race='P2',engines=engines)

if __name__=='__main__':main()
