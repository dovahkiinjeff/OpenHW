"""Check that user-imported Remastered assets are excluded from a release."""
import argparse
from pathlib import Path
import struct


def prohibited(name):
    parts = name.replace('\\','/').lower().split('/')
    return ('remasteredbackgrounds' in parts or 'importedassets' in parts or
            'rmnative' in parts or 'remasteredfx' in parts or
            'hwbackgrounds.big' in parts or
            (parts[-1].endswith(('.hod','.mad','.hlsl','.cso')) and any(x.startswith(('ez','bg_')) for x in parts)))


def audit(root):
    failures = []
    for path in root.rglob('*'):
        if not path.is_file(): continue
        relative = path.relative_to(root).as_posix()
        if prohibited(relative): failures.append(relative)
        if path.suffix.lower() == '.hwt':
            with path.open('rb') as stream:
                header = stream.read(24)
                if len(header)!=24: raise ValueError('Truncated texture pack')
                magic,version,count,data_base=struct.unpack('<8sIIQ',header)
                if magic!=b'HWTPACK1' or version!=1 or count>100000 or data_base!=24+count*272:
                    raise ValueError('Invalid texture pack')
                for _ in range(count):
                    row=stream.read(272)
                    if len(row)!=272: raise ValueError('Truncated texture index')
                    name=row[:256].split(b'\0')[0].decode('utf-8')
                    if prohibited(name): failures.append(relative+': '+name)
    return failures


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('release',type=Path)
    args=cli.parse_args()
    if not args.release.is_dir(): cli.error('release directory not found')
    failures=audit(args.release)
    for name in failures: print('EXCLUDE: '+name)
    if failures: raise SystemExit('Imported assets remain in the release: audit failed')
    print('Local-import asset exclusion audit passed (not a general licensing audit)')


if __name__=='__main__': main()
