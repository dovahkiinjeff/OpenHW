"""Import complete, legally owned classic HW1 voice sets; no voice downloads."""
from pathlib import Path
import struct
import shutil
import uuid
import openhw_updater as updater

NAMES = ('English','French','German','Spanish','Italian')
FILES = ('SoundFX/speechsentence_comp.lut','SoundFX/speechphrase_comp.lut','HW_Comp.vce')


def validate(paths):
    checksums=[]
    for path in paths:
        if path.stat().st_size < 16: raise ValueError('Incomplete voice pack: '+path.name)
        with path.open('rb') as source: checksums.append(struct.unpack('<II',source.read(8))[1])
    if len(set(checksums))!=1: raise ValueError('The speech bank and its two lookup tables do not match')


def available(root):
    choices={'0':'English / installed original audio'}
    root=Path(root).resolve()
    for i,name in enumerate(NAMES[1:],1):
        try:
            paths=[updater.inside(root,'Data/Localization/'+name+'/'+file) for file in FILES]
            validate(paths); choices[str(i)]=name+' / installed voice pack'
        except (OSError,ValueError,updater.UpdateError): pass
    return choices


def import_classic(root, source, language):
    updater.stop_if_game_running(); root=Path(root).resolve(); source=Path(source).resolve()
    if language not in NAMES[1:]: raise ValueError('Select the actual language of this classic HW1 speech set')
    # Use the existing dual-layout classic/RM BIG reader. No whole-game extraction.
    from extract_vanilla_ship_textures import read_archive, entry_bytes
    bank=source/'HW_Comp.vce'; archive=source/'Homeworld.big'
    if not bank.is_file() or not archive.is_file(): raise ValueError('Select a localized Homeworld 1 Classic Data folder containing Homeworld.big and HW_Comp.vce')
    blob, entries=read_archive(archive)
    index={entry.name.replace('\\','/').lower():entry for entry in entries}
    stage=updater.inside(root,'Development/Temp/VoicePack-'+uuid.uuid4().hex); stage.mkdir(parents=True)
    try:
        for name in FILES[:2]:
            entry=index.get(name.lower())
            if entry is None: raise ValueError('The classic archive is missing '+name)
            path=stage/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(entry_bytes(blob,entry))
        shutil.copy2(bank,stage/FILES[2]); validate([stage/name for name in FILES])
        target=updater.inside(root,'Data/Localization/'+language); previous=None
        if target.exists():
            previous=updater.inside(root,'Development/Backups/VoicePacks/'+language+'-'+uuid.uuid4().hex)
            previous.parent.mkdir(parents=True,exist_ok=True); target.rename(previous)
        try:
            target.parent.mkdir(parents=True,exist_ok=True); stage.rename(target)
        except OSError:
            if previous is not None and not target.exists(): previous.rename(target)
            raise
        return NAMES.index(language)
    finally:
        if stage.exists(): shutil.rmtree(stage)
