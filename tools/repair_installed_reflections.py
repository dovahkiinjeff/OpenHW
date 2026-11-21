"""Restore native reflection controls from this player's installed HOD archive."""
from pathlib import Path
import contextlib
import hashlib
import json
import os
import struct
import tempfile
import zlib
from stage_archive_reflections import stage
from native_hod_surfaces import load_reader

def repair(root):
    root=Path(root).resolve()
    archive=root/'OpenHW-HW1.obig'
    if not (root/'setup-complete.json').is_file() or not archive.is_file():
        raise ValueError('An installed OpenHW game is required')
    temporary=root/'Development/Temp'
    temporary.mkdir(parents=True,exist_ok=True)
    logs=root/'Development/Logs'
    logs.mkdir(parents=True,exist_ok=True)
    with (logs/'native-material-repair.log').open('a',encoding='utf-8',buffering=1) as log, contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
        with archive.open('rb') as f:
            magic,version,count,end=struct.unpack('<8sIIQ',f.read(24))
            if magic!=b'OBIGPK01' or version!=1 or not 0<count<=1_000_000 or end!=24+count*272:
                raise ValueError('Invalid installed archive')
            entries={name.rstrip(bytes(1)).decode():(size,crc) for name,offset,size,crc in struct.iter_unpack('<256sQII',f.read(end-24))}
        updated=[];preserved=[]
        with tempfile.TemporaryDirectory(prefix='native-reflections-',dir=temporary) as temp:
            output=Path(temp).resolve()
            if not output.is_relative_to(temporary.resolve()):
                raise ValueError('Invalid material staging folder')
            committed=[]
            def install(key,data):
                destination=root/key
                if not destination.resolve().is_relative_to(root):
                    raise ValueError('Invalid material output path')
                old=None
                if destination.exists():
                    current=destination.read_bytes()
                    if current==data:
                        return
                    if (len(current),zlib.crc32(current))!=entries[key]:
                        preserved.append(key)
                        return
                    old=output/'backup'/key
                    old.parent.mkdir(parents=True,exist_ok=True)
                    old.write_bytes(current)
                destination.parent.mkdir(parents=True,exist_ok=True)
                pending=output/'material.pending'
                pending.write_bytes(data)
                os.replace(pending,destination)
                committed.append((destination,old,hashlib.sha256(data).hexdigest()))
                updated.append(key)
            try:
                stage(archive,output,load_reader(root/'Development/SetupTools/remastered_asset_reader.dll'),writer=install)
            except BaseException:
                for destination,old,digest in reversed(committed):
                    if destination.is_file() and hashlib.sha256(destination.read_bytes()).hexdigest()==digest:
                        if old is None:
                            destination.unlink()
                        else:
                            os.replace(old,destination)
                raise
        receipt=root/'Development/Patches/1.1.14/material-reflections.json'
        receipt.parent.mkdir(parents=True,exist_ok=True)
        pending=receipt.with_suffix('.pending')
        pending.write_text(json.dumps(dict(updated=updated,preserved=preserved),indent=2),encoding='utf-8')
        os.replace(pending,receipt)
