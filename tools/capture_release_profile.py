"""Capture current authoring and only explicitly approved artwork for rebuilding.

This runs on the developer machine, never on an end user's computer. The
result is source input, independent of the developer's installed game path.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
from PIL import Image
from extract_vanilla_ship_textures import read_archive, entry_bytes, decode_lif
from release_content_policy import TEXT_TYPES, UI_PLATES, digest, verify_profile
from stage_release import defaults


def capture(repo, current, classic, output):
    repo, current, classic, output = map(lambda p: Path(p).resolve(), (repo,current,classic,output))
    if output.exists() and any(output.iterdir()):
        raise ValueError('Profile destination must be empty')
    output.mkdir(parents=True, exist_ok=True)
    records = {}
    def add(source, name, category, **lineage):
        target = output/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        records[name] = {'path':name, 'category':category, 'sha256':digest(target), **lineage}
    data = current/'Data'
    for path in sorted(data.rglob('*')):
        if not path.is_file() or path.suffix.lower() not in TEXT_TYPES:
            continue
        relative = path.relative_to(data).as_posix().lower()
        from release_compact import hw1_subsystem
        if relative.startswith('subsystems/') and not hw1_subsystem(relative.split('/')[1]):continue
        section = 'Authored' if path.suffix.lower() in TEXT_TYPES-{
            '.rtxmap','.rtxlight','.rtxdust','.rtxcloud'} else 'Content'
        add(path, section+'/'+relative, 'authored-text')
    from release_mission_authoring import select_mission_files
    for row in select_mission_files(current):
        add(row['source'],'Content/missions/'+row['name'].lower(),'authored-text')
    for name in sorted(UI_PLATES):
        path = data/'UI'/name
        if not path.is_file(): path = repo/'assets/UI'/name
        if not path.is_file(): raise ValueError('Approved UI plate missing: '+name)
        add(path,'Content/ui/'+name,'supplied-concept-art',
            approval='Project owner approved supplied concept-art panels for this release',
            public_domain_claim=False)
    for path in sorted((repo/'assets/Asteroids').iterdir()):
        if path.is_file() and re.fullmatch(r'asteroid[1-4](?:_lod[1-3]\.obj|\.obj|_(?:basecolor|normal|orm)\.dds|_albedo\.png)',path.name):
            add(path,'Content/asteroids/'+path.name,'owner-created-asteroid',
                approval='Project owner confirmed these asteroid models and textures are their own work')
    blob, entries = read_archive(classic)
    entries = {e.name.replace('\\','/').lower():e for e in entries}
    for path in sorted((data/'Cursors').glob('*.png')):
        original='cursorbitmaps/'+path.stem.lower()+'.lif'
        entry=entries.get(original)
        if entry is None:continue
        raw=entry_bytes(blob,entry);base,_,_,_=decode_lif(raw)
        with Image.open(path) as image:size=image.size
        if size[0]<=base.width or size[1]<=base.height:continue
        name='cursors/'+path.name.lower()
        add(path,'Content/'+name,'upscaled-classic-texture',classic_entry=original,
            classic_sha256=hashlib.sha256(raw).hexdigest(),classic_dimensions=list(base.size),
            output_dimensions=list(size),upscaled_base=name,
            approval='Project owner explicitly allowed the upscaled Classic textures')
    metadata = {}
    candidates = []
    for path in data.rglob('*.dds'):
        relative = path.relative_to(data).as_posix().lower()
        if '/rmnative/' in '/'+relative or relative.split('/')[0] not in {
            'r1','r2','p1','p2','p3','traders','derelicts','etg'}:
            continue
        stem = re.sub(r'_(?:normal|teameffect[01])$','',path.stem.lower())
        original = path.relative_to(data).with_name(stem+'.lif').as_posix().lower()
        entry = entries.get(original)
        if entry is None: continue
        if original not in metadata:
            raw = entry_bytes(blob,entry)
            try: image,_,_,_ = decode_lif(raw)
            except ValueError: continue
            metadata[original] = (image.size,hashlib.sha256(raw).hexdigest())
        try:
            with Image.open(path) as image: size = image.size
        except OSError: continue
        candidates.append((path,relative,original,size,metadata[original]))
    # A mask at original resolution is accepted only as a companion to a
    # demonstrably larger restored color texture of the same original LIF.
    enlarged = {original for _,relative,original,size,(base,_) in candidates
                if Path(relative).stem == Path(original).stem and
                size[0] > base[0] and size[1] > base[1]}
    for path,relative,original,size,(base,source_hash) in candidates:
        if original not in enlarged: continue
        add(path,'Content/'+relative,'upscaled-classic-texture',
            classic_entry=original,classic_sha256=source_hash,
            classic_dimensions=list(base),output_dimensions=list(size),
            upscaled_base=original[:-4]+'.dds',
            approval='Project owner explicitly allowed the upscaled Classic textures')
    (output/'defaults.cfg').write_text(defaults((current/'Homeworld.cfg').read_text()))
    presets = output/'Presets';presets.mkdir()
    for path in current.glob('*.cfg'):
        if path.name.startswith(('Parade-','RMHarvestableDust')):
            shutil.copyfile(path,presets/path.name)
    inventory = {'schema':2,'retail_assets':'Imported from selected Collection at setup',
                 'classic_archive_sha256':digest(classic),
                 'files': sorted(records.values(),key=lambda r:r['path'])}
    (output/'content-inventory.json').write_text(json.dumps(inventory,indent=2))
    report={'schema':1,'baseline':'Effective WIP authoring using runtime override precedence',
      'files':[{'mission':r['mission'],'path':r['name'],'sha256':r['sha256'],'origin':r['relativeSource']}
               for r in select_mission_files(current)]}
    (output/'mission-authoring.json').write_text(json.dumps(report,indent=2))
    from release_event_edits import capture as capture_events
    capture_events(current,current/'Dev/SetupSources/Remastered',output)
    verify_profile(output)
    print(json.dumps(dict(files=len(records),categories=dict(Counter(r['category'] for r in records.values()))),indent=2))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('repo','current','classic','output'):parser.add_argument('--'+key,type=Path,required=True)
    a=parser.parse_args();capture(a.repo,a.current,a.classic,a.output)
