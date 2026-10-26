"""Hash the working source inputs; generated builds and local dependencies stay out."""
import argparse
import json
from pathlib import Path
from release_content_policy import digest

EXCLUDED={'out','.deps','.git','.vs','.vscode','tmp','__pycache__','.texture-tools'}


def write(root):
    root=Path(root).resolve()
    files=[]
    for path in sorted(root.rglob('*')):
        if not path.is_file():continue
        relative=path.relative_to(root)
        if any(part in EXCLUDED for part in relative.parts) or relative.name=='SOURCE_MANIFEST.json' or path.suffix=='.pyc':continue
        if path.is_symlink():raise ValueError('Symlink is not a self-contained source input')
        files.append({'path':relative.as_posix(),'size':path.stat().st_size,'sha256':digest(path)})
    result={'schema':1,'version':'1.0','snapshot':'Complete current build inputs',
      'excluded':['generated out/ and .deps/','personal logs/saves and originating Git worktree metadata',
                  'retail data and unused Kharak textures'], 'files':files}
    (root/'SOURCE_MANIFEST.json').write_text(json.dumps(result,indent=2))
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path)
    a=p.parse_args();value=write(a.root)
    print('Source inputs:',len(value['files']),'files;',sum(r['size'] for r in value['files']),'bytes')
