"""OpenHW 1.0: read-only Collection import into the installer's own folder."""
import argparse,datetime,hashlib,json,os,shutil,sys,threading,queue,zipfile,subprocess
from pathlib import Path

def root_directory():
    if not getattr(sys,'frozen',False):return Path.cwd().resolve()
    executable=Path(sys.executable).resolve()
    if executable.parent.name=='Installer' and executable.parent.parent.name=='Development':return executable.parents[2]
    return executable.parent

def local_environment(root):
    temporary=root/'Development/Temp';temporary.mkdir(parents=True,exist_ok=True)
    for key in ('TEMP','TMP','TMPDIR'):os.environ[key]=str(temporary)
    os.environ['HW_PORTABLE_ROOT']=str(root)
    os.environ['HW_DEVELOPMENT_ROOT']=str(root/'Development')
    os.environ['PYTHONDONTWRITEBYTECODE']='1'
    if os.name=='nt' and not getattr(subprocess.Popen,'_openhw_hidden',False):
        class HiddenProcess(subprocess.Popen):
            _openhw_hidden=True
            def __init__(self,*args,**kwargs):
                kwargs['creationflags']=kwargs.get('creationflags',0)|subprocess.CREATE_NO_WINDOW
                if kwargs.get('stdout') is None and sys.stdout is not None:
                    try:kwargs['stdout']=sys.stdout.fileno()
                    except (AttributeError,OSError,ValueError):pass
                if kwargs.get('stderr') is None and kwargs.get('stdout') is not None:kwargs['stderr']=subprocess.STDOUT
                super().__init__(*args,**kwargs)
        subprocess.Popen=HiddenProcess

def verify_payload(root):
    manifest=json.loads((root/'Development/release.json').read_text())
    if manifest.get('schema')!=1 or manifest.get('version')!='1.0':raise ValueError('Unrecognized release payload')
    if manifest.get('content_policy')!=2:raise ValueError('Installer predates the reviewed content inventory; rebuild it.')
    seen=set()
    for record in manifest['files']:
        relative=record['path'];path=(root/relative).resolve()
        if path in seen or not path.is_relative_to(root):raise ValueError('Unsafe release payload path')
        seen.add(path)
        if not path.is_file() or path.stat().st_size!=record['size']:raise ValueError('Missing or incomplete installer file: '+relative)
        with path.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').hexdigest()
        if digest!=record['sha256']:raise ValueError('Installer checksum mismatch: '+relative)
    from release_content_policy import verify_profile
    verify_profile(root/'Development')
    return manifest

def install(root,selected):
    from portable_setup import collection_paths,import_sources,sha256
    from portable_convert import convert
    if (root/'setup-complete.json').is_file():raise ValueError('This folder is already installed; no files were changed.')
    collection_paths(selected)  # Reject an invalid selection before changing runtime files.
    manifest=verify_payload(root);dev=root/'Development'
    from release_compact import import_compact
    import_compact(selected,dev)
    # Never overwrite a returning user's settings, saves, authored maps or mods.
    with zipfile.ZipFile(dev/'runtime.zip') as archive:
        for record in archive.infolist():
            relative=Path(record.filename);target=(root/relative).resolve()
            if not target.is_relative_to(root) or relative.is_absolute():raise ValueError('Unsafe runtime archive path')
            if record.is_dir():continue
            preserve=relative.suffix.lower()=='.cfg' or relative.as_posix()=='mods/load-order.txt'
            if preserve and target.exists():continue
            target.parent.mkdir(parents=True,exist_ok=True)
            temporary=target.with_name(target.name+'.setup-pending')
            with archive.open(record) as source, temporary.open('wb') as output:shutil.copyfileobj(source,output)
            temporary.replace(target)
    result=convert(root,dev/'SetupTools',manifest['sky_size'])
    from release_sky_lighting import ensure as ensure_sky_lighting
    ensure_sky_lighting(root,selected=selected)
    from release_mission_authoring import verify_published_missions
    authored=verify_published_missions(dev/'Content/missions',root)
    (dev/'installed-mission-authoring.json').write_text(json.dumps(authored,indent=2))
    result['authoredMaps']=authored['maps'];result['authoredLighting']=authored['lighting']
    result.update(schema=1,status='ready',version='1.0',archive='OpenHW-HW1.obig',sha256=sha256(root/'OpenHW-HW1.obig'))
    pending=root/'setup-complete.pending';pending.write_text(json.dumps(result,indent=2));pending.replace(root/'setup-complete.json')
    shutil.copyfile(dev/'SetupSources/source-receipt.json',dev/'build-source-receipt.json')
    print('Validated game package; removing temporary conversion copies…',flush=True)
    for name in ('SetupWork','SetupSources'):
        disposable=dev/name
        if disposable.resolve().parent!=dev.resolve() or disposable.name not in ('SetupWork','SetupSources'):
            raise ValueError('Refusing unsafe conversion cleanup')
        try:shutil.rmtree(disposable)
        except OSError as error:print('Temporary files retained for manual cleanup:',disposable,error,flush=True)
    print('Setup complete. Start OpenHW.exe when ready.',flush=True)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--collection',type=Path);p.add_argument('--check',action='store_true');p.add_argument('--verify-payload',action='store_true')
    p.add_argument('--apply-hotfix',action='store_true')
    a=p.parse_args();root=root_directory();local_environment(root)
    if a.apply_hotfix:
        from release_patch import run
        run(root)
        return
    if a.verify_payload:
        manifest=verify_payload(root)
        import importlib
        for name in ('portable_convert','release_import','migrate_openhw_ship_documents',
                     'openhw_fx_presentation','openhw_audio_presentation','stage_openhw_subsystems'):
            importlib.import_module(name)
        from release_import import SPECIALS
        for name,_,_,_ in SPECIALS:importlib.import_module('stage_'+name+'_native')
        from native_hod_surfaces import load_reader
        load_reader(root/'Development/SetupTools/remastered_asset_reader.dll')
        import tempfile
        from release_mission_authoring import publish_mission_defaults,verify_published_missions
        with tempfile.TemporaryDirectory(dir=root/'Development/Temp') as folder:
            publish_mission_defaults(root/'Development/Content/missions',Path(folder))
            authored=verify_published_missions(root/'Development/Content/missions',Path(folder))
        (root/'Development/installer-check.json').write_text(json.dumps({'status':'passed','version':manifest['version'],'files':len(manifest['files']),'native_reader':'loaded','special_model_recipes':len(SPECIALS),'authoredMapsAtLoaderPath':authored['maps'],'authoredLightingAtLoaderPath':authored['lighting']},indent=2))
        return
    if a.check:
        from portable_setup import collection_paths
        if not a.collection:p.error('--check requires --collection')
        rm,classic=collection_paths(a.collection);print(json.dumps({'remastered':str(rm),'classic':str(classic),'destination':str(root)}));return
    if a.collection:
        folder=root/'Development/Logs';folder.mkdir(parents=True,exist_ok=True)
        with (folder/'setup-cli.log').open('a',encoding='utf-8',buffering=1) as log:
            sys.stdout=sys.stderr=log;install(root,a.collection)
        return
    import tkinter as tk
    from tkinter import ttk,filedialog,messagebox
    window=tk.Tk();window.withdraw()
    if (root/'setup-complete.json').exists():
        messagebox.showinfo('OpenHW','This folder is already installed. Start OpenHW.exe.\n\nNo settings or saved games have been changed.');window.destroy();return
    selected=filedialog.askdirectory(title='Select your Homeworld Remastered Collection folder',parent=window)
    if not selected:window.destroy();return
    from portable_setup import collection_paths
    try:collection_paths(selected)
    except Exception as error:messagebox.showerror('OpenHW setup',str(error));window.destroy();return
    folder=root/'Development/Logs';folder.mkdir(parents=True,exist_ok=True)
    log=(folder/('setup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'.log')).open('a',encoding='utf-8',buffering=1)
    messages=queue.Queue()
    class Progress:
        def write(self,text):
            log.write(text)
            if text.strip():messages.put(('progress',text.strip()[-400:]))
        def flush(self):log.flush()
        def fileno(self):return log.fileno()
    sys.stdout=sys.stderr=Progress();window.deiconify();window.title('OpenHW 1.0 setup');window.geometry('720x300')
    ttk.Label(window,text='Installing OpenHW 1.0',font=('Segoe UI',17)).pack(pady=18)
    ttk.Label(window,text='Installation folder: '+str(root),wraplength=680).pack()
    status=tk.StringVar(value='Checking installer and importing your Collection files…')
    ttk.Label(window,textvariable=status,wraplength=680).pack(pady=18)
    bar=ttk.Progressbar(window,mode='indeterminate');bar.pack(fill='x',padx=24);bar.start()
    complete=False
    def worker():
        try:install(root,Path(selected));messages.put(('done','Installation complete. Start OpenHW.exe in this folder.'))
        except BaseException as error:
            import traceback
            traceback.print_exc();messages.put(('error',str(error)))
    def poll():
        nonlocal complete
        while not messages.empty():
            kind,value=messages.get();status.set(value)
            if kind in ('done','error'):
                complete=True;bar.stop();log.flush()
                if kind=='error':messagebox.showerror('OpenHW setup failed',value+'\n\nDetails are in Development/Logs. Setup can resume completed conversion steps.')
                ttk.Button(window,text='Close',command=window.destroy).pack(pady=12)
        if not complete:window.after(100,poll)
    window.protocol('WM_DELETE_WINDOW',lambda:window.destroy() if complete else messagebox.showinfo('OpenHW setup','Conversion is still running. Its current step is shown in this window.'))
    threading.Thread(target=worker,daemon=False).start();window.after(100,poll);window.mainloop()

if __name__=='__main__':
    try:main()
    except BaseException as error:
        if isinstance(error,SystemExit):raise
        root=root_directory();folder=root/'Development/Logs';folder.mkdir(parents=True,exist_ok=True)
        import traceback
        with (folder/'setup-error.log').open('a',encoding='utf-8') as log:traceback.print_exc(file=log)
        if not any(x in sys.argv for x in ('--check','--verify-payload','--collection')):
            import tkinter as tk
            from tkinter import messagebox
            w=tk.Tk();w.withdraw();messagebox.showerror('OpenHW setup failed',str(error)+'\n\nSee Development/Logs.');w.destroy()
        raise SystemExit(1)
