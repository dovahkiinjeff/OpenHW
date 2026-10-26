"""Translate installed RM Kushan weapon visuals. Never execute source Lua.

Only data tables, quoted weapon/event references and numeric FX curves are read.
Source archives and gameplay stats are never changed. Unsupported render styles
are recorded in the report; events with no translated visible layer fall back.
"""
import argparse
import hashlib
import json
import math
import random
import re
import struct
from pathlib import Path
from PIL import Image
from extract_remastered_big import extract
from plan_kushan_hod_bindings import R1_HULLS

TOKEN = re.compile(r'\s+|--[^\n]*|"(?:\\.|[^"\\])*"|-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|[A-Za-z_][\w]*|[{}=,;*]')

def read_table(text, assignment='fx', preserve_keys=False, on_duplicate=None, allow_products=True):
    tokens=[]; end=0
    for m in TOKEN.finditer(text):
        if text[end:m.start()].strip(): raise ValueError('Not a data-only Lua table')
        end=m.end()
        if not m[0].isspace() and not m[0].startswith('--'): tokens.append(m[0])
    if text[end:].strip(): raise ValueError('Unexpected trailing source')
    i=0
    def value(depth=0):
        nonlocal i
        if depth>32 or i>=len(tokens): raise ValueError('Invalid/nested FX table')
        t=tokens[i]; i+=1
        if t=='{':
            result={}; n=0
            while i<len(tokens) and tokens[i]!='}':
                if i+1<len(tokens) and tokens[i+1]=='=':
                    key=tokens[i] if preserve_keys else tokens[i].lower(); i+=2
                else: key=n; n+=1
                if key in result:
                    if on_duplicate is None: raise ValueError('Duplicate data table key: '+str(key))
                    on_duplicate(key)
                result[key]=value(depth+1)
                if i<len(tokens) and tokens[i] in (',',';'): i+=1
                elif i<len(tokens) and tokens[i]!='}': raise ValueError('Expected separator')
            if i>=len(tokens): raise ValueError('Unterminated FX table')
            i+=1; return result
        if t.startswith('"'): return json.loads(t)
        if t in ('true','false'): return t=='true'
        if re.fullmatch(r'-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?',t):
            v=float(t)
            # Authored event durations include literal products (3.5*1.5).
            # Fold numeric literals only; never execute source expressions.
            while i<len(tokens) and tokens[i]=='*':
                if not allow_products:raise ValueError('Expressions are not allowed in this data grammar')
                i+=1
                if i>=len(tokens) or not re.fullmatch(r'-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?',tokens[i]):
                    raise ValueError('Expected literal numeric factor')
                v*=float(tokens[i]);i+=1
            if not math.isfinite(v): raise ValueError('Non-finite FX number')
            return v
        raise ValueError('Executable/unknown Lua expression: '+t)
    if assignment is None:
        result={}
        while i<len(tokens):
            if i+1>=len(tokens) or not re.fullmatch(r'[A-Za-z_][\w]*',tokens[i]) or tokens[i+1]!='=':
                raise ValueError('Expected data assignment')
            key=tokens[i] if preserve_keys else tokens[i].lower();i+=2
            if key in result: raise ValueError('Duplicate data assignment: '+key)
            result[key]=value()
            if i<len(tokens) and tokens[i]==';':i+=1
        return result
    if tokens[:2]!=[assignment,'=']: raise ValueError('Expected '+assignment+' assignment')
    i=2; result=value()
    if i!=len(tokens): raise ValueError('Extra source after FX data')
    return result

def properties(fx):
    return {p['name'].lower():p['value'] for p in fx['properties'].values()}

def curve(p,name,default,dimensions=1):
    v=p.get(name,default)
    if isinstance(v,dict):
        if all(isinstance(x,dict) for x in v.values()): rows=[list(x.values()) for x in v.values()]
        else:
            vals=list(v.values()); rows=[vals[i:i+dimensions+1] for i in range(0,len(vals),dimensions+1)]
    else: rows=[[0,*([v] if dimensions==1 else default)]]
    if not rows or len(rows)>128 or any(len(r)!=dimensions+1 for r in rows): raise ValueError('Invalid curve '+name)
    rows.sort(key=lambda r:r[0])
    return [[r[0],*r[1:],*([0]*(4-dimensions))] for r in rows]

def quoted_calls(text,name):
    text=re.sub(r'--[^\n]*','',text)
    return [re.findall(r'"([^"]*)"',m) for m in re.findall(name+r'\s*\(([^)]*)\)',text)]

def curve_value(rows,phase,axis=1):
    for a,b in zip(rows,rows[1:]):
        if phase<=b[0]:
            t=max(0,min(1,(phase-a[0])/(b[0]-a[0]))) if b[0]>a[0] else 0
            return a[axis]+(b[axis]-a[axis])*t
    return rows[-1][axis]

def spray_schedule(rate,duration):
    # Launch steam starts at zero. Integrate its ignition curve, rather than
    # sampling only time zero and silently reducing an entire spray to one puff.
    areas=[]; total=0
    for a,b in zip(rate,rate[1:]):
        area=max(0,(a[1]+b[1])*.5)*(b[0]-a[0])*duration
        areas.append((a,b,total,area));total+=area
    if not areas:
        count=max(0,min(24,math.ceil(rate[0][1]*duration)))
        return [(i+.5)*duration/count for i in range(count)]
    count=max(0,min(24,math.ceil(total))); result=[]
    for i in range(count):
        wanted=total*(i+.5)/count
        for a,b,start,area in areas:
            if area>0 and wanted<=start+area:
                lo,hi=a[0],b[0]
                for _ in range(24):
                    mid=(lo+hi)*.5
                    partial=(a[1]+curve_value([a,b],mid))*.5*(mid-a[0])*duration
                    if partial<wanted-start:lo=mid
                    else:hi=mid
                result.append((lo+hi)*.5*duration);break
    return result

def muzzle_events(text,timed=False):
    from openhw_presentation_events import parse_events
    result={}
    # The fragment form is retained for small authoring/importer tests.
    if not re.match(r'\s*effects\s*=',re.sub(r'--[^\n]*','',text)):
        text='effects={events={'+text+'}}'
    parsed=parse_events(text)
    durations={t['name'].casefold():t['length'] for t in parsed['timelines']}
    for event in parsed['events']:
        pairs=event['actions'];anim=pairs.get('anim','');match=re.fullmatch(r'Weapon_Gun(\d+)',anim,re.I)
        if (match or anim.casefold().startswith('weapon_')) and 'special' not in anim.lower() and pairs.get('fx') and event['time']>=0:
            # An ion gun can issue several simultaneous visual events.
            entry=(pairs['fx'],float(pairs.get('fx_scale','1')))
            if timed: entry=(*entry,event['time']*durations.get(anim.casefold(),1))
            result.setdefault(int(match[1]) if match else 0,[]).append(entry)
    return result

class Translator:
    def __init__(self,source,reader=None):
        self.source=source; self.effects=[]; self.layers=[]; self.keys=[]; self.textures=[]
        self.texids={}; self.ids={}; self.unsupported=set();self.reader=reader;self.meshes={}
        self.source_tables={};self.flattened={}
    def geometry(self,path,seed=()):
        from native_hod_surfaces import NativeSurfaces
        path=path.lower().removeprefix('data:').replace('\\','/').lstrip('/')
        if not path.startswith('art/fx/') or '..' in path.split('/'):raise ValueError('Unsafe FX mesh')
        if path.endswith('.ahod'):
            # AHOD here is a data-only random HOD list, not animated geometry.
            # Pick once per emission, never again as the camera/LOD changes.
            text=(self.source/path).read_text()
            values=list(read_table(re.sub(r'^\s*mesh\s*=', 'fx=',text)).values())
            if not values or len(values)>128 or any(not isinstance(v,str) for v in values):raise ValueError('Invalid mesh choice list')
            choice=int(hashlib.sha256((path+repr(seed)).encode()).hexdigest()[:8],16)%len(values)
            return self.geometry(values[choice])
        if path in self.meshes:return self.meshes[path]
        if not self.reader:raise ValueError('Native reader required for mesh FX')
        vertices=[]
        with NativeSurfaces(self.reader,self.source/path) as native:
            joints=native.joints()
            # These FX use a neutral Root, not a ship skeleton. Explicitly
            # reject a posed hierarchy rather than bake incorrect transforms.
            if any(j['parent'] or any(j['position']) or any(j['rotation']) for j in joints):
                raise ValueError('FX mesh has a non-neutral hierarchy: '+path)
            for index in range(self.reader.hwRmHodSceneMeshCount(native.scene)):
                mesh=self.reader.hwRmHodSceneMesh(native.scene,index).contents
                if mesh.lod:continue
                for part in mesh.parts[:mesh.partCount]:
                    if part.indexCount%3:raise ValueError('Non-triangle FX mesh')
                    for vi in part.indices[:part.indexCount]:
                        if vi>=part.vertexCount:raise ValueError('FX vertex index outside mesh')
                        v=part.vertices[vi]
                        vertices.extend([[0,*v.position,0],[0,*v.uv,0,0]])
        if not vertices or len(vertices)>65532:raise ValueError('FX geometry budget exceeded')
        result=(len(self.keys),len(vertices));self.keys+=vertices;self.meshes[path]=result
        return result
    def texture(self,path):
        path=path.lower().removeprefix('data:').replace('\\','/').lstrip('/')
        if not path.startswith(('art/fx/','effect/trails/','effect/lensflare/')) or '..' in path.split('/'): raise ValueError('Unsafe FX texture')
        if path not in self.texids:
            with Image.open(self.source/path) as im:
                im=im.convert('RGBA')
                if max(im.size)>4096: raise ValueError('Oversized FX texture')
                self.texids[path]=len(self.textures); self.textures.append((path,im.width,im.height,im.tobytes()))
        return self.texids[path]
    def layer(self,texture,width,length=1,colour=None,kind=2,loop=1):
        refs=[]
        for rows in ([[0,width,0,0,0]],[[0,length,0,0,0]],colour or [[0,1,1,1,1]],
                     [[0,0,0,0,0]],[[0,1,0,0,0]],[[0,0,0,0,0]]):
            refs.extend((len(self.keys),len(rows)));self.keys+=rows
        return (kind,self.texture(texture),2,loop,1,1,1,0,1.,0.,1.,0.,0.,0.,0.,*refs)
    def store(self,layers):
        if not layers:return 0xffffffff
        if len(layers)>512:raise ValueError('FX exceeds layer budget')
        index=len(self.effects)
        self.effects.append((len(self.layers),len(layers),max(x[8]+x[9] for x in layers)))
        self.layers+=layers
        return index
    def budget_layers(self,layers,label,limit=512):
        if len(layers)<=limit:return layers
        # Preserve each distinct visible component (flash, mesh, smoke, trace),
        # reducing only repeated spray samples. Never replace a large RM clip
        # with the old effect just because it contains hundreds of particles.
        groups={}
        for layer in layers:groups.setdefault(tuple(layer[:3]),[]).append(layer)
        if len(groups)>limit:raise ValueError('Too many distinct FX components')
        quotas={key:1 for key in groups};remaining=limit-len(groups)
        while remaining:
            choices=[key for key in groups if quotas[key]<len(groups[key])]
            if not choices:break
            key=max(choices,key=lambda k:len(groups[k])/(quotas[k]+1))
            quotas[key]+=1;remaining-=1
        result=[]
        for key,group in groups.items():
            q=quotas[key];result.extend(group[min(len(group)-1,int((i+.5)*len(group)/q))] for i in range(q))
        self.unsupported.add(label+':spray density bounded '+str(len(layers))+' -> '+str(len(result))+' layers; RM components retained')
        return result
    def bitmap(self,path):
        path=path.lower().removeprefix('data:').replace('\\','/').lstrip('/')
        if not path.endswith('.anim'): return self.texture(path),1,1,1,0,0
        text=(self.source/path).read_text()
        layout=re.search(r'layout\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*\}',text)
        fps=re.search(r'fps\s*=\s*([\d.]+)',text)
        entries=re.findall(r'\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*\}',text)
        if not layout:
            files=re.findall(r'"([^"\n]+\.(?:tga|dds))"',text,re.I)
            if not files: raise ValueError('Unknown animation layout '+path)
            self.unsupported.add(path+':random bitmap selection uses first authored bitmap')
            return self.texture(files[0]),1,1,1,0,0
        if not fps or not entries: raise ValueError('Unknown animation layout '+path)
        # RM specifies {rows, columns}; textureLayout entries are {row, column}.
        rows,cols=map(int,layout.groups())
        # Repack arbitrary source-sheet order without resizing any frame.
        if path not in self.texids:
            frames=[]
            for texture,y,x in entries:
                name=texture.lower().removeprefix('data:').replace('\\','/').lstrip('/')
                if not name.startswith('art/fx/') or '..' in name.split('/'): raise ValueError('Unsafe animation texture')
                with Image.open(self.source/name) as im:
                    w,h=im.width//cols,im.height//rows; x,y=int(x),int(y)
                    if not 0<=x<cols or not 0<=y<rows: raise ValueError('Animation cell outside sheet')
                    frames.append(im.convert('RGBA').crop((x*w,y*h,(x+1)*w,(y+1)*h)))
            if len({f.size for f in frames})!=1: raise ValueError('Mismatched animation frames')
            outcols=min(8,len(frames)); outrows=math.ceil(len(frames)/outcols)
            sheet=Image.new('RGBA',(w*outcols,h*outrows))
            for i,frame in enumerate(frames): sheet.paste(frame,((i//outrows)*w,(i%outrows)*h))
            self.texids[path]=len(self.textures); self.textures.append((path,sheet.width,sheet.height,sheet.tobytes()))
        return self.texids[path],min(8,len(entries)),math.ceil(len(entries)/min(8,len(entries))),len(entries),0,float(fps[1])
    def flatten(self,name,delay=0,scale=1,velocity=(0,0,0),stack=()):
        name=name.lower()
        if name in stack or len(stack)>12:raise ValueError('Cyclic FX dependency')
        key=(name,delay,scale,tuple(velocity))
        if key not in self.flattened:self.flattened[key]=tuple(self._flatten(name,delay,scale,velocity,stack))
        return list(self.flattened[key])
    def _flatten(self,name,delay=0,scale=1,velocity=(0,0,0),stack=()):
        name=name.lower()
        if name in stack or len(stack)>12: raise ValueError('Cyclic FX dependency')
        try:
            if name not in self.source_tables:self.source_tables[name]=read_table((self.source/'art/fx'/f'{name}.lua').read_text())
            fx=self.source_tables[name]
        except ValueError as error:raise ValueError(name+': '+str(error)) from error
        p=properties(fx); style=fx['style']
        stack=(*stack,name)
        if style=='STYLE_COMBO':
            result=[]
            for key,val in sorted(p.items()):
                if re.fullmatch('fx[0-9]+',key) and val: result+=self.flatten(val,delay,scale,velocity,stack)
            return result
        if style=='STYLE_LIGHT': return [] # existing authoritative muzzle/impact analytic lights
        if style=='STYLE_LENSFLARE':
            flare=str(p.get('lensflare','')).lower()
            if not re.fullmatch('[a-z0-9_]+',flare):raise ValueError('Unsafe lens flare name')
            text=(self.source/'effect/lensflare'/(flare+'.flare')).read_text()
            match=re.search(r'\bhotspots\s*=\s*(\{.*)',re.sub(r'--[^\n]*','',text),re.S)
            if not match:raise ValueError('Missing authored flare hotspots')
            result=[]
            for spot in read_table('fx='+match[1]).values():
                if spot.get('offset')!=1:
                    self.unsupported.add(name+':screen-reflected lens flare hotspot');continue
                radius=curve(p,'radius',1);colour=curve(p,'colour',[1,1,1,1],4)
                tint=list(spot.get('colour',{0:1,1:1,2:1,3:1}).values())
                for row in colour:
                    for axis in range(4):row[axis+1]*=tint[axis]
                refs=[]
                for rows in (radius,[[0,1,0,0,0]],colour,curve(p,'offset',[0,0,0],3),[[0,1,0,0,0]],[[0,0,0,0,0]]):
                    refs.extend((len(self.keys),len(rows)));self.keys+=rows
                result.append((2,self.texture(spot['texture']),2,int(p.get('loop',0)),1,1,1,0,
                               float(p.get('duration',1)),delay,scale*float(spot.get('scale',1)),*velocity,0.,*refs))
            return result
        if style=='STYLE_SPRAY':
            child=p.get('particle_fx',''); duration=float(p.get('emitter_duration',.1))
            rate=curve(p,'emitter_rate',1); speeds=curve(p,'particle_speed',0)
            deviations=curve(p,'emitter_deviation',0)
            smoke=any(word in (name+' '+child).lower() for word in ('smoke','steam','puff','cloud'))
            rng=random.Random(name)
            schedule=spray_schedule(rate,duration)
            if p.get('force_first',0) and (not schedule or schedule[0]>0): schedule=[0,*schedule]
            result=[]
            for born in schedule:
                phase=born/duration
                speed=curve_value(speeds,phase)
                deviation=math.radians(curve_value(deviations,phase))
                phi=rng.uniform(0,2*math.pi); angle=rng.uniform(0,deviation)
                emitted=(math.sin(angle)*math.cos(phi)*speed,math.sin(angle)*math.sin(phi)*speed,math.cos(angle)*speed)
                particle_scale=curve_value(curve(p,'particle_scale',1),phase)
                if particle_scale<=0 or scale<=0: continue
                # Particle_Scale changes size, not travel speed. ScaleSpeed is
                # the separate RM rule controlling the emitter's fx_scale.
                speed_scale=scale if p.get('emitter_scalespeed',0) else 1
                vel=tuple((velocity[a]*scale+emitted[a]*speed_scale)/(scale*particle_scale) for a in range(3))
                particles=self.flatten(child,delay+born,scale*particle_scale,vel,stack)
                flags=(512 | (1024 if p.get('emitter_inheritvelocity',0) else 0)) if smoke else 0
                for layer in particles:
                    layer=list(layer); layer[2] |= flags
                    volume=curve(p,'emitter_volume',[0,0,0],3)
                    center=curve(p,'emitter_offset',[0,0,0],3)
                    point=[rng.uniform(-.5,.5)*curve_value(volume,phase,axis+1)+curve_value(center,phase,axis+1) for axis in range(3)]
                    if any(point):
                        first,number=layer[21:23];new=[]
                        for row in self.keys[first:first+number]:
                            new.append([row[0],*[row[a+1]+point[a]*scale/layer[10] for a in range(3)],row[4]])
                        layer[21]=len(self.keys); self.keys+=new
                    result.append(tuple(layer))
            return result
        if style=='STYLE_TRAIL':
            # A spray particle's straight trace is represented by the source
            # textured beam; its position/velocity still follow particle age.
            p['colour']={0:{0:0,**{i+1:v for i,v in enumerate(p.get('colourhead',{0:1,1:1,2:1,3:1}).values())}},
                         1:{0:1,**{i+1:v for i,v in enumerate(p.get('colourtail',{0:0,1:0,2:0,3:0}).values())}}}
            style='STYLE_BEAM'
        if style not in ('STYLE_BEAM','STYLE_RING'):
            self.unsupported.add(name+':'+style); return []
        path=p.get('texture','')
        if not path: self.unsupported.add(name+':no texture'); return []
        curves=[curve(p,'width' if style=='STYLE_BEAM' else 'radius',1),curve(p,'length',1),
                curve(p,'colour',[1,1,1,1],4),curve(p,'offset',[0,0,0],3),
                curve(p,'texture_u_repeat',1),curve(p,'texture_u_offset',0)]
        refs=[]
        for rows in curves:
            refs.extend((len(self.keys),len(rows))); self.keys+=rows
        kind=1 if style=='STYLE_BEAM' else 2
        if p.get('usemesh',0):
            try:refs[-2:]=self.geometry(p.get('mesh',''),(name,delay,velocity));kind=3
            except ValueError as error:
                self.unsupported.add(name+':'+str(error));return []
        tex,cols,rows,frames,start,fps=self.bitmap(path)
        blend=int(p.get('blending',2)) | (256 if not p.get('usedepthtest',1) else 0)
        return [(kind,tex,blend,int(p.get('loop',0)),cols,rows,frames,start,
                 float(p.get('duration',1)),delay,scale,*velocity,fps,*refs)]
    def corvette_missile_impact(self):
        # The weapon-fire family is shared with capital missiles. Keep the
        # original RM flash/cloud pixels, not capital spikes/panel debris or
        # the oversized classic fallback. Scope this adaptation to P1 missiles.
        key='p1_corvette_compact_impact'
        if key not in self.ids:
            layers=self.flatten('dmg_cloud_flash_ring_missle',scale=.2)
            layers+=self.flatten('dmg_cloud_spray_missle',scale=.2)
            # Nested cloud sprays multiply particle_scale (roughly 50x here);
            # constrain the final product, not just the outer combo scale.
            layers=[tuple([*x[:8],min(x[8],1.2),x[9],min(x[10],.2)*10,*x[11:]]) for x in layers]
            if not layers or len(layers)>512: raise ValueError('Invalid compact corvette impact')
            self.ids[key]=len(self.effects)
            self.effects.append((len(self.layers),len(layers),max(x[8]+x[9] for x in layers)))
            self.layers+=layers
        return self.ids[key]
    def effect(self,name):
        if not name: return 0xffffffff
        if name.lower() not in self.ids:
            layers=self.flatten(name)
            layers=self.budget_layers(layers,name)
            if not layers: self.ids[name.lower()]=0xffffffff
            else:
                self.ids[name.lower()]=len(self.effects)
                self.effects.append((len(self.layers),len(layers),max(x[8]+x[9] for x in layers)))
                self.layers+=layers
        return self.ids[name.lower()]

def build(source,ships,output,raider_ships=None,taiidan_ships=None,kadeshi_ships=None,resource_dust=False,system_fx=False,reader=None):
    t=Translator(source,reader); maps=[]; report=[]
    aliases={'HeavyInterceptor':{0:1},'LightInterceptor':{0:1},'HeavyCorvette':{1:0},
             'StandardFrigate':{1:0,2:0,5:0},'MultiGunCorvette':{i:0 for i in range(1,6)},
             'MissileDestroyer':{4:0,5:1},'MinelayerCorvette':{1:0}}
    hulls=[(classic,hull,ships,aliases.get(classic,{})) for classic,hull in R1_HULLS.items()]
    if raider_ships:
        from stage_raiders_derelicts_native import P1_HULLS
        hulls += [(classic,hull,raider_ships,{}) for classic,hull in P1_HULLS.items()]
    if taiidan_ships:
        from stage_taiidan_native import R2_HULLS
        r2_aliases={'HeavyInterceptor':{0:1},'HeavyCorvette':{0:0,1:0,2:1,3:1},
                    'AdvanceSupportFrigate':{0:1,1:2},'StandardDestroyer':{1:0,3:2}}
        hulls += [('Taiidan_'+classic,hull,taiidan_ships,r2_aliases.get(classic,{}))
                  for classic,hull in R2_HULLS.items()]
    if kadeshi_ships:
        from stage_kadeshi_native import P2_HULLS
        hulls += [(classic,hull,kadeshi_ships,{}) for classic,hull in P2_HULLS.items()]
    for classic,hull,ship_source,gun_aliases in hulls:
        folder=ship_source/'ship'/hull
        descriptor=folder/(hull+'.ship')
        if not descriptor.exists() and classic=='LightDefender':
            descriptor=ship_source/'ship/kus_defender/kus_defender.ship'
            report.append(dict(ship=classic,compatibility='RM prototype lacks script; uses RM defender visual events'))
        if not descriptor.exists(): continue
        text=descriptor.read_text(); weapons={}
        for args in quoted_calls(text,'StartShipWeaponConfig'):
            if len(args)<3 or args[0].lower().endswith('heal'): continue
            match=re.search(r'Gun(\d+)',args[1])
            if match: weapons[int(match[1])]=args[0].lower()
        for args in quoted_calls(text,'StartShipHardPointConfig'):
            if len(args)<6 or args[2]!='Weapon': continue
            match=re.search(r'Gun(\d+)',args[0])
            if match: weapons[int(match[1])]=args[5].lower()
        if classic=='HeavyCruiser': weapons.update({i:'kus_cruiserion' for i in range(4)})
        if classic=='Drone': weapons={0:'kus_drone'}
        if classic=='LightDefender':weapons={0:'kus_defender',1:'kus_defender'}
        if classic=='P2FuelPod':
            weapons={0:'hw1_mothershipgun'}
            report.append(dict(ship=classic,compatibility='HW1 armed fuel pod uses generic RM weapon FX; RM fuel pod is unarmed'))
        if classic=='Mothership': weapons={i:'hw1_mothershipgun' for i in range(6)}
        if classic=='Taiidan_Mothership': weapons={i:'hw1_mothershipgun' for i in range(5)}
        if classic=='Taiidan_HeavyCruiser': weapons.update({i:'tai_cruiserion' for i in range(6,10)})
        original_weapons=weapons.copy()
        if classic in ('Taiidan_HeavyCorvette','Taiidan_AdvanceSupportFrigate'): weapons={}
        for dest,src in gun_aliases.items():
            if src in original_weapons: weapons[dest]=original_weapons[src]
        events=(folder/(hull+'.events')).read_text() if (folder/(hull+'.events')).exists() else ''
        muzzle=muzzle_events(events,timed=True)
        if system_fx:
            from openhw_presentation_events import parse_events
            event_durations={e['name'].casefold():e['length'] for e in parse_events(events)['timelines']}
            transforms={};entity_radius=None
            if reader:
                from native_hod_surfaces import NativeSurfaces
                with NativeSurfaces(reader,folder/(hull+'.hod')) as native:
                    lo,hi=native.hull_bounds();entity_radius=float(max(hi-lo)*.5)
                    joints=native.joints()
                    names=[j['name'] for j in joints if sum(k['name']==j['name'] for k in joints)==1]
                    transforms=native.marker_transforms(names)
            # Ship exhaust is specified in .ship, not in weaponfire Lua. Keep
            # its own slots, so it cannot alias a combat gun or a death event.
            faction=hull.split('_')[0]
            colour_path=source/'effect/trails'/(faction+'_trail_clr.tga')
            colours=[[0,1,1,1,1],[1,0,0,0,0]]
            if colour_path.exists():
                with Image.open(colour_path) as im:
                    im=im.convert('RGBA')
                    colours=[[i/15,*[v/255 for v in im.getpixel((round(i*(im.width-1)/15),im.height//2))]] for i in range(16)]
                    colours[-1][4]=0
            burn_match=re.search(r'setEngineBurn\s*\(\s*NewShipType\s*,([^)]*)\)',re.sub(r'--[^\n]*','',text))
            burn=[float(x.strip()) for x in burn_match[1].split(',')] if burn_match else []
            trail_matches=re.findall(r'setEngineTrail\s*\(\s*NewShipType\s*,\s*(\d+)\s*,\s*([\d.]+)\s*,\s*"([^"]+)"([^)]*)\)',text)
            if not trail_matches and burn:trail_matches=[('0',str(max(.5,burn[1]*2)),'','')]
            for engine,width,texture,tail in trail_matches:
                engine=int(engine);width=float(width)
                if engine>=8:raise ValueError('Too many authored engine emitters')
                ribbon=t.store([t.layer('effect/trails/'+texture,width,colour=colours,kind=1)]) if texture else 0xffffffff
                # The compatibility renderer uses RM's exhaust sprite rather
                # than the previous procedural multi-layer Classic plume.
                glow=t.store([t.layer('effect/trails/glowtex.tga',width,colour=[colours[0]])])
                maps.append((classic,48+engine,ribbon,glow,0xffffffff,1))
                report.append(dict(ship=classic,engine=engine,engine_burn=burn,engine_trail=[width,texture,tail],translated=[ribbon,glow,0xffffffff]))
            # Keep all authored death events and delays. Never import damage,
            # lifetime or weapon statistics from RM into the HW1 simulation.
            death_layers=[]
            for block in re.findall(r'\w+\s*=\s*\{\s*((?:\{[^{}]*\}\s*,?\s*)+)\}',re.sub(r'--[^\n]*','',events)):
                pairs=dict(re.findall(r'\{\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,?\s*\}',block))
                if pairs.get('anim','').lower()!='death' or not pairs.get('fx'):continue
                if pairs.get('fx_scale')=='entity':
                    # The visual package's unit-scale bounding radius is
                    # computed by the same native HOD reader as the runtime.
                    # Entity scaling needs a native bounds adapter; do not
                    # silently turn it into an enormous literal scale.
                    if entity_radius is None:
                        t.unsupported.add(hull+':entity-scaled death event requires native reader');continue
                    event_scale=entity_radius
                else:event_scale=float(pairs.get('fx_scale','1'))
                for marker in pairs.get('marker','Root').split(','):
                    marker=marker.strip()
                    if marker.lower() in ('root','centre','center'):point=[0,0,0]
                    elif marker in transforms:point=transforms[marker][12:15]
                    elif re.fullmatch(r'(top|bottom|left|right|front|back)_\d+',marker) and entity_radius is not None:
                        # RM generates named hull-surface damage/death locations;
                        # they are not HOD joints. Adapt these on native bounds,
                        # never on the discarded Classic model's collision size.
                        side=marker.split('_')[0];seed=int(hashlib.sha256(marker.encode()).hexdigest()[:8],16)
                        rng=random.Random(seed);point=[float(lo[a]+(hi[a]-lo[a])*rng.uniform(.15,.85)) for a in range(3)]
                        axis,high={'top':(2,True),'bottom':(2,False),'left':(1,False),'right':(1,True),'front':(0,True),'back':(0,False)}[side]
                        point[axis]=float(hi[axis] if high else lo[axis])
                    else:
                        t.unsupported.add(hull+':missing death marker '+marker);continue
                    for name in pairs['fx'].split(','):
                        if not name.strip():continue
                        phase=float(pairs.get('animtime','0'))
                        if phase<0:continue
                        pieces=t.flatten(name.strip(),phase*event_durations.get(pairs.get('anim','').casefold(),1),event_scale)
                        for piece in pieces:
                            piece=list(piece);first,number=piece[21:23];piece[21]=len(t.keys)
                            t.keys += [[row[0],*[row[a+1]+point[a]/piece[10] for a in range(3)],row[4]] for row in t.keys[first:first+number]]
                            death_layers.append(tuple(piece))
            if death_layers:
                death_id=t.store(t.budget_layers(death_layers,hull+':death'))
                maps.append((classic,56,death_id,0xffffffff,0xffffffff,1))
                report.append(dict(ship=classic,death=True,translated=[death_id,0xffffffff,0xffffffff]))
        for gun,weapon in sorted(weapons.items()):
            wf=(ship_source/'weapon'/weapon/(weapon+'.wepn'))
            if not wf.exists(): wf=source/'weapon'/weapon/(weapon+'.wepn')
            if not wf.exists():
                # A hardpoint names a subsystem, not necessarily its weapon.
                # Resolve its normal weapon; never import a special ability.
                subs=ship_source/'subsystem'/weapon/(weapon+'.subs')
                if not subs.exists(): subs=source/'subsystem'/weapon/(weapon+'.subs')
                calls=quoted_calls(subs.read_text(),'StartSubSystemWeaponConfig') if subs.exists() else []
                normal=next((c[0].lower() for c in calls if c and 'special' not in c[0].lower()),None)
                if normal:
                    wf=source/'weapon'/normal/(normal+'.wepn')
            if not wf.exists(): raise ValueError('Missing weapon descriptor '+weapon)
            args=quoted_calls(wf.read_text(),'StartWeaponConfig')
            if not args: continue
            family=args[0][2].lower()
            fire=source/'scripts/weaponfire'/family/(family+'.wf')
            if not fire.exists(): continue
            from openhw_weapon_presentation import parse_weaponfire
            presentation=parse_weaponfire(fire.read_text())
            fxnames={slot['name']+'fx':slot['fx'] for slot in presentation['slots']}
            events_for_gun=muzzle.get(gun_aliases.get(gun,gun),[])
            if not events_for_gun and classic in ('LightDefender','P2FuelPod'):
                events_for_gun=[('muzzle_flash_combo',.5,0)]
            subsystem=ship_source/'subsystem'/weapon/(weapon+'.events')
            if not subsystem.exists(): subsystem=source/'subsystem'/weapon/(weapon+'.events')
            if subsystem.exists():
                se=muzzle_events(subsystem.read_text(),timed=True)
                events_for_gun=se.get(0,events_for_gun)
            # Flatten ALL muzzle events, preserving their individual authored scales.
            muzzle_id=0xffffffff
            if events_for_gun:
                layers=[]
                for name,scale,event_delay in events_for_gun: layers+=t.flatten(name,delay=event_delay,scale=scale)
                if layers:
                    muzzle_id=len(t.effects); t.effects.append((len(t.layers),len(layers),max(x[8]+x[9] for x in layers))); t.layers+=layers
            impact_id=t.corvette_missile_impact() if classic=='P1MissileCorvette' else t.effect(fxnames.get('hitfx',''))
            ids=[t.effect(fxnames.get('bulletfx','')),muzzle_id,impact_id]
            maps.append((classic,gun,*ids,1))
            report.append(dict(ship=classic,gun=gun,weapon=weapon,projectile=fxnames.get('bulletfx'),muzzle=events_for_gun,impact=fxnames.get('hitfx'),translated=ids,
                               source_presentation=presentation))
    if system_fx:
        for kind,name in enumerate(('dmg_damage_spark_combo','dmg_damage_puff_combo','dmg_damage_fire_combo',
                                    'dmg_fighter_combo','dmg_capital_main_explosion_combo','dmg_sml_missle_combo',
                                    'hyperspace_gate_glow_combo')):
            maps.append(('LegacyFX',kind,t.effect(name),0xffffffff,0xffffffff,1))
        for kind,name in enumerate(('harvesting_beam_kus_combo','harvesting_beam_tai_combo','repair_beam_combo')):
            maps.append(('Utility',kind,t.effect(name),0xffffffff,0xffffffff,1))
        for kind,name in enumerate(('kus_missile','tai_missile','tur_smallmissile')):
            descriptor=source/'missile'/name/(name+'.miss')
            if not descriptor.exists():raise ValueError('Missing missile exhaust descriptor '+name)
            match=re.search(r'setEngineTrail\s*\(\s*NewShipType\s*,\s*0\s*,\s*([\d.]+)\s*,\s*"([^"]+)"',descriptor.read_text())
            if not match:raise ValueError('Missing missile trail '+name)
            width=float(match[1]);ribbon=t.store([t.layer('effect/trails/'+match[2],width,kind=1)])
            glow=t.store([t.layer('effect/trails/glowtex.tga',width)])
            maps.append(('MissileExhaust',kind,ribbon,glow,0xffffffff,1))
    if resource_dust:
        for kind in range(4):
            name='res_dustcloud'+str(kind)
            effect=t.effect(name)
            if effect==0xffffffff: raise ValueError('RM resource cloud is not renderable: '+name)
            maps.append(('HarvestableDust',kind,effect,0xffffffff,0xffffffff,1))
            report.append(dict(ship='HarvestableDust',gun=kind,projectile=name,translated=[effect,0xffffffff,0xffffffff]))
    # Flattening explores dependencies before selecting clips. Do not ship
    # unreferenced curves/meshes from discarded branches or duplicate copies
    # of the same immutable HOD geometry.
    compact_keys=[];references={};compact_layers=[]
    for layer in t.layers:
        layer=list(layer)
        for curve_index in range(6):
            slot=15+curve_index*2;ref=tuple(layer[slot:slot+2])
            if ref not in references:
                references[ref]=len(compact_keys);compact_keys+=t.keys[ref[0]:ref[0]+ref[1]]
            layer[slot]=references[ref]
        compact_layers.append(tuple(layer))
    t.layers=compact_layers;t.keys=compact_keys
    if len(maps)>1024 or len(t.effects)>1024 or len(t.layers)>32768 or len(t.keys)>2000000 or len(t.textures)>256:
        raise ValueError('Combined runtime FX package exceeds bounded storage')
    # Fixed little-endian ABI; all textures are original RGBA pixels, no AI edits.
    data=bytearray(struct.pack('<8s5I',b'HWRMFX01',len(maps),len(t.effects),len(t.layers),len(t.keys),len(t.textures)))
    for name,gun,a,b,c,scale in maps: data+=struct.pack('<32s4If',name.encode(),gun,a,b,c,scale)
    for e in t.effects: data+=struct.pack('<2If',*e)
    for layer in t.layers: data+=struct.pack('<8I7f12I',*layer)
    for key in t.keys: data+=struct.pack('<5f',*key)
    for _,w,h,pixels in t.textures: data+=struct.pack('<2I',w,h)+pixels
    output.mkdir(parents=True,exist_ok=True)
    (output/'Kushan.rfx').write_bytes(data)
    (output/'import-report.json').write_text(json.dumps(dict(mappings=report,unsupported=sorted(t.unsupported),
        texture_count=len(t.textures),effect_count=len(t.effects),layer_count=len(t.layers),
        gameplay_imported=False,sha256=hashlib.sha256(data).hexdigest()),indent=2))
    print(f'{len(maps)} gun mappings, {len(t.effects)} effects, {len(t.layers)} layers, {len(t.textures)} textures')

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('--archive',type=Path,required=True); p.add_argument('--ships',type=Path,required=True)
    p.add_argument('--work',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    p.add_argument('--raider-ships',type=Path); p.add_argument('--taiidan-ships',type=Path)
    p.add_argument('--kadeshi-ships',type=Path); p.add_argument('--resource-dust',action='store_true');p.add_argument('--system-fx',action='store_true');p.add_argument('--reader',type=Path); a=p.parse_args()
    prefixes=('art/fx/','scripts/weaponfire/','weapon/hw1','subsystem/kus_')
    if a.raider_ships: prefixes += ('weapon/tur_','subsystem/tur_')
    if a.taiidan_ships: prefixes += ('weapon/tai_','subsystem/tai_')
    if a.kadeshi_ships: prefixes += ('weapon/kad_','subsystem/kad_')
    if a.system_fx:prefixes+=('effect/trails/','effect/lensflare/','missile/kus_','missile/tai_','missile/tur_')
    extract(a.archive,a.work,prefixes)
    from native_hod_surfaces import load_reader
    build(a.work,a.ships,a.output,a.raider_ships,a.taiidan_ships,a.kadeshi_ships,a.resource_dust,a.system_fx,load_reader(a.reader) if a.reader else None)
