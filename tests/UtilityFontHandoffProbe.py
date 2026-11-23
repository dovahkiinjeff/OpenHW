from pathlib import Path
import os,sys,subprocess,time,shutil,zipfile,json
r=Path.cwd();out=r/'out/launcher-font-fix';fixture=out/'fresh-115-fixture';fixture.mkdir(parents=True,exist_ok=True)
library=r/'out/release-1.1.15/frozen/OpenHW-Setup/Development'
for source in library.rglob('*'):
 if source.is_file():
  target=fixture/'Development/Installer/Development'/source.relative_to(library)
  target.parent.mkdir(parents=True,exist_ok=True)
  if not target.exists():os.link(source,target)
with zipfile.ZipFile(r/'out/release-1.1.15/launcher/updater.zip') as archive:
 for name in archive.namelist():
  if name.startswith('Development/Launcher/Theme/'):archive.extract(name,fixture)
(fixture/'version.json').write_text('{"version":"1.1.15"}')
(fixture/'OpenHW.exe').write_bytes(b'game file fixture, never executed')
(fixture/'setup-complete.json').write_text('{"status":"ready","version":"1.1.15"}')
holder=out/'hold-fonts.py'
holder.write_text('''from pathlib import Path
import ctypes,sys,time
root=Path(sys.argv[1]);add=ctypes.windll.gdi32.AddFontResourceExW
add.argtypes=[ctypes.c_wchar_p,ctypes.c_uint,ctypes.c_void_p];add.restype=ctypes.c_int
for name in ('Rajdhani-Regular.ttf','Rajdhani-Bold.ttf','ShareTechMono-Regular.ttf'):
 assert add(str(root/'Development/Launcher/Theme'/name),0x10,None)>0
import tkinter as tk
from tkinter import font
window=tk.Tk();window.withdraw()
fonts=[font.Font(window,family='Rajdhani',size=20),font.Font(window,family='Rajdhani',size=20,weight='bold'),font.Font(window,family='Share Tech Mono',size=12)]
for item in fonts:assert item.measure('OPENHW UPDATE CHANNEL')>0
# Model the write/delete-denying font handles observed in the report. Font
# cache behavior varies across Windows installations; the handoff must work
# regardless of whether this particular machine's GDI cache keeps them open.
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p]
kernel.CreateFileW.restype=ctypes.c_void_p
handles=[kernel.CreateFileW(str(root/'Development/Launcher/Theme'/name),0x80000000,1,None,3,0,None) for name in ('Rajdhani-Regular.ttf','Rajdhani-Bold.ttf','ShareTechMono-Regular.ttf')]
assert all(handle and handle!=ctypes.c_void_p(-1).value for handle in handles)
(root/'fonts-held.ready').write_text('registered')
time.sleep(float(sys.argv[2]))
''')
def run(package,seconds):
 ready=fixture/'fonts-held.ready';ready.unlink(missing_ok=True)
 process=subprocess.Popen([sys.executable,str(holder.resolve()),str(fixture.resolve()),str(seconds)])
 try:
  deadline=time.monotonic()+10
  while not ready.exists():
   assert process.poll() is None,'Font holder failed'
   assert time.monotonic()<deadline,'Font holder timed out'
   time.sleep(.03)
  shutil.copyfile(package,fixture/'Updater.exe')
  start=time.monotonic()
  result=subprocess.run([str((fixture/'Updater.exe').resolve()),'--launcher-update','--wait-engine',str(process.pid),
       '--wait-bootstrap','0','--verify-payload'],cwd=fixture,timeout=45)
  return result.returncode,time.monotonic()-start
 finally:
  if process.poll() is None:process.terminate()
  process.wait()
code,elapsed=run(r/'out/release-1.1.15/Updater.exe',6)
log=fixture/'Development/Logs/updater-bootstrap.log';old=log.read_text(encoding='utf-8',errors='replace')
assert code!=0,(code,elapsed,'Old updater did not reproduce')
assert all(name in old for name in ('Rajdhani-Regular.ttf','Rajdhani-Bold.ttf','ShareTechMono-Regular.ttf')),old
(out/'reproduced-old-bootstrap.log').write_text(old,encoding='utf-8');log.unlink()
print('Released 1.1.15 updater reproduced all three locked-font extraction errors.',flush=True)
code,elapsed=run(out/'Updater.exe',4)
assert code==0,(code,elapsed,log.read_text(encoding='utf-8',errors='replace'))
assert elapsed>=3.5,elapsed
check=json.loads((fixture/'Development/Updater/self-check.json').read_text());assert check['version']=='1.1.11' and check['status']=='passed'
(out/'Handoff-Validation.json').write_text(json.dumps(dict(status='passed',old_115_failure_reproduced=True,
 all_three_font_errors_reproduced=True,new_bootstrap_waited_before_extraction=True,new_updater_verified=True,
 elapsed_seconds=elapsed,live_installation_updated=False),indent=2)+'\n')
print('New updater waited for font-owning Launcher process to exit, extracted successfully and passed verification.',flush=True)
