"""Transactional user-side import; source installations are always read-only."""
from __future__ import annotations

import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import uuid

VERSION = 'remastered-background-import-6'


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def safe_relative(name):
    from extract_remastered_big import safe_path
    return safe_path(name)


def atomic_json(path, value):
    temporary = path.with_name(path.name+'.'+uuid.uuid4().hex+'.tmp')
    temporary.write_text(json.dumps(value, indent=2), encoding='utf-8')
    os.replace(temporary, path)


def stamp(path):
    stat = Path(path).stat()
    return dict(path=str(Path(path).resolve()), size=stat.st_size, mtime_ns=stat.st_mtime_ns)


def run_module(name, arguments):
    original = sys.argv
    try:
        sys.argv = [name]+[str(x) for x in arguments]
        status = importlib.import_module(name).main()
        if status not in (None, 0):
            raise RuntimeError(f'{name} failed: {status}')
    finally:
        sys.argv = original


def discover(explicit_rm=None, runtime=None):
    candidates = []
    def add(value):
        if value:
            p = Path(value).expanduser().resolve()
            if p not in candidates:
                candidates.append(p)
    add(explicit_rm)
    add(os.getenv('HW_REMASTERED_INSTALL'))
    add(os.getenv('HW_Data'))
    if runtime:
        for ancestor in Path(runtime).resolve().parents:
            add(ancestor)
            add(ancestor/'HomeworldRM')
    if os.name == 'nt':
        import winreg
        for hive, key, field in (
            (winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam', 'SteamPath'),
            (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath')):
            try:
                with winreg.OpenKey(hive, key) as reg:
                    steam = Path(winreg.QueryValueEx(reg, field)[0])
                roots = [steam]
                libraries = steam/'steamapps/libraryfolders.vdf'
                if libraries.is_file():
                    roots += [Path(x.replace('\\\\', '\\')) for x in re.findall(r'"path"\s*"([^"]+)"', libraries.read_text(encoding='utf-8'))]
                for root in roots:
                    add(root/'steamapps/common/Homeworld')
            except OSError:
                pass
        # GOG installs are named differently by edition/language; inspect their
        # registry inventory rather than guessing a fixed drive/folder.
        for root in (r'SOFTWARE\GOG.com\Games', r'SOFTWARE\WOW6432Node\GOG.com\Games'):
            try:
                with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, root) as reg:
                    for i in range(winreg.QueryInfoKey(reg)[0]):
                        with winreg.OpenKey(reg, winreg.EnumKey(reg, i)) as game:
                            try:
                                title = winreg.QueryValueEx(game, 'gameName')[0]
                                if 'homeworld' in title.lower():
                                    add(winreg.QueryValueEx(game, 'path')[0])
                            except OSError:
                                pass
            except OSError:
                pass
    rm = None
    for p in candidates:
        for folder in (p, p/'Data', p/'HomeworldRM/Data'):
            if rm is None and (folder/'HWBackgrounds.big').is_file():
                rm = folder
    # Explicit invalid selections must not silently use an unrelated auto match.
    if explicit_rm and not any((p/'HWBackgrounds.big').is_file() for p in
        (Path(explicit_rm), Path(explicit_rm)/'Data', Path(explicit_rm)/'HomeworldRM/Data')):
        raise ValueError('Selected Remastered folder contains no HWBackgrounds.big')
    return rm


def import_backgrounds(folder, work, generated, tools, side, reuse=None, compact=False):
    from extract_remastered_big import extract
    from inspect_remastered_scene import inspect_scene
    source = work/'remastered'
    extract(folder/'HWBackgrounds.big', source, ('background/stars/',) if compact else ('background/ez', 'background/stars/'))
    from translate_remastered_scene import translate, translate_flares
    # Read shader inputs locally when available; a clean installation keeps
    # these in Homeworld2.big. Never modify the source Data directory.
    shaders=folder/'shaders'
    if not (shaders/'gl_prog/bg_planet.prog').is_file():
        shader_root=work/'remastered-programs'
        extract(folder/'Homeworld2.big',shader_root,('shaders/gl_prog/bg_', 'shaders/gl_surf/bg_'))
        shaders=shader_root/'shaders'
    supported, unsupported = [], []
    effects=folder/'effect/lensflare'
    if not (effects/'ez01_flare.flare').is_file():
        effect_root=work/'remastered-effects'
        extract(folder/'Homeworld2.big',effect_root,('effect/lensflare/',))
        effects=effect_root/'effect/lensflare'
    destination=generated/'RemasteredBackgrounds'
    destination.mkdir(parents=True,exist_ok=True)
    for mission in range(1, 17):
        stem = f'ez{mission:02d}'
        if compact:extract(folder/'HWBackgrounds.big',source,('background/'+stem+'/',))
        root = source/'background'/stem
        light = root/(stem+'_light.hod')
        if reuse:
            for path in (reuse/'RemasteredBackgrounds').glob(f'mission{mission:02d}*'):
                if path.suffix!='.hwf6':shutil.copy2(path,destination/path.name)
            if mission == 3:
                translate(light,shaders,destination,mission)
        else:
            translate(light,shaders,destination,mission)
            run_module('translate_remastered_background', ['--source', root/(stem+'.hod'),
                '--light-source', light, '--mission', mission, '--output',destination,
                '--encoder', tools/'bc6h_encode.exe', '--side', side,
                '--foreground-scene',destination/f'mission{mission:02d}_scene.bin'])
        translate_flares(root,effects,destination,mission)
        supported.append(mission)
        if compact:
            from release_compact import discard
            discard(root,work)
    return dict(status='partial' if unsupported else 'complete', missions=supported,
                unsupported_foreground_missions=unsupported)


def validate_cache(root, receipt, deep=True):
    if receipt.get('version') != VERSION or not receipt.get('outputs'):
        return False
    try:
        for row in receipt['sources']:
            if stamp(row['path']) != {k: row[k] for k in ('path', 'size', 'mtime_ns')}:
                return False
        for row in receipt['outputs']:
            path = root/safe_relative(row['path'])
            if not path.resolve().is_relative_to(root.resolve()) or path.stat().st_size != row['size']:
                return False
            if deep and digest(path) != row['sha256']:
                return False
        return True
    except (OSError, ValueError, KeyError, TypeError):
        return False


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--cache', type=Path, required=True)
    cli.add_argument('--tools', type=Path, required=True)
    cli.add_argument('--runtime', type=Path)
    cli.add_argument('--remastered', type=Path)
    cli.add_argument('--only', choices=('remastered',), default='remastered')
    cli.add_argument('--side', type=int, default=1024)
    cli.add_argument('--detect', action='store_true')
    cli.add_argument('--first-boot', action='store_true')
    cli.add_argument('--force', action='store_true')
    args = cli.parse_args()
    if not 64 <= args.side <= 4096:
        cli.error('side must be 64..4096')
    cache = args.cache.resolve()
    try:
        remembered = json.loads((cache/'installations.json').read_text(encoding='utf-8'))
        if not args.remastered and remembered.get('remastered') and (Path(remembered['remastered'])/'HWBackgrounds.big').is_file():
            args.remastered = Path(remembered['remastered'])
    except (OSError, ValueError, TypeError):
        pass
    rm = discover(args.remastered, args.runtime)
    print(json.dumps(dict(remastered=str(rm) if rm else None)), flush=True)
    if args.detect:
        return 0
    # Never write beneath any discovered installation (or alias of one).
    for folder in (rm,):
        if folder and cache.is_relative_to(folder.resolve()):
            raise ValueError('Cache must be outside source installations')
    cache.mkdir(parents=True, exist_ok=True)
    active = cache/'active.json'
    options = dict(side=args.side, only=args.only)
    expected_sources = []
    if rm: expected_sources.append(rm/'HWBackgrounds.big')
    if rm:
        if (rm/'shaders/gl_prog/bg_planet.prog').is_file():
            expected_sources += sorted(p for p in (rm/'shaders').rglob('bg_*') if p.is_file())
        else:
            expected_sources.append(rm/'Homeworld2.big')
        if (rm/'effect/lensflare/ez01_flare.flare').is_file():
            expected_sources += sorted(p for p in (rm/'effect/lensflare').rglob('*') if p.is_file())
        elif rm/'Homeworld2.big' not in expected_sources:
            expected_sources.append(rm/'Homeworld2.big')
    options['tools'] = {p.relative_to(args.tools).as_posix(): digest(p) for p in sorted(args.tools.rglob('*')) if p.is_file()}
    reuse=None
    if active.is_file() and not args.force:
        try:
            prior = json.loads(active.read_text(encoding='utf-8'))
            oldroot = cache/safe_relative(prior['generation'])
            receipt = json.loads((oldroot/'receipt.json').read_text(encoding='utf-8'))
            same_sources = [str(p.resolve()) for p in expected_sources] == [x['path'] for x in receipt['sources']]
            # v4/v5 use the same RMSC3 boundary. Verify prior sources/outputs,
            # regenerate Mission 3's burned campaign surface and flares in a
            # NEW generation. Never mutate the active generation in place.
            if (receipt.get('version') in ('remastered-background-import-4','remastered-background-import-5') and
                same_sources and
                receipt.get('options')==options and
                receipt.get('features',{}).get('remastered',{}).get('missions')==list(range(1,17)) and
                validate_cache(oldroot/'runtime',{**receipt,'version':VERSION})):
                reuse=oldroot/'runtime'
            if same_sources and receipt.get('options') == options and validate_cache(oldroot/'runtime', receipt):
                atomic_json(cache/'status.json', dict(status='ready', features=receipt['features']))
                # Repair a pointer interrupted between the two atomic commits.
                pointer = cache/('active-root.'+uuid.uuid4().hex+'.tmp')
                pointer.write_text(str(oldroot/'runtime'), encoding='utf-8')
                os.replace(pointer, cache/'active-root.txt')
                atomic_json(cache/'installations.json', dict(remastered=str(rm) if rm else None))
                print('Verified import cache; no conversion required', flush=True)
                return 0
        except (OSError, ValueError, KeyError, TypeError):
            print('Previous receipt is invalid; rebuilding without replacing it until success', flush=True)
    if not rm:
        atomic_json(cache/'status.json', dict(status='not-installed'))
        return 0
    if args.first_boot and os.name == 'nt':
        import ctypes
        answer = ctypes.windll.user32.MessageBoxW(None,
            'Import backgrounds from your installed Homeworld Remastered?\n\n'
            'Conversion may take several minutes. Original files and mission edits will not be changed.\n'
            'The existing ships and your authored mission lighting/clouds will remain unchanged.\n\n'
            'Yes: convert now. No: continue using the current build; ask again next launch.',
            'Homeworld Modern — local asset setup', 0x24)
        if answer != 6:
            return 0
    lock = cache/'import.lock'
    # OS-held lock: process termination releases it, so interrupted work is retryable.
    with lock.open('a+b') as handle:
        if os.name == 'nt':
            import msvcrt
            if lock.stat().st_size == 0: handle.write(b'0'); handle.flush()
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        generation = 'generation-'+uuid.uuid4().hex
        stage = cache/generation
        stage.mkdir()
        work, generated = stage/'work', stage/'runtime'
        work.mkdir(); generated.mkdir()
        sources = [dict(**stamp(p), sha256=digest(p)) for p in expected_sources]
        atomic_json(cache/'status.json', dict(status='converting', generation=generation))
        features = {}
        try:
            if rm:
                print('Importing supported Remastered backgrounds', flush=True)
                if reuse:
                    features['remastered']=import_backgrounds(rm,work,generated,args.tools.resolve(),args.side,reuse)
                else:
                    features['remastered']=import_backgrounds(rm,work,generated,args.tools.resolve(),args.side)
            for source in sources:
                if stamp(source['path']) != {k: source[k] for k in ('path','size','mtime_ns')} or digest(source['path']) != source['sha256']:
                    raise ValueError('Source archive changed during conversion')
            outputs = [dict(path=p.relative_to(generated).as_posix(), size=p.stat().st_size, sha256=digest(p))
                       for p in sorted(generated.rglob('*')) if p.is_file()]
            receipt = dict(version=VERSION, options=options, sources=sources, outputs=outputs, features=features)
            atomic_json(stage/'receipt.json', receipt)
            if not validate_cache(generated, receipt):
                raise ValueError('Generated cache validation failed')
            # Single atomic pointer switch; the previous generation remains recoverable.
            atomic_json(active, dict(generation=generation))
            pointer = cache/'active-root.txt'
            temp = cache/('active-root.'+uuid.uuid4().hex+'.tmp')
            temp.write_text(str(generated), encoding='utf-8')
            os.replace(temp, pointer)
            atomic_json(cache/'status.json', dict(status='ready', features=features))
            # Keep explicit/manual installations discoverable on the next boot.
            atomic_json(cache/'installations.json', dict(remastered=str(rm) if rm else None))
            print(json.dumps(features, indent=2), flush=True)
        except BaseException as error:
            atomic_json(cache/'status.json', dict(status='failed', generation=generation, error=str(error)))
            raise
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
