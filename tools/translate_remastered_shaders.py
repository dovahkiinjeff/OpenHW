"""Translate the verified RM background GLSL subset to native D3D bytecode.

Input shaders are read from the user's installation; translated source/bytecode
belongs in the local conversion cache, not in the source repository.
Unknown matrix expressions fail closed rather than compiling incorrect math.
"""
import argparse
import ctypes as C
import json
import re
from pathlib import Path


def blob_bytes(pointer):
    if not pointer.value:
        return b''
    table = C.cast(pointer, C.POINTER(C.POINTER(C.c_void_p))).contents
    address = C.WINFUNCTYPE(C.c_void_p, C.c_void_p)(table[3])(pointer)
    size = C.WINFUNCTYPE(C.c_size_t, C.c_void_p)(table[4])(pointer)
    result = C.string_at(address, size)
    C.WINFUNCTYPE(C.c_ulong, C.c_void_p)(table[2])(pointer)
    return result


def d3d_compile(source, preprocess=False):
    library = C.WinDLL('d3dcompiler_47.dll')
    code, errors = C.c_void_p(), C.c_void_p()
    source = source.encode()
    if preprocess:
        function = library.D3DPreprocess
        function.argtypes = [C.c_char_p,C.c_size_t,C.c_char_p,C.c_void_p,C.c_void_p,
                             C.POINTER(C.c_void_p),C.POINTER(C.c_void_p)]
        function.restype = C.c_long
        status = function(source,len(source),b'RemasteredLocal',None,None,C.byref(code),C.byref(errors))
    else:
        function = library.D3DCompile
        function.argtypes = [C.c_char_p,C.c_size_t,C.c_char_p,C.c_void_p,C.c_void_p,
                             C.c_char_p,C.c_char_p,C.c_uint,C.c_uint,
                             C.POINTER(C.c_void_p),C.POINTER(C.c_void_p)]
        function.restype = C.c_long
        status = function(source,len(source),b'RemasteredLocal',None,None,b'BackgroundPS',b'ps_5_0',
                          1<<15,0,C.byref(code),C.byref(errors))
    messages, result = blob_bytes(errors), blob_bytes(code)
    if status < 0:
        raise ValueError(messages.decode(errors='replace'))
    return result.decode() if preprocess else result


SHARED = r'''
cbuffer BackgroundData : register(b0) {
    float4 Camera0,Camera1,Camera2,Object0,Object1,Object2,Object3,Projection;
    float4 RMParams[128];
};
SamplerState RMSampler : register(s0);
struct BackgroundOutput {
    float4 position:SV_Position; float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;
    float3 normal:TEXCOORD2; float3 tangent:TEXCOORD3; float3 binormal:TEXCOORD4;
    float3 world:TEXCOORD5; float3 object:TEXCOORD6; float3 normworld:TEXCOORD7;
    float4 color:COLOR0;
};
float2 GVec2(float v){return v.xx;} float2 GVec2(float2 v){return v;}
float2 GVec2(float x,float y){return float2(x,y);}
float3 GVec3(float v){return v.xxx;} float3 GVec3(float3 v){return v;}
float3 GVec3(float4 v){return v.xyz;} float3 GVec3(float x,float y,float z){return float3(x,y,z);}
float3 GVec3(float2 xy,float z){return float3(xy,z);}
float4 GVec4(float v){return v.xxxx;} float4 GVec4(float4 v){return v;}
float4 GVec4(float3 v,float w){return float4(v,w);}
float4 GVec4(float2 a,float2 b){return float4(a,b);}
float4 GVec4(float x,float y,float z,float w){return float4(x,y,z,w);}
float4 getBackExp(){return float4(1,1,1,1);}
float RMLinear(float v){v=max(v,0);return v<=0.04045?v/12.92:pow((v+0.055)/1.055,2.4);}
'''


def translated_shader(root, name):
    program = (root/'gl_prog'/(name+'.prog')).read_text()
    pieces = []
    for line in program.splitlines():
        match = re.match(r'\s*add\s+frag_(text|script)\s+(.+)', line)
        if match:
            kind, value = match.groups()
            if kind == 'text':
                if not value.startswith('#version'):
                    pieces.append(value)
            else:
                pieces.append((root/'gl_prog'/value.strip()).read_text())
    source = d3d_compile('\n'.join(pieces), True)
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    source = re.sub(r'^#.*$', '', source, flags=re.M)
    source = re.sub(r'\s*([().,\[\]*])\s*',r'\1',source).rstrip('\0')
    uniforms = {}
    textures = []
    defines = []
    slot = 0
    pattern = r'uniform\s+(sampler2D|samplerCube|vec[234]|float|mat4)\s+(\w+)(?:\[(\d+)\])?\s*;'
    declarations = list(re.finditer(pattern, source))
    source = re.sub(pattern, '', source)
    for match in declarations:
        typ, variable, count = match.groups()
        if variable in uniforms or variable in textures:
            continue
        if typ.startswith('sampler'):
            if len(textures) >= 10:
                raise ValueError('too many background texture bindings')
            # RM reflection cubemaps are sampled through the same converted
            # dual-paraboloid sky mapping; no flat 2D approximation of a cube.
            defines.append(f'Texture2D<float4> {variable}:register(t{len(textures)});')
            textures.append(variable)
        else:
            length = int(count or (4 if typ == 'mat4' else 1))
            uniforms[variable] = dict(slot=slot, vectors=length, type=typ)
            if count:
                source = re.sub(r'\b'+variable+r'\[([^\]]+)\]',lambda m:f'RMParams[{slot}+({m[1]})]',source)
            elif typ == 'mat4':
                defines.append(f'#define {variable} transpose(float4x4('+','.join(f'RMParams[{slot+i}]' for i in range(4))+'))')
            else:
                suffix = '.x' if typ == 'float' else { 'vec2':'.xy','vec3':'.xyz','vec4':'' }[typ]
                defines.append(f'#define {variable} RMParams[{slot}]{suffix}')
            slot += length
    if slot > 128:
        raise ValueError('material constants exceed native layout')
    source = re.sub(pattern, '', source)
    source = re.sub(r'smooth\s+in\s+\w+\s+\w+\s*;', '', source)
    source = re.sub(r'layout\s*\([^)]*\)\s*out\s+vec4\s+finalCol0\s*;', '', source)
    for variable, field in dict(outUV0='uv0',outUV1='uv1',outNorm='normal',outTan='tangent',
                                outBiNorm='binormal',outPos_W='world',outEye_W='world',
                                outPos='object',outNorm_W='normworld',outCol0='color').items():
        defines.append(f'#define {variable} input.{field}')
    source = source.replace('mat3(outTan,outBiNorm,outNorm)*mapNorm',
        '(outTan*mapNorm.x+outBiNorm*mapNorm.y+outNorm*mapNorm.z)')
    source = source.replace('mat3(outTan,outBiNorm,outNorm)*texNorm',
        '(outTan*texNorm.x+outBiNorm*texNorm.y+outNorm*texNorm.z)')
    source = re.sub(r'mat3\(inMatMV\)\s*\*\s*(\w+)',r'mul((float3x3)inMatMV,\1)',source)
    source = source.replace('surfPos*mat3(cross(axisy,origin),axisy,origin)',
        'float3(dot(surfPos,cross(axisy,origin)),dot(surfPos,axisy),dot(surfPos,origin))')
    if 'mat3(' in source:
        raise ValueError('untranslated matrix operation')
    # Source constant arrays have GLSL constructors; preserve exact values.
    source = re.sub(r'const\s+vec4\s+(\w+)\[(\d+)\]\s*=\s*vec4\[\d+\]\((.*?)\);',
                    r'static const float4 \1[\2]={\3};',source,flags=re.S)
    for n in (2,3,4):
        source = re.sub(r'\bvec'+str(n)+r'\s*\(',f'GVec{n}(',source)
        source = re.sub(r'\bvec'+str(n)+r'\b',f'float{n}',source)
    source = re.sub(r'\bmat4\b','float4x4',source)
    source = re.sub(r'\bmix\b','lerp',source)
    source = re.sub(r'\bfract\b','frac',source)
    source = re.sub(r'\batan\(', 'atan2(', source) # all RM uses are atan(y,x)
    for variable in textures:
        source = re.sub(r'\btexture\('+variable+r'\s*,', variable+'.Sample(RMSampler,',source)
    # Cubemap coordinates use the runtime sky's dual-paraboloid mapping.
    for variable in ('inTexEnv0','inTexEnv1'):
        source = source.replace(variable+'.Sample(RMSampler, envNorm)',f'RMSampleSky({variable},envNorm)')
    for variable in ('znear','zfar'):
        source = re.sub(r'float '+variable+r'\s*=\s*([^;]+);',lambda m:'#define '+variable+' '+m[1]+'\n',source)
    source = re.sub(r'^([ \t]*)(float\s+(?:maxEvent|sumEdge|maxEdge|maxEventC|maxEdgeC|addEvent)\s*=\s*0\.0\s*;)',r'\1static \2',source,flags=re.M)
    source = re.sub(r'void\s+main\s*\(\s*\)', 'float4 BackgroundPS(BackgroundOutput input):SV_Target',source)
    start = source.index('float4 BackgroundPS')
    body = source[start:]
    body = body.replace('{','{\nfloat4 finalCol0=0;',1)
    end = body.rfind('}')
    body = body[:end]+'return float4(RMLinear(finalCol0.r),RMLinear(finalCol0.g),RMLinear(finalCol0.b),finalCol0.a);\n'+body[end:]
    sky_helper = r'''
float2 RMParaboloid(float3 d,bool front){
 float2 q=d.xy/max(1e-5,1+abs(d.z)); if(!front)q.x=-q.x;
 q=q/1.0625*.5+.5;return float2(q.x*.5+(front?0:.5),q.y);
}
float4 RMSampleSky(Texture2D<float4> tex,float3 d){
 d=normalize(d);float4 a=tex.Sample(RMSampler,RMParaboloid(d,true));
 float4 b=tex.Sample(RMSampler,RMParaboloid(d,false));float4 c=lerp(b,a,smoothstep(-.055,.055,d.z));
 // The translated source material works in its original gamma space. Our
 // BC6H environment is linear, and finalCol0 is linearized once at the end.
 c.rgb=float3(c.r<=.0031308?12.92*c.r:1.055*pow(max(c.r,0),1/2.4)-.055,
             c.g<=.0031308?12.92*c.g:1.055*pow(max(c.g,0),1/2.4)-.055,
             c.b<=.0031308?12.92*c.b:1.055*pow(max(c.b,0),1/2.4)-.055);return c;
}
'''
    hlsl = SHARED+'\n'+'\n'.join(defines)+'\n'+sky_helper+source[:start]+body
    return hlsl,dict(uniforms=uniforms,textures=textures)


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument('--shaders',type=Path,required=True)
    cli.add_argument('--output',type=Path,required=True)
    args = cli.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    names = ['bg_planet','bg_planetmelt','bg_atmos','bg_atmosmelt','bg_moon','bg_cosmic','bg_cosmic_a']
    layouts = {}
    for name in names:
        source, layout = translated_shader(args.shaders,name)
        (args.output/(name+'.hlsl')).write_text(source)
        code = d3d_compile(source)
        (args.output/(name+'.cso')).write_bytes(code)
        layouts[name] = layout
        print(name,len(code),'bytes',flush=True)
    (args.output/'shader_layouts.json').write_text(json.dumps(layouts,indent=2))


if __name__=='__main__':
    main()
