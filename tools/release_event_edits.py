"""Source-bound metadata edits; never bundle converted retail effect binaries."""
import copy
import hashlib
import json
from pathlib import Path
import re
from openhw_presentation_events import parse_source_events, compile_events


def changes(before, after, path=()):
    result=[]
    if isinstance(before,dict) and isinstance(after,dict):
        for key in before.keys()-after.keys():result.append({'path':[*path,key],'remove':True})
        for key in after:
            if key not in before:result.append({'path':[*path,key],'value':after[key]})
            else:result.extend(changes(before[key],after[key],(*path,key)))
    elif isinstance(before,list) and isinstance(after,list):
        for i in range(min(len(before),len(after))):result.extend(changes(before[i],after[i],(*path,i)))
        for i in reversed(range(len(after),len(before))):result.append({'path':[*path,i],'remove':True})
        for i in range(len(before),len(after)):result.append({'path':[*path,i],'append':after[i]})
    elif before!=after:result.append({'path':list(path),'value':after})
    return result


def apply(value, edits):
    result=copy.deepcopy(value)
    for edit in edits:
        path=edit['path']
        if not path:raise ValueError('Whole-document replacement is not a source-bound edit')
        parent=result
        for key in path[:-1]:parent=parent[key]
        key=path[-1]
        if edit.get('remove'):del parent[key]
        elif 'append' in edit:
            if not isinstance(parent,list) or key!=len(parent):raise ValueError('Invalid event edit append')
            parent.append(edit['append'])
        else:parent[key]=edit['value']
    return result


def capture(current, sources, profile):
    current,sources,profile=map(Path,(current,sources,profile))
    descriptors={}
    for suffix in ('.ship','.subs'):
        for path in sources.rglob('*'+suffix):
            descriptors.setdefault(hashlib.sha256(path.read_bytes()).hexdigest(),[]).append(path)
    records=[];verified=0
    for target in (current/'Data').rglob('*.owevents'):
        paired=target.with_name(target.stem+'.source.events')
        if not paired.is_file():continue
        candidates=[]
        for suffix in ('.ship','.subs'):
            descriptor=target.with_name(target.stem+'.source'+suffix)
            if descriptor.is_file():candidates+=descriptors.get(hashlib.sha256(descriptor.read_bytes()).hexdigest(),[])
        candidates=[p.with_suffix('.events') for p in candidates if p.with_suffix('.events').is_file()]
        if not candidates:
            definition=target.parent/'subsystem.owsubsystem'
            if definition.is_file():
                hull=re.search(r'^hull\s+@/Subsystems/([^/]+)/',definition.read_text(encoding='latin1'),re.M)
                if hull:candidates=[sources/'subsystem'/hull[1]/(hull[1]+'.events')]
        if not candidates:raise ValueError('Cannot identify original event source: '+str(target))
        source=candidates[0];harvest=source.parent.name in ('kus_resourcecollector','tai_resourcecollector')
        original=parse_source_events(source.read_bytes());wanted=parse_source_events(paired.read_bytes())
        compiled,_=compile_events(wanted,classic_harvest=harvest)
        if compiled!=target.read_bytes():raise ValueError('Editable event source does not reproduce runtime: '+str(target))
        verified+=1
        edits=changes(original,wanted)
        if apply(original,edits)!=wanted:raise ValueError('Event edit failed roundtrip')
        records.append({'target':target.relative_to(current/'Data').as_posix().lower(),
            'source':source.relative_to(sources).as_posix(),
            'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'output_sha256':hashlib.sha256(compiled).hexdigest(),
            'classic_harvest':harvest,'edits':edits})
    (profile/'event-edits.json').write_text(json.dumps({'schema':1,'verified_guides':verified,'edits':records},indent=2))
    print('Event source audit:',verified,'guides verified;',sum(bool(r['edits']) for r in records),'source-bound changes',flush=True)


def rebuild(root,generated):
    root,generated=Path(root),Path(generated)
    recipe=root/'Development/event-edits.json'
    if not recipe.is_file():return
    payload=json.loads(recipe.read_text())
    if payload.get('schema')!=1:raise ValueError('Unrecognized event edits')
    sources=root/'Development/SetupSources/Remastered'
    for row in payload['edits']:
        source=(sources/row['source']).resolve();target=(generated/row['target']).resolve()
        if not source.is_relative_to(sources.resolve()) or not target.is_relative_to(generated.resolve()):
            raise ValueError('Unsafe event recipe path')
        raw=source.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=row['source_sha256']:raise ValueError('Event source version differs: '+row['source'])
        parsed=apply(parse_source_events(raw),row['edits'])
        compiled,_=compile_events(parsed,classic_harvest=row['classic_harvest'])
        if hashlib.sha256(compiled).hexdigest()!=row['output_sha256']:raise ValueError('Event recipe output differs')
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(compiled)
