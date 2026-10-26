"""Import RM weaponfire presentation data, never weapon simulation settings.

The seven slots follow the recovered 006d527f loader. This is an RM import
boundary, not a second public OpenHW weapon grammar.
"""
import math
import re
from pathlib import Path
from import_kushan_weapon_fx import read_table

SLOTS=('fire','bullet','hit','anticipate','miss','nopenetrate','blowthrough')


def weapon_configs(script,kind,source):
    """Read presentation references and activation labels, not RM ballistics."""
    from plan_kushan_hod_bindings import literal_calls
    function,owner=('StartShipWeaponConfig','NewShipType') if kind=='ship' else ('StartSubSystemWeaponConfig','NewSubSystemType')
    rows=[];source=Path(source).resolve()
    for args in literal_calls(script,function,owner):
        if len(args)!=3:raise ValueError('Invalid source weapon mount declaration')
        weapon,mount,animation=args;name=weapon.lower()
        if not re.fullmatch(r'[a-z0-9_]{1,255}',name):raise ValueError('Unsafe RM weapon reference')
        path=source/'weapon'/name/(name+'.wepn')
        if not path.is_file():raise ValueError('Missing RM presentation weapon: '+name)
        text=re.sub(r'--[^\n]*','',path.read_text())
        # Only the four leading string parameters are consumed. Speed, range,
        # cadence, damage results, movement and aiming settings are not imported.
        matches=re.findall(r'(?m)^\s*StartWeaponConfig\(\s*NewWeaponType\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,',text)
        if len(matches)!=1:raise ValueError('Missing/ambiguous source weapon presentation header: '+name)
        _,fire_type,family,activation=matches[0]
        if not re.fullmatch(r'[A-Za-z0-9_]{1,255}',family):raise ValueError('Unsafe weaponfire reference')
        native=source/'scripts'/'weaponfire'/family.lower()/(family.lower()+'.wf')
        projectile=source/'missile'/family.lower()/(family.lower()+'.miss')
        rows.append(dict(weapon=weapon,mount=mount,animation=animation,weaponfire=family,
                         weaponfire_path=('@/RemasteredFX/WeaponFire/'+family.lower()+'.wf') if native.is_file() else None,
                         projectile_source=projectile.relative_to(source).as_posix() if projectile.is_file() else None,
                         fire_type=fire_type,activation=activation))
    return rows


def matching_weapon_configs(configs,markers):
    markers=set(markers)-{'','-'};result=[]
    for config in configs:
        base=config['mount']
        if any(m in (base+'_Position',base+'_Muzzle') or
               re.fullmatch(re.escape(base)+r'Slave\d*_(?:Position|Muzzle)',m) for m in markers):
            result.append(config)
    return result


def parse_weaponfire(text):
    if len(text)>1024*1024:raise ValueError('Oversized weaponfire definition')
    if any(ord(c)>126 or (ord(c)<32 and c not in '\r\n\t') for c in text):raise ValueError('Invalid weaponfire source character')
    data=read_table(text,None,preserve_keys=True,allow_products=False)
    accepted={'deathtype','scartype'}
    result=[]
    def number(value,field):
        if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value):
            raise ValueError('Invalid weaponfire number: '+field)
        return float(value)
    def name(field):
        value=data.get(field,'')
        if not isinstance(value,str) or len(value)>4096 or any(ord(c)<32 or ord(c)>126 for c in value):
            raise ValueError('Invalid weaponfire reference: '+field)
        return value
    for slot in SLOTS:
        accepted.update((slot+'fx',slot+'_sfx'))
        row=dict(name=slot,fx=name(slot+'fx'),sound=name(slot+'_sfx'))
        # No fire_* option reads exist in this RM loader. Preserve ignored
        # source keys in diagnostics instead of inventing aliases/behavior.
        if slot!='fire':
            for suffix in ('_nlips','_clamp','_sm'):accepted.add(slot+suffix)
            if slot+'_nlips' in data:row['nlips']=number(data[slot+'_nlips'],slot+'_nlips')
            if slot+'_clamp' in data:
                values=data[slot+'_clamp']
                if not isinstance(values,dict) or set(values)!={0,1}:
                    raise ValueError('Weaponfire clamp must have two values: '+slot)
                row['clamp']=[number(values[i],slot+'_clamp') for i in range(2)]
            if slot+'_sm' in data:
                value=data[slot+'_sm']
                if not isinstance(value,bool):raise ValueError('Weaponfire SM must be boolean: '+slot)
                row['sm']=value
        result.append(row)
    return dict(slots=result,deathType=name('deathtype'),scarType=name('scartype'),
                diagnostics=['RM loader does not consume source field: '+key for key in sorted(set(data)-accepted)])


def stage_weaponfire(source,output):
    """Stage original .wf bytes; the engine reads their native data grammar."""
    import hashlib
    import json
    import shutil
    source,output=Path(source).resolve(),Path(output).resolve()
    if source.is_relative_to(output) or output.is_relative_to(source):raise ValueError('Overlapping weaponfire staging')
    rows=[];names=set()
    for path in sorted(source.rglob('*.wf')):
        if path.is_symlink() or not path.resolve().is_relative_to(source):raise ValueError('Unsafe weaponfire source')
        name=path.stem.lower()
        if name in names or not re.fullmatch(r'[a-z0-9_]{1,255}',name):raise ValueError('Ambiguous weaponfire identity')
        raw=path.read_bytes();parsed=parse_weaponfire(raw.decode('ascii'));names.add(name)
        rows.append((path,name,dict(name=name,source=path.relative_to(source).as_posix(),
                                  sha256=hashlib.sha256(raw).hexdigest(),diagnostics=parsed['diagnostics'])))
    if not rows:raise ValueError('Missing native RM weaponfire set')
    destination=output/'RemasteredFX'/'WeaponFire';destination.mkdir(parents=True,exist_ok=True)
    for path,name,_ in rows:shutil.copyfile(path,destination/(name+'.wf'))
    (destination/'import.json').write_text(json.dumps([r for _,_,r in rows],indent=2),encoding='utf-8')
    return len(rows)


def stage_projectiles(source,output,families):
    """Stage referenced native missile models/events, never their simulation.

    The .miss file is retained as a source snapshot only. The receipt names
    original HOD/MAD and compiled events; it is not a public projectile grammar.
    No RM speed, health, lifetime, damage or guidance assignment is evaluated.
    """
    import hashlib
    import json
    import shutil
    from openhw_presentation_events import parse_source_events,compile_events,stage_events
    source,output=Path(source).resolve(),Path(output).resolve()
    if source.is_relative_to(output) or output.is_relative_to(source):
        raise ValueError('Overlapping projectile staging')
    families=set(families)
    if any(not isinstance(name,str) or not re.fullmatch(r'[a-z0-9_]{1,255}',name) for name in families):
        raise ValueError('Unsafe projectile identity')
    rows=[]
    for name in sorted(families):
        folder=source/'missile'/name;files=[]
        for extension in ('.miss','.hod','.events','.mad','.madstate'):
            path=folder/(name+extension)
            if not path.is_file():
                if extension in ('.miss','.hod','.events'):
                    raise ValueError('Missing native projectile source: '+name+extension)
                continue
            if path.is_symlink() or not path.resolve().is_relative_to(source):
                raise ValueError('Unsafe projectile source')
            raw=path.read_bytes()
            if not 0<len(raw)<=256*1024*1024:raise ValueError('Invalid projectile source size')
            if extension=='.events':
                try:parsed=parse_source_events(raw);compile_events(parsed)
                except ValueError as error:raise ValueError('Invalid projectile events '+name+': '+str(error)) from error
            files.append(dict(path=path,extension=extension,sha256=hashlib.sha256(raw).hexdigest(),bytes=len(raw)))
        rows.append((name,files))
    # Validate all selected families before creating any output.
    for name,_ in rows:
        destination=output/'RemasteredFX'/'Projectiles'/name
        for filename in (name+'.source.miss',name+'.hod',name+'.mad',name+'.madstate',
                         'events.owevents','events.source.events','events.import.json'):
            target=destination/filename
            if target.is_symlink() or not target.resolve().is_relative_to(output):
                raise ValueError('Projectile output escapes staging directory')
    index=output/'RemasteredFX'/'Projectiles'/'import.json'
    if index.is_symlink() or not index.resolve().is_relative_to(output):
        raise ValueError('Projectile receipt escapes staging directory')
    receipt=[]
    for name,files in rows:
        destination=output/'RemasteredFX'/'Projectiles'/name
        destination.mkdir(parents=True,exist_ok=True)
        for item in files:
            extension=item['extension']
            if extension=='.events':stage_events(item['path'],destination)
            else:
                target=name+'.source.miss' if extension=='.miss' else name+extension
                shutil.copyfile(item['path'],destination/target)
        receipt.append(dict(name=name,model=name+'.hod',events='events.owevents',
            animation=name+'.mad' if any(f['extension']=='.mad' for f in files) else None,
            source_files=[dict(source=f['path'].relative_to(source).as_posix(),
                               sha256=f['sha256'],bytes=f['bytes']) for f in files]))
    if receipt:
        (output/'RemasteredFX'/'Projectiles'/'import.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    return len(receipt)
