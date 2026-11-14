"""Portable OpenHW launcher. Player files stay beside Launcher.exe."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import queue

import openhw_updater as updater
from launcher_settings import SETTINGS
import launcher_languages

VERSION = '1.1.4'
MOD_NAME = re.compile(r'[A-Za-z0-9_-]{1,64}\Z')
LANGUAGES = ('English', 'French', 'German', 'Spanish', 'Italian')


def safe(root, relative):
    return updater.inside(root, relative)


def backup(root, path):
    if not path.exists(): return
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    dest = safe(root, 'Development/Backups/Launcher/'+stamp+'/'+path.relative_to(root).as_posix())
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, dest)


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name+'.launcher-pending')
    if temporary.exists() and (temporary.is_symlink() or temporary.is_junction()):
        raise updater.UpdateError('Linked settings file refused')
    try:
        with temporary.open('wb') as out:
            out.write(data); out.flush(); os.fsync(out.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.is_file(): temporary.unlink()


class Configuration:
    """Change only requested fields; preserve unknown options, comments and encoding."""
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.path = safe(self.root, 'Homeworld.cfg')
        self.original = self.path.read_bytes() if self.path.exists() else b''
        self.encoding = 'utf-8-sig' if self.original.startswith(b'\xef\xbb\xbf') else 'cp1252'
        self.text = self.original.decode(self.encoding)
        self.values = {}
        for line in self.text.splitlines():
            match = re.match(r'^\s*([A-Za-z][\w.]*)\s+(.*?)\s*$', line)
            if match: self.values[match[1]] = match[2]

    def save(self, changed):
        updater.stop_if_game_running()
        if (self.path.read_bytes() if self.path.exists() else b'') != self.original:
            raise updater.UpdateError('Settings changed since they were opened. Reload settings and try again.')
        allowed = {row['key']: row for row in SETTINGS}
        output = dict(changed)
        for key, value in output.items():
            if key not in allowed: raise updater.UpdateError('Unsupported setting: '+key)
            validate_setting(allowed[key], str(value))
        if 'voiceLanguage' in output and str(output['voiceLanguage']) not in launcher_languages.available(self.root):
            raise ValueError('Install a complete matching voice pack before selecting that language')
        # These are two names for the same runtime field. Never allow the old
        # alias later in the file to silently undo the launcher selection.
        if 'antiAliasing' in output: output['dlaa'] = output['antiAliasing']
        if 'SPResourcePopulationPercent' in output:
            output['DisableSPResourceScaling'] = '1' if int(output['SPResourcePopulationPercent']) == 100 else '0'
        if 'screenWidth' in output or 'screenHeight' in output:
            output['detectDisplayOnFirstBoot']='0'
        lines = []
        remaining = set(output)
        for line in self.text.splitlines(keepends=True):
            match = re.match(r'^(\s*)([A-Za-z][\w.]*)(\s+)(.*?)(\r?\n)?$', line)
            if match and match[2] in output:
                key = match[2]; remaining.discard(key)
                line = match[1]+key+match[3]+str(output[key])+(match[5] or '')
            lines.append(line)
        text = ''.join(lines)
        if remaining:
            if text and not text.endswith('\n'): text += '\r\n'
            text += '\r\n[launcher options]\r\n'
            text += ''.join(key+'    '+str(output[key])+'\r\n' for key in sorted(remaining))
        data = text.encode(self.encoding)  # Reject unrepresentable legacy player names, never corrupt them.
        if data != self.original:
            backup(self.root, self.path); atomic_write(self.path, data)
        self.__init__(self.root)


def validate_setting(row, value):
    if '\n' in value or '\r' in value or '\x00' in value: raise ValueError('Invalid '+row['label'])
    if row['kind'] == 'text':
        if len(value) > 63: raise ValueError(row['label']+' is too long')
        return
    if row['kind'] == 'bool':
        if value not in ('0','1','TRUE','FALSE'): raise ValueError('Invalid '+row['label'])
        return
    number = int(value, 10)
    if not row.get('min', 0) <= number <= row.get('max', 4294967295):
        raise ValueError(row['label']+' is outside its supported range')
    if 'choices' in row and value not in row['choices']: raise ValueError('Invalid '+row['label'])


def read_mods(root):
    root = Path(root).resolve(); folder = safe(root, 'mods')
    if not folder.exists(): return []
    available = {}
    for path in folder.iterdir():
        if MOD_NAME.fullmatch(path.name) and path.name.upper() not in ('CON','NUL'):
            safe(root, 'mods/'+path.name)
            if path.is_dir(): available[path.name.lower()] = path.name
    result = []; seen = set(); order = safe(root, 'mods/load-order.txt')
    for line in (order.read_text(encoding='utf-8-sig').splitlines() if order.exists() else []):
        enabled = not line.startswith('#disabled=')
        name = line if enabled else line[10:]
        if name.lower() in available and name.lower() not in seen:
            seen.add(name.lower()); result.append(dict(name=available[name.lower()], enabled=enabled))
    result += [dict(name=name, enabled=False) for name in sorted(available.values()) if name.lower() not in seen]
    if len(result) > 128: raise updater.UpdateError('OpenHW supports at most 128 mod folders')
    return result


def save_mods(root, rows):
    updater.stop_if_game_running(); root = Path(root).resolve()
    available = {row['name'].lower() for row in read_mods(root)}; seen = set()
    if len(rows) != len(available): raise updater.UpdateError('Mod folders changed. Refresh the list before saving.')
    text = '# OpenHW load order. Later enabled mods override earlier mods.\n'
    for row in rows:
        name = row['name']; folded = name.lower()
        if not MOD_NAME.fullmatch(name) or folded not in available or folded in seen:
            raise updater.UpdateError('Invalid or duplicate mod folder')
        seen.add(folded); text += ('' if row['enabled'] else '#disabled=')+name+'\n'
    path = safe(root, 'mods/load-order.txt'); data = text.encode('utf-8')
    if not path.exists() or path.read_bytes() != data:
        backup(root, path); atomic_write(path, data)


def rebuild_shader_cache(root):
    updater.stop_if_game_running(); root = Path(root).resolve()
    cache = safe(root, 'Development/ShaderCache')
    if not cache.exists(): return 'Shader cache is empty. It will be built when you next start the game.'
    # Rename the whole local cache, keeping recovery possible. No driver or
    # system cache is cleared, and no renderer can be using these files.
    for path in cache.rglob('*'): safe(root, path.relative_to(root).as_posix())
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    target = safe(root, 'Development/Backups/ShaderCache/'+stamp)
    target.parent.mkdir(parents=True, exist_ok=True); cache.rename(target)
    return 'Shader cache reset. The next OpenHW startup will rebuild it and take longer.'


def start_game(root, diagnostics=False):
    root = Path(root).resolve(); updater.detect_version(root); updater.stop_if_game_running()
    executable = safe(root, 'OpenHW-Daignostics.exe' if diagnostics else 'OpenHW.exe')
    if not executable.is_file(): raise updater.UpdateError('The selected game executable is missing')
    env = os.environ.copy()
    # Do not inherit a past diagnostic launcher's renderer experiments.
    for key in tuple(env):
        if key.startswith(('HW_TEMPORAL_', 'HW_FG_', 'HW_FSR_FG_', 'HW_BENCHMARK')): env.pop(key)
    for key in ('TEMP','TMP','TMPDIR'): env[key] = str(safe(root, 'Development/Temp'))
    return subprocess.Popen([str(executable)], cwd=root, env=env)


def run_gui(root):
    from openhw_launcher_ui import run
    run(root)


def prepare_launcher_update(root, progress):
    """Verify/cache everything before handing off the locked Launcher EXE."""
    root=Path(root).resolve();updater.stop_if_game_running()
    with updater.update_lock(root):
        feed=updater.fetch_manifest();installed=updater.detect_version(root)
        route=updater.update_plan(feed,installed)
        component=feed.get('launcher')
        if not route and (not component or updater.compare_versions(VERSION,component['version'])>=0):return None
        for patch in route:updater.download(patch,updater.inside(root,'Development/Updater/Downloads'),progress,lambda:False)
        if component:updater.download(component,updater.inside(root,'Development/Updater/Downloads'),progress,lambda:False)
        own=feed.get('updater')
        if not own:raise updater.UpdateError('The server has not published its update helper yet.')
        candidate=updater.download(own,updater.inside(root,'Development/Updater/Downloads'),progress,lambda:False)
        target=updater.inside(root,'Updater.exe')
        if not target.exists() or updater.digest(target)!=own['sha256']:
            if target.exists():backup(root,target)
            pending=updater.inside(root,'Updater.exe.update-pending')
            try:
                shutil.copyfile(candidate,pending)
                if updater.digest(pending)!=own['sha256']:raise updater.UpdateError('Update helper failed verification')
                os.replace(pending,target)
            finally:pending.unlink(missing_ok=True)
        return [str(target),'--launcher-update','--wait-engine',str(os.getpid()),
                '--wait-bootstrap',os.environ.get('OPENHW_UPDATER_BOOTSTRAP_PID','0')]


def start_update_handoff(root, command):
    return subprocess.Popen(command,cwd=root,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))


def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--root',type=Path,required=True); parser.add_argument('--verify-payload',action='store_true')
    parser.add_argument('--diagnostics-report',action='store_true')
    parser.add_argument('--check-update-feed',action='store_true')
    args=parser.parse_args(); root=args.root.resolve()
    if args.check_update_feed:
        updater.check_update_feed(root, 'Launcher')
    elif args.verify_payload:
        from tkinter import ttk, messagebox, filedialog, simpledialog, colorchooser
        import extract_vanilla_ship_textures
        import openhw_launcher_ui
        import openhw_diagnostics
        for name in ('hangar-1000.png','hangar-1440.png','hangar-2200.png','mark.png','Rajdhani-Regular.ttf','Rajdhani-Bold.ttf','ShareTechMono-Regular.ttf'):
            if not safe(root,'Development/Launcher/Theme/'+name).is_file(): raise RuntimeError('Launcher theme file missing: '+name)
        updater.atomic_json(safe(root,'Development/Launcher/self-check.json'),dict(status='passed',version=VERSION,settings=len(SETTINGS),gameRun=False))
    elif args.diagnostics_report:
        from openhw_diagnostics import run
        run(root)
    else: run_gui(root)


if __name__=='__main__': main()
