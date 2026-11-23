"""Private, bounded AI experiments. Standard-library only; no network listener."""
import argparse,asyncio,contextlib,datetime,hashlib,json,os,random,shutil,signal,struct,sys,time,uuid
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'server'))
from protocol import default_config,pack_config,LOAD_PREFIX,HEADER,VERSION,MAX_MESSAGE,envelope,WORKER_INIT,WORKER_READY,WORKER_STEP,WORKER_SYNC,STOP
INSPECT=206
HARD=2
MAX_BYTES=100_000_000_000 # decimal GB, shared across ALL retained runs
CONTROL_RESERVE=1_000_000

class QuotaExceeded(RuntimeError):pass
class Budget:
 def __init__(self,root,limit=MAX_BYTES):
  self.root=Path(root).resolve();self.root.mkdir(parents=True,exist_ok=True)
  self.limit=min(int(limit),MAX_BYTES);self.used=0
  if self.limit<CONTROL_RESERVE*2:raise ValueError('Budget too small')
  for p in self.root.rglob('*'):
   if p.is_symlink():raise ValueError('Results directory must not contain symlinks')
   if p.is_file():self.used+=p.stat().st_size
  if self.used>self.limit-CONTROL_RESERVE:raise QuotaExceeded('Retained logs already fill the shared budget')
 def path(self,name):
  p=(self.root/name).resolve()
  if not p.is_relative_to(self.root):raise ValueError('Unsafe log path')
  p.parent.mkdir(parents=True,exist_ok=True);return p
 def append(self,name,data,control=False):
  ceiling=self.limit if control else self.limit-CONTROL_RESERVE
  if self.used+len(data)>ceiling:raise QuotaExceeded('Shared diagnostic budget reached; stopping every worker')
  # Single event-loop writer: reserve before writing and count partial I/O too.
  self.used+=len(data);p=self.path(name)
  with p.open('ab',buffering=0) as f:
   view=memoryview(data)
   while view:
    n=f.write(view)
    if not n:raise OSError('Short diagnostic write')
    view=view[n:]
 def json(self,name,value,control=False):
  p=self.path(name);old=p.stat().st_size if p.exists() else 0
  pending=str(name)+'.pending'
  stale=self.path(pending)
  if stale.exists():self.used-=stale.stat().st_size;stale.unlink()
  data=(json.dumps(value,ensure_ascii=False,indent=2,allow_nan=False)+'\n').encode('utf-8')
  self.append(pending,data,control);os.replace(self.path(pending),p);self.used-=old

def scenarios(maps,games,seed,difficulty=HARD):
 if not 1<=games<=10:raise ValueError('Choose 1 through 10 games')
 eligible=sorted(k for k,v in maps.items() if v[0]<=8<=v[1])
 if not eligible:raise ValueError('No installed eight-player maps')
 rng=random.Random(seed);result=[]
 for i in range(games):
  config=default_config(rng.choice(eligible));teams=[1]*4+[2]*4;rng.shuffle(teams)
  config['flags'] |= (1<<11)|(1<<14) # team victory and ordinary hyperspace tactics
  # Race mixes and slot positions vary; no changes to costs, fleets or rules.
  for slot,team in enumerate(teams):
   race=rng.randrange(2)
   config['slots'][slot]=dict(kind=2,race=race,difficulty=difficulty,team=team,ready=1,
      base=rng.randrange(0x202020,0xc0c0c0),stripe=0xff6040 if team==1 else 0x40b0ff,
      emission=0xff6040 if team==1 else 0x40b0ff,name=f'{("Easy","Normal","Hard","Expert")[difficulty]} AI {slot+1}')
  result.append(dict(index=i,seed=rng.randrange(1,2**32),config=config))
 return result

async def read_worker(stream,timeout):
 raw=await asyncio.wait_for(stream.readexactly(HEADER.size),timeout)
 magic,version,kind,size=HEADER.unpack(raw)
 if magic!=b'OHMP' or version!=VERSION or size>MAX_MESSAGE:raise RuntimeError('Invalid private worker message')
 return kind,await asyncio.wait_for(stream.readexactly(size),timeout)

class Analysis:
 """Flag observations, not presumed causes. Full telemetry remains available."""
 def __init__(self):
  self.players={};self.ships={};self.last_frame=0;self.world={};self.team_moves={};self.completed_frame=-1
 def feed(self,line):
  if not line.startswith(b'AI_LAB\t'):return
  try:r=json.loads(line[7:])
  except (ValueError,UnicodeError):return
  kind=r.get('kind');self.last_frame=r.get('frame',self.last_frame)
  if kind=='world':self.world=r
  elif kind=='snapshot_end':self.completed_frame=r['frame']
  elif kind=='player':
   p=r['player'];old=self.players.get(p,{});changed=any(old.get(k)!=r.get(k) for k in ('ru','ships','tech','spent'))
   r['last_economic_change']=r['frame'] if changed else old.get('last_economic_change',r['frame']);self.players[p]=r
  elif kind=='ship':
   key=r['id'];old=self.ships.get(key)
   moved=not old or any(abs(a-b)>20 for a,b in zip(old['position'],r['position'])) or old['order']!=r['order']
   r['last_activity']=r['frame'] if moved else old.get('last_activity',r['frame'])
   self.ships[key]=r
  elif kind=='team':
   key=(r['player'],r['id']);old=self.team_moves.get(key)
   r['since']=r['frame'] if not old or old['move']!=r['move'] or old['ships']!=r['ships'] else old['since'];self.team_moves[key]=r
 def summary(self):
  frame=self.last_frame;recent=[s for s in self.ships.values() if s['frame']==frame]
  return dict(last_frame=frame,simulated_seconds=frame/32,world=self.world,players=list(self.players.values()),
   observations=dict(invalid_positions=[s['id'] for s in recent if not s['position_valid']],
    stationary_120s=[dict(id=s['id'],player=s['player'],type=s['type'],order=s['order']) for s in recent if frame-s['last_activity']>=3840][:200],
    unchanged_economy_180s=[p['player'] for p in self.players.values() if frame-p['last_economic_change']>=5760 and p['alive']],
    unchanged_team_orders_180s=[dict(player=p,id=i,move=t['move'],ships=len(t['ships'])) for (p,i),t in self.team_moves.items() if t['frame']==frame and frame-t['since']>=5760][:200]),
   interpretation='Flags are observations requiring review, not automatic proof of a bug.')

async def run_batch(settings,options):
 import fcntl
 root=Path(settings['results_root']).resolve();root.mkdir(parents=True,exist_ok=True);root.chmod(0o700)
 lock=(root/'.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 budget=Budget(root,settings.get('max_log_bytes',MAX_BYTES));stop=asyncio.Event();children=set()
 runid=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]
 plans=scenarios(settings['maps'],options.games,options.seed,options.difficulty)
 manifest=dict(id=runid,seed=options.seed,games=plans,difficulty=options.difficulty,
  simulated_seconds_limit=options.sim_seconds,wall_seconds_limit=options.wall_seconds,
  snapshot_sim_seconds=options.snapshot_seconds,ticks_per_second=64,ticks_per_frame=2,
  acceleration='Fixed original ticks, no wall-clock pacing; no altered time scale.',
  started=datetime.datetime.now(datetime.timezone.utc).isoformat(),state='running',results=[],
  worker_sha256=hashlib.sha256(Path(settings['worker_executable']).read_bytes()).hexdigest())
 budget.json(runid+'/manifest.json',manifest);init_gate=asyncio.Semaphore(2)
 loop=asyncio.get_running_loop()
 for sig in (signal.SIGTERM,signal.SIGINT):loop.add_signal_handler(sig,stop.set)
 started=time.monotonic();status_path=runid+'/status.json';active={}
 async def capture(process,name,analysis):
  pending=b'';part=0;part_bytes=0
  while True:
   chunk=await process.stderr.read(65536)
   if not chunk:break
   if part_bytes+len(chunk)>64*1024*1024:part+=1;part_bytes=0
   try:budget.append(name+f'/engine-{part:05d}.log',chunk);part_bytes+=len(chunk)
   except (QuotaExceeded,OSError):stop.set();raise
   pending+=chunk
   lines=pending.split(b'\n');pending=lines.pop()
   if len(pending)>65536:pending=pending[-65536:]
   for line in lines:analysis.feed(line)
 async def game(plan):
  name=runid+'/match-'+str(plan['index']+1).zfill(2);analysis=Analysis();process=None;capture_task=None;frame=0;reason='simulation_limit';commands=0;begun=time.monotonic()
  try:
   async with init_gate:
    if stop.is_set():reason='not_started_batch_stopped';return
    # Dedicated entry bypasses local GUI/settings. All heavy assets remain
    # shared read-only; runtime files are never copied into diagnostics.
    env={**os.environ,**settings.get('worker_environment',{}),'OPENHW_AI_LAB':'1','WINEDEBUG':'-all'}
    process=await asyncio.create_subprocess_exec(*settings['worker_command'],cwd=settings['content_root'],env=env,
       stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE,limit=1024*1024,start_new_session=True)
    children.add(process);capture_task=asyncio.create_task(capture(process,name,analysis))
    load=LOAD_PREFIX.pack(plan['seed'],0,0,8,0)+pack_config(plan['config'])
    budget.append(name+'/replay.ohmp',envelope(WORKER_INIT,load));process.stdin.write(envelope(WORKER_INIT,load));await process.stdin.drain()
    kind,ready=await read_worker(process.stdout,600)
    if kind!=WORKER_READY or len(ready)!=48:raise RuntimeError('Lab worker failed initialization')
    facts=struct.unpack('<10I',ready[:40])
    if facts[1]!=8 or list(facts[2:])!=[options.difficulty+1]*8:raise RuntimeError('Expected eight genuine AI players at the requested difficulty')
    budget.json(name+'/ready.json',dict(ships=facts[0],players=facts[1],ai=facts[2:],signature=ready[40:48].hex()))
   interval=options.snapshot_seconds*32
   while frame<options.sim_seconds*32 and not stop.is_set():
    if time.monotonic()-started>options.wall_seconds:reason='wall_limit';stop.set();break
    if capture_task.done():await capture_task;raise RuntimeError('Engine log stream closed unexpectedly')
    if (root/'STOP').exists():reason='operator_stop';stop.set();break
    if frame%interval==0:
     process.stdin.write(envelope(INSPECT));await process.stdin.drain();kind,body=await read_worker(process.stdout,30)
     if kind!=INSPECT:raise RuntimeError('Lab telemetry unavailable')
     deadline=time.monotonic()+10
     while analysis.completed_frame<frame:
      if time.monotonic()>deadline:raise RuntimeError('Incomplete diagnostic snapshot')
      if capture_task.done():await capture_task;raise RuntimeError('Diagnostic stream stopped')
      await asyncio.sleep(.001)
     active[plan['index']]=dict(frame=frame,sim_seconds=frame/32,commands=commands)
     budget.json(name+'/analysis.json',analysis.summary())
    process.stdin.write(envelope(WORKER_STEP,struct.pack('<IH',frame,0)));await process.stdin.drain()
    kind,body=await read_worker(process.stdout,30)
    if kind==STOP:reason='match_ended';break
    if kind!=WORKER_SYNC:raise RuntimeError('Unexpected step response')
    budget.append(name+'/replay.ohmp',envelope(WORKER_SYNC,body));commands+=struct.unpack_from('<H',body,20)[0];frame+=1
    if frame%128==0:await asyncio.sleep(0)
   if stop.is_set() and reason=='simulation_limit':reason='batch_stopped'
  except Exception as error:
   reason='error: '+str(error)
   if isinstance(error,(QuotaExceeded,OSError)):stop.set()
  finally:
   if process:
    if process.returncode is None:
     with contextlib.suppress(Exception):process.stdin.write(envelope(STOP));await process.stdin.drain()
     try:await asyncio.wait_for(process.wait(),5)
     except TimeoutError:
      with contextlib.suppress(ProcessLookupError):os.killpg(process.pid,signal.SIGTERM)
      try:await asyncio.wait_for(process.wait(),5)
      except TimeoutError:
       with contextlib.suppress(ProcessLookupError):os.killpg(process.pid,signal.SIGKILL)
       await process.wait()
    children.discard(process)
   if capture_task:
    try:await asyncio.wait_for(capture_task,5)
    except Exception: capture_task.cancel()
   result=dict(match=plan['index']+1,frames=frame,simulated_seconds=frame/32,wall_seconds=time.monotonic()-begun,commands=commands,reason=reason,analysis=analysis.summary())
   manifest['results'].append(result);active.pop(plan['index'],None)
   budget.json(name+'/result.json',result,control=True)
 async def monitor():
  while not stop.is_set():
   if (root/'STOP').exists() or shutil.disk_usage(root).free<settings.get('min_free_bytes',5_000_000_000):stop.set()
   try:budget.json(status_path,dict(id=runid,state='running',active=active,completed=len(manifest['results']),bytes=budget.used,limit=budget.limit,wall_seconds=time.monotonic()-started),control=True)
   except (QuotaExceeded,OSError):stop.set();return
   await asyncio.sleep(2)
 async def stop_workers():
  await stop.wait()
  for process in tuple(children):
   if process.returncode is None:
    with contextlib.suppress(ProcessLookupError):os.killpg(process.pid,signal.SIGTERM)
 monitor_task=asyncio.create_task(monitor())
 stop_task=asyncio.create_task(stop_workers())
 try:await asyncio.gather(*(game(plan) for plan in plans))
 finally:
  stop.set();monitor_task.cancel()
  with contextlib.suppress(asyncio.CancelledError):await monitor_task
  await stop_task
  manifest['state']='finished';manifest['finished']=datetime.datetime.now(datetime.timezone.utc).isoformat()
  budget.json(runid+'/manifest.json',manifest,control=True)
  budget.json(status_path,dict(id=runid,state='finished',completed=len(manifest['results']),bytes=budget.used,limit=budget.limit),control=True)
  print(json.dumps(dict(id=runid,results=len(manifest['results']),bytes=budget.used)),flush=True)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--config',type=Path,required=True)
 p.add_argument('--games',type=int,default=10);p.add_argument('--seed',type=int,default=20261123)
 p.add_argument('--difficulty',type=int,choices=range(4),default=HARD);p.add_argument('--sim-seconds',type=int,default=7200)
 p.add_argument('--wall-seconds',type=int,default=21600);p.add_argument('--snapshot-seconds',type=int,default=10)
 a=p.parse_args()
 if min(a.sim_seconds,a.wall_seconds,a.snapshot_seconds)<=0:raise ValueError('Durations must be positive')
 settings=json.loads(a.config.read_text(encoding='utf-8'));asyncio.run(run_batch(settings,a))
if __name__=='__main__':main()
