"""Inspect licensed RM sound parameters without executing their Lua.

This is import bookkeeping, not another runtime sound-file grammar. Missing
parameters stay missing: RM keeps an authored-parameter mask and combines
parameter records; substituting defaults at this boundary loses that data.
"""
import math
from pathlib import Path
from import_kushan_weapon_fx import read_table

# Source parameter order follows RM's 30-entry parameter table. Presence stays
# separate from values; absence must not override an inherited parameter.
PARAMETERS=('priority','compression','volume','volumeRand','frequency','frequencyRand',
    'destination','patch','numChannels','maxPolyphony','loopStart','loopEnd','startOffset',
    'loopCount','loopCountRand','type3D','doppler','coneMin','coneMax','envelope','effects',
    'cardiodScale','cardiodLowFreq','cardiodHighFreq','container','playlist','randContainer',
    'loopingPlaylist','silenceBreak','silenceRandom')
EFFECT_PARAMETERS=('filterMinFreq','filterMaxFreq','toneMinFreq','toneMaxFreq','toneDuration',
    'toneMute','toneCount','breakMaxRate','breakMaxDur','noiseMaxRate','noiseMaxDur',
    'scaleLevel','noiseLevel','toneLevel','limiterLevel','pitchShift','distance','volume',
    'reverb','duration')


def parse_parameters(text):
    if not 0 < len(text) <= 1024*1024:
        raise ValueError('Invalid sound parameter storage')
    data=read_table(text,None,preserve_keys=True,allow_products=False)
    if data.get('version') != 4:
        raise ValueError('Unsupported RM sound parameter version')
    def finite(value):
        return not isinstance(value,bool) and isinstance(value,(int,float)) and math.isfinite(value)
    def array(value):
        if not isinstance(value,dict) or set(value)!=set(range(len(value))):
            raise ValueError('Noncontiguous sound parameter array')
        return [value[i] for i in range(len(value))]
    result={}
    for name,value in data.items():
        if name=='envelope':
            points=array(value)
            if not 0<len(points)<=256:raise ValueError('Invalid sound envelope size')
            normalized=[];previous=-1
            for point in points:
                if not isinstance(point,dict) or set(point)!={'distance','volume','reverb','duration','equalizer'}:
                    raise ValueError('Invalid sound envelope point')
                row=dict(point);row['equalizer']=array(row['equalizer'])
                if len(row['equalizer'])!=8 or not all(finite(n) for n in row['equalizer']):
                    raise ValueError('Invalid sound equalizer')
                if not all(finite(row[n]) for n in ('distance','volume','reverb','duration')):
                    raise ValueError('Invalid sound envelope number')
                if row['distance']<previous or row['distance']<0:
                    raise ValueError('Unordered sound envelope distances')
                previous=row['distance'];normalized.append(row)
            result[name]=normalized
        elif name=='effects':
            if not isinstance(value,dict) or len(value)>128:
                raise ValueError('Invalid sound effect parameters')
            row=dict(value)
            for key,entry in row.items():
                if key=='equalizer':
                    entry=array(entry)
                    if len(entry)!=8 or not all(finite(n) for n in entry):
                        raise ValueError('Invalid sound effect equalizer')
                    row[key]=entry
                elif not isinstance(key,str) or not finite(entry):
                    raise ValueError('Invalid sound effect number')
            result[name]=row
        else:
            if not finite(value):raise ValueError('Invalid sound parameter: '+str(name))
            result[name]=value
    return result


def parameter_inventory(source):
    """Retain each authored record and its on-disk sample membership.

    A directory's same-named Lua describes that directory. A sample's
    same-named Lua describes that sample. Do not recursively flatten directory
    samples: nested random containers/playlist nodes remain separate nodes.
    Sample order and random/playlist execution semantics are not inferred.
    """
    source=Path(source).resolve();nodes=[];identities=set()
    for path in sorted(source.rglob('*.lua')):
        if path.is_symlink() or not path.resolve().is_relative_to(source):
            raise ValueError('Sound parameter source escapes import directory')
        if path.stat().st_size>1024*1024:raise ValueError('Oversized sound parameters')
        parameters=parse_parameters(path.read_text(encoding='ascii'))
        relative=path.relative_to(source).as_posix()
        directory=path.stem.casefold()==path.parent.name.casefold()
        identity=(path.parent if directory else path.with_suffix('')).relative_to(source).as_posix()
        if identity=='.':identity=''
        if identity.casefold() in identities:raise ValueError('Duplicate sound parameter identity')
        identities.add(identity.casefold())
        samples=sorted(path.parent.glob('*.fda')) if directory else [path.with_suffix('.fda')]
        members=[]
        for sample in samples:
            if not sample.is_file() or sample.is_symlink() or not sample.resolve().is_relative_to(source):
                raise ValueError('Missing or unsafe sound parameter sample: '+relative)
            members.append(sample.relative_to(source).as_posix())
        nodes.append(dict(path=relative,identity=identity,scope='directory' if directory else 'sample',
                          authored=parameters,samples=members))
    return nodes


def compile_sound_bank(source,parameters=None):
    """One bounded runtime index, preserving directory and sample profiles.

    No flattening of nested containers and no import of default parameter
    values. The runtime can inspect every authored scalar, curve and DSP field.
    """
    import struct
    source=Path(source).resolve()
    parameters=parameter_inventory(source) if parameters is None else parameters
    profiles={n['identity'].casefold():n['authored'] for n in parameters}
    strings=bytearray(b'\0');offsets={'':0}
    def string(value):
        if value not in offsets:
            raw=value.encode('ascii')
            if len(raw)>511 or any(c<32 or c>126 for c in raw):raise ValueError('Invalid sound index string')
            offsets[value]=len(strings);strings.extend(raw+b'\0')
        return offsets[value]
    identities={'':dict(parent=None,sample='')}
    for path in sorted(source.rglob('*.fda')):
        if path.is_symlink() or not path.resolve().is_relative_to(source):raise ValueError('Unsafe sound bank sample')
        name=path.relative_to(source).as_posix().lower()
        identity=name[:-4]
        if identity in identities:raise ValueError('Ambiguous sound identity: '+identity)
        parent=Path(identity).parent.as_posix()
        if parent=='.':parent=''
        identities[identity]=dict(parent=parent,sample='remasteredaudio/sfx/etg/'+name)
        while parent:
            ancestor=Path(parent).parent.as_posix()
            if ancestor=='.':ancestor=''
            if parent in identities:
                if identities[parent]['sample']:raise ValueError('Sound sample/directory identity collision')
                break
            identities[parent]=dict(parent=ancestor,sample='');parent=ancestor
    names=sorted(identities);indices={name:i for i,name in enumerate(names)}
    children={name:[] for name in names}
    for name,node in identities.items():
        if node['parent'] is not None:children[node['parent']].append(indices[name])
    nodes=bytearray();edges=[];records=bytearray();envelopes=bytearray();profile_count=0;envelope_count=0
    for name in names:
        node=identities[name];profile=profiles.get(name);profile_index=0xffffffff
        if profile is not None:
            unknown=set(profile)-set(PARAMETERS)-{'version','randSampContainer'}
            if unknown:raise ValueError('Unsupported sound bank parameter: '+', '.join(sorted(unknown)))
            profile_index=profile_count;profile_count+=1
            mask=sum(1<<i for i,key in enumerate(PARAMETERS) if key in profile)
            values=[float(profile.get(key,0)) if key not in ('envelope','effects') else 0 for key in PARAMETERS]
            curve=profile.get('envelope',[])
            if len(curve)>5:raise ValueError('RM sound envelope has more than five points')
            for point in curve:
                envelopes+=struct.pack('<12f',point['distance'],point['volume'],point['reverb'],point['duration'],*point['equalizer'])
            effect=profile.get('effects',{});unknown=set(effect)-set(EFFECT_PARAMETERS)-{'equalizer'}
            if unknown:raise ValueError('Unsupported sound DSP parameter')
            effect_mask=sum(1<<i for i,key in enumerate(EFFECT_PARAMETERS) if key in effect)
            if 'equalizer' in effect:effect_mask|=1<<20
            effect_values=[effect.get(key,0) for key in EFFECT_PARAMETERS]+effect.get('equalizer',[0]*8)
            records+=struct.pack('<IiIII58f',mask,int(profile.get('randSampContainer',-1)),envelope_count,len(curve),effect_mask,*values,*effect_values)
            envelope_count+=len(curve)
        direct=sorted(children[name]);first=len(edges);edges+=direct
        nodes+=struct.pack('<7I',string(name),string(node['sample']),
            indices[node['parent']] if node['parent'] is not None else 0xffffffff,
            profile_index,first,len(direct),int(bool(node['sample'])))
    if not 0<len(names)<=4096 or len(edges)>4096 or profile_count>1024 or envelope_count>5120 or len(strings)>1024*1024:
        raise ValueError('Sound bank exceeds runtime limits')
    return struct.pack('<8s5I',b'OWSFX001',len(names),len(edges),profile_count,envelope_count,len(strings))+nodes+struct.pack('<'+str(len(edges))+'I',*edges)+records+envelopes+strings
