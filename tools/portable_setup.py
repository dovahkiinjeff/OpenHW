"""OpenHW development setup: local Collection import, never redistributable assets.

This module stages source data only. A completed source receipt is deliberately
not a runtime-ready marker: conversion and runtime validation must follow.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import uuid
import sys
import datetime

from extract_remastered_big import extract

# Overlay order is explicit. Shared engine content precedes HW1-specific data.
RM_ARCHIVES = ('Homeworld2.big', 'HW1Ships.big', 'HW1Campaign.big',
               'HWBackgrounds.big', 'Music.big', 'MusicHW1Campaign.big',
               'English.big', 'EnglishHW1Campaign.big',
               'EnglishSpeech.big', 'EnglishSpeechHW1Campaign.big')
CLASSIC_FILES = ('Homeworld.big', 'Update.big', 'HW_Comp.vce', 'HW_Music.wxd')


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def collection_paths(selected):
    selected = Path(selected).resolve(strict=True)
    candidates = (selected, selected/'Data', selected/'HomeworldRM'/'Data')
    rm = next((p for p in candidates if (p/'HW1Ships.big').is_file()), None)
    if rm is None:
        raise ValueError('Select the Remastered Collection or HomeworldRM/Data folder')
    classic = rm.parent.parent/'Homeworld1Classic'
    if not (classic/'Homeworld.big').is_file() and (classic/'Data'/'Homeworld.big').is_file():
        classic = classic/'Data'
    if not (classic/'Homeworld.big').is_file():
        raise ValueError('Homeworld1Classic/Homeworld.big is required from the same Collection installation')
    missing = [n for n in RM_ARCHIVES if not (rm/n).is_file()]
    if missing:
        raise ValueError('Missing required Remastered archives: '+', '.join(missing))
    return rm, classic


def import_sources(selected, destination):
    rm, classic = collection_paths(selected)
    destination = Path(destination).resolve()
    collection = rm.parent.parent.resolve()
    if destination == collection or destination.is_relative_to(collection) or collection.is_relative_to(destination):
        raise ValueError('Fresh test destination must be separate from the original Collection')
    source_root = destination/'SetupSources'
    if source_root.exists():
        raise ValueError('SetupSources already exists; refusing to overwrite an earlier import')
    destination.mkdir(parents=True, exist_ok=True)
    stage = destination/('setup-pending-'+uuid.uuid4().hex)
    stage.mkdir()
    records = []
    try:
        data = stage/'Classic'
        data.mkdir()
        for name in CLASSIC_FILES:
            source = classic/name
            if not source.is_file():
                continue
            target = data/name
            before = sha256(source)
            shutil.copyfile(source, target)
            if sha256(target) != before or sha256(source) != before:
                raise ValueError('Classic source changed during import: '+name)
            records.append(dict(archive=name, sha256=before, output='Classic/'+name))
        if (classic/'Movies').is_dir():
            shutil.copytree(classic/'Movies', data/'Movies')
        for name in RM_ARCHIVES:
            source = rm/name
            print('Extracting '+name, flush=True)
            before = sha256(source)
            outputs = extract(source, stage/'Remastered', ('',))
            if sha256(source) != before:
                raise ValueError('Remastered archive changed during import: '+name)
            records.append(dict(archive=name, sha256=before, files=len(outputs)))
            if name in ('Homeworld2.big', 'HW1Ships.big', 'HWBackgrounds.big'):
                archive_copy = stage/'Archives'/name
                archive_copy.parent.mkdir(exist_ok=True)
                shutil.copyfile(source, archive_copy)
                if sha256(archive_copy) != before:
                    raise ValueError('Archive copy failed: '+name)
        receipt = dict(schema=1, status='sources-imported-not-runtime-ready',
                       archives=records, outputs=[dict(path=p.relative_to(stage).as_posix(),
                       size=p.stat().st_size, sha256=sha256(p))
                       for p in sorted(stage.rglob('*')) if p.is_file()])
        (stage/'source-receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        os.replace(stage, source_root)
        return source_root
    except BaseException:
        # Retain failed output and logs for diagnosis; never activate partial data.
        print('Import incomplete; diagnostic staging retained at '+str(stage), flush=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--collection', type=Path)
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--import-only', action='store_true')
    parser.add_argument('--sky-size', type=int, choices=(512,1024,2048), default=1024)
    parser.add_argument('--apply-update', type=Path)
    parser.add_argument('--rollback-update', action='store_true')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--refresh-subsystems',action='store_true')
    args = parser.parse_args()
    graphical = not any((args.collection,args.apply_update,args.rollback_update,args.check,args.verify,args.refresh_subsystems))
    root = args.destination.resolve() if args.destination else Path(sys.executable).parent if getattr(sys,'frozen',False) else Path.cwd()
    if args.refresh_subsystems:
        from stage_openhw_subsystems import build
        from native_hod_surfaces import load_reader
        build(root/'SetupSources'/'Remastered',load_reader(root/'SetupTools'/'remastered_asset_reader.dll'),root/'Subsystems')
        return
    if args.apply_update or args.rollback_update:
        from openhw_update import apply, rollback
        print(rollback(root) if args.rollback_update else apply(root,args.apply_update))
        return
    if args.verify:
        from openhw_obig import validate
        print('Verified',len(validate(root/'OpenHW-HW1.obig',True)),'archive entries')
        return
    if not args.collection and not args.check:
        import tkinter as tk
        from tkinter import filedialog, messagebox
        window=tk.Tk();window.withdraw()
        if (root/'setup-complete.json').is_file():
            selected=messagebox.askyesnocancel('OpenHW development setup',
                'OpenHW is installed here.\n\nYes: apply an update package\nNo: verify the installed game package\nCancel: close setup')
            if selected is None:return
            if selected:
                update=filedialog.askopenfilename(title='Select OpenHW update',filetypes=[('OpenHW update','*.oupdate'),('ZIP package','*.zip')])
                if update:
                    from openhw_update import apply
                    apply(root,Path(update));messagebox.showinfo('OpenHW','Update installed. You can start OpenHW.')
            else:
                from openhw_obig import validate
                validate(root/'OpenHW-HW1.obig',True);messagebox.showinfo('OpenHW','Installed package verified.')
            return
        args.collection=Path(filedialog.askdirectory(title='Select your Homeworld Remastered Collection installation'))
        if str(args.collection)=='.':return
        window.destroy()
    if args.check:
        if not args.collection:parser.error('--check requires --collection')
        rm, classic = collection_paths(args.collection)
        print(json.dumps(dict(remastered=str(rm), classic=str(classic))))
    else:
        logs=root/'Logs';logs.mkdir(parents=True,exist_ok=True)
        log=(logs/('setup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'.log')).open('a',encoding='utf-8',buffering=1)
        def perform():
            if not (root/'SetupSources'/'source-receipt.json').is_file():
                import_sources(args.collection, root)
            if not args.import_only:
                from portable_convert import convert
                result=convert(root,root/'SetupTools',args.sky_size)
                result.update(schema=1,status='ready',archive='OpenHW-HW1.obig',sha256=sha256(root/'OpenHW-HW1.obig'))
                pending=root/'setup-complete.pending';pending.write_text(json.dumps(result,indent=2));os.replace(pending,root/'setup-complete.json')
                print('Setup completed. Start OpenHW.exe when ready.',flush=True)
        if not graphical:
            sys.stdout=sys.stderr=log
            perform()
        else:
            import queue
            import threading
            import tkinter as tk
            from tkinter import ttk,messagebox
            messages=queue.Queue()
            class ProgressLog:
                def write(self,value):
                    log.write(value)
                    if value.strip():messages.put(('progress',value.strip()[-300:]))
                def flush(self):log.flush()
            sys.stdout=sys.stderr=ProgressLog()
            window=tk.Tk();window.title('OpenHW development setup');window.geometry('640x260')
            status=tk.StringVar(value='Importing your Collection files…')
            ttk.Label(window,text='Preparing OpenHW in this folder',font=('Segoe UI',15)).pack(pady=20)
            ttk.Label(window,text=str(root),wraplength=600).pack()
            ttk.Label(window,textvariable=status,wraplength=600).pack(pady=15)
            progress=ttk.Progressbar(window,mode='indeterminate');progress.pack(fill='x',padx=25);progress.start()
            complete=False
            def worker():
                try:perform();messages.put(('done','Setup complete. Start OpenHW.exe in this folder.'))
                except BaseException as error:
                    import traceback
                    traceback.print_exc();messages.put(('error',str(error)))
            def poll():
                nonlocal complete
                while not messages.empty():
                    kind,value=messages.get();status.set(value)
                    if kind in ('done','error'):
                        complete=True;progress.stop()
                        if kind=='error':messagebox.showerror('OpenHW setup failed',value+'\n\nSee Logs in this folder. Re-running setup can resume completed steps.')
                        ttk.Button(window,text='Close',command=window.destroy).pack(pady=12)
                if not complete:window.after(150,poll)
            window.protocol('WM_DELETE_WINDOW',lambda:window.destroy() if complete else messagebox.showinfo('OpenHW','Setup is still running. This window will show when it has finished.'))
            threading.Thread(target=worker,daemon=False).start();window.after(150,poll);window.mainloop()


if __name__ == '__main__':
    try:
        main()
    except BaseException as error:
        if isinstance(error,SystemExit):raise
        import traceback
        traceback.print_exc()
        if getattr(sys,'frozen',False):
            import tkinter as tk
            from tkinter import messagebox
            window=tk.Tk();window.withdraw();messagebox.showerror('OpenHW setup failed',str(error)+'\n\nDetails are in this folder’s Logs directory.');window.destroy()
        raise
