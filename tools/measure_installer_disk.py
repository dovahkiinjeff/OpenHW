"""Measure a real headless installer run; all run data stays on the named drive."""
import argparse,ctypes,datetime,hashlib,json,os,shutil,subprocess,threading,time,traceback
from pathlib import Path
from ctypes import wintypes as w
p=argparse.ArgumentParser();p.add_argument('run',type=Path);p.add_argument('--installer',type=Path,required=True);p.add_argument('--collection',type=Path,required=True);a=p.parse_args()
run=a.run.resolve();collection=a.collection.resolve()
if run==collection or run.is_relative_to(collection) or collection.is_relative_to(run):p.error('Measurement destination must be separate from the source Collection')
a.installer=a.installer.resolve(strict=True)
run.mkdir(parents=True,exist_ok=False);root=run/'Game';root.mkdir();report=run/'measurement.json';stop=threading.Event();start=time.monotonic()
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.GetDiskFreeSpaceExW.argtypes=[w.LPCWSTR,ctypes.POINTER(ctypes.c_ulonglong),ctypes.POINTER(ctypes.c_ulonglong),ctypes.POINTER(ctypes.c_ulonglong)]
k.CreateFileW.argtypes=[w.LPCWSTR,w.DWORD,w.DWORD,ctypes.c_void_p,w.DWORD,w.DWORD,w.HANDLE];k.CreateFileW.restype=w.HANDLE
k.GetFileInformationByHandleEx.argtypes=[w.HANDLE,ctypes.c_int,ctypes.c_void_p,w.DWORD];k.CloseHandle.argtypes=[w.HANDLE]
class Standard(ctypes.Structure):_fields_=[('allocated',ctypes.c_longlong),('size',ctypes.c_longlong),('links',w.DWORD),('delete',w.BYTE),('directory',w.BYTE)]
def free():
 available,total,remaining=ctypes.c_ulonglong(),ctypes.c_ulonglong(),ctypes.c_ulonglong()
 if not k.GetDiskFreeSpaceExW(str(run.anchor),ctypes.byref(available),ctypes.byref(total),ctypes.byref(remaining)):raise ctypes.WinError(ctypes.get_last_error())
 return remaining.value
def allocation(path):
 handle=k.CreateFileW(str(path),0,7,None,3,0,None)
 if handle==ctypes.c_void_p(-1).value:return 0
 try:
  info=Standard()
  if not k.GetFileInformationByHandleEx(handle,1,ctypes.byref(info),ctypes.sizeof(info)):return 0
  return info.allocated
 finally:k.CloseHandle(handle)
baseline=free();state=dict(schema=1,started=datetime.datetime.now(datetime.timezone.utc).isoformat(),run=str(run),root=str(root),installer=str(a.installer),drive=str(run.anchor),baseline_free_bytes=baseline,volume_sample_interval_seconds=.25,tree_sample_interval_seconds=15,volume_peak_delta_bytes=0,tree_peak_unique_allocated_bytes=0,tree_peak_logical_bytes=0,events=[],status='running')
lock=threading.Lock()
def save():
 with lock:
  pending=report.with_suffix('.pending');pending.write_text(json.dumps(state,indent=2)+'\n');pending.replace(report)
def event(name):
 state['phase']=name;state['events'].append(dict(seconds=round(time.monotonic()-start,3),phase=name));save()
def volume_monitor():
 with (run/'volume-samples.jsonl').open('w',buffering=1) as log:
  while not stop.is_set():
   remaining=free();delta=baseline-remaining;seconds=round(time.monotonic()-start,3)
   if delta>state['volume_peak_delta_bytes']:state.update(volume_peak_delta_bytes=delta,volume_peak_seconds=seconds,volume_peak_phase=state.get('phase'))
   log.write(json.dumps(dict(seconds=seconds,free_bytes=remaining,delta_bytes=delta,phase=state.get('phase')))+'\n');stop.wait(.25)
def snapshot():
 seen=set();logical=allocated=files=0;groups={};pending=[root]
 while pending:
  directory=pending.pop()
  try:
   with os.scandir(directory) as entries:
    for entry in entries:
     try:
      if entry.is_symlink():continue
      if entry.is_dir(follow_symlinks=False):pending.append(Path(entry.path));continue
      if not entry.is_file(follow_symlinks=False):continue
      # DirEntry.stat() reports inode zero on Windows; os.stat() obtains the
      # real file ID needed to count NTFS hard-linked payloads just once.
      st=os.stat(entry.path,follow_symlinks=False);files+=1;logical+=st.st_size
      relative=Path(entry.path).relative_to(root);group='/'.join(relative.parts[:3]) if relative.parts[0]=='Development' else relative.parts[0]
      row=groups.setdefault(group,dict(logical_bytes=0,unique_allocated_bytes=0,files=0));row['logical_bytes']+=st.st_size;row['files']+=1
      key=(st.st_dev,st.st_ino)
      if key not in seen:
       seen.add(key);n=allocation(entry.path);allocated+=n;row['unique_allocated_bytes']+=n
     except (OSError,ValueError):continue
  except OSError:continue
 data=dict(seconds=round(time.monotonic()-start,3),phase=state.get('phase'),logical_bytes=logical,unique_allocated_bytes=allocated,files=files,groups=groups)
 state['tree_peak_logical_bytes']=max(logical,state['tree_peak_logical_bytes'])
 if allocated>state['tree_peak_unique_allocated_bytes']:state.update(tree_peak_unique_allocated_bytes=allocated,tree_peak_snapshot=data)
 return data
def tree_monitor():
 with (run/'tree-samples.jsonl').open('w',buffering=1) as log:
  while not stop.is_set():log.write(json.dumps(snapshot())+'\n');save();stop.wait(15)
threads=[threading.Thread(target=f,daemon=True) for f in (volume_monitor,tree_monitor)]
for t in threads:t.start()
env=dict(os.environ,TEMP=str(root/'Development/Temp'),TMP=str(root/'Development/Temp'),TMPDIR=str(root/'Development/Temp'),PYTHONDONTWRITEBYTECODE='1')
(root/'Development/Temp').mkdir(parents=True)
def execute(command,name):
 event(name)
 with (run/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(x) for x in command],cwd=root,env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
 if result.returncode:raise RuntimeError(f'{name} failed with exit {result.returncode}; see logs')
try:
 event('copy-installer');shutil.copy2(a.installer,root/'OpenHW-Setup.exe')
 with (root/'OpenHW-Setup.exe').open('rb') as f:state['installer_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
 execute([root/'OpenHW-Setup.exe','--verify-payload'],'extract-and-verify')
 command=[root/'Development/Installer/OpenHW-SetupEngine.exe','--collection',a.collection]
 execute(command,'full-install')
 receipt=json.loads((root/'setup-complete.json').read_text());state.update(status='passed',setup_receipt=receipt)
except BaseException as error:
 state.update(status='failed',error=str(error));(run/'failure.txt').write_text(traceback.format_exc())
finally:
 event('final-measurement');stop.set()
 for t in threads:t.join()
 state.update(final=snapshot(),final_volume_delta_bytes=baseline-free(),elapsed_seconds=round(time.monotonic()-start,3),finished=datetime.datetime.now(datetime.timezone.utc).isoformat());save()
 print(json.dumps({key:state[key] for key in ('status','run','elapsed_seconds','volume_peak_delta_bytes','tree_peak_unique_allocated_bytes','tree_peak_logical_bytes')},indent=2),flush=True)
 if state['status']!='passed':raise SystemExit(1)
