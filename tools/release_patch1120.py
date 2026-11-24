"""Installed-game patch UI; game executables are replaced by the native bootstrap."""
from pathlib import Path
import datetime,hashlib,json,queue,sys,threading

def run(root):
    import tkinter as tk
    from tkinter import ttk,filedialog,messagebox
    from release_sky_lighting import ensure,valid_pair
    root=Path(root).resolve()
    if not (root/'setup-complete.json').is_file():raise ValueError('Select an installed OpenHW 1.1.19 folder')
    log_dir=root/'Development/Logs';log_dir.mkdir(parents=True,exist_ok=True)
    log=(log_dir/'patch-1.1.20.log').open('a',encoding='utf-8',buffering=1)
    window=tk.Tk();window.title('OpenHW 1.1.20 patch');window.geometry('680x240')
    ttk.Label(window,text='Updating OpenHW to 1.1.20',font=('Segoe UI',17)).pack(pady=16)
    status=tk.StringVar(value='Checking the update...')
    ttk.Label(window,textvariable=status,wraplength=630).pack(pady=12)
    bar=ttk.Progressbar(window,mode='indeterminate');bar.pack(fill='x',padx=24);bar.start()
    messages=queue.Queue();finished=False
    class Progress:
        def write(self,text):
            log.write(text)
            if text.strip():messages.put(('progress',text.strip()[-400:]))
        def flush(self):log.flush()
        def fileno(self):return log.fileno()
    sys.stdout=sys.stderr=Progress();selected=None
    def worker():
        try:
            manifest=json.loads((root/'Development/Patches/1.1.20/patch.json').read_text())
            for row in manifest['files']:
                p=(root/row['path']).resolve()
                if not p.is_relative_to(root) or not p.is_file():raise ValueError('Missing patch file: '+row['path'])
                with p.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
                if digest!=row['sha256']:raise ValueError('Incomplete patch file: '+row['path'])
            # Art and game files have already been verified above.
            version=root/'version.json';pending=root/'version.pending';data=json.loads(version.read_text())
            hashes={row['path']:row['sha256'] for row in manifest['files']}
            for field in ('local_build','base_release','release_hashes_before_local_candidate','release_hashes_before_local_fix'):data.pop(field,None)
            if 'OpenHW.exe' in hashes:data['normal']=hashes['OpenHW.exe']
            if 'OpenHW-Daignostics.exe' in hashes:data['diagnostic']=hashes['OpenHW-Daignostics.exe']
            data.update(version='1.1.20',previous=data.get('version','1.0'));pending.write_text(json.dumps(data,indent=2));pending.replace(version)
            (root/'Development/Patches/1.1.20/applied.json').write_text(json.dumps(dict(version='1.1.20',applied=datetime.datetime.now().isoformat()),indent=2))
            messages.put(('done','Patch complete. Start OpenHW.exe when ready.'))
        except Exception as error:
            import traceback
            traceback.print_exc();messages.put(('error',str(error)))
    def poll():
        nonlocal finished
        while not messages.empty():
            kind,text=messages.get();status.set(text)
            if kind in ('done','error'):
                finished=True;bar.stop();log.flush()
                if kind=='error':messagebox.showerror('OpenHW patch',text+'\nSee Development/Logs/patch-1.1.20.log',parent=window)
                ttk.Button(window,text='Close',command=window.destroy).pack(pady=12)
        if not finished:window.after(100,poll)
    window.protocol('WM_DELETE_WINDOW',lambda:window.destroy() if finished else messagebox.showinfo('OpenHW patch','The patch is still running.',parent=window))
    threading.Thread(target=worker,daemon=False).start();window.after(100,poll);window.mainloop()


def restore_campaign_defaults(root):
    import re,shutil
    from openhw_updater import inside,digest,atomic_json
    root=Path(root).resolve()
    bundle=inside(root,'Development/Content/campaign-defaults-recovery')
    manifest=json.loads((bundle/'repair.json').read_text(encoding='utf-8'))
    restored=[];preserved=[]
    for row in manifest['files']:
        name=row['path']
        if not re.fullmatch(r'(?:Data/Missions|missions)/mission(?:0[1-9]|1[0-6])\.rtxmap',name):
            raise ValueError('Invalid campaign recovery target')
        source=inside(root,'Development/Content/campaign-defaults-recovery/'+row['source'])
        if digest(source)!=row['original_sha256']:raise ValueError('Campaign recovery data failed verification')
        target=inside(root,name)
        if not target.is_file():continue
        current=digest(target)
        if current==row['original_sha256']:continue
        if current!=row['accidental_sha256']:
            preserved.append(name);continue
        backup=inside(root,'Development/Patches/1.1.20/CampaignRecovery/'+name)
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():shutil.copyfile(target,backup)
        pending=inside(root,name+'.campaign-recovery-pending')
        shutil.copyfile(source,pending);pending.replace(target);restored.append(name)
    atomic_json(inside(root,'Development/Logs/campaign-defaults-recovery.json'),
        dict(restored=restored,preserved_custom_files=preserved))
