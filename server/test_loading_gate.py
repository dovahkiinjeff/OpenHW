"""Call the actual frontend loading gate without a renderer or peer packets."""
import argparse
import os
from pathlib import Path
import subprocess
from protocol import default_config, LOAD_PREFIX, pack_config, envelope, WORKER_INIT


def run(exe,root,logs):
    logs.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ);env['PATH']=str(root)+os.pathsep+env['PATH']
    for humans in (1,2):
        config=default_config('GardenSalad%d')
        for i in range(humans+1):
            config['slots'][i]=dict(kind=1 if i<humans else 2,race=i%2,
                difficulty=1,ready=1,team=i+1,base=0xff303030,
                stripe=0xff2020ff,emission=0xff2020ff,name=f'Player {i+1}')
        load=LOAD_PREFIX.pack(1,0,humans,humans+1,0)+pack_config(config)
        result=subprocess.run([str(exe),'--dedicated-replica','--check-loading-gate'],
            cwd=root,env=env,input=envelope(WORKER_INIT,load),capture_output=True,timeout=45)
        (logs/f'loading-gate-{humans}.log').write_bytes(result.stderr)
        assert result.returncode==0,(humans,result.returncode,result.stderr[-2000:])
        assert b'dedicated local loading gate passed' in result.stderr
        print(f'PASS: real loading gate completes for {humans} human(s), with no legacy peer progress packets',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('root',type=Path)
    p.add_argument('--logs',type=Path,required=True);a=p.parse_args()
    run(a.exe.resolve(),a.root.resolve(),a.logs.resolve())
