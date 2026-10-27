"""Installed-game patch UI; game executables are replaced by the native bootstrap."""
from pathlib import Path
import datetime,hashlib,json,queue,sys,threading

def run(root):
    import tkinter as tk
    from tkinter import ttk,filedialog,messagebox
    from release_sky_lighting import ensure,valid_pair
    root=Path(root).resolve()
    if not (root/'setup-complete.json').is_file():raise ValueError('Select an installed OpenHW 1.0 folder')
    log_dir=root/'Development/Logs';log_dir.mkdir(parents=True,exist_ok=True)
    log=(log_dir/'patch-1.0.2.log').open('a',encoding='utf-8',buffering=1)
    window=tk.Tk();window.title('OpenHW 1.0.2 patch');window.geometry('680x240')
    ttk.Label(window,text='Updating OpenHW to 1.0.2',font=('Segoe UI',17)).pack(pady=16)
    status=tk.StringVar(value='Checking the gameplay, skirmish and multiplayer update...')
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
    receipt=root/'Development/build-source-receipt.json'
    info=json.loads(receipt.read_text()) if receipt.is_file() else {}
    if not all(valid_pair(root/'Data/Missions',m) for m in range(1,17)) and not (Path(info.get('classic',''))/'Homeworld.big').is_file():
        selected=filedialog.askdirectory(title='Select your Homeworld Remastered Collection folder',parent=window)
        if not selected:window.destroy();return
    def worker():
        try:
            manifest=json.loads((root/'Development/Patches/1.0.2/patch.json').read_text())
            for row in manifest['files']:
                p=(root/row['path']).resolve()
                if not p.is_relative_to(root) or not p.is_file():raise ValueError('Missing patch file: '+row['path'])
                with p.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
                if digest!=row['sha256']:raise ValueError('Incomplete patch file: '+row['path'])
            ensure(root,selected=selected)
            version=root/'version.json';pending=root/'version.pending';data=json.loads(version.read_text())
            data.update(version='1.0.2',previous=data.get('version','1.0'));pending.write_text(json.dumps(data,indent=2));pending.replace(version)
            (root/'Development/Patches/1.0.2/applied.json').write_text(json.dumps(dict(version='1.0.2',applied=datetime.datetime.now().isoformat()),indent=2))
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
                if kind=='error':messagebox.showerror('OpenHW patch',text+'\nSee Development/Logs/patch-1.0.2.log',parent=window)
                ttk.Button(window,text='Close',command=window.destroy).pack(pady=12)
        if not finished:window.after(100,poll)
    window.protocol('WM_DELETE_WINDOW',lambda:window.destroy() if finished else messagebox.showinfo('OpenHW patch','The patch is still running.',parent=window))
    threading.Thread(target=worker,daemon=False).start();window.after(100,poll);window.mainloop()
