"""Inspect the real single-file installer and the complete source snapshot."""
import argparse
from collections import Counter
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile
from release_content_policy import audit_archive,digest,safe_name


def audit(installer,source):
    installer,source=Path(installer),Path(source)
    with installer.open('rb') as f:
        f.seek(-64,2);magic,offset,size,expected=struct.unpack('<16sQQ32s',f.read(64))
        if magic!=b'OPENHW1SETUPZIP\0' or offset+size+64!=installer.stat().st_size:
            raise ValueError('Invalid standalone installer footer')
        f.seek(offset);h=hashlib.sha256();left=size
        while left:
            block=f.read(min(left,8*1024*1024))
            if not block:raise ValueError('Truncated installer')
            h.update(block);left-=len(block)
        if h.digest()!=expected:raise ValueError('Installer payload checksum failed')
    content=audit_archive(installer)
    markers=[b'HW_TEMPORAL_CAPTURE',b'HW_FG_ZOOM_CAPTURE',b'HW_FSR_FG_DEBUG',
             b'HW_CAPTURE_SUN_SHAFTS',b'benchmark-%04u',b'renderer-%04u',
             b'[Optimization]',b'[FrameWorkers]',b'[ModernGraphics]',b'muzzle-fx-%',b'[NativeRaster]']
    import pefile
    with zipfile.ZipFile(installer) as z:
        inventory=json.loads(z.read('Development/content-inventory.json'))
        mission_authoring=json.loads(z.read('Development/mission-authoring.json'))
        for row in mission_authoring['files']:
            name='Development/Content/missions/'+row['path'].lower()
            if hashlib.sha256(z.read(name)).hexdigest()!=row['sha256']:
                raise ValueError('Effective WIP mission authoring differs in installer: '+row['path'])
        map_missions={row['mission'] for row in mission_authoring['files'] if row['path'].endswith('.rtxmap')}
        if map_missions!=set(range(1,17)):raise ValueError('Complete authored campaign maps are missing')
        with zipfile.ZipFile(io.BytesIO(z.read('Development/runtime.zip'))) as runtime:
            game_exes=sorted(n for n in runtime.namelist() if n.lower().endswith('.exe'))
            if game_exes!=['OpenHW-Daignostics.exe','OpenHW.exe']:raise ValueError('Unexpected game executables')
            builds={}
            for name,logging in [('OpenHW.exe',False),('OpenHW-Daignostics.exe',True)]:
                data=runtime.read(name)
                present=[m in data or m.decode('ascii').encode('utf-16le') in data for m in markers]
                if any(value!=logging for value in present):raise ValueError('Logging marker contract failed: '+name)
                pe=pefile.PE(data=data)
                imports={i.name for entry in getattr(pe,'DIRECTORY_ENTRY_IMPORT',[]) for i in entry.imports}
                if (b'MiniDumpWriteDump' in imports)!=logging:raise ValueError('Crash logging contract failed: '+name)
                builds[name]={'sha256':hashlib.sha256(data).hexdigest(),'logging':logging}
            settings=runtime.read('Homeworld.cfg').decode()
            keys={line.split()[0].lower():line.split()[1:] for line in settings.splitlines() if len(line.split())>1}
            if keys.get('detectdisplayonfirstboot')!=['1'] or keys.get('rasterglowbrightness')!=['29']:
                raise ValueError('Release display/glow defaults changed')
            if {'screenwidth','screenheight','refreshrate','devicecrc','homeworlddatapath'} & keys.keys():
                raise ValueError('Developer display/path settings leaked')
            if any(line.strip() and not line.startswith('#') for line in runtime.read('mods/load-order.txt').decode().splitlines()):
                raise ValueError('A developer mod is enabled')
    snapshot=json.loads((source/'SOURCE_MANIFEST.json').read_text())
    raw_suffixes={'.big','.obig','.hod','.mad','.lif','.geo','.peo','.vce','.wxd','.bik','.wav','.ogg','.mp3'}
    for row in snapshot['files']:
        name=safe_name(row['path']);path=source/name
        if path.suffix.lower() in raw_suffixes:raise ValueError('Retail format in source snapshot: '+name)
        if path.stat().st_size!=row['size'] or digest(path)!=row['sha256']:raise ValueError('Source snapshot changed: '+name)
    return {'status':'passed','installer':{'bytes':installer.stat().st_size,'sha256':digest(installer)},
       'content':content,'contentCategories':dict(Counter(r['category'] for r in inventory['files'])),
       'builds':builds,'sourceFiles':len(snapshot['files']),'sourceManifestSha256':digest(source/'SOURCE_MANIFEST.json'),
       'defaults':{'firstBootDisplayDetection':True,'globalNonPTGlowPercent':29},
       'missionAuthoring':{'maps':len(map_missions),'lighting':sum(r['path'].endswith('.rtxlight') for r in mission_authoring['files']),
                           'physicalLoaderPath':'Data/Missions','source':'Effective WIP runtime files'},
       'notTested':'Interactive installer and gameplay; reserved for the project owner'}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--installer',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path)
    a=p.parse_args();value=audit(a.installer,a.source);text=json.dumps(value,indent=2)
    if a.output:a.output.write_text(text)
    print(text)
