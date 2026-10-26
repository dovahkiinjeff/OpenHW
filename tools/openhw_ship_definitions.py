"""Export reviewed runtime rig mappings to editable OpenHW ship definitions."""
import json
import re
from pathlib import Path


def weapons(text):
    """Split embedded HW1 weapon settings without changing their values."""
    text=text.replace('\r','')
    # Normalize the redundant opener in imported Classic support-frigate data.
    text=re.sub(r'(\{\s*(?:(?://|;)[^\n]*\n\s*)*)\{',r'\1',text)
    blocks={}
    pattern=re.compile(r'^GUN\s+(\d+)[^\n]*\n(?:[^{}]*?)\{[^{}]*\}',re.M)
    def take(match):
        index=int(match[1])
        if index in blocks:raise ValueError('Duplicate GUN index')
        blocks[index]=match[0][match[0].index('\n')+1:]+'\n'
        return ''
    settings=pattern.sub(take,text)
    if re.search(r'^GUN\s+',settings,re.M):raise ValueError('Unparsed gun block')
    settings=re.sub(r'^NUMBER_OF_GUNS[^\n]*\n?','',settings,flags=re.M)
    return settings,blocks


def author_weapon_cues(gameplay, guns, events, configs=None):
    from openhw_presentation_events import parse_events, weapon_animation
    from openhw_weapon_presentation import matching_weapon_configs
    parsed={slot:parse_events(text) for slot,text in events.items()}
    settings,blocks=weapons(gameplay);report=[]
    for gun in guns:
        index,slot,*joints=gun
        if index not in blocks:continue # source rig may expose unused RM barrels
        matched=matching_weapon_configs((configs or {}).get(slot,[]),joints)
        normal=[row for row in matched if row['activation']=='Normal']
        families={row.get('weaponfire_path') for row in normal if row.get('weaponfire_path')}
        if len(families)>1:
            report.append(dict(gun=index,slot=slot,status='ambiguous normal weaponfire family'));continue
        fire=next(iter(families),None)
        if fire:
            if re.search(r'^\s*WeaponFire\b',blocks[index],re.M):raise ValueError('Duplicate imported WeaponFire')
            blocks[index]=blocks[index].replace('{','{\nWeaponFire '+fire,1)
        source=parsed.get(slot)
        if source is None:
            report.append(dict(gun=index,slot=slot,status='weaponfire only' if fire else 'missing component events',weaponfire=fire));continue
        allowed={row['animation'] for row in normal} if matched else None
        try:cue=weapon_animation(source,joints,allowed)
        except ValueError as error:
            report.append(dict(gun=index,slot=slot,status=str(error)));continue
        if not cue:
            report.append(dict(gun=index,slot=slot,status='weaponfire only' if fire else 'no unique authored firing attachment',weaponfire=fire));continue
        if re.search(r'^\s*FireAnimation\b',blocks[index],re.M):
            raise ValueError('Duplicate imported FireAnimation')
        blocks[index]=blocks[index].replace('{','{\nFireAnimation '+cue,1)
        report.append(dict(gun=index,slot=slot,status='bound',animation=cue,weaponfire=fire))
    output=settings+'\nNUMBER_OF_GUNS '+str(len(blocks))+'\n'
    output+='\n'.join('GUN '+str(index)+'\n'+block for index,block in sorted(blocks.items()))
    return output,report


def reviewed_bindings(source):
    text = Path(source).read_text(encoding='utf-8')
    guns, events = {}, {}
    section = text.split('static const nativeGunMap nativeGuns[] = {', 1)[1].split('\n};', 1)[0]
    for raw in section.splitlines():
        line = raw.strip()
        call = re.fullmatch(r'(FIXED|HINGE|ATTACHED|SPLIT)_GUN\((.*?)\),?', line)
        if call:
            args = re.findall(r'"([^"]*)"|(\d+)', call[2])
            args = [a or b for a, b in args]
            kind, identity, index = call[1], args[0], int(args[1])
            slot = args[2] if kind in ('ATTACHED', 'SPLIT') else 'Hull'
            prefix = args[3] if kind in ('ATTACHED', 'SPLIT') else args[2]
            position, latitude = prefix+'_Position', prefix+'_Latitude'
            row = [index, slot, '-' if kind == 'FIXED' else position,
                   latitude if kind == 'SPLIT' else position if kind == 'ATTACHED' else '-',
                   '-' if kind == 'FIXED' else latitude if kind == 'SPLIT' else position,
                   prefix+'_Muzzle']
        elif line.startswith('{'):
            args = re.findall(r'"([^"]*)"|(\d+)|(NULL)', line)
            args = [a or b or '-' for a, b, _ in args]
            if len(args) != 7:
                raise ValueError('Unrecognized reviewed gun mapping: '+line)
            identity, row = args[0], [int(args[1])]+args[2:]
        else:
            continue
        guns.setdefault(identity, []).append(row)
    section = text.split('static const nativeEventMap nativeEvents[] = {', 1)[1].split('\n};', 1)[0]
    for identity, event, count, clips in re.findall(r'\{"([^"]+)","([^"]+)",(\d+),\{([^}]+)\}\}', section):
        clips = re.findall(r'"([^"]+)"', clips)
        if len(clips) != int(count):
            raise ValueError('Animation clip count mismatch')
        events.setdefault(identity, []).append([event]+clips)
    return dict(schema=1, guns=guns, events=events)


def generate(runtime, bindings, source=None):
    runtime = Path(runtime)
    count = 0
    for visual in sorted(runtime.rglob('visual.rmnative')):
        lines = visual.read_text(encoding='ascii').splitlines()[1:]
        identity = next(line.split()[1] for line in lines if line.startswith('type '))
        # Use the stable derelict identity for the intact scaffold too.
        if identity == 'Scaffold':
            identity = 'Derelict_Scaffold'
            lines = ['type '+identity if line.startswith('type ') else line for line in lines]
        race, name = visual.relative_to(runtime).parts[:2]
        is_ship = race.lower() != 'derelicts'
        if is_ship:
            original = runtime/race/(name+'.shp')
            if not original.is_file():
                raise ValueError('Missing gameplay definition: '+str(original))
            sources={};configs={}
            for line in lines:
                fields=line.split()
                if fields and fields[0]=='presentation':
                    snapshot=(visual.parent/fields[2]).with_suffix('.source.events')
                    if snapshot.is_file():sources[fields[1]]=snapshot.read_text(encoding='utf-8-sig')
                    if source is not None:
                        from openhw_weapon_presentation import weapon_configs
                        extension='.ship' if fields[1]=='Hull' else '.subs'
                        descriptor=(visual.parent/fields[2]).with_suffix('.source'+extension)
                        if descriptor.is_file():configs[fields[1]]=weapon_configs(descriptor.read_text(), 'ship' if extension=='.ship' else 'subsystem',source)
            gameplay,report=author_weapon_cues(original.read_text(encoding='latin1'),bindings['guns'].get(identity,[]),sources,configs)
            (visual.parent/'gameplay.shp').write_text(gameplay,encoding='latin1')
            (visual.parent/'weapon-presentation-import.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
            lines.append('gameplay gameplay.shp')
        lines += ['gun '+' '.join(map(str, row)) for row in bindings['guns'].get(identity, [])]
        lines += ['event '+' '.join(row) for row in bindings['events'].get(identity, [])]
        (visual.parent/'ship.owshipdef').write_text('OPENHW_SHIP_1\n'+'\n'.join(lines)+'\n', encoding='ascii')
        count += 1
    return count
