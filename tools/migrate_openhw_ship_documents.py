"""Migrate installed rigs/settings to editable ship and weapon subsystem documents.

Reads the installed archive and library without modifying them. Writes only to
the explicit staging directory. Weapon values and global indices are preserved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import ctypes
from audit_openhw_weapon_subsystems import audit
from openhw_obig import validate
from openhw_ship_definitions import weapons


def migrate(root, output):
    root, output = Path(root), Path(output)
    # Migration always derives from the immutable imported baseline. Never mix
    # user-authored loose definitions with archived weapon settings.
    report = audit(root, archive_only=True)
    if report['unresolved_models']:
        raise ValueError('Unresolved subsystem HODs; migration refused')
    index = validate(root/'OpenHW-HW1.obig')
    variants = {}
    from native_hod_surfaces import NativeSurfaces, load_reader
    setup_tools=root/'Development/SetupTools' if (root/'Development/release.json').is_file() else root/'SetupTools'
    reader=load_reader(setup_tools/'remastered_asset_reader.dll')
    with (root/'OpenHW-HW1.obig').open('rb') as stream:
        def read(name):
            offset,size,_ = index[name.lower()]
            stream.seek(offset)
            return stream.read(size).decode('latin1').replace('\r','')
        for ship in report['ships']:
            path = ship['path']; folder = path.rsplit('/',1)[0]
            old = read(path)
            gameplay = next(line.split()[1] for line in old.splitlines() if line.startswith('gameplay '))
            settings, blocks = weapons(read(folder+'/'+gameplay))
            declared = int(re.search(r'^NUMBER_OF_GUNS\s+(\d+)',read(folder+'/'+gameplay),re.M)[1]) if blocks else 0
            if set(blocks)!=set(range(declared)):
                raise ValueError('Non-contiguous source guns: '+path)
            rig = []
            for line in old.splitlines()[1:]:
                f=line.split()
                if not f:continue
                if f[0] in ('attach','gameplay'):continue
                if f[0]=='presentation' and f[1]!='Hull':continue
                if f[0]=='surface' and f[1]!='0':continue
                if f[0]=='gun' and f[2]!='Hull':continue
                if f[0]=='gun' and int(f[1]) not in blocks:
                    # RM may expose more launch points than the HW1 ship has
                    # simulated weapons (e.g. Kushan missile destroyer 6/4).
                    # Keep the source marker in the HOD, not a dangling gun.
                    rig.append('# Unused RM launch-point binding: '+line)
                    continue
                rig.append(line)
            for attach in ship['attachments']:
                base=attach['library_matches'][0]
                bound=sorted((g for g in ship['guns'] if g['slot']==attach['slot']),key=lambda g:g['index'])
                source=(root/'Subsystems'/base/'subsystem.owsubsystem').read_text()
                model=root/'Subsystems'/base/'model.hod'
                with NativeSurfaces(reader,model) as scene:
                    joint_parents={joint['name']:joint['parent'] for joint in scene.joints()}
                visual=[]
                for line in source.splitlines()[1:]:
                    f=line.split()
                    if not f or f[0]=='type':continue
                    if f[0]=='hull':line='hull @/Subsystems/'+base+'/'+f[1]
                    if f[0]=='surface':line=' '.join(f[:3])+ ' @/Subsystems/'+base+'/'+f[3]
                    if f[0]=='presentation':line=' '.join(f[:2])+ ' @/Subsystems/'+base+'/'+f[2]
                    visual.append(line)
                local=[]
                for i,g in enumerate(bound):
                    if g['index'] not in blocks:raise ValueError('Binding without gun block: '+path)
                    joints=list(g['joints'])
                    # The old visual map used Position for both rotations on
                    # several RM turrets. Their HODs have a real child Latitude;
                    # pitching Position tips the entire mount into the hull.
                    if joints[0]==joints[1] and joints[0].endswith('_Position'):
                        latitude=joints[0][:-len('_Position')]+'_Latitude'
                        if joint_parents.get(latitude)==joints[0]:
                            joints[1]=latitude
                            if joints[2]==joints[0]:joints[2]=latitude
                    visual.append('gun '+str(i)+' Hull '+' '.join(joints))
                    local.append('GUN '+str(i)+'\n'+blocks.pop(g['index']))
                body='\n'.join(visual)+'\n\n[Weapons]\n'+'\n'.join(local)+'[End]\n'
                # Sharing a mesh does not imply identical weapon values. Preserve
                # each distinct weapon configuration as a reusable named variant.
                signature=hashlib.sha256(body.encode('latin1')).hexdigest()
                key=(base,signature)
                if key not in variants:
                    variant=base+'_hw1_'+signature[:10]
                    variants[key]=variant
                    target=output/'Subsystems'/variant/'subsystem.owsubsystem'
                    target.parent.mkdir(parents=True,exist_ok=True)
                    target.write_text('OPENHW_SUBSYSTEM_2\ntype Subsystem_'+variant+'\n'+body,encoding='latin1')
                variant=variants[key]
                anchor=attach['anchor'].removesuffix('_Position')
                role={1:'Weapon',2:'Engine',3:'Utility'}[attach['role']]
                rig.append(f'StartShipHardPointConfig(NewShipType, "{attach["slot"]}", "{anchor}", "{role}", "Innate", "Ship", "{variant}");')
                for i,g in enumerate(bound):rig.append(f'weaponSlot {attach["slot"]} {i} {g["index"]}')
            # Hull-mounted weapons are not subsystem meshes. Bind only exact
            # numbered muzzle joints actually present in this ship's HOD.
            # Never infer a rotating turret or substitute a nearby joint.
            hull=next(line.split()[1] for line in old.splitlines() if line.startswith('hull '))
            offset,size,_=index[(folder+'/'+hull).lower()]
            stream.seek(offset);raw=ctypes.create_string_buffer(stream.read(size))
            scene=ctypes.c_void_p();error=ctypes.create_string_buffer(512)
            if not reader.hwRmHodSceneRead(raw,size,ctypes.byref(scene),error,len(error)):
                raise ValueError('Cannot audit hull weapon joints: '+path+': '+error.value.decode())
            try:
                joint_parents={reader.hwRmHodSceneJoint(scene,i).contents.name.decode():
                               reader.hwRmHodSceneJoint(scene,i).contents.parent.decode()
                               for i in range(reader.hwRmHodSceneJointCount(scene))}
            finally:reader.hwRmHodSceneFree(scene)
            joints=set(joint_parents)
            for n,line in enumerate(rig):
                fields=line.split()
                if len(fields)!=7 or fields[0]!='gun' or fields[2]!='Hull' or \
                   fields[3]!=fields[4] or not fields[3].endswith('_Position'):
                    continue
                latitude=fields[3][:-len('_Position')]+'_Latitude'
                if joint_parents.get(latitude)==fields[3]:
                    fields[4]=latitude
                    if fields[5]==fields[3]:fields[5]=latitude
                    rig[n]=' '.join(fields)
            mapped={g['index'] for g in ship['guns']}
            ship['added_hull_bindings']=[]
            ship['unresolved_hull_weapons']=[]
            for i in sorted(blocks):
                if i in mapped:continue
                muzzle=f'Weapon_Gun{i}_Muzzle'
                if muzzle in joints:
                    rig.append(f'gun {i} Hull - - - {muzzle}')
                    ship['added_hull_bindings'].append(dict(index=i,muzzle=muzzle))
                else:ship['unresolved_hull_weapons'].append(i)
            target=output/'Data'/path
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text('OPENHW_SHIP_2\n\n'+'\n'.join(rig)+'\n\n[Ship]\n'+settings+'\n'+
                              '\n'.join('GUN '+str(i)+'\n'+b for i,b in sorted(blocks.items()))+'\n[End]\n',encoding='latin1')
        # The roster is defined by shipped ship gameplay files, not by which
        # ships already received RM visual packages. Preserve existing models
        # for the remaining ships while moving ALL their editable stats into v2.
        races={'r1','r2','p1','p2','p3','traders'}
        roster=sorted(p for p in index if len(p.split('/'))==2 and p.split('/')[0] in races
                      and p.endswith('.shp') and Path(p).stem not in ('mine','missile'))
        converted={s['path'].split('/rmnative/')[0]+'.shp' for s in report['ships']}
        enum_path=Path(__file__).resolve().parent.parent/'src/Ships/ShipDefs.h'
        names={n.lower():n for n in re.findall(r'^\s*([A-Za-z]\w*)\s*(?:=\s*\d+)?\s*,',
                    enum_path.read_text() if enum_path.is_file() else '',re.M)}
        # Frozen setup has no source checkout. The original section name is
        # authoritative when present; filename spelling is otherwise valid.
        report['legacy_visual_ships']=[]
        for path in roster:
            if path in converted:continue
            source=read(path);settings,blocks=weapons(source)
            section=re.search(r'^\[([^\]]+)\]',source,re.M)
            identity=names.get(Path(path).stem,section[1] if section else Path(path).stem)
            target=output/'Data'/path[:-4]/'rmnative'/'ship.owshipdef';target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text('OPENHW_SHIP_2\ntype '+identity+'\nvisual legacy\n\n[Ship]\n'+settings+'\n'+
                              '\n'.join('GUN '+str(i)+'\n'+b for i,b in sorted(blocks.items()))+'\n[End]\n',encoding='latin1')
            report['legacy_visual_ships'].append(path)
        report['roster']=roster
        report['complete_ship_count']=len(roster)
        report['missing_definitions']=[p for p in roster if not (output/'Data'/p[:-4]/'rmnative'/'ship.owshipdef').is_file()]
        if report['missing_definitions']:raise ValueError('Incomplete ship roster migration')
    report['subsystem_variants']=len(variants)
    (output/'migration-report.json').write_text(json.dumps(report,indent=2))
    return report


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    args=p.parse_args();report=migrate(args.root,args.output)
    print('Migrated',report['ship_count'],'ships and',report['attachment_count'],'mounts;',report['subsystem_variants'],'weapon variants')
