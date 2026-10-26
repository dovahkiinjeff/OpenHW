"""Read-only byte identity audit of packaged HODs versus extracted RM sources."""
import argparse
import hashlib
import json
from pathlib import Path
from openhw_obig import validate


def audit(root):
    root=Path(root);source=root/'SetupSources/Remastered';originals={}
    for path in sorted(source.rglob('*.hod')):
        checksum=hashlib.sha256(path.read_bytes()).hexdigest()
        originals.setdefault(checksum,[]).append(path.relative_to(source).as_posix())
    archive=root/'OpenHW-HW1.obig';index=validate(archive);rows=[]
    with archive.open('rb') as stream:
        for name,(offset,size,_) in sorted(index.items()):
            if not name.endswith('.hod'):continue
            stream.seek(offset);checksum=hashlib.sha256(stream.read(size)).hexdigest()
            rows.append(dict(packaged=name,sha256=checksum,size=size,
                             original_sources=originals.get(checksum,[])))
    return dict(packaged_hods=len(rows),unchanged=sum(bool(r['original_sources']) for r in rows),
                unmatched=[r for r in rows if not r['original_sources']],files=rows)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();result=audit(a.root);a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!='files'},indent=2))
