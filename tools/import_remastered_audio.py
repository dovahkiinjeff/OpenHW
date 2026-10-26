"""Stage licensed RM audio locally; HW1 scheduling remains authoritative.

This copies licensed local assets into a generated archive input directory.
It does not distribute audio, execute RM Lua, or modify the installed game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import wave
from openhw_audio_presentation import parameter_inventory,compile_sound_bank

TRACKS = [
    'ambient/a02_mission2and4.wav', 'ambient/a04_mission5.wav',
    'ambient/a05_mission6and12.wav', 'ambient/a06_mission7and8.wav',
    'ambient/a07_mission9.wav', 'ambient/a08_mission10.wav',
    'ambient/a09_mission11and15.wav', 'ambient/a11_mission13.wav',
    'ambient/a12_mission14.wav', 'ambient/tutorial.wav',
    'battle/a01_mission1and16.wav', 'battle/a03_mission3.wav',
    'battle/b01_turanicraiderslong.wav', 'battle/b02_turanicraidersshort.wav',
    'battle/b03_swarmers.wav', 'battle/b04_evilempire.wav', 'battle/nislet01.wav',
    'nis/n01_r1opening.wav', 'nis/n01_r2opening.wav', 'nis/n02_p1intro.wav',
    'nis/n03_tradersintro.wav', 'nis/n04_learnofsacking.wav', 'nis/n05_p2intro.wav',
    'nis/n06_supernova.wav', 'nis/n07_p3vstraders.wav', 'nis/n08_awareness.wav',
    'nis/n09_headshot.wav', 'nis/n10_miningfacility.wav', None, None, None,
    'animatic/anim_00_opening.wav',
    *[f'animatic/anim_{i:02d}_{i+1:02d}.wav' for i in range(1,16)],
    'animatic/anim_ending.wav',
]

def inventory(source):
    source = Path(source).resolve()
    rows = []
    for track, name in enumerate(TRACKS):
        if name is None:
            continue
        path = source/name
        if path.is_symlink() or not path.resolve().is_relative_to(source):
            raise ValueError('Music source escapes import directory: '+name)
        if not path.is_file() or not 0 < path.stat().st_size <= 128*1024*1024:
            raise ValueError('Missing or oversized RM music: '+name)
        with wave.open(str(path),'rb') as stream:
            channels, width, rate, frames = (stream.getnchannels(),stream.getsampwidth(),
                                           stream.getframerate(),stream.getnframes())
            if channels not in (1,2) or width != 2 or rate not in (22050,44100,48000) or frames <= 0:
                raise ValueError('Unsupported RM music PCM format: '+name)
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream,'sha256').hexdigest()
        rows.append(dict(track=track, path=name, bytes=path.stat().st_size,
                         channels=channels, rate=rate, frames=frames, sha256=digest))
    return rows

def stage(source, generated):
    source, generated = Path(source).resolve(), Path(generated).resolve()
    if generated.is_relative_to(source) or source.is_relative_to(generated):
        raise ValueError('Generated output must be separate from source assets')
    rows = inventory(source)  # Validate the complete set before copying anything.
    destination = generated/'RemasteredAudio'/'music'
    for row in rows:
        target = destination/row['path']
        if not target.resolve().is_relative_to(generated):
            raise ValueError('Music output escapes generated directory')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source/row['path'],target)
    (destination/'import.json').write_text(json.dumps(dict(
        schema='OPENHW_RM_MUSIC_1', tracks=rows, unmapped=[28,29,30]),indent=2),encoding='utf-8')
    return len(rows)


def stage_effects(source,generated):
    """Keep original FDA and companion metadata, including random containers.

    No audio recompression, guessed metadata translation or Lua execution.
    """
    source,generated=Path(source).resolve(),Path(generated).resolve()
    if generated.is_relative_to(source) or source.is_relative_to(generated):
        raise ValueError('Generated output must be separate from source assets')
    if not source.is_dir():raise ValueError('Missing RM effect audio directory')
    parameters=parameter_inventory(source)
    bank=compile_sound_bank(source,parameters)
    bank_target=generated/'RemasteredAudio'/'sfx'/'effects.sfxbank'
    if bank_target.is_symlink() or not bank_target.resolve().is_relative_to(generated):
        raise ValueError('Sound bank output escapes generated directory')
    rows=[]
    for path in sorted(source.rglob('*')):
        if path.suffix.lower() not in ('.fda','.lua') or not path.is_file():continue
        name=path.relative_to(source).as_posix()
        if path.is_symlink() or not path.resolve().is_relative_to(source):
            raise ValueError('Effect audio source escapes import directory: '+name)
        if not 0<path.stat().st_size<=128*1024*1024:
            raise ValueError('Empty or oversized RM effect audio: '+name)
        with path.open('rb') as stream:
            if path.suffix.lower()=='.fda' and stream.read(4)!=b'FORM':
                raise ValueError('Invalid RM effect audio container: '+name)
            stream.seek(0);digest=hashlib.file_digest(stream,'sha256').hexdigest()
        target=generated/'RemasteredAudio'/'sfx'/'etg'/name
        if not target.resolve().is_relative_to(generated):raise ValueError('Effect audio output escapes generated directory')
        rows.append(dict(path=name,bytes=path.stat().st_size,sha256=digest))
    if not any(row['path'].lower().endswith('.fda') for row in rows):
        raise ValueError('RM effect audio set has no samples')
    for row in rows:
        target=generated/'RemasteredAudio'/'sfx'/'etg'/row['path']
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source/row['path'],target)
    destination=generated/'RemasteredAudio'/'sfx'/'etg'
    (destination.parent/'effects.sfxbank').write_bytes(bank)
    (destination/'import.json').write_text(json.dumps(dict(schema='OPENHW_RM_EFFECT_AUDIO_1',files=rows,
        parameter_nodes=parameters),indent=2),encoding='utf-8')
    return len(rows)

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--effects-source',type=Path)
    args=parser.parse_args()
    if args.output: print('Staged RM tracks:',stage(args.source,args.output))
    else: print(json.dumps(inventory(args.source),indent=2))
    if args.effects_source:
        if not args.output:parser.error('--effects-source requires --output')
        print('Staged RM effect audio files:',stage_effects(args.effects_source,args.output))
