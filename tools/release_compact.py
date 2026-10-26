"""Bounded release import from read-only, indexed Collection archives."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
from extract_remastered_big import BigArchive

HW1_PREFIXES=('kus_','tai_','tur_','kad_','hw1_')
SPECIAL_SHIPS=('ben_bentusiexchange','der_ghostship','junk_junkyardoffice','misc_t-matmegaship')
SHARED_TAIIDAN_BEAM=frozenset((
 'scripts/weaponfire/vgrbeam/vgrbeam.wf','art/fx/vgr_laser_combo.lua',
 'art/fx/vgr_beam_muzzle_tip.lua','art/fx/vgr_beam_muzzle_flash_ring.lua',
 'art/fx/vgr_pulse_beam.lua','art/fx/vgr_laser_squirt.lua','art/fx/vgr_beam_light_large.lua',
 'art/fx/vgrbeam/vgr_beam.anim','art/fx/vgrbeam/vgr_beam_tp.tga',
 'art/fx/ion_beam/vgr_ion_beam_glow_ball.tga'))

def shared_hw1_effect(name):
 name=name.replace('\\','/').lower()
 return name in SHARED_TAIIDAN_BEAM or name=='remasteredfx/weaponfire/vgrbeam.wf' or any(name.endswith('/'+p) for p in SHARED_TAIIDAN_BEAM)

def excluded_faction(name):
 if shared_hw1_effect(name):return False
 return re.search(r'(^|[/\\_\-.])(hgn_|vgr_|vgrbeam|hiigaran|vaygr)',name.lower()) is not None

def hw1_subsystem(name):return name.lower().startswith(HW1_PREFIXES)

def required_source(name):
 if excluded_faction(name):return False
 parts=name.split('/')
 if len(parts)<2:return False
 if parts[0]=='ship':return parts[1].startswith(('kus_','tai_','kad_','tur_','der_')) or parts[1] in SPECIAL_SHIPS
 if parts[0]=='subsystem':return hw1_subsystem(parts[1])
 if parts[0] in ('weapon','missile'):return parts[1].startswith(HW1_PREFIXES)
 if name.startswith(('art/fx/','effect/trails/','effect/lensflare/','scripts/weaponfire/')):return True
 if name.startswith(('resource/dustcloud/','resource/nebula/')):return True
 if name.startswith('sound/sfx/etg/'):return Path(name).suffix in ('.lua','.fda')
 if name.startswith('sound/music/'):
  from import_remastered_audio import TRACKS
  return name[len('sound/music/'):] in TRACKS
 return False

def source_locations(sources):
 receipt=Path(sources)/'source-receipt.json'
 data=json.loads(receipt.read_text()) if receipt.exists() else {}
 if data.get('schema')==2:
  rm=Path(data['remastered']).resolve(strict=True);classic=Path(data['classic']).resolve(strict=True)
  for row in data['archives']:
   p=Path(row['path'])
   if p.parent not in (rm,classic) or not p.is_file():raise ValueError('Missing or changed Collection source')
   st=p.stat()
   if st.st_size!=row['size'] or st.st_mtime_ns!=row['mtime_ns']:raise ValueError('Collection changed since setup began: '+p.name)
  return rm,classic
 return Path(sources)/'Archives',Path(sources)/'Classic'

def import_compact(selected,destination):
 from portable_setup import collection_paths,RM_ARCHIVES,CLASSIC_FILES
 rm,classic=collection_paths(selected);destination=Path(destination).resolve();collection=rm.parent.parent.resolve()
 if destination==collection or destination.is_relative_to(collection) or collection.is_relative_to(destination):raise ValueError('Setup destination must be separate from the Collection')
 root=destination/'SetupSources';root.mkdir(parents=True,exist_ok=True);receipt=root/'source-receipt.json'
 if receipt.exists():
  previous=json.loads(receipt.read_text())
  if previous.get('schema')!=2:raise ValueError('This folder contains an older full-extraction setup. Use a new empty installation folder for the compact installer; existing files were preserved.')
  old_rm,old_classic=source_locations(root)
  if old_rm!=rm.resolve() or old_classic!=classic.resolve():raise ValueError('Resume setup with the same Collection folder originally selected.')
  return root
 records=[];selected_count=0;selected_bytes=0
 for p in [*(classic/n for n in CLASSIC_FILES if (classic/n).is_file()),*(rm/n for n in RM_ARCHIVES)]:
  st=p.stat();row=dict(archive=p.name,path=str(p),size=st.st_size,mtime_ns=st.st_mtime_ns)
  with p.open('rb') as f:row['sha256']=hashlib.file_digest(f,'sha256').hexdigest()
  records.append(row)
  if p.parent==rm:
   a=BigArchive(p);names=[n for n in a.entries if required_source(n)]
   print('Reading required entries from '+p.name+': '+str(len(names)),flush=True)
   a.extract(root/'Remastered',predicate=required_source)
   selected_count+=len(names);selected_bytes+=sum(a.entries[n][2] for n in names)
  if p.stat().st_size!=st.st_size or p.stat().st_mtime_ns!=st.st_mtime_ns:raise ValueError('Source changed during import: '+p.name)
 result=dict(schema=2,status='sources-imported-not-runtime-ready',remastered=str(rm),classic=str(classic),archives=records,selectedEntries=selected_count,selectedExpandedBytes=selected_bytes,copiedRetailArchives=0,sourcePolicy='HW1 only; no Hiigaran/Vaygr factions; Taiidan Defense Fighter shared VgrBeam dependency retained',approvedSharedEffectFiles=sorted(SHARED_TAIIDAN_BEAM))
 pending=receipt.with_suffix('.pending');pending.write_text(json.dumps(result,indent=2));pending.replace(receipt)
 return root

def discard(path,within):
 path=Path(path);within=Path(within).resolve();resolved=path.resolve()
 if resolved==within or not resolved.is_relative_to(within) or path.is_symlink():raise ValueError('Unsafe scratch cleanup: '+str(path))
 if path.exists():shutil.rmtree(path)

def link_or_copy(source,target):
 source,target=Path(source),Path(target);target.parent.mkdir(parents=True,exist_ok=True)
 if source.suffix.lower() in ('.json','.txt','.cfg','.owevents','.owshipdef','.owsubsystem','.shp','.rmnative','.events'):return shutil.copyfile(source,target)
 temporary=target.with_name(target.name+'.link-pending')
 if temporary.exists():temporary.unlink()
 try:os.link(source,temporary)
 except OSError:shutil.copyfile(source,temporary)
 temporary.replace(target)
 return str(target)

def audit_factions(root):
 from openhw_obig import validate
 root=Path(root);found=[n for n in validate(root/'OpenHW-HW1.obig') if excluded_faction(n)]
 for folder in ('Subsystems','Data','Modding'):
  found += [str(p.relative_to(root)) for p in (root/folder).rglob('*') if p.is_file() and excluded_faction(p.relative_to(root).as_posix())]
 if found:raise ValueError('Excluded Hiigaran/Vaygr assets reached release: '+', '.join(found[:10]))
 return dict(hiigaranAssets=0,vaygrAssets=0)
