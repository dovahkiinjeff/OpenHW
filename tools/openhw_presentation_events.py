"""Data-only RM event ingestion. Does not execute Lua or import gameplay.

Retains named timelines, every event action, marker lists and source spelling.
This is an ingestion/validation layer, not a claim of renderer compatibility.
"""
import math
import struct
import hashlib
import json
from pathlib import Path
from import_kushan_weapon_fx import read_table

ACTION_FIELDS = frozenset(('anim', 'animtime', 'marker', 'fx', 'sound',
                          'fx_scale', 'fx_nlips', 'fx_sm', 'gameEv',
                          'fx_orphan', 'fx_length', 'fx_other',
                          'fx_othermarker', 'fx_transform'))
ANIMATION_FIELDS = frozenset(('name', 'length', 'loop', 'parent', 'minimum',
                             'maximum', 'markers'))

# Verified against an independent extraction from Homeworld2.big. This source
# has a missing `events = {` opening, not a second supported event grammar.
# Never perform a structural repair on arbitrary user/mod source. Original
# bytes remain in .source.events and the correction is recorded in the receipt.
SOURCE_EVENT_CORRECTIONS={
    '9584d1611bc11ef3dd731b7ce413561887903f2f4e0403564bd26e8d5c9f8912':
        (b'    },\r\n        event1 =',b'    },\r\n    events =\r\n    {\r\n        event1 =',
         'RM tur_smallmissile.events: inserted missing events table opening (verified archive hash)'),
    # Both original archives' HW1 missile events use this obsolete marker.
    # Their native models and RM engine lookup (004c5b24/004c6a88) use the
    # unique EngineNozzle1 anchor. Correct only these exact source bytes,
    # never teach the runtime a second marker spelling or arbitrary alias.
    '523040ac1d2c50d26d7391ba0ed45086467346e41c2e14586c2f0c3e1ae3c420':
        (b'"MissileEngine1"',b'"EngineNozzle1"',
         'RM kus_missile/tai_missile.events: corrected obsolete nozzle marker to native EngineNozzle1 (verified archive hash)')
}


def parse_source_events(raw):
    correction=SOURCE_EVENT_CORRECTIONS.get(hashlib.sha256(raw).hexdigest())
    diagnostics=[];data=raw
    if correction:
        before,after,reason=correction
        if raw.count(before)!=1:raise ValueError('Known event source correction precondition failed')
        data=raw.replace(before,after,1);diagnostics.append(reason)
    parsed=parse_events(data.decode('utf-8-sig'))
    parsed['diagnostics'][:0]=diagnostics
    return parsed


def finite_number(value, field):
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise ValueError('Invalid event number: '+field) from None
    if not math.isfinite(result):
        raise ValueError('Non-finite event number: '+field)
    return result


def parse_events(text):
    if len(text) > 1024*1024:
        raise ValueError('Oversized event definition')
    if not text.strip():
        return dict(schema='OPENHW_PRESENTATION_EVENTS_1', timelines=[], events=[],
                    diagnostics=['Empty source event file'])
    # Two shipped mothership files repeat event identifiers. Match Lua's
    # last-definition-wins semantics, but expose the loss in import diagnostics.
    diagnostics = []
    source = read_table(text, 'effects', preserve_keys=True,
                        on_duplicate=lambda key: diagnostics.append(
                            'Duplicate source key (last definition used): '+str(key)))
    if not isinstance(source, dict) or set(source)-{'animations', 'events'}:
        raise ValueError('Unsupported event root fields')
    timelines, events = [], []
    for section in ('animations', 'events'):
        if not isinstance(source.get(section, {}), dict):
            raise ValueError('Invalid event section: '+section)
        if len(source.get(section, {})) > 4096:
            raise ValueError('Too many event records: '+section)
    names = set()
    for identifier, row in source.get('animations', {}).items():
        if not isinstance(row, dict) or set(row)-ANIMATION_FIELDS:
            raise ValueError('Unsupported animation fields: '+str(identifier))
        name = row.get('name')
        if not isinstance(name, str) or not name:
            raise ValueError('Missing animation name')
        folded = name.casefold()
        if folded in names:
            raise ValueError('Ambiguous animation name: '+name)
        names.add(folded)
        length = finite_number(row.get('length'), 'length')
        loop = finite_number(row.get('loop', 0), 'loop')
        if length <= 0 or loop not in (0, 1):
            raise ValueError('Invalid animation duration/loop: '+name)
        parent = row.get('parent', '')
        if not isinstance(row.get('markers', {}), dict):
            raise ValueError('Invalid animation markers: '+name)
        markers = list(row.get('markers', {}).values())
        if not isinstance(parent, str) or any(not isinstance(m, str) for m in markers):
            raise ValueError('Invalid animation parent/markers: '+name)
        timelines.append(dict(id=identifier, name=name, length=length, loop=bool(loop),
                              parent=parent, minimum=finite_number(row.get('minimum', 0), 'minimum'),
                              maximum=finite_number(row.get('maximum', 0), 'maximum'), markers=markers))
    for timeline in timelines:
        if timeline['parent'] and timeline['parent'].casefold() not in names:
            diagnostics.append('Missing parent timeline: '+timeline['parent'])
    parents = {t['name'].casefold(): t['parent'].casefold() for t in timelines}
    for name in parents:
        visited = set()
        current = name
        while current in parents:
            if current in visited:
                raise ValueError('Cyclic event timeline parent: '+name)
            visited.add(current)
            current = parents[current]
    for identifier, row in source.get('events', {}).items():
        if not isinstance(row, dict):
            raise ValueError('Invalid event: '+str(identifier))
        fields = {}
        for pair in row.values():
            if not isinstance(pair, dict) or set(pair) != {0, 1}:
                raise ValueError('Invalid event field pair: '+str(identifier))
            key, value = pair[0], pair[1]
            if not isinstance(key, str) or not isinstance(value, str) or key in fields:
                raise ValueError('Invalid/duplicate event field: '+str(identifier))
            fields[key] = value
        unknown = set(fields)-ACTION_FIELDS
        if unknown:
            raise ValueError('Unsupported event actions: '+', '.join(sorted(unknown)))
        name = fields.get('anim', '')
        if not name or name.casefold() not in names:
            diagnostics.append('Missing event timeline: '+name+' ('+str(identifier)+')')
        time = finite_number(fields.get('animtime', '0'), 'animtime')
        # RM authors -1 on damage/radiation events. Preserve this sentinel;
        # it must not be clamped into an ordinary start-of-animation burst.
        if (time < 0 and time != -1) or time > 1:
            raise ValueError('Event phase outside -1 or 0..1')
        scale = fields.get('fx_scale', '1')
        if scale.casefold() not in ('entity', 'radius') and finite_number(scale, 'fx_scale') < 0:
            raise ValueError('Negative effect scale')
        # Do not coalesce simultaneous events: smoke, sound and muzzle layers
        # may share the same timestamp but have different attachment markers.
        events.append(dict(id=identifier, timeline=name, time=time,
                           markers=[m.strip() for m in fields.get('marker', '').split(',') if m.strip()],
                           actions=fields))
    return dict(schema='OPENHW_PRESENTATION_EVENTS_1', timelines=timelines,
                events=events, diagnostics=diagnostics)


def compile_events(parsed, *, classic_harvest=False):
    """Bounded runtime representation; original source remains editable.

    Unresolved source timelines are reported and omitted explicitly. This
    never invents an animation length or changes an HW1 gameplay parameter.
    """
    strings=bytearray(b'\0');offsets={'':0}
    diagnostics=list(parsed['diagnostics'])
    def text(value):
        if value not in offsets:
            encoded=value.encode('ascii')
            if len(encoded)>4096 or any(c<32 or c>126 for c in encoded):
                raise ValueError('Invalid runtime event string')
            offsets[value]=len(strings);strings.extend(encoded+b'\0')
        return offsets[value]
    timelines=parsed['timelines'];names={t['name'].casefold():i for i,t in enumerate(timelines)}
    table=bytearray();records=bytearray();count=0
    for timeline in timelines:
        duration=timeline['length']
        if not 0<duration<=3600:raise ValueError('Runtime timeline duration out of range')
        table+=struct.pack('<IIfffI',text(timeline['name']),names.get(timeline['parent'].casefold(),0xffffffff),
                           duration,timeline['minimum'],timeline['maximum'],int(timeline['loop']))
    for event in parsed['events']:
        index=names.get(event['timeline'].casefold())
        if index is None:
            diagnostics.append('Not compiled: unresolved event '+str(event['id']));continue
        action=event['actions'];scale=action.get('fx_scale','1');flags=0
        fx=action.get('fx','')
        if classic_harvest and event['timeline'].casefold().startswith('resourcing') and fx:
            # HW1's resource ETG owns the nozzle-to-resource geometry and uses
            # the installed upscaled texture set. RM event timing and sound
            # remain intact; only the duplicate RM visual cue is omitted.
            diagnostics.append('HW1 resource ETG replaces RM '+fx+' on '+event['timeline'])
            fx=''
        if scale.casefold()=='entity':scale=1;flags|=1
        elif scale.casefold()=='radius':scale=1;flags|=8
        else:scale=finite_number(scale,'fx_scale')
        for key,flag in (('fx_nlips',2),('fx_sm',4)):
            value=action.get(key,'off').casefold()
            if value not in ('on','off'):raise ValueError('Invalid event switch '+key)
            if value=='on':flags|=flag
        if not 0<=scale<=100000 or event['time']>1:
            raise ValueError('Runtime event scale/time out of range')
        records+=struct.pack('<IffIIIII',index,event['time'],scale,flags,
            text(','.join(event['markers'])),text(fx),text(action.get('sound','')),
            text(action.get('gameEv','')))
        # Presence, not truthiness, controls orphaning in RM's dispatcher.
        length=action.get('fx_length','1');length_mode=0
        modes={'entity':1,'radius':2,'distance':3}
        if length.casefold() in modes:
            length_mode=modes[length.casefold()];length=1
        else:length=finite_number(length,'fx_length')
        if not 0<=length<=100000:raise ValueError('Runtime event length out of range')
        other=action.get('fx_other','').casefold()
        transform=action.get('fx_transform','').casefold()
        others={'':0,'self':1,'target':2}
        transforms={'':0,'selftoother':1,'othermarker':2,'othertoself':3,'none':4,'between':5}
        if other not in others or transform not in transforms:
            raise ValueError('Invalid event attachment mode')
        attachment=int('fx_orphan' in action)|(length_mode<<1)
        records+=struct.pack('<IfIIII',attachment,length,others[other],transforms[transform],
                             text(action.get('fx_othermarker','')),text(action.get('fx_orphan','')))
        count+=1
    if len(strings)>1024*1024:raise ValueError('Runtime event string pool too large')
    return struct.pack('<8sIII',b'OWEVENT2',len(timelines),count,len(strings))+table+records+strings,diagnostics


def weapon_animation(parsed, markers, allowed=None):
    """Resolve exact authored attachments, never a guessed gameplay gun index."""
    roots={t['name'].casefold() for t in parsed['timelines'] if not t['parent']}
    markers=set(markers)-{'','-'};matches={}
    for event in parsed['events']:
        if (event['time']>=0 and event['timeline'].casefold() in roots and
            (allowed is None or event['timeline'].casefold() in {name.casefold() for name in allowed}) and
            markers.intersection(event['markers']) and
            (event['actions'].get('fx') or event['actions'].get('sound'))):
            matches[event['timeline'].casefold()]=event['timeline']
    if len(matches)>1:raise ValueError('Ambiguous weapon event attachment: '+', '.join(sorted(matches.values())))
    return next(iter(matches.values()),None)


def stage_events(source, destination, stem='events'):
    source, destination=Path(source),Path(destination)
    if not source.is_file():return None
    if not stem.isascii() or not stem or any(not(c.isalnum() or c=='_') for c in stem):
        raise ValueError('Invalid staged event identity')
    raw=source.read_bytes()
    parsed=parse_source_events(raw)
    classic_harvest=source.parent.name.casefold() in ('kus_resourcecollector','tai_resourcecollector')
    compiled,diagnostics=compile_events(parsed,classic_harvest=classic_harvest)
    destination.mkdir(parents=True,exist_ok=True)
    name=stem+'.owevents'
    (destination/name).write_bytes(compiled)
    (destination/(stem+'.source.events')).write_bytes(raw)
    category=source.parent.parent.name
    if category in ('ship','subsystem'):
        extension='.ship' if category=='ship' else '.subs'
        descriptor=source.with_suffix(extension)
        if descriptor.is_file():
            (destination/(stem+'.source'+extension)).write_bytes(descriptor.read_bytes())
    (destination/(stem+'.import.json')).write_text(json.dumps(dict(
        source_sha256=hashlib.sha256(raw).hexdigest(),runtime_sha256=hashlib.sha256(compiled).hexdigest(),
        timelines=len(parsed['timelines']),source_events=len(parsed['events']),diagnostics=diagnostics),indent=2),encoding='utf-8')
    return name


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();count=0
    source=args.source.resolve();output=args.output.resolve()
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError('Event staging must be separate from source')
    for category in ('ship','subsystem'):
        for path in sorted((source/category).rglob('*.events')):
            stage_events(path,output/path.relative_to(source).parent);count+=1
    print('Staged event definitions:',count)
