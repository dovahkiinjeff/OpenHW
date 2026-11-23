import json,tempfile,unittest,subprocess,sys,os
from pathlib import Path
from lab import Budget,QuotaExceeded,MAX_BYTES,CONTROL_RESERVE,scenarios,Analysis
class LabTests(unittest.TestCase):
 def test_teams_are_reproducible_eight_hard_ai(self):
  maps={'small':[2,4],'large':[2,8]};a=scenarios(maps,10,42);self.assertEqual(a,scenarios(maps,10,42))
  self.assertNotEqual(a,scenarios(maps,10,43))
  for game in a:
   slots=game['config']['slots'];self.assertEqual([s['team'] for s in slots].count(1),4)
   self.assertEqual([s['team'] for s in slots].count(2),4)
   self.assertTrue(all(s['kind']==2 and s['difficulty']==2 for s in slots));self.assertEqual(game['config']['map'],'large')
 def test_shared_budget_counts_prior_runs_and_stops_before_limit(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'previous';p.write_bytes(b'x'*100)
   b=Budget(tmp,2_000_000);self.assertEqual(b.used,100)
   b.append('new/run',b'x'*(1_000_000-100))
   with self.assertRaises(QuotaExceeded):b.append('new/run',b'x')
   b.json('status.json',{'stopped':'quota'},control=True)
   self.assertLessEqual(sum(p.stat().st_size for p in Path(tmp).rglob('*') if p.is_file()),2_000_000)
 def test_cap_cannot_be_raised_above_100_decimal_gb(self):
  with tempfile.TemporaryDirectory() as tmp:self.assertEqual(Budget(tmp,MAX_BYTES*2).limit,MAX_BYTES)
 def test_atomic_metadata_accounting_and_path_safety(self):
  with tempfile.TemporaryDirectory() as tmp:
   b=Budget(tmp);b.json('status.json',{'a':1});b.json('status.json',{'a':123})
   self.assertEqual(b.used,(Path(tmp)/'status.json').stat().st_size)
   with self.assertRaises(ValueError):b.append('../escape',b'x')
 def test_analysis_marks_stationary_observation_without_claiming_bug(self):
  a=Analysis()
  for frame in (0,4000):a.feed(b'AI_LAB\t'+json.dumps(dict(kind='ship',frame=frame,id=1,player=0,type=2,order=0,position=[0,0,0],position_valid=1)).encode())
  a.feed(b'AI_LAB\t'+json.dumps(dict(kind='snapshot_end',frame=4000)).encode())
  self.assertEqual(len(a.summary()['observations']['stationary_120s']),1);self.assertEqual(a.completed_frame,4000)
 @unittest.skipUnless(os.name=='posix','Private runner uses Linux process groups')
 def test_ten_transport_workers_and_quota_shutdown(self):
  # A synthetic transport fixture only: never loads the game or starts AI.
  fake='''import sys,struct,json
sys.path.insert(0,sys.argv[1])
from protocol import *
def send(kind,body=b''):
 sys.stdout.buffer.write(envelope(kind,body));sys.stdout.buffer.flush()
frame=0
while True:
 header=sys.stdin.buffer.read(HEADER.size)
 if len(header)!=HEADER.size:break
 magic,v,kind,n=HEADER.unpack(header);body=sys.stdin.buffer.read(n)
 if kind==WORKER_INIT:send(WORKER_READY,struct.pack('<10IQ',8,8,*([3]*8),123))
 elif kind==206:
  for data in [dict(kind='world',frame=frame,time=frame/32,ru=1000,resources=10),dict(kind='snapshot_end',frame=frame)]:
   sys.stderr.write('AI_LAB\\t'+json.dumps(data)+'\\n')
  sys.stderr.flush();send(206,struct.pack('<I',frame))
 elif kind==WORKER_STEP:
  if sys.argv[-1]=='flood':sys.stderr.buffer.write(b'x'*200000+b'\\n');sys.stderr.buffer.flush()
  send(WORKER_SYNC,bytes(24));frame+=1
 elif kind==STOP:break
'''
  import lab
  for flood in (False,True):
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);worker=root/'fake.py';worker.write_text(fake)
    config=dict(worker_executable=str(worker),worker_command=[sys.executable,str(worker),str(Path(lab.__file__).resolve().parents[2]/'server'),'flood' if flood else 'normal'],content_root=tmp,results_root=str(root/'results'),maps={'test':[2,8]},max_log_bytes=2_000_000,min_free_bytes=0)
    path=root/'config.json';path.write_text(json.dumps(config))
    completed=subprocess.run([sys.executable,str(Path(lab.__file__).resolve()),'--config',str(path),'--games','10','--sim-seconds','1','--snapshot-seconds','1','--wall-seconds','20'],capture_output=True,text=True,timeout=35)
    self.assertEqual(completed.returncode,0,completed.stderr)
    manifest=json.loads(next((root/'results').glob('*/manifest.json')).read_text())
    self.assertEqual(len(manifest['results']),10)
    self.assertLessEqual(sum(p.stat().st_size for p in (root/'results').rglob('*') if p.is_file()),2_000_000)
    if not flood:self.assertTrue(all(r['frames']==32 for r in manifest['results']))
if __name__=='__main__':unittest.main()
