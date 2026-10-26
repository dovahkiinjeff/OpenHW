"""Restore mission lighting skies from the user's Classic archive, preserving authoring."""
from pathlib import Path
import json,os,shutil,struct,subprocess

def valid_pair(root,mission):
    stem=f'mission{mission:02d}_sky'
    try:
        with (root/(stem+'.dds')).open('rb') as f:header=f.read(148)
        text=(root/(stem+'.hdrmeta')).read_text(encoding='ascii').split()
        return len(header)==148 and header[:4]==b'DDS ' and struct.unpack_from('<II',header,12)==(2048,4096) and struct.unpack_from('<I',header,128)[0]==95 and len(text)==16 and text[0]=='HWSKYHDR1'
    except (OSError,ValueError,struct.error):return False

def ensure(root,classic=None,selected=None):
    root=Path(root).resolve();destination=root/'Data/Missions'
    if all(valid_pair(destination,m) for m in range(1,17)):return 0
    if classic is None:
        if selected is not None:
            from portable_setup import collection_paths
            _,classic_dir=collection_paths(selected);classic=classic_dir/'Homeworld.big'
        else:
            for name in ('build-source-receipt.json','SetupSources/source-receipt.json'):
                receipt=root/'Development'/name
                if not receipt.is_file():continue
                data=json.loads(receipt.read_text())
                if data.get('schema')==2:classic=Path(data['classic'])/'Homeworld.big'
                elif (root/'Development/SetupSources/Classic/Homeworld.big').is_file():classic=root/'Development/SetupSources/Classic/Homeworld.big'
                if classic is not None and Path(classic).is_file():break
    if classic is None or not Path(classic).is_file():raise ValueError('Select the original Homeworld Collection to repair mission lighting.')
    classic=Path(classic).resolve();work=root/'Development/SkyRepair';work.mkdir(parents=True,exist_ok=True)
    btg_dir=work/'btg';btg_dir.mkdir(exist_ok=True)
    from extract_vanilla_ship_textures import read_archive,entry_bytes
    from generate_mission_skies import (MISSION_BTG,UNIQUE_ASSET_MISSIONS,parse_btg,render_btg,add_star,enforce_spherical_edges,analyze_sky,reproject_dual_paraboloid,write_hwfloat,write_metadata)
    from PIL import Image
    names={n+'.btg' for n in MISSION_BTG.values()};found=set()
    for archive in (classic,classic.with_name('Update.big')):
        if not archive.is_file():continue
        blob,entries=read_archive(archive)
        for entry in entries:
            name=entry.name.replace('\\','/').split('/')[-1].lower()
            if name in names:(btg_dir/name).write_bytes(entry_bytes(blob,entry));found.add(name)
        del blob,entries
    if found!=names:raise ValueError('Missing Classic sky sources: '+', '.join(sorted(names-found)))
    material=Image.open(Path(__file__).parent/'sky_materials/btg_microstructure.png').convert('L')
    encoder=root/'Development/SetupTools/bc6h_encode.exe'
    if not encoder.is_file():raise ValueError('Installed sky converter is missing')
    destination.mkdir(parents=True,exist_ok=True);created=0
    for mission in UNIQUE_ASSET_MISSIONS:
        if valid_pair(destination,mission):continue
        print(f'Restoring mission {mission:02d} lighting sky...',flush=True)
        btg=parse_btg(btg_dir/(MISSION_BTG[mission]+'.btg'))
        background=render_btg(btg,4096,2048,material,mission);metadata_image=background.copy()
        for star in btg['stars']:add_star(metadata_image,star,btg['header']['page_width'],btg['header']['page_height'])
        enforce_spherical_edges(metadata_image);metadata=analyze_sky(metadata_image,mission);del metadata_image
        image=reproject_dual_paraboloid(background);del background
        raw=work/'current.hwf6';write_hwfloat(raw,image);del image
        stem=f'mission{mission:02d}_sky';dds=work/(stem+'.dds');meta=work/(stem+'.hdrmeta')
        subprocess.run([str(encoder),str(raw),str(dds)],cwd=root,check=True);write_metadata(meta,metadata)
        os.replace(dds,destination/dds.name);os.replace(meta,destination/meta.name);raw.unlink();created+=1
    if not valid_pair(destination,3):
        for suffix in ('.dds','.hdrmeta'):
            target=destination/('mission03_sky'+suffix);pending=target.with_suffix(target.suffix+'.pending')
            shutil.copyfile(destination/('mission01_sky'+suffix),pending);pending.replace(target)
        created+=1
    if not all(valid_pair(destination,m) for m in range(1,17)):raise ValueError('Mission lighting repair incomplete')
    print('All 16 mission lighting skies are ready.',flush=True)
    return created
