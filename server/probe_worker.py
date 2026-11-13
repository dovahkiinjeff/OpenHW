"""Run the real headless executable, never a mock renderer or a game window."""
import argparse
import os
import struct
import subprocess
from pathlib import Path
from protocol import *

def read(stream):
    header=stream.read(HEADER.size)
    if len(header)!=HEADER.size:raise RuntimeError('Worker exited before responding')
    magic,version,kind,size=HEADER.unpack(header)
    if magic!=b'OHMP' or version!=VERSION or size>MAX_MESSAGE:raise RuntimeError('Worker protocol is corrupt')
    payload=stream.read(size)
    if len(payload)!=size:raise RuntimeError('Truncated worker message')
    return kind,payload

def run(exe,root,steps,log):
    config=default_config('GardenSalad%d')
    config['slots'][0]=dict(kind=1,race=0,difficulty=1,team=1,ready=1,base=0x303030,stripe=0xff2020,emission=0xff2020,name='Probe')
    config['slots'][1]=dict(kind=2,race=1,difficulty=2,team=2,ready=1,base=0x707070,stripe=0xffc040,emission=0xffc040,name='AI Advanced')
    load=LOAD_PREFIX.pack(1,0,1,2,0)+pack_config(config)
    env=dict(os.environ);env['PATH']=str(root)+os.pathsep+env['PATH'];env['HW_PORTABLE_ROOT']=str(root)
    with open(log,'wb') as stderr:
        worker=subprocess.Popen([str(exe),'--dedicated-worker'],cwd=root,env=env,
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=stderr)
        try:
            worker.stdin.write(envelope(WORKER_INIT,load));worker.stdin.flush()
            kind,ready=read(worker.stdout)
            if kind!=WORKER_READY:raise RuntimeError(f'Expected ready, got {kind}')
            ships,players,*ai=struct.unpack('<10I',ready[:40])
            assert players==2 and ai[:2]==[0,3],(ships,players,ai)
            print(f'Real world ready: {ships} ships, {players} players, advanced AI active',flush=True)
            command_count=0;frames=[]
            for frame in range(steps):
                worker.stdin.write(envelope(WORKER_STEP,struct.pack('<IH',frame,0)));worker.stdin.flush()
                kind,sync=read(worker.stdout)
                if kind!=WORKER_SYNC:raise RuntimeError(f'Unexpected worker message {kind}')
                actual=struct.unpack_from('<I',sync,4)[0]
                assert actual==frame,(actual,frame)
                command_count+=struct.unpack_from('<H',sync,20)[0]
                frames.append(sync)
            print(f'{steps*2} simulation ticks; {command_count} server AI orders; canonical frames ordered',flush=True)
            assert command_count>0,'AI did not issue any commands'
            worker.stdin.write(envelope(STOP));worker.stdin.flush()
            assert worker.wait(timeout=10)==0
        finally:
            if worker.poll() is None:worker.kill();worker.wait()
    with open(str(log)+'.replica','wb') as stderr:
        replica=subprocess.Popen([str(exe),'--dedicated-replica'],cwd=root,env=env,
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=stderr)
        try:
            replica.stdin.write(envelope(WORKER_INIT,load));replica.stdin.flush()
            kind,replica_ready=read(replica.stdout);assert kind==WORKER_READY
            assert replica_ready[40:48]==ready[40:48], 'Initial worlds differ'
            for frame in frames:
                replica.stdin.write(envelope(WORKER_SYNC,frame));replica.stdin.flush()
                kind,response=read(replica.stdout);assert kind==WORKER_SYNC and response==frame
            replica.stdin.write(envelope(STOP));replica.stdin.flush();assert replica.wait(timeout=10)==0
            print(f'Replica replayed {steps} authoritative frames; no legacy synchronization errors',flush=True)
        finally:
            if replica.poll() is None:replica.kill();replica.wait()

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('exe',type=Path);parser.add_argument('root',type=Path)
    parser.add_argument('--steps',type=int,default=512);parser.add_argument('--log',default='worker-probe.log')
    args=parser.parse_args();run(args.exe,args.root,args.steps,args.log)
