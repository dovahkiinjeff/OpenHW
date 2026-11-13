"""Real VPS + two headless Windows replicas. No renderer or game window.

The service must be running the exact same build. Requires owned private game
data locally; this test never downloads, publishes or replaces player data.
"""
import argparse
import asyncio
import os
import math
from pathlib import Path
import struct
import subprocess
from websockets.asyncio.client import connect
from protocol import *
from probe_worker import read

async def receive(ws,kind,condition=lambda payload:True,timeout=180):
    async with asyncio.timeout(timeout):
        while True:
            actual,payload=unpack(await ws.recv())
            if actual in (ERROR,STOP):raise RuntimeError(f'Server: {payload.decode(errors="replace")}')
            if actual==kind and condition(payload):return payload

def snapshot(payload):
    return LOBBY_PREFIX.unpack_from(payload),parse_config(payload[LOBBY_PREFIX.size:],{'GardenSalad%d':(2,6)})

class Replica:
    def __init__(self,exe,root,load,log):
        self.log=open(log,'wb')
        env=dict(os.environ);env['PATH']=str(root)+os.pathsep+env['PATH'];env['HW_PORTABLE_ROOT']=str(root)
        self.process=subprocess.Popen([str(exe),'--dedicated-replica'],cwd=root,env=env,
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.log)
        self.process.stdin.write(envelope(WORKER_INIT,load));self.process.stdin.flush()
    async def request(self,kind,payload=b''):
        self.process.stdin.write(envelope(kind,payload));self.process.stdin.flush()
        return await asyncio.to_thread(read,self.process.stdout)
    async def ready(self):
        kind,ready=await asyncio.to_thread(read,self.process.stdout)
        assert kind==WORKER_READY and len(ready)==48
        return ready
    def close(self):
        if self.process.poll() is None:
            try:self.process.stdin.write(envelope(STOP));self.process.stdin.flush();self.process.wait(timeout=5)
            except (OSError,subprocess.TimeoutExpired):self.process.kill();self.process.wait()
        self.log.close()

def command(player,kind,body):
    header=bytearray(24);struct.pack_into('<HHI',header,0,0xcccc,player,0);struct.pack_into('<H',header,20,1)
    return bytes(header)+struct.pack('<H',kind|(player<<8))+body

async def inspect(replicas):
    facts=[]
    for replica in replicas:
        kind,info=await replica.request(WORKER_INSPECT);assert kind==WORKER_INSPECT
        counts=struct.unpack_from('<10I',info)
        ships=[struct.unpack_from('<4H3f2H',info,40+i*24) for i in range(counts[0])]
        facts.append((counts,ships))
    assert facts[0]==facts[1], 'Replica ship state differs'
    return facts[0]

def pilot(player,ship,sequence,action,command_mode=0,squad=0):
    frame=struct.pack('<I6H8B10f128s',sequence,ship,0xffff,0xffff,0xffff,0,0,
        action,squad,0,command_mode,0,0,0,command_mode,
        0.,0.,100000.,0.,0.,1.,1.,1500.,0.,0.,bytes(128))
    assert len(frame)==192
    return command(player,20,frame)

async def run(args):
    replicas=[]
    fingerprint=bytes.fromhex(args.fingerprint.read_text().strip())
    try:
        async with connect(args.endpoint,compression=None,max_size=65548) as host,connect(args.endpoint,compression=None,max_size=65548) as guest:
            for ws,name in ((host,'Host test'),(guest,'Guest test')):
                await ws.send(envelope(HELLO_MSG,HELLO.pack(encode_text(name,48),fingerprint)));await receive(ws,WELCOME)
            await host.send(envelope(CREATE));room=await receive(host,LOBBY)
            prefix,config=snapshot(room)
            await guest.send(envelope(JOIN,prefix[0]))
            room=await receive(host,LOBBY,lambda p:sum(s is not None for s in snapshot(p)[1]['slots'])==2)
            for difficulty,team in ((0,2),(2,2)):
                count=sum(s is not None for s in snapshot(room)[1]['slots'])
                await host.send(envelope(ADD_AI,bytes([1,difficulty,team])))
                room=await receive(host,LOBBY,lambda p:sum(s is not None for s in snapshot(p)[1]['slots'])==count+1)
            prefix,config=snapshot(room)
            config['slots'][0]['team']=config['slots'][1]['team']=1
            config['slots'][1]['race']=1
            config['rules']=[1,1,0,2,1,1,75,1,1,1,1,1,1,0]
            if args.controls:
                config['fleet']=2
                config['rules'][5]=0
            if args.finish:
                config['flags']|=2048 # allied victory after both human motherships are scuttled
            await host.send(envelope(CONFIGURE,struct.pack('<I',prefix[1])+pack_config(config)))
            room=await receive(host,LOBBY,lambda p:snapshot(p)[0][1]>prefix[1])
            prefix,config=snapshot(room)
            for ws in (host,guest):await ws.send(envelope(READY,struct.pack('<IB',prefix[1],1)))
            await receive(host,LOBBY,lambda p:all(s['ready'] for s in snapshot(p)[1]['slots'] if s))
            await host.send(envelope(START))
            loads=await asyncio.gather(receive(host,LOAD),receive(guest,LOAD))
            for index,load in enumerate(loads):(args.logs/f'live-load-{index}.bin').write_bytes(load)
            assert [LOAD_PREFIX.unpack_from(p)[1] for p in loads]==[0,1]
            for index,load in enumerate(loads):
                exe=args.second_exe if index==1 and args.second_exe else args.exe
                replicas.append(Replica(exe.resolve(),args.root.resolve(),load,args.logs/f'live-replica-{index}.log'))
            ready=await asyncio.gather(*(r.ready() for r in replicas))
            assert ready[0]==ready[1], 'Different player perspectives created different initial worlds'
            facts=await inspect(replicas)
            assert list(facts[0][2:6])==[2,1,8,4],facts[0] # allied humans vs allied AI
            for ws,value in zip((host,guest),ready):await ws.send(envelope(LOADED,value[40:48]))
            print(f'Both player perspectives loaded: {facts[0][0]} ships; allied humans vs easy + hard AI; waiting for VPS barrier',flush=True)
            pilot_ships=[];wing_members=[];wing_leader=None;wing_start=None
            if args.controls:
                assert args.frames>=192
                pilot_ships=[next(s for s in facts[1] if s[3]==owner and s[1]==19) for owner in (0,1)]
                wing_members=[s for s in facts[1] if s[3]==0 and s[1] in (1,3,6,12,15,16)][:2]
                assert len(wing_members)==2, 'Test fleet needs two fighters'
            seen_move=seen_paint=seen_spread=False
            bad=good=paint=spread=b''
            for frame in range(args.frames):
                packets=await asyncio.gather(receive(host,SYNC),receive(guest,SYNC))
                assert packets[0]==packets[1]
                assert struct.unpack_from('<I',packets[0],4)[0]==frame
                for r in replicas:
                    kind,response=await r.request(WORKER_SYNC,packets[0]);assert kind==WORKER_SYNC and response==packets[0]
                if frame==0:
                    ships=facts[1];owned=[s for s in ships if s[3]==0];enemy=next(s for s in ships if s[3]==2)
                    s=owned[0]
                    good=command(0,1,struct.pack('<6fHH',*s[4:7],s[4]+500,s[5]+300,s[6],1,s[0]))
                    bad=command(1,1,struct.pack('<6fHH',*enemy[4:7],500001,500002,500003,1,enemy[0]))
                    paint=command(0,21,struct.pack('<HHI6I',s[0],0,3,0x40c0ff,0xffc040,0xff2020,0,0,0))
                    # NetMiscCommand retains two bytes of native trailing ABI
                    # padding in sizeofNetMiscCommand for a two-ship selection.
                    spread=command(0,10,struct.pack('<IHH2H2x',2000,5,2,owned[0][0],owned[1][0]))
                    for p in (good,paint,spread):await host.send(envelope(COMMAND,p))
                    await guest.send(envelope(COMMAND,bad))
                if frame>0:
                    seen_move|=good[24:] in packets[0][24:]
                    seen_paint|=paint[24:] in packets[0][24:]
                    seen_spread|=spread[24:] in packets[0][24:]
                    assert bad[24:] not in packets[0][24:],'Server accepted control of another player\'s ship'
                if args.controls:
                    if frame==8:
                        for owner,ws in enumerate((host,guest)):
                            await ws.send(envelope(COMMAND,pilot(owner,pilot_ships[owner][0],1,1,1-owner)))
                    if frame==56:
                        current=await inspect(replicas)
                        for owner,ws in enumerate((host,guest)):
                            ship=next(s for s in current[1] if s[0]==pilot_ships[owner][0])
                            assert ship[8], ('Pilot session did not engage',owner,ship)
                            assert math.dist(ship[4:7],pilot_ships[owner][4:7])>5, ('Pilot ship did not move',owner)
                            await ws.send(envelope(COMMAND,pilot(owner,ship[0],2,0)))
                        formation=command(0,3,struct.pack('<4H',2,2,*(s[0] for s in wing_members)))
                        await host.send(envelope(COMMAND,formation))
                        print('EVE command and direct piloting moved both ships through the server',flush=True)
                    if frame==72:
                        current=await inspect(replicas)
                        member=next(s for s in current[1] if s[0]==wing_members[0][0])
                        wing_leader=member[7]
                        assert wing_leader in [s[0] for s in wing_members], ('Wing formation missing',member)
                        wing_start={s[0]:s[4:7] for s in current[1] if s[0] in [w[0] for w in wing_members]}
                        await host.send(envelope(COMMAND,pilot(0,wing_leader,3,1,squad=1)))
                    if frame==160:
                        current=await inspect(replicas)
                        for ship in current[1]:
                            if ship[0] in wing_start:
                                assert ship[8], ('Wing member not controlled',ship)
                                assert math.dist(ship[4:7],wing_start[ship[0]])>5, ('Wing member did not move',ship)
                        await host.send(envelope(COMMAND,pilot(0,wing_leader,4,0,squad=1)))
                        print('X formation leader and wing member moved together through the server',flush=True)
                if frame%32==31:print(f'{frame+1} VPS frames replayed by both replicas; checksums agree',flush=True)
            assert seen_move and seen_paint and seen_spread,(seen_move,seen_paint,seen_spread)
            await host.send(envelope(PAUSE,b'\1'))
            await asyncio.gather(receive(host,PAUSED),receive(guest,PAUSED))
            await asyncio.sleep(.3) # let a frame already in progress settle
            for ws in (host,guest):
                while True:
                    try:await asyncio.wait_for(ws.recv(),.05)
                    except TimeoutError:break
                try:await asyncio.wait_for(ws.recv(),.4)
                except TimeoutError:pass
                else:raise AssertionError('Server emitted simulation frames while paused')
            await guest.send(envelope(PAUSE,b'\0'))
            kind,reason=unpack(await asyncio.wait_for(guest.recv(),3));assert kind==ERROR and b'host' in reason
            await host.send(envelope(PAUSE,b'\0'))
            await asyncio.gather(receive(host,PAUSED),receive(guest,PAUSED))
            await receive(host,SYNC,timeout=3)
            # This gateway has no incoming game ports or P2P traffic: both
            # clients received and replayed the VPS's canonical frames.
            print('PASS: human orders, 2x formation spread, per-ship colors, ownership rejection, AI, teams, host pause and Windows/Wine replay',flush=True)
            if args.finish:
                for owner,ws in enumerate((host,guest)):
                    mothership=next(s for s in facts[1] if s[3]==owner and s[1]==19)
                    scuttle=command(owner,10,struct.pack('<IHHH2x',0,1,1,mothership[0]))
                    await ws.send(envelope(COMMAND,scuttle))
                print('Checking server victory and battle shutdown after human mothership scuttles',flush=True)
                async def until_stopped(ws):
                    async with asyncio.timeout(90):
                        while True:
                            kind,payload=unpack(await ws.recv())
                            if kind==STOP:
                                assert payload==b'Match finished',payload
                                return
                            if kind==ERROR:raise AssertionError(payload)
                await asyncio.gather(until_stopped(host),until_stopped(guest))
                print('PASS: dedicated worker completed victory and sent a clean match-finished result',flush=True)
    finally:
        for r in replicas:r.close()

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('root',type=Path);p.add_argument('fingerprint',type=Path)
    p.add_argument('--endpoint',default='wss://openhw.tech/multiplayer/v1');p.add_argument('--frames',type=int,default=256)
    p.add_argument('--second-exe',type=Path)
    p.add_argument('--controls',action='store_true')
    p.add_argument('--finish',action='store_true')
    p.add_argument('--logs',type=Path,required=True);args=p.parse_args();args.logs.mkdir(parents=True,exist_ok=True)
    asyncio.run(run(args))
