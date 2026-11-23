"""Administrator CLI. Optional SSH uses your existing SSH config, never credentials in source."""
import argparse,json,os,shlex,subprocess,sys
from pathlib import Path
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ssh',help='Existing SSH alias, supplied by the operator')
 p.add_argument('--remote-source',default='/opt/openhw-ai-lab/tools/ai-lab/control.py')
 p.add_argument('--config',default='/etc/openhw-ai-lab/config.json')
 p.add_argument('action',choices=('start','status','stop','report'))
 p.add_argument('--games',type=int,default=10);p.add_argument('--seed',type=int,default=20261123)
 p.add_argument('--difficulty',type=int,choices=range(4),default=2);p.add_argument('--sim-seconds',type=int,default=7200)
 p.add_argument('--wall-seconds',type=int,default=21600);a=p.parse_args()
 if a.ssh:
  args=['python3',a.remote_source,'--config',a.config,a.action,'--games',str(a.games),'--seed',str(a.seed),'--difficulty',str(a.difficulty),'--sim-seconds',str(a.sim_seconds),'--wall-seconds',str(a.wall_seconds)]
  return subprocess.run(['ssh',a.ssh,shlex.join(args)]).returncode
 settings=json.loads(Path(a.config).read_text(encoding='utf-8'));root=Path(settings['results_root'])
 if a.action=='start':
  if not 1<=a.games<=10 or min(a.sim_seconds,a.wall_seconds)<=0:raise ValueError('Invalid batch limits')
  state=subprocess.run(['systemctl','is-active','--quiet','openhw-ai-lab.service']).returncode
  if state==0:raise RuntimeError('An AI laboratory batch is already running')
  root.mkdir(parents=True,exist_ok=True);(root/'STOP').unlink(missing_ok=True)
  subprocess.run(['systemctl','reset-failed','openhw-ai-lab.service'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  command=['systemd-run','--unit=openhw-ai-lab','--collect','--property=User=openhw-ai-lab',
    '--property=CPUQuota=250%','--property=MemoryHigh=8G','--property=MemoryMax=10G','--property=MemorySwapMax=8G','--property=TasksMax=4096',
    '--property=KillMode=control-group','--property=TimeoutStopSec=20','--property=UMask=0077',
    '--property=StandardOutput=null','--property=StandardError=null','--property=ProtectSystem=strict',
    '--property=ProtectHome=yes','--property=PrivateTmp=yes',
    '--property=ReadWritePaths='+str(root)+' '+settings['worker_environment']['WINEPREFIX']+' '+settings['worker_environment']['HOME'],
    sys.executable,str(Path(__file__).with_name('lab.py')),'--config',a.config,'--games',str(a.games),'--seed',str(a.seed),
    '--difficulty',str(a.difficulty),'--sim-seconds',str(a.sim_seconds),'--wall-seconds',str(a.wall_seconds)]
  print(f'Starting {a.games} isolated 4v4 AI games; difficulty {a.difficulty}; seed {a.seed}.',flush=True)
  subprocess.run(command,check=True)
 elif a.action=='stop':
  (root/'STOP').touch();print('Requested a safe stop of all AI laboratory matches.')
 elif a.action=='status':
  runs=sorted(root.glob('*/status.json'))
  print(runs[-1].read_text() if runs else 'No laboratory runs recorded.')
  subprocess.run(['systemctl','show','openhw-ai-lab.service','-p','ActiveState','-p','MemoryCurrent','-p','CPUUsageNSec'])
 else:
  runs=sorted(root.glob('*/manifest.json'))
  if not runs:raise RuntimeError('No laboratory results recorded')
  manifest=json.loads(runs[-1].read_text());results=manifest.get('results',[])
  analyses=[json.loads(p.read_text()) for p in runs[-1].parent.glob('match-*/analysis.json')]
  print(json.dumps(dict(id=manifest['id'],seed=manifest['seed'],state=manifest['state'],results=results,live_analyses=analyses),indent=2))
 return 0
if __name__=='__main__':raise SystemExit(main())
