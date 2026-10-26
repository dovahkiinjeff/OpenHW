"""Resolve actual editor/lighting precedence and publish loose runtime defaults.

Both loaders deliberately bypass the archive for these files. Packing a map
into OBIG or copying it only to root/missions is insufficient.
"""
import hashlib
from pathlib import Path
import re
import shutil
import json


def select_mission_files(root):
    root=Path(root).resolve()
    export=root/('Dev/RTXExports' if (root/'openhw.loose').is_file() else 'RTXExports')
    result=[]
    for mission in range(1,17):
        for suffix,folders in (
            ('rtxmap',(export/'Maps',root/'Data/Missions')),
            ('rtxlight',(root/'RTXLighting',root/'Data/Missions'))):
            name=f'Mission{mission:02d}.{suffix}'
            selected=next((folder/name for folder in folders if (folder/name).is_file()),None)
            if selected is None:continue
            text=selected.read_text(encoding='utf-8-sig')
            if suffix=='rtxmap':
                match=re.search(r'^MISSION\s+(\d+)\s*$',text,re.M)
                if not text.startswith('RTXMAP ') or not match or int(match[1])!=mission:
                    raise ValueError('Invalid mission map: '+str(selected))
            elif not text.startswith('RTXMISSIONLIGHTING '):
                raise ValueError('Invalid mission lighting: '+str(selected))
            result.append({'mission':mission,'name':name,'source':selected,
                           'relativeSource':selected.relative_to(root).as_posix(),
                           'sha256':hashlib.sha256(selected.read_bytes()).hexdigest()})
    return result


def publish_mission_defaults(source,root):
    """Write the exact Data/Missions paths opened by native stdio/File.c."""
    source,root=Path(source),Path(root)
    count=0
    for path in sorted(source.iterdir()):
        if not path.is_file() or not re.fullmatch(r'mission(?:0[1-9]|1[0-6])\.rtx(?:map|light)',path.name,re.I):continue
        for folder in (root/'Data/Missions',root/'missions'):
            target=folder/path.name
            target.parent.mkdir(parents=True,exist_ok=True)
            pending=target.with_name(target.name+'.authoring-pending')
            shutil.copyfile(path,pending);pending.replace(target)
            if target.read_bytes()!=path.read_bytes():raise ValueError('Mission default copy differs: '+str(target))
        count+=1
    return count


def verify_published_missions(source,root):
    source,root=Path(source),Path(root)
    maps=lights=0;files=[]
    for path in sorted(source.iterdir()):
        if not path.is_file() or not re.fullmatch(r'mission(?:0[1-9]|1[0-6])\.rtx(?:map|light)',path.name,re.I):continue
        target=root/'Data/Missions'/path.name
        if not target.is_file() or target.read_bytes()!=path.read_bytes():
            raise ValueError('Authored mission missing/different at runtime loader path: '+str(target))
        maps+=path.suffix.lower()=='.rtxmap';lights+=path.suffix.lower()=='.rtxlight'
        files.append({'path':target.relative_to(root).as_posix(),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
    if not maps:raise ValueError('No authored mission maps were installed')
    return {'maps':maps,'lighting':lights,'loaderDirectory':'Data/Missions','files':files}


def promote_wip_missions(wip,profile,assets):
    """Refresh the release profile from files the WIP actually opens."""
    wip,profile,assets=map(Path,(wip,profile,assets))
    selected=select_mission_files(wip)
    if {r['mission'] for r in selected if r['name'].endswith('.rtxmap')}!=set(range(1,17)):
        raise ValueError('All sixteen WIP map configurations must be available')
    inventory_path=profile/'content-inventory.json'
    inventory=json.loads(inventory_path.read_text())
    records={r['path']:r for r in inventory['files']}
    promoted=[]
    for row in selected:
        relative='Content/missions/'+row['name'].lower()
        for target in (profile/relative,assets/row['name']):
            target.parent.mkdir(parents=True,exist_ok=True)
            pending=target.with_name(target.name+'.promotion-pending')
            shutil.copyfile(row['source'],pending);pending.replace(target)
        records[relative]={'path':relative,'category':'authored-text','sha256':row['sha256']}
        promoted.append({'mission':row['mission'],'path':row['name'],'sha256':row['sha256'],
                         'origin':row['relativeSource']})
    inventory['files']=sorted(records.values(),key=lambda r:r['path'])
    inventory_path.write_text(json.dumps(inventory,indent=2))
    report={'schema':1,'baseline':'Effective WIP Missions 1-16, using runtime override precedence',
            'files':promoted}
    (assets/'authored-defaults.json').write_text(json.dumps(report,indent=2))
    (profile/'mission-authoring.json').write_text(json.dumps(report,indent=2))
    return report
