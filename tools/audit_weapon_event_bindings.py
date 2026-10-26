"""Read-only coverage audit of reviewed HW1 mounts against native RM events."""
import argparse
from collections import Counter
import json
from pathlib import Path
from openhw_ship_definitions import reviewed_bindings
from openhw_presentation_events import parse_events,weapon_animation
from openhw_weapon_presentation import weapon_configs,matching_weapon_configs


def audit(source,plans,bindings):
    source=Path(source).resolve();rows=[]
    for plan in plans:
        for ship in json.loads(Path(plan).read_text())['ships']:
            if ship['classic_race'] not in ('R1','R2','P1','P2'):continue
            identity=('Taiidan_' if ship['classic_race']=='R2' else '')+ship['classic_type']
            components={'Hull':ship['hull']}
            components.update({a.get('visual_instance',a['slot']):a['model']
                               for a in ship['attachments'] if a.get('model')})
            parsed={};configs={}
            for slot,model in components.items():
                path=(source/model).with_suffix('.events')
                if not path.resolve().is_relative_to(source):raise ValueError('Unsafe event source')
                if path.is_file():parsed[slot]=parse_events(path.read_text(encoding='utf-8-sig'))
                kind='ship' if model.startswith('ship/') else 'subsystem'
                descriptor=path.with_suffix('.ship' if kind=='ship' else '.subs')
                if descriptor.is_file():configs[slot]=weapon_configs(descriptor.read_text(),kind,source)
            for gun,slot,*joints in bindings['guns'].get(identity,[]):
                record=dict(ship=identity,gun=gun,slot=slot,model=components.get(slot),markers=joints)
                matched=matching_weapon_configs(configs.get(slot,[]),joints)
                normal=[row for row in matched if row['activation']=='Normal']
                record['weaponfire']=[row['weaponfire_path'] for row in normal if row['weaponfire_path']]
                record['projectiles']=sorted({row['projectile_source'] for row in normal if row.get('projectile_source')})
                record['fire_types']=list({row['fire_type'] for row in normal})
                if slot not in parsed:record['status']='weaponfire only' if record['weaponfire'] else 'missing component events'
                else:
                    try:
                        allowed={row['animation'] for row in normal} if matched else None
                        cue=weapon_animation(parsed[slot],joints,allowed)
                        record.update(status='bound' if cue else 'weaponfire only' if record['weaponfire'] else 'no unique authored firing attachment',animation=cue)
                    except ValueError as error:record['status']=str(error)
                rows.append(record)
    return dict(guns=len(rows),bound=sum(r['status']=='bound' for r in rows),
                statuses=dict(Counter(r['status'] for r in rows)),weapons=rows)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--plan',type=Path,action='append',required=True)
    p.add_argument('--bindings',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=audit(a.source,a.plan,reviewed_bindings(a.bindings))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='weapons'},indent=2))
