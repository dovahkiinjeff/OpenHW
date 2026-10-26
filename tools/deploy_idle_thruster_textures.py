"""Replace previously imported always-on RM thruster pages with idle pages.

This is a narrow migration for a loose-data development install. It only
touches texture sets whose current bytes match a verified On cache variant.
The source importer now chooses Off for fresh conversions.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.digest()


def discover(cache_roots):
    by_source = {}
    for root in cache_roots:
        for manifest in root.rglob('manifest.json'):
            for material in json.loads(manifest.read_text())['materials']:
                if material['shader'] != 'thruster':
                    continue
                variants = {v['state']: v for v in material['variants']}
                if set(variants) != {'On', 'Off'}:
                    raise RuntimeError(f'Incomplete thruster variant: {manifest}')
                key = (material['source_sha256'].lower(), material['index'])
                entry = (manifest.parent, variants['On']['files'], variants['Off']['files'])
                if key in by_source and by_source[key][1:] != entry[1:]:
                    # Duplicate caches may have different relative paths, but
                    # their authored outputs must be byte-for-byte identical.
                    prior = by_source[key]
                    for kind in entry[1]:
                        if prior[1][kind]['sha256'] != entry[1][kind]['sha256'] or \
                           prior[2][kind]['sha256'] != entry[2][kind]['sha256']:
                            raise RuntimeError(f'Conflicting thruster cache: {manifest}')
                by_source[key] = entry
    return by_source


def plan(data_root, cache_roots, active=False):
    by_source = discover(cache_roots)
    replacements = []
    roots = list(data_root.rglob('RMNative')) + list((data_root/'Subsystems').glob('*'))
    for folder in roots:
        hods = [('c0_m', folder/'hull.hod')]
        hods += [(f'c{i}_m', hod) for i in range(1, 20)
                 if (hod := folder/f'component{i}.hod').is_file()]
        if (folder.parent.name == 'Subsystems'):
            hods = [('m', folder/'model.hod')]
        for prefix, hod in hods:
            if not hod.is_file():
                continue
            source_hash = digest(hod).hex()
            for (known_hash, index), (cache, on, off) in by_source.items():
                if known_hash != source_hash:
                    continue
                target = folder/f'{prefix}{index}.dds'
                if not target.is_file():
                    continue
                current = digest(target).hex()
                if current == off['diffuse']['sha256'] and not active:
                    continue
                if current not in (on['diffuse']['sha256'], off['diffuse']['sha256']):
                    raise RuntimeError(f'Unknown thruster texture: {target}')
                group = []
                for kind, old in on.items():
                    new = on[kind] if active else off[kind]
                    suffix = '.lif' if kind == 'bootstrap' else \
                             '.dds' if kind == 'diffuse' else f'_{kind}.dds'
                    live = target.with_name(target.stem + ('_engineOn' if active else '') + suffix)
                    if not active and not live.is_file():
                        raise RuntimeError(f'Missing live thruster texture: {live}')
                    replacement = cache/new['path']
                    if not replacement.is_file() or digest(replacement).hex() != new['sha256']:
                        raise RuntimeError(f'Missing/stale Off cache: {replacement}')
                    group.append((replacement, live, live.is_file() and digest(live).hex() != old['sha256']))
                replacements.append(group)
    return replacements


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--cache', type=Path, action='append', required=True)
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--active', action='store_true', help='Add On sidecars without changing the idle pages')
    args = parser.parse_args()
    data = args.data.resolve()
    if data != Path(r'F:\OpenHWWIP\Data').resolve():
        raise SystemExit('This migration is scoped to F:\\OpenHWWIP\\Data')
    groups = plan(data, [path.resolve() for path in args.cache], args.active)
    modified = [(source, target) for group in groups for source, target, changed in group if changed]
    print(f'{len(groups)} idle thruster surfaces, {sum(map(len, groups))} files verified; '
          f'{len(modified)} locally modified sidecars need backup')
    if args.apply:
        if modified and args.backup is None:
            raise SystemExit('Specify --backup before replacing locally modified sidecars')
        if args.backup is not None:
            backup = args.backup.resolve()
            if not backup.is_relative_to(data.parent/'Dev'/'Backups'):
                raise SystemExit('Backup must be below F:\\OpenHWWIP\\Dev\\Backups')
            for _, target in modified:
                destination = backup/target.relative_to(data)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, destination)
        for group in groups:
            for source, target, _ in group:
                shutil.copy2(source, target)
        print('Idle thruster pages deployed')


if __name__ == '__main__':
    main()
