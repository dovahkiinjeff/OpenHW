"""Transactional consolidated installation; never copy an installed game archive."""
from pathlib import Path
import contextlib
import datetime
import hashlib
import json
import os
import shutil
import tempfile
import zipfile
import zlib

VERSION='1.1.15'


def atomic_json(path, value):
    path.parent.mkdir(parents=True,exist_ok=True)
    pending=path.with_suffix('.pending')
    pending.write_text(json.dumps(value,indent=2),encoding='utf-8')
    os.replace(pending,path)


def compatible(root):
    """Only the reviewed compact 1.1-series archive can be reused."""
    try:
        receipt=json.loads((root/'setup-complete.json').read_text())
        version=json.loads((root/'version.json').read_text()).get('version',receipt.get('version',''))
        if not str(version).startswith('1.1'):return False
        from openhw_obig import validate
        entries=validate(root/'OpenHW-HW1.obig')
        return all(k in entries for k in ('r1/lightinterceptor/rmnative/hull.hod','r1/lightinterceptor/rmnative/ship.owshipdef'))
    except (OSError,ValueError,KeyError):return False


def find_collection(root):
    from portable_setup import collection_paths
    for name in ('Development/build-source-receipt.json','Development/SetupSources/source-receipt.json'):
        try:
            receipt=json.loads((root/name).read_text())
            for key in ('remastered','collection','source'):
                if receipt.get(key):
                    try:
                        path=Path(receipt[key]);collection_paths(path);return path
                    except (OSError,ValueError):pass
        except (OSError,ValueError):pass
    return None


def destination(root):
    from openhw_updater import inside,compare_versions
    root=Path(root).resolve()
    if root.parent==root:raise ValueError('Choose a dedicated writable OpenHW folder')
    root.mkdir(parents=True,exist_ok=True)
    for name in ('OpenHW.exe','version.json','setup-complete.json','Development'):
        inside(root,name)
    if (root/'version.json').is_file():
        version=json.loads((root/'version.json').read_text()).get('version','0')
        if compare_versions(version,VERSION)>0:raise ValueError('This installer cannot downgrade a newer OpenHW installation')
    if not (root/'setup-complete.json').exists() and not (root/'OpenHW.exe').exists():
        # Downloaded installer and its own staging directory are permitted.
        unexpected=[p for p in root.iterdir() if p.name!='Development' and p.suffix.lower()!='.exe']
        if unexpected:raise ValueError('Choose an existing OpenHW installation or a new dedicated folder')
    return root


def stage_reflections(root, staging):
    from openhw_obig import validate
    from stage_archive_reflections import stage
    from native_hod_surfaces import load_reader
    entries=validate(root/'OpenHW-HW1.obig')
    def write(key,data):
        from openhw_updater import inside
        current=inside(root,key)
        if current.is_file():
            old=current.read_bytes()
            if old==data:return
            # Keep custom loose overrides; replace only original archive-equivalent pages.
            offset,size,crc=entries[key]
            if len(old)!=size or zlib.crc32(old)!=crc:return
        target=inside(staging,key);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    stage(root/'OpenHW-HW1.obig',staging,load_reader(staging/'Development/SetupTools/remastered_asset_reader.dll'),writer=write)


def recover(root):
    from openhw_updater import inside
    folder=inside(root,'Development/FullInstall')
    for journal in sorted(folder.glob('*/transaction.json')):
        state=json.loads(journal.read_text())
        if state['state'] in ('complete','recovered'):continue
        for row in reversed(state['files']):
            target=inside(root,row['path']);backup=inside(journal.parent,'backup/'+row['path'])
            target.with_name(target.name+'.full-install-pending').unlink(missing_ok=True)
            if backup.exists():os.replace(backup,target)
            elif not row['existed']:target.unlink(missing_ok=True)
        state['state']='recovered';atomic_json(journal,state)


def commit(root, staging, progress=print):
    from openhw_updater import inside
    import uuid
    folder=inside(root,'Development/FullInstall/'+uuid.uuid4().hex)
    folder.mkdir(parents=True)
    journal=folder/'transaction.json';state=dict(schema=1,state='applying',version=VERSION,files=[])
    files=[]
    for p in staging.rglob('*'):
        if p.is_symlink():raise ValueError('Unsafe installation source')
        if not p.is_file():continue
        key=p.relative_to(staging).as_posix()
        if key.startswith(('Development/SetupWork/','Development/SetupSources/','Development/Temp/','Development/Logs/','Development/FullInstall/')):continue
        target=inside(root,key)
        if target.exists() and (p.suffix.lower()=='.cfg' or key=='mods/load-order.txt'):continue
        if key in ('version.json','setup-complete.json'):continue
        files.append((key,p))
    # Completion receipts are the last changes, after all runtime/data files.
    for key in ('setup-complete.json','version.json'):
        if (staging/key).exists():files.append((key,staging/key))
    atomic_json(journal,state)
    try:
        for index,(key,source) in enumerate(files):
            target=inside(root,key);target.parent.mkdir(parents=True,exist_ok=True)
            old=target.is_file();backup=inside(folder,'backup/'+key)
            if old:
                backup.parent.mkdir(parents=True,exist_ok=True)
                # Same-volume hardlinks retain recovery without duplicating large archives.
                try:os.link(target,backup)
                except OSError:
                    if key=='OpenHW-HW1.obig':raise ValueError('A same-volume recovery link is required for replacing this large archive')
                    shutil.copy2(target,backup)
            state['files'].append(dict(path=key,existed=old));atomic_json(journal,state)
            pending=target.with_name(target.name+'.full-install-pending')
            if key=='OpenHW-HW1.obig':os.replace(source,pending)
            else:shutil.copy2(source,pending)
            os.replace(pending,target)
            if index%25==0:progress('Installing program and data files: '+str(index+1)+' / '+str(len(files)))
        state['state']='complete';atomic_json(journal,state)
    except BaseException:
        recover(root)
        raise
    # Retain small program-file recovery, release the prior archive's recovery link.
    (folder/'backup/OpenHW-HW1.obig').unlink(missing_ok=True)
    return len(files)


def install(root, staging, collection=None, progress=print):
    from openhw_updater import stop_if_game_running,wait_for_install_launcher,inside,update_lock
    root=destination(root);staging=Path(staging).resolve()
    if root==staging or not staging.is_relative_to(root/'Development/Temp'):raise ValueError('Full setup must use its private staging folder inside the selected game folder')
    stop_if_game_running()
    wait_for_install_launcher(root,progress)
    from release_setup import verify_payload
    manifest=verify_payload(staging)
    if manifest['version']!=VERSION:raise ValueError('Wrong full release payload')
    reuse=compatible(root)
    collection=Path(collection) if collection else find_collection(root)
    if not reuse:
        if not collection:raise ValueError('Select your owned Homeworld Remastered Collection to rebuild this older installation')
        from portable_setup import collection_paths
        collection_paths(collection)
        if root.is_relative_to(collection.resolve()) or collection.resolve().is_relative_to(root):raise ValueError('OpenHW must be separate from the owned Collection')
    with update_lock(root):
        recover(root)
        progress('Installing the complete OpenHW '+VERSION+' release...')
        with zipfile.ZipFile(staging/'Development/runtime.zip') as archive:
            for row in archive.infolist():
                if row.is_dir():continue
                target=inside(staging,row.filename)
                target.parent.mkdir(parents=True,exist_ok=True)
                with archive.open(row) as source,target.open('wb') as out:shutil.copyfileobj(source,out)
        if reuse:
            progress('Reusing the installed game archive; updating native material controls...')
            stage_reflections(root,staging)
            receipt=json.loads((root/'setup-complete.json').read_text())
        else:
            from release_compact import import_compact
            from portable_convert import convert
            progress('Importing the Collection with the compact installer...')
            import_compact(collection,staging/'Development')
            receipt=convert(staging,staging/'Development/SetupTools',manifest['sky_size'])
            shutil.copy2(staging/'Development/SetupSources/source-receipt.json',staging/'Development/build-source-receipt.json')
        from release_mission_authoring import publish_mission_defaults,verify_published_missions
        publish_mission_defaults(staging/'Development/Content/missions',staging)
        verify_published_missions(staging/'Development/Content/missions',staging)
        # Newly supplied UI art must also reach older existing installations.
        ui=staging/'Development/Content/ui'
        if ui.exists():shutil.copytree(ui,staging/'Data/UI',dirs_exist_ok=True)
        receipt.update(schema=1,status='ready',version=VERSION,archive='OpenHW-HW1.obig')
        atomic_json(staging/'setup-complete.json',receipt)
        count=commit(root,staging,progress)
        result=dict(version=VERSION,archive_reused=reuse,archive_copied=False,files=count,completed=datetime.datetime.now(datetime.timezone.utc).isoformat())
        atomic_json(root/'Development/FullInstall/last-install.json',result)
        progress('OpenHW '+VERSION+' is installed. Start Launcher when ready.')
        return result


def run(root,staging,collection=None,headless=False):
    root=Path(root);root.mkdir(parents=True,exist_ok=True)
    logs=root/'Development/Logs';logs.mkdir(parents=True,exist_ok=True)
    with (logs/'full-install-1.1.15.log').open('a',encoding='utf-8',buffering=1) as log,contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
        if headless:
            install(root,staging,collection)
            return
        import tkinter as tk
        from tkinter import ttk,filedialog,messagebox
        import queue,threading,traceback
        window=tk.Tk();window.withdraw()
        if not compatible(root) and not collection:
            collection=find_collection(root) or filedialog.askdirectory(parent=window,title='Select your owned Homeworld Remastered Collection')
            if not collection:window.destroy();raise ValueError('Installation cancelled; the existing game was preserved')
        window.deiconify();window.title('OpenHW '+VERSION+' full installation');window.geometry('740x285')
        ttk.Label(window,text='OpenHW '+VERSION+' — Complete release',font=('Segoe UI',17)).pack(pady=18)
        ttk.Label(window,text=str(root),wraplength=690).pack()
        status=tk.StringVar(value='Preparing the full installation...')
        ttk.Label(window,textvariable=status,wraplength=690).pack(pady=20)
        bar=ttk.Progressbar(window,mode='indeterminate');bar.pack(fill='x',padx=25);bar.start()
        messages=queue.Queue();done=False;success=False
        def progress(text):print(text,flush=True);messages.put(('progress',text))
        def worker():
            try:install(root,staging,collection,progress);messages.put(('done','OpenHW '+VERSION+' is ready.'))
            except BaseException as error:traceback.print_exc();messages.put(('error',str(error)))
        def poll():
            nonlocal done,success
            while not messages.empty():
                kind,text=messages.get();status.set(text)
                if kind in ('done','error'):
                    done=True;success=kind=='done';bar.stop()
                    if not success:messagebox.showerror('OpenHW full installation',text,parent=window)
                    ttk.Button(window,text='Close',command=window.destroy).pack(pady=10)
            if not done:window.after(100,poll)
        window.protocol('WM_DELETE_WINDOW',lambda:window.destroy() if done else None)
        threading.Thread(target=worker,daemon=False).start();window.after(100,poll);window.mainloop()
        if not success:raise ValueError('Full installation did not complete; see Development/Logs/full-install-1.1.15.log')
