"""Compile data-only RM FX graphs without flattening or changing their scale.

OWFX0001 is an internal resource index, not another authored FX grammar.
Original source properties, typed curves, graph edges and resource names survive.
No particles are pre-generated and no visual style is silently discarded.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import shutil
from import_kushan_weapon_fx import read_table

STYLES=('STYLE_COMBO','STYLE_SPRAY','STYLE_RING','STYLE_BEAM','STYLE_LIGHT',
        'STYLE_TRAIL','STYLE_LENSFLARE','STYLE_HYPERSPACE')
TYPES=('VARTYPE_BOOL','VARTYPE_FLOAT','VARTYPE_INT','VARTYPE_STRING',
       'VARTYPE_ARRAY_TIMEFLOAT','VARTYPE_ARRAY_TIMEVECTOR3',
       'VARTYPE_ARRAY_TIMECOLOUR','VARTYPE_COLOUR')
DIMENSIONS={4:1,5:3,6:4}

def identity(value):
    if not isinstance(value,str) or not re.fullmatch(r'[A-Za-z0-9_ -]{1,255}',value) or value!=value.strip():
        raise ValueError('Invalid FX identity: '+str(value))
    return value.lower()

def number(value):
    if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or abs(value)>1e30:
        raise ValueError('Invalid FX number')
    return float(value)

def effect_choices(value):
    """RM 0064fa97/0064fa5e: comma tokens, duplicates retain weight; max 32.
    This is event choice semantics, not a filename alias or combo fan-out.
    """
    if not isinstance(value,str) or len(value)>1023:raise ValueError('Invalid FX choice list')
    return [identity(token) for token in value.split(',') if token][:32]

def parse_fx(raw):
    text=raw.decode('utf-8-sig') if isinstance(raw,bytes) else raw
    if len(text)>1024*1024:raise ValueError('Oversized FX source')
    table=read_table(text,preserve_keys=True)
    if not isinstance(table,dict) or not {'style','properties'}<=set(table) or set(table)-{'style','properties','frames'} or table['style'] not in STYLES:
        raise ValueError('Unknown FX root/style')
    if not isinstance(table['properties'],dict) or len(table['properties'])>512:
        raise ValueError('Invalid FX properties')
    result=[];seen=set()
    for entry in table['properties'].values():
        if not isinstance(entry,dict) or set(entry)!={'name','type','value'} or entry['type'] not in TYPES:
            raise ValueError('Unknown FX property fields/type')
        name=entry['name']
        if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9_% -]{1,127}',name):
            raise ValueError('Invalid FX property name')
        name=name.lower()
        if name in seen:raise ValueError('Duplicate FX property: '+name)
        seen.add(name);kind=TYPES.index(entry['type']);value=entry['value'];values=[];string=''
        if kind==3:
            if not isinstance(value,str) or len(value)>4096 or '\0' in value or not value.isascii():
                raise ValueError('Invalid FX string')
            string=value
        elif kind in DIMENSIONS:
            if not isinstance(value,dict) or not value:raise ValueError('Empty FX curve')
            width=DIMENSIONS[kind]+1;rows=list(value.values())
            if all(isinstance(row,dict) for row in rows):rows=[list(row.values()) for row in rows]
            elif all(isinstance(v,(int,float)) and not isinstance(v,bool) for v in rows):
                if len(rows)%width:raise ValueError('Truncated FX curve row')
                rows=[rows[i:i+width] for i in range(0,len(rows),width)]
            else:raise ValueError('Mixed FX curve layout')
            if len(rows)>4096:raise ValueError('Oversized FX curve')
            rows=sorted(rows,key=lambda row:number(row[0]) if row else -math.inf)
            previous=-math.inf
            for row in rows:
                if len(row)!=width:raise ValueError('Invalid FX curve dimensions')
                row=[number(v) for v in row]
                if not 0<=row[0]<=1 or row[0]<previous:raise ValueError('Unordered/out-of-range FX curve')
                previous=row[0];values.extend(row)
        elif kind==7:
            if not isinstance(value,dict) or len(value)!=4:raise ValueError('Invalid FX colour')
            values=[number(v) for v in value.values()]
        else:
            value=number(value)
            # Shipped BOOL properties can contain 99. Preserve the integer;
            # native truth testing is nonzero, not an importer value rewrite.
            if kind in (0,2) and value!=int(value):raise ValueError('Invalid FX integer/boolean')
            values=[value]
        result.append(dict(name=name,type=kind,string=string,values=values))
    if 'frames' in table:
        frames=table['frames']
        if not isinstance(frames,dict) or set(frames)!={'timeStart'} or not isinstance(frames['timeStart'],dict):
            raise ValueError('Unknown FX frame fields')
        values=[number(v) for v in frames['timeStart'].values()]
        if not values or len(values)>4096:raise ValueError('Invalid FX frame timings')
        result.append(dict(name='@frames.timestart',type=8,string='',values=values))
    return dict(style=STYLES.index(table['style']),properties=result)

def dependencies(effect):
    style=STYLES[effect['style']]
    for prop in effect['properties']:
        if ((style=='STYLE_COMBO' and re.fullmatch(r'fx[0-9]+',prop['name'])) or
            (style=='STYLE_SPRAY' and prop['name']=='particle_fx') or
            (style=='STYLE_RING' and prop['name'] in ('birthspawn_fx','spawn_fx'))):
            if prop['type']!=3:raise ValueError('Non-string FX graph edge')
            if prop['string']:yield identity(prop['string'])

def compile_graph(source,roots):
    source=Path(source).resolve();graphs={};hashes={};visiting=set();heights={}
    def load(name,depth=0):
        name=identity(name)
        if name in visiting or depth>64:raise ValueError('Cyclic/deep FX graph: '+name)
        if name in graphs:return heights[name]
        if len(graphs)>=8192:raise ValueError('FX graph count exceeds runtime bound')
        path=source/(name+'.lua')
        if path.is_symlink() or not path.resolve().is_relative_to(source):raise ValueError('FX source escapes directory')
        raw=path.read_bytes();effect=parse_fx(raw);visiting.add(name);height=0
        for child in dependencies(effect):height=max(height,1+load(child,depth+1))
        if height>64:raise ValueError('Deep FX graph: '+name)
        visiting.remove(name);graphs[name]=effect;hashes[name]=hashlib.sha256(raw).hexdigest();heights[name]=height
        return height
    for root in roots:
        for choice in effect_choices(root):load(choice)
    if not graphs:raise ValueError('No FX roots')
    strings=bytearray(b'\0');interned={'':0};nodes=[];properties=[];values=[]
    def intern(value):
        if value not in interned:
            interned[value]=len(strings);strings.extend(value.encode('ascii')+b'\0')
        return interned[value]
    for name,effect in sorted(graphs.items()):
        first=len(properties)
        for prop in effect['properties']:
            properties.append((intern(prop['name']),prop['type'],intern(prop['string']),len(values),len(prop['values'])))
            values.extend(prop['values'])
        nodes.append((intern(name),effect['style'],first,len(properties)-first))
    if len(properties)>1000000 or len(values)>8000000 or len(strings)>4*1024*1024:
        raise ValueError('FX graph tables exceed runtime bounds')
    output=bytearray(struct.pack('<8s4I',b'OWFX0001',len(nodes),len(properties),len(values),len(strings)))
    for node in nodes:output.extend(struct.pack('<4I',*node))
    for prop in properties:output.extend(struct.pack('<5I',*prop))
    for value in values:output.extend(struct.pack('<f',value))
    output.extend(strings)
    return bytes(output),dict(schema='OPENHW_FX_GRAPH_IMPORT_1',effects=hashes,
        nodes=len(nodes),properties=len(properties),values=len(values),
        flattened=False,particle_count_reduction=False,scale_adjustments=False)

def stage_resources(source,output,receipt):
    """Copy referenced original images, animations, flare lists and HOD meshes.
    Validate the complete closure first; never bake mesh vertices or resize FX.
    """
    from extract_remastered_big import safe_path
    source,output=Path(source).resolve(),Path(output).resolve()
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError('FX resources must be staged separately from source')
    resources={};visiting=set()
    def path_name(value):
        value=value.replace('\\','/').lower()
        if value.startswith('data:'):value=value[5:].lstrip('/')
        path=safe_path(value);name=path.as_posix()
        if not name.startswith(('art/fx/','effect/lensflare/','effect/trails/')):
            raise ValueError('FX resource outside presentation directories: '+name)
        return name
    def strings(value):
        if isinstance(value,str):yield value
        elif isinstance(value,dict):
            for child in value.values():yield from strings(child)
    def load(value,depth=0):
        name=path_name(value)
        if name in visiting or depth>32:raise ValueError('Cyclic/deep FX resource list')
        if name in resources:return
        path=source/name
        if path.is_symlink() or not path.resolve().is_relative_to(source):raise ValueError('FX resource escapes source')
        if path.suffix not in ('.tga','.dds','.anim','.hod','.ahod','.flare'):
            raise ValueError('Unsupported native FX resource type: '+name)
        raw=path.read_bytes()
        if not raw or len(raw)>128*1024*1024:raise ValueError('Empty/oversized native FX resource: '+name)
        visiting.add(name)
        if path.suffix in ('.anim','.ahod','.flare'):
            table=read_table(raw.decode('utf-8-sig'),assignment=None,preserve_keys=True)
            for child in strings(table):
                if child.lower().endswith(('.tga','.dds','.hod','.ahod','.anim')):load(child,depth+1)
        visiting.remove(name);resources[name]=(path,len(raw),hashlib.sha256(raw).hexdigest())
        if len(resources)>8192 or sum(row[1] for row in resources.values())>1024**3:
            raise ValueError('Native FX resource closure exceeds staging budget')
    for name,digest in receipt['effects'].items():
        path=source/'art/fx'/(identity(name)+'.lua');raw=path.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=digest:raise ValueError('FX source changed after graph compilation')
        effect=parse_fx(raw);props={p['name']:p for p in effect['properties']}
        mesh=props.get('usemesh');use_mesh=mesh and mesh['values'] and mesh['values'][0]!=0
        for key in ('texture','edgetexture','edgetexture2','mesh'):
            prop=props.get(key)
            if prop and prop['string'] and (key!='mesh' or use_mesh):load(prop['string'])
        flare=props.get('lensflare')
        if effect['style']==6 and flare and flare['string']:
            load('effect/lensflare/'+identity(flare['string'])+'.flare')
    rows=[]
    for name,(path,size,digest) in sorted(resources.items()):
        target=output/name
        if target.is_symlink() or not target.resolve().is_relative_to(output):raise ValueError('FX resource output escapes staging')
        rows.append(dict(path=name,bytes=size,sha256=digest))
    for row in rows:
        target=output/row['path'];target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source/row['path'],target)
    return rows

def compile_resources(source,resources):
    """Index original frames, mesh choices and flares without baking assets."""
    from extract_remastered_big import safe_path
    source=Path(source).resolve();bitmaps=[];frames=[];meshes=[];choices=[];flares=[];hotspots=[];strings=bytearray(b'\0');pool={'':0}
    def text(value):
        if value not in pool:pool[value]=len(strings);strings.extend(value.encode('ascii')+b'\0')
        return pool[value]
    def canonical(value):
        value=value.replace('\\','/').lower()
        if value.startswith('data:'):value=value[5:].lstrip('/')
        return safe_path(value).as_posix()
    def image(value):
        value=canonical(value)
        if value not in names or not value.endswith(('.tga','.dds')):raise ValueError('Missing FX bitmap image: '+value)
        return text(value)
    def vector(value,count):
        if not isinstance(value,dict) or len(value)!=count:raise ValueError('Invalid FX resource vector')
        return [number(v) for v in value.values()]
    names={row['path'] for row in resources}
    for name in sorted(names):
        extension=Path(name).suffix
        if extension in ('.hod','.ahod'):
            first=len(choices)
            values=[name] if extension=='.hod' else list(read_table((source/name).read_text(encoding='utf-8-sig'),assignment='mesh',preserve_keys=True).values())
            for value in values:
                value=canonical(value)
                if value not in names or not value.endswith('.hod'):raise ValueError('Missing native FX mesh choice: '+value)
                choices.append(text(value))
            if first==len(choices) or len(choices)-first>1024:raise ValueError('Empty/oversized native FX mesh list')
            meshes.append((text(name),first,len(choices)-first));continue
        if extension=='.flare':
            table=read_table((source/name).read_text(encoding='utf-8-sig'),assignment=None,preserve_keys=True)
            if set(table)!={'fadetimes','hotspots','washoutColour','washoutStrength'}:raise ValueError('Unknown native flare fields')
            first=len(hotspots)
            for entry in table['hotspots'].values():
                if set(entry)!={'colour','offset','rotation','scale','texture'}:raise ValueError('Unknown native flare hotspot')
                hotspots.append((image(entry['texture']),number(entry['offset']),number(entry['scale']),number(entry['rotation']),*vector(entry['colour'],4)))
            flares.append((text(name),first,len(hotspots)-first,*vector(table['fadetimes'],4),*vector(table['washoutColour'],4),number(table['washoutStrength'])));continue
        if extension not in ('.tga','.dds','.anim'):continue
        first=len(frames);fps=0;start=0;loops=0
        if extension!='.anim':frames.append((image(name),0,0,1,1))
        else:
            table=read_table((source/name).read_text(encoding='utf-8-sig'),assignment=None,preserve_keys=True)
            if set(table) not in ({'fps','startFrame','loopCount','texture'},
                                  {'fps','startFrame','loopCount','layout','textureLayout'}):
                raise ValueError('Unknown native bitmap animation fields: '+name)
            fps=number(table['fps']);start=number(table['startFrame']);loops=number(table['loopCount'])
            if not 0<=fps<=1000 or start!=int(start) or not 0<=start<=65535 or loops!=int(loops) or not 0<=loops<=65535:
                raise ValueError('Invalid native bitmap animation timing')
            if 'texture' in table:
                for value in table['texture'].values():frames.append((image(value),0,0,1,1))
            else:
                layout=list(table['layout'].values())
                if len(layout)!=2 or any(number(v)!=int(v) or not 1<=v<=128 for v in layout):raise ValueError('Invalid native bitmap layout')
                rows,columns=map(int,layout)
                for entry in table['textureLayout'].values():
                    values=list(entry.values())
                    if len(values)!=3:raise ValueError('Invalid native bitmap frame')
                    path,row,column=values
                    if number(row)!=int(row) or number(column)!=int(column) or not 0<=row<rows or not 0<=column<columns:
                        raise ValueError('Native bitmap cell outside layout')
                    frames.append((image(path),int(row),int(column),rows,columns))
            if len(frames)==first or start>=len(frames)-first:raise ValueError('Empty/out-of-range native bitmap frames')
        bitmaps.append((text(name),first,len(frames)-first,fps,int(start),int(loops)))
    if len(bitmaps)>8192 or len(frames)>1000000 or len(meshes)>8192 or len(choices)>65536 or len(flares)>1024 or len(hotspots)>16384 or len(strings)>4*1024*1024:raise ValueError('FX resource index exceeds bounds')
    result=bytearray(struct.pack('<8s7I',b'OWFXRS01',len(bitmaps),len(frames),len(meshes),len(choices),len(flares),len(hotspots),len(strings)))
    for row in bitmaps:result.extend(struct.pack('<IIIfII',*row))
    for row in frames:result.extend(struct.pack('<5I',*row))
    for row in meshes:result.extend(struct.pack('<3I',*row))
    for value in choices:result.extend(struct.pack('<I',value))
    for row in flares:result.extend(struct.pack('<3I9f',*row))
    for row in hotspots:result.extend(struct.pack('<I7f',*row))
    result.extend(strings);return bytes(result)

def selected_roots(source,runtime):
    """Collect from installed model/event snapshots and their weapon mounts.
    Do not load unrelated HW2 weapons just because they share the source BIG.
    """
    from openhw_presentation_events import parse_source_events
    from openhw_weapon_presentation import weapon_configs,parse_weaponfire
    source,runtime=Path(source).resolve(),Path(runtime).resolve()
    roots=set();families=set();snapshots=[]
    for path in sorted(runtime.rglob('*.source.events')):
        if path.is_symlink() or not path.resolve().is_relative_to(runtime):raise ValueError('Unsafe event snapshot')
        parsed=parse_source_events(path.read_bytes());snapshots.append(path.relative_to(runtime).as_posix())
        for event in parsed['events']:
            roots.update(effect_choices(event['actions'].get('fx','')))
    for suffix,kind in (('ship','ship'),('subs','subsystem')):
        for path in sorted(runtime.rglob('*.source.'+suffix)):
            if path.is_symlink() or not path.resolve().is_relative_to(runtime):raise ValueError('Unsafe weapon snapshot')
            for config in weapon_configs(path.read_text(encoding='utf-8-sig'),kind,source):
                if config['weaponfire_path']:families.add(config['weaponfire'].lower())
    for family in sorted(families):
        path=source/'scripts/weaponfire'/family/(family+'.wf')
        parsed=parse_weaponfire(path.read_text(encoding='ascii'))
        for slot in parsed['slots']:roots.update(effect_choices(slot['fx']))
    return sorted(roots),dict(event_snapshots=snapshots,weaponfire_families=sorted(families))


def stage_selected(source,runtime,output):
    source,runtime,output=Path(source).resolve(),Path(runtime).resolve(),Path(output).resolve()
    if source.is_relative_to(output) or output.is_relative_to(source):raise ValueError('Overlapping native FX import')
    roots,selection=selected_roots(source,runtime)
    destination=output/'RemasteredFX/Native'
    for filename in ('effects.fxbank','effects.fxresources','import.json'):
        target=destination/filename
        if target.is_symlink() or not target.resolve().is_relative_to(output):raise ValueError('FX index output escapes staging')
    blob,receipt=compile_graph(source/'art/fx',roots)
    receipt['selection']=selection;receipt['roots']=roots
    receipt['resources']=stage_resources(source,output,receipt)
    resources=compile_resources(source,receipt['resources'])
    destination.mkdir(parents=True,exist_ok=True)
    (destination/'effects.fxbank').write_bytes(blob)
    (destination/'effects.fxresources').write_bytes(resources)
    (destination/'import.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    return receipt


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--resources-source',type=Path)
    parser.add_argument('roots',nargs='+')
    args=parser.parse_args();blob,receipt=compile_graph(args.source,args.roots)
    output=args.output.resolve()
    if output.is_relative_to(args.source.resolve()):raise ValueError('Output must not replace source assets')
    output.mkdir(parents=True,exist_ok=True)
    if args.resources_source:
        receipt['resources']=stage_resources(args.resources_source,output,receipt)
        (output/'effects.fxresources').write_bytes(compile_resources(args.resources_source,receipt['resources']))
    (output/'effects.fxbank').write_bytes(blob)
    (output/'import.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    print('Native FX graph nodes:',receipt['nodes'])

if __name__=='__main__':main()
