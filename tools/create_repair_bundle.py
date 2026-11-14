"""Create verified offline recovery inputs for the one-time portable Updater."""
from pathlib import Path
import argparse,hashlib,json,shutil

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('patch','launcher','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--version',default='1.1.1');p.add_argument('--from-version',default='1.1');p.add_argument('--launcher-version',default='1.1.3')
 a=p.parse_args();downloads=a.output/'Development/Updater/Downloads';downloads.mkdir(parents=True,exist_ok=True)
 def row(path,name):
  with path.open('rb') as source:sha=hashlib.file_digest(source,'sha256').hexdigest()
  shutil.copy2(path,downloads/(sha+'.exe'))
  return dict(url=f'https://github.com/dovahkiinjeff/OpenHW/releases/download/v{a.version}/{name}',size=path.stat().st_size,sha256=sha,mirrors=[f'https://openhw.tech/downloads/v{a.version}/{name}'])
 patch=dict(to=a.version,format='openhw-exe-zip-v1',post_actions=[],**{'from':[a.from_version]},**row(a.patch,f'OpenHW-Patch-{a.version}.exe'))
 launcher=dict(version=a.launcher_version,**row(a.launcher,'Launcher.exe'))
 target=a.output/'Development/Updater/Bundled/manifest.json';target.parent.mkdir(parents=True,exist_ok=True)
 target.write_text(json.dumps(dict(schema=1,channel='stable',latest=a.version,minimum_protocol=3,patches=[patch],launcher=launcher),indent=2)+'\n',encoding='utf-8',newline='\n')
 print('Verified recovery bundle created:',target)
if __name__=='__main__':main()
