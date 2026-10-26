"""Translate verified HOD2 foreground geometry and source material programs.

Generated files are local cache data, never repository assets. Source Lua is
parsed as data, not executed. Unsupported layouts fail before publication.
"""
import argparse
import io
import json
import re
import struct
from pathlib import Path
import numpy as np
from PIL import Image
from inspect_remastered_scene import inspect_scene, Reader
from translate_remastered_background import chunks, light_pools
from translate_remastered_shaders import translated_shader, d3d_compile
from extract_remastered_big import safe_path


def text(path):
    return re.sub(r'--[^\n]*|//[^\n]*', '', path.read_text(encoding='utf-8'))


def locals_from(source):
    values = {}
    for typ, name, rest in re.findall(r'^\s*local\s+(\w+)\s+(\w+)\s+([^\n]+)', source, re.M):
        if typ.startswith('float'):
            values[name] = [float(x) for x in rest.split()]
        elif typ == 'tex':
            values[name] = rest.strip()
    return values


def decode_texture(row, pool):
    w, h = row['mips'][0]
    raw = pool[row['pool_offset']:row['pool_offset']+row['bytes']]
    if row['format'] == '8888':
        return raw[:w*h*4]
    # Pillow's DDS decoder validates the block compression layout; preserve
    # the actual diffuse alpha, normal and warp channels without generating any.
    header = bytearray(128)
    header[:4] = b'DDS '
    struct.pack_into('<7I', header, 4, 124, 0x81007, h, w, len(raw), 0, 1)
    struct.pack_into('<II4s', header, 76, 32, 4, row['format'].encode())
    struct.pack_into('<I', header, 108, 0x1000)
    return Image.open(io.BytesIO(header+raw)).convert('RGBA').tobytes()


def vertices_from(part, pool, faces):
    count, stride, mask = part['vertices'], part['stride'], part['mask']
    raw = pool[part['vertex_pool_offset']:part['vertex_pool_offset']+count*stride]
    output = np.zeros((count,20), '<f4')
    output[:,12:16] = 1
    cursor = 0
    # The source stream places tangents after all UV sets (not bit order).
    fields = [(1,16,0,3),(2,16,3,3),(4,4,12,4),(8,8,16,2),
              (16,8,18,2),(32,8,None,2),(0x2000,12,6,3),(0x4000,12,9,3)]
    for bit,size,target,components in fields:
        if not mask & bit: continue
        if bit == 4:
            color = np.ndarray((count,), '<u4', buffer=raw, offset=cursor, strides=(stride,))
            output[:,12:16] = np.stack([(color >> b)&255 for b in (24,16,8,0)], -1)/255
        elif target is not None:
            output[:,target:target+components] = np.ndarray((count,components), '<f4',
                buffer=raw, offset=cursor, strides=(stride,4))
        cursor += size
    if cursor != stride or not np.isfinite(output).all():
        raise ValueError('Invalid translated vertex layout')
    indices = np.frombuffer(faces, '<u2', part['triangles']*3, part['face_pool_offset'])
    if np.any(indices >= count): raise ValueError('Invalid foreground index')
    return output[indices]


def rotation(euler):
    x,y,z = euler
    cx,cy,cz = np.cos([x,y,z]); sx,sy,sz = np.sin([x,y,z])
    rx=np.array([[1,0,0],[0,cx,-sx],[0,sx,cx]])
    ry=np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]])
    rz=np.array([[cz,-sz,0],[sz,cz,0],[0,0,1]])
    return rz@ry@rx


def animation_curves(path):
    if not path.is_file(): return {}
    top = dict(chunks(path.read_bytes()))
    blocks = dict(chunks(top[b'MAD ']))
    info=struct.unpack('<5i',blocks[b'INFO'])
    strings=blocks[b'STRI']
    reader=Reader(blocks[b'CURV']); curves={}
    for _ in range(info[2]):
        offset,n=reader.unpack('<ii')
        if not 0<=offset<len(strings) or not 1<=n<=100000:
            raise ValueError('Invalid MAD curve')
        end=strings.find(b'\0',offset)
        if end<0: raise ValueError('Unterminated MAD channel')
        name=strings[offset:end].decode('ascii')
        keys=np.array([reader.unpack('<dd4f') for _ in range(n)])
        if not np.isfinite(keys).all() or np.any(np.diff(keys[:,0])<0):
            raise ValueError('Invalid animation keys')
        reader.unpack('<ii')
        curves[name]=keys
    reader.done()
    return curves


def poses(info, curves, rate):
    times=[v[:,0] for v in curves.values()]
    start=min((v.min() for v in times),default=0)
    end=max((v.max() for v in times),default=0)
    duration=(end-start)/rate if rate>0 else 0
    frames=257 if duration>0 else 1
    result={j['name']:[] for j in info['joints']}
    for t in np.linspace(start,end,frames):
        cache={}
        def joint_matrix(name):
            if name in cache: return cache[name]
            j=next(j for j in info['joints'] if j['name']==name)
            pos=np.array(j['position'],dtype=float); rot=np.array(j['rotation'],dtype=float); scale=np.ones(3)
            for channel,target in [('translate',pos),('rotate',rot),('scale',scale)]:
                for axis in range(3):
                    key=name+'_'+channel+'XYZ'[axis]
                    if key in curves:
                        keys=curves[key]
                        target[axis]=np.interp(t,keys[:,0],keys[:,1])
            matrix=np.eye(4); matrix[:3,:3]=rotation(rot)@np.diag(scale); matrix[:3,3]=pos
            cache[name]=joint_matrix(j['parent'])@matrix if j['parent'] else matrix
            return cache[name]
        for j in info['joints']: result[j['name']].append(joint_matrix(j['name']))
    return {k:np.asarray(v,dtype='<f4') for k,v in result.items()},duration


def campaign_material(material, mission):
    # RM singleplayerflow.lua singlePlayerOnStartOrLoad switches KharakBurn
    # to this completed surface in Mission 3. The HOD stores the untriggered
    # destruction-animation surface, which otherwise displays intact Kharak.
    if (mission == 3 and material.get('name') == 'KharakBurn' and
            material.get('shader') == 'bg_planetmelt'):
        return {**material, 'shader': 'bg_planetmelted'}
    return material


def surface_layers(root, material):
    source=text(root/'gl_surf'/(material['shader']+'.surf'))
    pieces=re.split(r'^\s*layer\s*$',source,flags=re.M)
    if len(pieces)<2: raise ValueError('Surface has no layer')
    defaults=locals_from(pieces[0])
    for p in material['parameters']:
        key=p['name']
        key='BG_'+key.lstrip('$') if not key.startswith('BG_') else key
        if p['type']==5: defaults[key]=struct.unpack('<I',bytes.fromhex(p['value_hex']))[0]
        elif p['type']==4: defaults[key]=list(struct.unpack('<4f',bytes.fromhex(p['value_hex'])))
        else: raise ValueError('Unknown material parameter type')
    for layer in pieces[1:]:
        name=re.search(r'^\s*program\s+(\w+)',layer,re.M).group(1)
        aliases=dict((b,a) for a,b in re.findall(r'^\s*alias\s+(\w+)\s+(\w+)',layer,re.M))
        blend=re.search(r'blendOps\s+(\w+)\s+(\w+)',layer).groups()
        flags={('one','zero'):0,('srcAlpha','invSrcAlpha'):1,('srcAlpha','srcInvAlpha'):1,
               ('one','one'):2,('srcAlpha','one'):3}.get(blend)
        if flags is None: raise ValueError('Unsupported background blend: '+str(blend))
        cull=re.search(r'\bcull\s+(\w+)',layer).group(1)
        flags|={'none':0,'back':4,'front':8}[cull]
        yield name,aliases,defaults,flags


def translate(light, shaders, output, mission):
    data=light.read_bytes(); streams=light_pools(data);info=inspect_scene(data,streams)
    texture_pool,vertex_pool,face_pool=streams
    curves=animation_curves(light.with_suffix('.mad'))
    extra=text(light.parent/(f'ez{mission:02d}_extra.lua'))
    match=re.search(r'Anim_Rate\s*=\s*([\d.eE+-]+)',extra)
    transforms,duration=poses(info,curves,float(match.group(1)) if match else 0)
    programs=[]; program_index={}; draws=[]
    textures=[]; texture_index={}
    def texture(value):
        key=str(value)
        if key in texture_index:return texture_index[key]
        if isinstance(value,int):
            row=info['textures'][value]; w,h=row['mips'][0]; rgba=decode_texture(row,texture_pool)
        else:
            colors={'default_white':(255,255,255,255),'default_black':(0,0,0,255),
                    'default_grey':(128,128,128,255),'default_normal':(128,128,255,255)}
            if value not in colors:raise ValueError('Unknown material default: '+value)
            w=h=1;rgba=bytes(colors[value])
        texture_index[key]=len(textures); textures.append((w,h,rgba));return texture_index[key]
    for mesh in info['meshes']:
        if mesh['name']=='PRE_Space' or mesh['lod']!=0:continue
        for part in mesh['parts']:
            material=campaign_material(info['materials'][part['material']],mission)
            vertices=vertices_from(part,vertex_pool,face_pool)
            for name,aliases,defaults,flags in surface_layers(shaders,material):
                if name not in program_index:
                    hlsl,layout=translated_shader(shaders,name)
                    program_index[name]=len(programs);programs.append((d3d_compile(hlsl),layout))
                program=program_index[name];layout=programs[program][1]
                params=np.zeros((128,4),'<f4')
                uniform_defaults=locals_from(text(shaders/'gl_prog'/(name+'.prog')))
                for uniform,row in layout['uniforms'].items():
                    val=defaults.get(aliases.get(uniform,''),uniform_defaults.get(uniform))
                    if isinstance(val,list): params[row['slot'],:min(4,len(val))]=val[:4]
                if 'inFade' in layout['uniforms']:params[layout['uniforms']['inFade']['slot']]=1
                if 'inLightCore' in layout['uniforms']:
                    slot=layout['uniforms']['inLightCore']['slot']
                    # Direction follows the source flare joint. Do not overwrite
                    # the user's mission lighting settings to light a background.
                    for off,name_joint in ((1,'Flare_Key'),(4,'Flare_Fill')):
                        d=transforms.get(name_joint,np.array([np.eye(4)]))[0,:3,3]
                        if np.linalg.norm(d)<1e-6:d=np.array([1,1,1]) if off==1 else np.array([-1,-1,1])
                        params[slot+off,:3]=d/np.linalg.norm(d)
                    params[slot,:3]=.2
                    params[slot+2,:3]=params[slot+3,:3]=1
                    params[slot+5,:3]=params[slot+6,:3]=.35
                bindings=[0xffffffff]*10
                for i,uniform in enumerate(layout['textures']):
                    if uniform in ('inTexEnv0','inTexEnv1'):bindings[i]=0xfffffffe
                    else:
                        value=defaults.get(aliases.get(uniform,''))
                        if value is None: raise ValueError('Missing texture alias: '+name+'/'+uniform)
                        bindings[i]=texture(value)
                getslot=lambda name: layout['uniforms'].get(name,{}).get('slot',-1)
                expansion=params[getslot('inAtmoInfo'),0] if name.startswith('bg_atmos') else 0
                draws.append((program,flags,vertices,transforms[mesh['parent']],duration,
                    float(expansion),getslot('inTime'),getslot('inFade'),getslot('inMatMV'),bindings,params))
    output.mkdir(parents=True,exist_ok=True)
    path=output/f'mission{mission:02d}_scene.bin'
    with path.open('wb') as stream:
        stream.write(b'RMSC3\0\0\0'+struct.pack('<3I',len(textures),len(programs),len(draws)))
        for w,h,rgba in textures:
            stream.write(struct.pack('<3I',w,h,len(rgba)));stream.write(rgba)
        for code,_ in programs:stream.write(struct.pack('<I',len(code)));stream.write(code)
        for prog,flags,vertices,frames,cycle,expansion,ts,fs,ms,bindings,params in draws:
            stream.write(struct.pack('<4I2f3i10I',prog,flags,len(vertices),len(frames),cycle,expansion,ts,fs,ms,*bindings))
            stream.write(params.tobytes());stream.write(frames.transpose(0,2,1).copy().tobytes());stream.write(vertices.tobytes())
    report=dict(mission=mission,draws=len(draws),textures=len(textures),programs=len(programs),
                animated=bool(curves),cycle_seconds=duration,source_sha256=info['sha256'],
                campaign_surfaces=[dict(material=m['name'],surface=campaign_material(m,mission)['shader'])
                    for m in info['materials'] if campaign_material(m,mission) is not m])
    (output/f'mission{mission:02d}_scene.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report),flush=True)
    return report


def translate_flares(root, effects, output, mission):
    """Import source hotspot stacks as data; do not run the Lua interpreter."""
    data=(root/f'ez{mission:02d}_light.hod').read_bytes()
    hierarchy=next(p for k,p in chunks(next(p for k,p in chunks(data) if k==b'DTRM')) if k==b'HIER')
    r=Reader(hierarchy);n,=r.unpack('<i');joints=[]
    if not -10000<n<0:raise ValueError('Invalid flare hierarchy')
    for _ in range(-n):
        name,parent=r.string(),r.string();v=r.unpack('<9f')
        joints.append(dict(name=name,parent=parent,position=v[:3],rotation=v[3:6]))
    r.done();transforms,_=poses(dict(joints=joints),{},0)
    config=text(root/f'ez{mission:02d}.lua')
    textures=[];indices={};records=[]
    for body in re.findall(r'lensflare\d+\s*=\s*\{([^}]+)',config):
        name=re.search(r'\bname\s*=\s*"([^"]+)"',body)
        follow=re.search(r'\bfollow\s*=\s*"([^"]+)"',body)
        if not name or not follow:raise ValueError('Unsupported source flare placement')
        if follow.group(1) not in transforms:raise ValueError('Flare joint not found')
        direction=transforms[follow.group(1)][0,:3,3]
        source=text(effects/(name.group(1)+'.flare'))
        # Each hotspot has exactly one nested colour tuple; field order is not
        # assumed. Comments were removed before parsing.
        spots=re.findall(r'\{\s*texture\s*=\s*"([^"]+)"\s*,(.*?)colour\s*=\s*\{([^}]+)\}',source,re.S)
        if not spots:raise ValueError('No source flare hotspots: '+name.group(1))
        for filename,properties,color in spots:
            relative=safe_path(filename.removeprefix('data:'))
            if relative.parts[:2]!=('Effect','LensFlare'):raise ValueError('Unexpected flare texture path')
            path=effects/relative.name
            if relative.as_posix().lower() not in indices:
                image=Image.open(path).convert('RGBA');pixels=np.asarray(image,dtype=np.float32)/255
                rgb=np.where(pixels[:,:,:3]<=.04045,pixels[:,:,:3]/12.92,((pixels[:,:,:3]+.055)/1.055)**2.4)
                pixels[:,:,:3]=rgb*pixels[:,:,3:4]
                pixels[:,:,3]=0
                indices[relative.as_posix().lower()]=len(textures)
                textures.append((image.width,image.height,np.uint8(np.clip(pixels,0,1)*255+.5).tobytes()))
            numbers=[]
            for field in ('offset','scale','rotation'):
                match=re.search(r'\b'+field+r'\s*=\s*([\d.eE+-]+)',properties)
                if not match:raise ValueError('Missing flare property '+field)
                numbers.append(float(match.group(1)))
            rgba=[float(x) for x in color.split(',') if x.strip()]
            row=[*direction,*numbers,*rgba]
            if len(row)!=10 or not np.isfinite(row).all():raise ValueError('Invalid flare hotspot')
            records.append((indices[relative.as_posix().lower()],row))
    path=output/f'mission{mission:02d}_flares.bin'
    with path.open('wb') as stream:
        stream.write(b'RMLF1\0\0\0'+struct.pack('<2I',len(textures),len(records)))
        for w,h,pixels in textures:
            stream.write(struct.pack('<3I',w,h,len(pixels)));stream.write(pixels)
        for texture,row in records:stream.write(struct.pack('<I10f',texture,*row))
    print(f'Mission {mission:02d}: {len(records)} source flare hotspots, {len(textures)} textures',flush=True)
    return len(records)


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--light',type=Path,required=True)
    cli.add_argument('--shaders',type=Path,required=True)
    cli.add_argument('--output',type=Path,required=True)
    cli.add_argument('--mission',type=int,required=True)
    args=cli.parse_args();translate(args.light,args.shaders,args.output,args.mission)


if __name__=='__main__':main()
