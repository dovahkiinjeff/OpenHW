"""Player-approved diagnostic reports; no background telemetry or game assets."""
from pathlib import Path
import datetime
import hashlib
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
        for parent, directories, files in os.walk(folder, followlinks=False):
            directories[:] = [name for name in directories if not (Path(parent)/name).is_symlink()
                and not getattr(Path(parent)/name, 'is_junction', lambda: False)()]
            for name in files:
                path = Path(parent)/name
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
    return [name for _, name in logs], sorted(saves)


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


def upload_report(path, profile=None):
    path = Path(path)
    if path.stat().st_size > MAX_UPLOAD: raise ValueError('Report exceeds the upload limit.')
    # urllib streams this file with an explicit length, keeping memory bounded.
    with path.open('rb') as source:
        request = urllib.request.Request(ENDPOINT, data=source, method='POST', headers={
            'Content-Type': 'application/zip', 'Content-Length': str(path.stat().st_size),
            'User-Agent': 'OpenHW-Diagnostics/1.1',
            **({'Authorization': 'Bearer ' + profile['upload_token']} if profile else {})})
        with urllib.request.build_opener(NoRedirect()).open(request, timeout=120) as response:
            if response.status != 201: raise ValueError('Server did not confirm receipt of your report.')
            result = json.loads(response.read(8193))
    report_id = result.get('report_id', '')
    if not isinstance(report_id, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,160}', report_id):
        raise ValueError('Server returned an invalid report receipt.')
    if profile and result.get('profile_id') != profile['profile_id']:
        raise ValueError('Server did not confirm the selected diagnostics folder.')
    receipt = path.with_suffix('.receipt.json')
    receipt.write_text(json.dumps(dict(report_id=report_id, endpoint=ENDPOINT), indent=2) + '\n')
    return report_id


CHUNK_BYTES = 32 * 1024 * 1024


def state_path(root):
    root = Path(root).resolve()
    folder = root / 'Development/Reports'
    for path in (folder, folder.parent):
        if path.is_symlink() or getattr(path, 'is_junction', lambda: False)():
            raise ValueError('The reports folder must be inside your OpenHW installation.')
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / 'profiles.json'
    if path.exists(): local_file(root, path.relative_to(root))
    return path


def load_state(root):
    path = state_path(root)
    if not path.exists(): return dict(schema=1, profiles=[], selected=None, uploaded={})
    data = json.loads(path.read_text(encoding='utf-8'))
    if (data.get('schema') != 1 or not isinstance(data.get('profiles'), list) or
            not isinstance(data.get('uploaded'), dict)):
        raise ValueError('The saved diagnostics profiles are damaged; restore profiles.json from backup.')
    return data


def save_state(root, data):
    import uuid
    path = state_path(root)
    pending = path.with_name('.profiles-' + uuid.uuid4().hex + '.json')
    try:
        with pending.open('x', encoding='utf-8', newline='\n') as output:
            json.dump(data, output, ensure_ascii=True, indent=2)
            output.write('\n'); output.flush(); os.fsync(output.fileno())
        os.replace(pending, path)
    finally: pending.unlink(missing_ok=True)


def save_profile(root, state, name, comment, existing=None):
    name = name.strip()
    if not name or len(name) > 80 or any(ord(c) < 32 for c in name):
        raise ValueError('Use your community alias as the save name (1 to 80 characters).')
    if len(comment) > 4000 or '\0' in comment:
        raise ValueError('Please keep the comment under 4,000 characters.')
    body = dict(schema=1, name=name, comment=comment)
    headers = {'Content-Type': 'application/json', 'User-Agent': 'OpenHW-Diagnostics/1.1'}
    if existing:
        body['profile_id'] = existing['profile_id']
        headers['Authorization'] = 'Bearer ' + existing['upload_token']
    request = urllib.request.Request(ENDPOINT.rsplit('/', 1)[0] + '/profiles',
        data=json.dumps(body, ensure_ascii=True).encode('utf-8'), method='POST', headers=headers)
    with urllib.request.build_opener(NoRedirect()).open(request, timeout=30) as response:
        if response.status != 201: raise ValueError('Server did not save your diagnostics folder.')
        result = json.loads(response.read(8193))
    identity = result.get('profile_id', '')
    token = result.get('upload_token', existing['upload_token'] if existing else '')
    if (not isinstance(identity, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,100}', identity) or
            not isinstance(token, str) or not re.fullmatch(r'[A-Za-z0-9_-]{43}', token) or
            (existing and identity != existing['profile_id'])):
        raise ValueError('The server returned an invalid saved-folder receipt.')
    profile = dict(profile_id=identity, upload_token=token, name=name, comment=comment)
    state['profiles'] = [p for p in state['profiles'] if p['profile_id'] != identity] + [profile]
    state['selected'] = identity
    save_state(root, state)
    return profile


def pending_logs(root, state):
    """Content-addressed chunks: appended/replaced logs are eligible again."""
    result = []
    for relative in inventory(root)[0]:
        path = local_file(root, relative)
        observed = path.stat().st_size
        with path.open('rb') as source:
            offset = 0
            while offset < observed or (observed == 0 and offset == 0):
                remaining = min(CHUNK_BYTES, observed - offset)
                digest = hashlib.sha256(); count = 0
                while count < remaining:
                    data = source.read(min(1024 * 1024, remaining - count))
                    if not data: raise ValueError('A log changed while being scanned. Refresh and try again.')
                    digest.update(data); count += len(data)
                sha = digest.hexdigest()
                key = hashlib.sha256((relative + '\0' + str(offset) + '\0' + str(count) + '\0' + sha).encode()).hexdigest()
                if key not in state['uploaded']:
                    result.append(dict(source=relative, offset=offset, size=count, sha256=sha, key=key))
                offset += count
                if not count: break
    return result


def build_batch(root, profile, chunks):
    import uuid
    output = state_path(root).parent / ('report-' + uuid.uuid4().hex + '.zip')
    metadata = dict(schema=1, profile_id=profile['profile_id'], alias=profile['name'],
        comment=profile['comment'], created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), files=[])
    try:
        with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=3) as archive:
            for item in chunks:
                # Opaque unique member names avoid basename collisions. Original
                # relative paths and offsets are recorded for private inspection.
                member = 'logs/' + item['key'] + Path(item['source']).suffix.lower()
                digest = hashlib.sha256(); remaining = item['size']
                with local_file(root, item['source']).open('rb') as source, archive.open(member, 'w', force_zip64=True) as destination:
                    source.seek(item['offset'])
                    while remaining:
                        data = source.read(min(1024 * 1024, remaining))
                        if not data: raise ValueError('A log changed during upload preparation; please retry.')
                        destination.write(data); digest.update(data); remaining -= len(data)
                if digest.hexdigest() != item['sha256']:
                    raise ValueError('A log changed during upload preparation; please retry.')
                metadata['files'].append(dict(path=member, **{k:item[k] for k in ('source','offset','size','sha256')}))
            archive.writestr('report.json', json.dumps(metadata, ensure_ascii=True).encode())
        if output.stat().st_size > MAX_UPLOAD: raise ValueError('Diagnostic batch exceeds its upload bound.')
        return output
    except Exception:
        output.unlink(missing_ok=True)
        raise


def upload_pending(root, state, profile, progress=lambda text: None):
    chunks = pending_logs(root, state)
    if not chunks: return 0
    batches, batch, size = [], [], 0
    for item in chunks:
        if batch and (size + item['size'] > 48 * 1024 * 1024 or len(batch) >= 60):
            batches.append(batch); batch, size = [], 0
        batch.append(item); size += item['size']
    if batch: batches.append(batch)
    for index, batch in enumerate(batches, 1):
        progress(f'Uploading new logs: batch {index} of {len(batches)}...')
        archive = build_batch(root, profile, batch)
        try:
            receipt = upload_report(archive, profile)
        finally:
            archive.unlink(missing_ok=True)
        for item in batch:
            state['uploaded'][item['key']] = dict(source=item['source'], offset=item['offset'],
                size=item['size'], sha256=item['sha256'], report_id=receipt, profile_id=profile['profile_id'])
        # Only acknowledged batches are marked. Failure/restart retains pending work.
        save_state(root, state)
        archive.unlink(missing_ok=True)
    return len({item['source'] for item in chunks})


def run(root):
    import tkinter as tk
    from tkinter import ttk
    import openhw_launcher_ui as ui
    root = Path(root).resolve(); state = load_state(root)
    window = tk.Tk(); window.withdraw(); window.title('OpenHW Diagnostics')
    window.configure(bg=ui.LINE); ui.load_fonts(root / 'Development/Launcher/Theme')
    messages = queue.Queue(); busy = False; current = None
    status = tk.StringVar(value='Save a name to create your private server folder, then upload all new logs.')
    def close():
        if not busy: window.destroy()
        else: status.set('Please wait for the current server operation to finish.')
    chrome = ui.Chrome(window, close, title='OPENHW   /   DIAGNOSTICS')
    body = tk.Frame(window, bg=ui.PANEL); body.pack(fill='both', expand=True, padx=2, pady=(0,2))
    body.columnconfigure(0, weight=1)
    def caption(text, row, color=ui.MUTED):
        ui.label(body,text,size=ui.SMALL,color=color,justify='left',wraplength=850).grid(row=row,column=0,sticky='w',padx=24,pady=(12,5))
    caption('SAVED DIAGNOSTICS FOLDER',0,ui.GOLD)
    selected = tk.StringVar()
    style=ttk.Style(); style.theme_use('clam')
    style.configure('Report.TCombobox',fieldbackground=ui.RAISED,background=ui.LINE,foreground=ui.INK)
    dropdown=ttk.Combobox(body,textvariable=selected,state='readonly',style='Report.TCombobox',font=ui.SMALL)
    dropdown.grid(row=1,column=0,sticky='ew',padx=24)
    caption('SAVE NAME / USE YOUR HOMEWORLD COMMUNITY ALIAS',2,ui.GOLD)
    name=tk.Entry(body,bg=ui.RAISED,fg=ui.INK,insertbackground=ui.GOLD,font=ui.BODY,relief='flat')
    name.grid(row=3,column=0,sticky='ew',padx=24,ipady=7)
    caption('COMMENT / WHAT HAPPENED, MISSION, SETTINGS AND STEPS TO REPRODUCE',4,ui.GOLD)
    comment=tk.Text(body,height=5,bg=ui.RAISED,fg=ui.INK,insertbackground=ui.GOLD,font=ui.SMALL,relief='flat',wrap='word')
    comment.grid(row=5,column=0,sticky='ew',padx=24)
    caption('Saving creates a private folder on the OpenHW server. Select the same saved name next time.\nThe name identifies your diagnostics folder; it is separate from campaign saves.',6)
    folder_label=tk.StringVar(value='No server folder saved yet.')
    ui.label(body,'',size=ui.SMALL,color=ui.CYAN,textvariable=folder_label,wraplength=850).grid(row=7,column=0,sticky='w',padx=24,pady=8)
    caption('UPLOAD ALL NEW LOGS',8,ui.GOLD)
    caption('Includes every log and crash dump in Development/Logs that has not already been uploaded.\nChanged logs are eligible again. Large files are split into bounded batches. Successful uploads are remembered.\nLogs can contain local paths. Nothing is sent until you save a folder or click Upload.',9)
    ui.label(body,'',size=ui.SMALL,color=ui.CYAN,textvariable=status,wraplength=850).grid(row=10,column=0,sticky='ew',padx=24,pady=12)
    def refresh_profiles(identity=None):
        nonlocal current
        dropdown.configure(values=['NEW DIAGNOSTICS FOLDER']+[p['name']+' / '+p['profile_id'][-8:] for p in state['profiles']])
        current=next((p for p in state['profiles'] if p['profile_id']==identity),None)
        dropdown.current(state['profiles'].index(current)+1 if current else 0)
        name.delete(0,'end'); comment.delete('1.0','end')
        if current:
            name.insert(0,current['name']); comment.insert('1.0',current['comment'])
            folder_label.set('Saved server folder: '+current['profile_id'])
        else: folder_label.set('Save a name to create a new private server folder.')
    def choose(event=None):
        if busy:
            refresh_profiles(current['profile_id'] if current else None); return
        index=dropdown.current()-1
        refresh_profiles(state['profiles'][index]['profile_id'] if index>=0 else None)
    dropdown.bind('<<ComboboxSelected>>',choose)
    refresh_profiles(state.get('selected'))
    def start(upload=False):
        nonlocal busy
        if busy:return
        identity=name.get(); text=comment.get('1.0','end-1c'); existing=current
        if upload and (not existing or identity.strip()!=existing['name'] or text!=existing['comment']):
            status.set('Save the name and comment first, then upload all new logs.'); return
        busy=True; status.set('Scanning and uploading new logs...' if upload else 'Saving your private server folder...')
        def work():
            try:
                if upload:
                    count=upload_pending(root,state,existing,lambda text:messages.put(('progress',text)))
                    messages.put(('done',f'Uploaded new data from {count} log files. Folder: '+existing['name'] if count else 'All current logs have already been uploaded.'))
                else:
                    profile=save_profile(root,state,identity,text,existing)
                    messages.put(('saved',profile['profile_id']))
            except Exception as error:messages.put(('done','Could not complete: '+str(error)+'. Already confirmed uploads are kept; you can retry.'))
        threading.Thread(target=work,daemon=True).start()
    buttons=tk.Frame(body,bg=ui.PANEL);buttons.grid(row=11,column=0,sticky='ew',padx=24,pady=(6,22))
    ui.Button(buttons,'BACK',close,width=100).pack(side='left')
    ui.Button(buttons,'SAVE NAME + COMMENT',lambda:start(False),width=240).pack(side='left',padx=12)
    ui.Button(buttons,'UPLOAD ALL NEW LOGS',lambda:start(True),primary=True,width=270).pack(side='right')
    def poll():
        nonlocal busy
        try:
            while True:
                kind,text=messages.get_nowait()
                if kind=='saved':
                    busy=False; refresh_profiles(text);status.set('Folder saved on the server. Ready to upload all new logs.')
                else:
                    if kind=='done':busy=False
                    status.set(text)
        except queue.Empty:pass
        window.after(100,poll)
    width=min(980,window.winfo_screenwidth()-60);height=min(790,window.winfo_screenheight()-80)
    window.geometry(f'{width}x{height}+{(window.winfo_screenwidth()-width)//2}+{(window.winfo_screenheight()-height)//2}')
    window.deiconify();window.update_idletasks();chrome.taskbar();window.lift();window.focus_force();window.after(100,poll)
    window.mainloop()

