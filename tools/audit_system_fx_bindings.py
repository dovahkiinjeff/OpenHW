"""Cross-check actual combined FX mappings against the native gun registry."""
import argparse
import json
import re
from pathlib import Path

def audit(report,registry):
    data=json.loads(report.read_text())
    mapped={(row['ship'],row['gun']) for row in data['mappings'] if 'gun' in row}
    expected={(name,int(gun)) for name,gun in re.findall(
        r'(?:FIXED_GUN|ATTACHED_GUN|SPLIT_GUN|SINGLE_AXIS_GUN)\("([^"]+)",(\d+)',registry.read_text())}
    missing=sorted(expected-mapped)
    for item in missing:print('Unmapped native gun:',*item)
    if missing:raise SystemExit(1)
    print(f'All {len(expected)} registered native gun bindings have RM FX mappings')

if __name__=='__main__':
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('report',type=Path)
    cli.add_argument('--registry',type=Path,default=Path(__file__).resolve().parents[1]/'src/Game/NativeVisualEvents.c')
    args=cli.parse_args();audit(args.report,args.registry)
