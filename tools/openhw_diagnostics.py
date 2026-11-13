"""Player-approved diagnostic reports; no background telemetry or game assets."""
from pathlib import Path
import datetime
import json
import os
import queue
import re
import threading
import urllib.request
import zipfile

ENDPOINT = 'https://openhw.tech/diagnostics/v1/reports'
MAX_UPLOAD = 64 * 1024 * 1024
MAX_EXPANDED = 256 * 1024 * 1024
LOG_SUFFIXES = {'.log', '.txt', '.json', '.dmp'}


def local_file(root, relative):
    root = Path(root).resolve()
    path = root / relative
    if path.is_symlink() or not path.resolve().is_relative_to(root):
        raise ValueError('Linked files outside your OpenHW folder cannot be included.')
    for parent in path.parents:
        if parent == root: break
        if parent.is_symlink() or getattr(parent, 'is_junction', lambda: False)():
            raise ValueError('Linked diagnostic folders cannot be included.')
    if not path.is_file(): raise ValueError('This report file no longer exists: ' + str(relative))
    return path


def inventory(root):
    root = Path(root).resolve()
    logs = []
    folder = root / 'Development/Logs'
    if folder.is_dir():
        for path in folder.iterdir():
            if path.suffix.lower() in LOG_SUFFIXES:
                try:
                    safe = local_file(root, path.relative_to(root))
                    logs.append((safe.stat().st_mtime, path.relative_to(root).as_posix()))
                except (OSError, ValueError): pass
    logs.sort(reverse=True)
    saves = []
    folder = root / 'SavedGames'
    if folder.is_dir():
        for path in folder.rglob('*'):
            try:
                if path.is_file() and not path.name.startswith('.'):
                    local_file(root, path.relative_to(root))
                    saves.append(path.relative_to(root).as_posix())
            except (OSError, ValueError): pass
    return [name for _, name in logs[:200]], sorted(saves)


def build_report(root, alias, comment, logs, save=None):
    """Snapshot only explicitly selected files into a bounded, local archive."""
    root = Path(root).resolve()
    alias = alias.strip()
    if not alias or len(alias) > 80 or any(ord(c) < 32 for c in alias):
        raise ValueError('Enter your modding-community alias (1 to 80 characters).')
    if len(comment) > 4000: raise ValueError('Please keep your comment under 4,000 characters.')
    available, saves = inventory(root)
    if not logs or len(logs) > 60 or len(set(logs)) != len(logs):
        raise ValueError('Select between 1 and 60 log files.')
    if any(name not in available for name in logs) or (save and save not in saves):
        raise ValueError('Choose files from the displayed logs and saves.')
    entries = [('logs/' + Path(name).name, name) for name in logs]
    if save: entries.append(('saves/' + Path(save).name, save))
    expanded = sum(local_file(root, name).stat().st_size for _, name in entries)
    if expanded > MAX_EXPANDED: raise ValueError('Selected files exceed 256 MB; choose fewer logs.')
    target = root / 'Development/Reports'
    target.mkdir(parents=True, exist_ok=True)
    if target.is_symlink() or getattr(target, 'is_junction', lambda: False)():
        raise ValueError('The local reports folder must not be a link.')
    # Unique filenames never interpret the supplied alias as a local path.
    import uuid
    output = target / ('report-' + uuid.uuid4().hex + '.zip')
    metadata = dict(schema=1, alias=alias, comment=comment,
        created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), files=[])
    try:
        version_path=local_file(root,'version.json')
        if version_path.stat().st_size <= 4096:
            value=json.loads(version_path.read_text(encoding='utf-8-sig')).get('version')
            if isinstance(value,str) and len(value)<=32:metadata['game_version']=value
    except (OSError,ValueError):pass
    try:
        with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            total = 0
            for member, relative in entries:
                path = local_file(root, relative)
                # A live log can grow while the main menu is open. Snapshot the
                # observed length; do not include indefinitely appended bytes.
                remaining = path.stat().st_size
                total += remaining
                if total > MAX_EXPANDED: raise ValueError('Files grew too large; select fewer logs.')
                size = 0
                with path.open('rb') as source, archive.open(member, 'w', force_zip64=True) as dest:
                    while remaining:
                        chunk = source.read(min(1024 * 1024, remaining))
                        if not chunk: break
                        dest.write(chunk); remaining -= len(chunk); size += len(chunk)
                        if output.stat().st_size > MAX_UPLOAD:
                            raise ValueError('Report exceeds 64 MB; select fewer files or omit the save.')
                metadata['files'].append(dict(path=member, size=size))
            archive.writestr('report.json', json.dumps(metadata, ensure_ascii=False).encode('utf-8'))
        if output.stat().st_size > MAX_UPLOAD: raise ValueError('Report exceeds 64 MB; select fewer files.')
        return output
    except Exception:
        output.unlink(missing_ok=True)
        raise


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise ValueError('Report upload redirected unexpectedly; no files were sent to another host.')


def upload_report(path):
    path = Path(path)
    if path.stat().st_size > MAX_UPLOAD: raise ValueError('Report exceeds the upload limit.')
    # urllib streams this file with an explicit length, keeping memory bounded.
    with path.open('rb') as source:
        request = urllib.request.Request(ENDPOINT, data=source, method='POST', headers={
            'Content-Type': 'application/zip', 'Content-Length': str(path.stat().st_size),
            'User-Agent': 'OpenHW-Diagnostics/1.1'})
        with urllib.request.build_opener(NoRedirect()).open(request, timeout=120) as response:
            if response.status != 201: raise ValueError('Server did not confirm receipt of your report.')
            result = json.loads(response.read(8193))
    report_id = result.get('report_id', '')
    if not isinstance(report_id, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,160}', report_id):
        raise ValueError('Server returned an invalid report receipt.')
    receipt = path.with_suffix('.receipt.json')
    receipt.write_text(json.dumps(dict(report_id=report_id, endpoint=ENDPOINT), indent=2) + '\n')
    return report_id


def run(root):
    import tkinter as tk
    from tkinter import ttk
    import openhw_launcher_ui as ui
    root = Path(root).resolve()
    logs, saves = inventory(root)
    window = tk.Tk(); window.withdraw(); window.title('OpenHW Diagnostics')
    window.configure(bg=ui.LINE)
    ui.load_fonts(root / 'Development/Launcher/Theme')
    messages = queue.Queue(); busy = False
    status = tk.StringVar(value='Choose logs, add context, then upload to the OpenHW team.')
    def close():
        if not busy: window.destroy()
        else: status.set('Upload in progress. Please wait for the server receipt.')
    chrome = ui.Chrome(window, close, title='OPENHW   /   DIAGNOSTICS')
    body = tk.Frame(window, bg=ui.PANEL); body.pack(fill='both', expand=True, padx=2, pady=(0,2))
    body.columnconfigure(0, weight=1)
    def caption(text, row, color=ui.MUTED):
        ui.label(body, text, size=ui.SMALL, color=color, justify='left', wraplength=860).grid(row=row, column=0, sticky='w', padx=24, pady=(9,3))
    caption('COMMUNITY ALIAS / REPORT NAME', 0, ui.GOLD)
    alias = tk.Entry(body, bg=ui.RAISED, fg=ui.INK, insertbackground=ui.GOLD, font=ui.BODY, relief='flat')
    alias.grid(row=1,column=0,sticky='ew',padx=24,ipady=5)
    caption('Use the alias you go by in the Homeworld / modding community so we can find your report.', 2)
    caption('COMMENT / WHAT HAPPENED, MISSION, SETTINGS AND STEPS TO REPRODUCE', 3, ui.GOLD)
    comment = tk.Text(body,height=4,bg=ui.RAISED,fg=ui.INK,insertbackground=ui.GOLD,font=ui.SMALL,relief='flat',wrap='word')
    comment.grid(row=4,column=0,sticky='ew',padx=24)
    caption('LOG FILES / newest first; use Ctrl or Shift to select multiple', 5, ui.GOLD)
    frame=tk.Frame(body,bg=ui.PANEL);frame.grid(row=6,column=0,sticky='nsew',padx=24)
    body.rowconfigure(6,weight=1)
    listing=tk.Listbox(frame,selectmode='extended',exportselection=False,height=7,bg=ui.RAISED,
        fg=ui.INK,selectbackground=ui.LINE,selectforeground=ui.GOLD,font=ui.SMALL,relief='flat',highlightthickness=0)
    listing.pack(side='left',fill='both',expand=True)
    scroll=tk.Scrollbar(frame,command=listing.yview);scroll.pack(side='right',fill='y');listing.configure(yscrollcommand=scroll.set)
    for name in logs:
        listing.insert('end',name + '  (' + str(round((root/name).stat().st_size/1024)) + ' KB)')
    # Default to the newest few logs; every included file remains visible.
    if logs: listing.selection_set(0,min(4,len(logs)-1))
    caption('OPTIONAL SAVED GAME / choose the save that reproduces the issue', 7, ui.GOLD)
    selected_save=tk.StringVar(value='Do not include a save')
    style=ttk.Style();style.theme_use('clam')
    style.configure('Report.TCombobox',fieldbackground=ui.RAISED,background=ui.LINE,foreground=ui.INK,font=ui.SMALL)
    dropdown=ttk.Combobox(body,textvariable=selected_save,values=['Do not include a save']+saves,
        state='readonly',style='Report.TCombobox',font=ui.SMALL)
    dropdown.grid(row=8,column=0,sticky='ew',padx=24)
    caption('Only selected logs, your optional save, alias and comment are sent. Logs may contain local paths.\nReports are private to the OpenHW team. No upload happens until you click UPLOAD REPORT.', 9)
    state=ui.label(body,'',size=ui.SMALL,color=ui.CYAN,wraplength=880)
    state.configure(textvariable=status);state.grid(row=10,column=0,sticky='ew',padx=24,pady=8)
    def submit():
        nonlocal busy
        if busy:return
        names=[logs[i] for i in listing.curselection()]
        chosen=selected_save.get(); chosen=chosen if chosen in saves else None
        identity=alias.get(); text=comment.get('1.0','end-1c')
        busy=True; status.set('Preparing and uploading your selected files...')
        def work():
            try:
                archive=build_report(root,identity,text,names,chosen)
                receipt=upload_report(archive)
                messages.put((True,'Uploaded. Report ID: '+receipt+'. Share this ID when reporting your issue.'))
            except Exception as error:messages.put((False,'Upload failed: '+str(error)))
        threading.Thread(target=work,daemon=True).start()
    buttons=tk.Frame(body,bg=ui.PANEL);buttons.grid(row=11,column=0,sticky='ew',padx=24,pady=(4,18))
    ui.Button(buttons,'BACK',close,width=140).pack(side='left')
    ui.Button(buttons,'UPLOAD REPORT',submit,primary=True,width=240).pack(side='right')
    def poll():
        nonlocal busy
        try:
            success,text=messages.get_nowait();busy=False;status.set(text)
        except queue.Empty:pass
        window.after(100,poll)
    width=min(960,window.winfo_screenwidth()-60);height=min(830,window.winfo_screenheight()-80)
    window.geometry(f'{width}x{height}+{(window.winfo_screenwidth()-width)//2}+{(window.winfo_screenheight()-height)//2}')
    window.deiconify();window.update_idletasks();chrome.taskbar();window.lift();window.focus_force();window.after(100,poll)
    window.mainloop()
