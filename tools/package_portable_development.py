"""Create an uninstalled private dev ZIP; exclude all user/RM import output."""
import argparse
import json
from pathlib import Path
import zipfile
from portable_setup import sha256

EXCLUDED={'setupsources','setupwork','remasteredbackgrounds','data','savedgames','logs',
          'shadercache','updates','diagnostics','openhw-hw1.obig','setup-complete.json',
          'hw_comp.vce','hw_music.wxd','subsystems'}
FORBIDDEN_CONTENT=('cataclysm','somtaaw','catascout','cataacolyte','cataacv',
                   'sacolyte','srecon','sacv')


def package(root, output):
    root,output=Path(root).resolve(),Path(output).resolve()
    if output.is_relative_to(root):raise ValueError('ZIP destination must be outside its staged source')
    rows=[]
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=1,allowZip64=True) as archive:
        for path in sorted(root.rglob('*')):
            if not path.is_file():continue
            relative=path.relative_to(root)
            if relative.parts[0].lower() in EXCLUDED:continue
            if relative.parts[:2]==('Modding','RuntimeDefinitions'):continue
            if any(token in relative.as_posix().lower() for token in FORBIDDEN_CONTENT):
                raise ValueError('Excluded expansion content in package: '+str(relative))
            if path.is_symlink() or not path.resolve().is_relative_to(root):raise ValueError('ZIP source escapes portable folder')
            rows.append(dict(path=relative.as_posix(),size=path.stat().st_size,sha256=sha256(path)))
            archive.write(path,'OpenHW/'+relative.as_posix())
        archive.writestr('OpenHW/package-files.json',json.dumps(dict(private_development=True,files=rows),indent=2))
    with zipfile.ZipFile(output) as archive:
        if archive.testzip():raise ValueError('Portable ZIP CRC verification failed')
        names=set(archive.namelist())
        for name in ('OpenHW.exe','OpenHW-Setup.exe','openhw.portable','SetupTools/native-bindings.json'):
            if 'OpenHW/'+name not in names:raise ValueError('Portable ZIP missing '+name)
        if any('RMNative/' in n or n.endswith('.hod') for n in names):raise ValueError('Imported RM model leaked into installer')
    print('Created',output,'with',len(rows),'files; SHA256',sha256(output))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();package(args.root,args.output)
