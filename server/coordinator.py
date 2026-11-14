"""Private dedicated workers behind an outbound-only TLS WebSocket gateway.

Bind to loopback; Caddy terminates HTTPS. Worker command and working directory
come exclusively from the administrator's local configuration, never a client.
"""
import asyncio
import contextlib
import json
import logging
import os
from pathlib import Path
import struct
import time
from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed
from lobby import Lobby, LobbyError
from protocol import *

LOG=logging.getLogger('openhw.multiplayer')

# Universe.h: 16 * (1 << 2) physics ticks/second in the modern engine.
# Both workerRun and the gameplay client advance TWO physics ticks per SYNC.
# The old Classic 8 Hz packet schedule only supplied 16/64 simulated seconds
# per real second. Keep command frames at 32 Hz; never scale ship velocities.
PHYSICS_TICKS_PER_SECOND = 64
PHYSICS_TICKS_PER_SYNC = 2
SYNC_PERIOD = PHYSICS_TICKS_PER_SYNC / PHYSICS_TICKS_PER_SECOND

async def read_worker(stream, timeout):
    header=await asyncio.wait_for(stream.readexactly(HEADER.size),timeout)
    magic,version,kind,length=HEADER.unpack(header)
    if magic!=b'OHMP' or version!=VERSION or length>MAX_MESSAGE: raise ValueError('Invalid worker response')
    body=await asyncio.wait_for(stream.readexactly(length),timeout)
    return kind,body

class Gateway:
    def __init__(self, settings):
        self.settings=settings
        self.lobby=Lobby(settings['maps'],bytes.fromhex(settings['fingerprint']))
        self.connections=set();self.active=0;self.lock=asyncio.Lock()

    async def send(self, member, kind, payload=b''):
        await asyncio.wait_for(member.connection.send(envelope(kind,payload)),5)

    async def broadcast(self, room, kind=None, payload=b''):
        results=await asyncio.gather(*(self.send(m,kind,payload) if kind else
            self.send(m,LOBBY,room.snapshot(m)) for m in tuple(room.members.values())),return_exceptions=True)
        return results

    async def stop_match(self, room, reason):
        if room.state==3:return
        room.state=3
        await self.broadcast(room,STOP,reason.encode('utf-8')[:512])
        if room.worker and room.worker.returncode is None:
            with contextlib.suppress(ProcessLookupError):room.worker.terminate()
            try:await asyncio.wait_for(room.worker.wait(),5)
            except TimeoutError:
                with contextlib.suppress(ProcessLookupError):room.worker.kill()
                await room.worker.wait()

    async def match(self, room):
        worker=None;reserved=False
        try:
            async with self.lock:
                if self.active>=self.settings.get('max_matches',1): raise LobbyError('Server is running its maximum number of matches')
                self.active+=1
                reserved=True
            log_dir=Path(self.settings['log_dir']);log_dir.mkdir(parents=True,exist_ok=True)
            with (log_dir/f'{room.code}-{int(time.time())}.log').open('wb') as log:
                worker=await asyncio.create_subprocess_exec(*self.settings['worker_command'],
                    cwd=self.settings['game_directory'],env={**os.environ,**self.settings.get('worker_environment',{})},
                    stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=log)
                room.worker=worker
                worker.stdin.write(envelope(WORKER_INIT,room.load_payload()));await worker.stdin.drain()
                # Tell clients to load concurrently. No simulation ticks begin
                # until every client and the authoritative worker are ready.
                await asyncio.gather(*(self.send(m,LOAD,room.load_payload(m)) for m in room.members.values()))
                load_timeout=max(180,min(1800,self.settings.get('load_timeout_seconds',900)))
                LOG.info('Room %s loading: %d human players',room.code,len(room.members))
                kind,ready=await read_worker(worker.stdout,load_timeout)
                if kind!=WORKER_READY or len(ready)!=48:raise ValueError('Worker failed to create the match')
                players=struct.unpack_from('<I',ready,4)[0]
                if players!=sum(s is not None for s in room.config['slots']):raise ValueError('Worker player count mismatch')
                LOG.info('Room %s worker ready; waiting for clients',room.code)
                deadline=time.monotonic()+load_timeout
                while room.state==1 and not all(m.loaded for m in room.members.values()):
                    if time.monotonic()>deadline:raise LobbyError('A player did not finish loading')
                    await asyncio.sleep(.1)
                if room.state!=1:return
                if any(m.signature!=ready[40:48] for m in room.members.values()):
                    raise LobbyError('Loaded map, fleet or gameplay data differs from the server; use matching unmodified game data')
                LOG.info('Room %s loaded worlds agree; starting simulation (%d physics ticks/s, %.0f sync frames/s)',room.code,PHYSICS_TICKS_PER_SECOND,1/SYNC_PERIOD)
                room.state=2;await self.broadcast(room)
                frame=0;next_frame=time.monotonic()
                while room.state==2:
                    if room.paused:
                        await asyncio.sleep(.05);next_frame=time.monotonic();continue
                    commands=room.commands;room.commands=[]
                    body=struct.pack('<IH',frame,len(commands))
                    for player,packet in commands:body+=struct.pack('<BI',player,len(packet))+packet
                    worker.stdin.write(envelope(WORKER_STEP,body));await worker.stdin.drain()
                    kind,sync=await read_worker(worker.stdout,15)
                    if kind==STOP:
                        await self.stop_match(room,'Match finished');return
                    if kind!=WORKER_SYNC or not 24<=len(sync)<=48000:raise ValueError('Invalid simulation frame')
                    await self.broadcast(room,SYNC,sync)
                    frame+=1;next_frame+=SYNC_PERIOD
                    delay=next_frame-time.monotonic()
                    if delay>0:await asyncio.sleep(delay)
                    elif delay < -2: next_frame=time.monotonic() # do not flood catch-up frames
        except asyncio.CancelledError:
            await self.stop_match(room,'Match closed');raise
        except Exception as error:
            LOG.exception('Room %s stopped',room.code)
            await self.stop_match(room,str(error) if isinstance(error,LobbyError) else 'Dedicated simulation stopped; server log saved')
        finally:
            try:
                if worker and worker.returncode is None:
                    with contextlib.suppress(ProcessLookupError):worker.terminate()
                    with contextlib.suppress(TimeoutError):await asyncio.wait_for(worker.wait(),5)
                    if worker.returncode is None:
                        with contextlib.suppress(ProcessLookupError):worker.kill()
                        await worker.wait()
            finally:
                # The last connection can cancel a match already waiting for
                # Wine to exit. Even cancellation of that wait must release
                # the reservation, or the service stays permanently "full".
                if reserved:self.active-=1

    async def handler(self, connection):
        member=None
        if connection.request.path!='/multiplayer/v1':
            await connection.close(1008,'Unknown endpoint');return
        if len(self.connections)>=64:
            await connection.close(1013,'Server connection limit');return
        self.connections.add(connection)
        try:
            kind,payload=unpack(await asyncio.wait_for(connection.recv(),10))
            if kind!=HELLO_MSG:raise LobbyError('Send hello first')
            member=self.lobby.hello(payload);member.connection=connection
            await self.send(member,WELCOME)
            async for message in connection:
                try:
                    member.consume();kind,payload=unpack(message)
                    room=member.room
                    if kind==LIST_ROOMS:
                        if payload:raise LobbyError('Unexpected room-list payload')
                        await self.send(member,ROOM_LIST,self.lobby.directory());continue
                    elif kind==CREATE:
                        if payload:raise LobbyError('Unexpected create payload')
                        room=self.lobby.create(member)
                    elif kind==JOIN:room=self.lobby.join(member,payload)
                    elif kind==LEAVE:
                        if payload:raise LobbyError('Unexpected leave payload')
                        await self.leave(member);await self.send(member,WELCOME);continue
                    elif room is None:raise LobbyError('Create or join a room first')
                    elif kind==ADD_AI:room.add_ai(member,payload)
                    elif kind==REMOVE_SLOT:room.remove_slot(member,payload)
                    elif kind==CONFIGURE:room.configure(member,payload)
                    elif kind==READY:room.ready(member,payload)
                    elif kind==START:
                        if payload:raise LobbyError('Unexpected start payload')
                        room.begin(member);room.task=asyncio.create_task(self.match(room))
                    elif kind==LOADED:
                        if len(payload)!=8 or room.state!=1:raise LobbyError('Unexpected loaded message')
                        member.signature=payload;member.loaded=True
                        LOG.info('Room %s player slot %d finished loading',room.code,member.slot)
                    elif kind==PAUSE:
                        if member is not room.host or room.state!=2 or len(payload)!=1 or payload[0]>1:
                            raise LobbyError('Only the match host can pause or resume the battle')
                        room.paused=bool(payload[0]);await self.broadcast(room,PAUSED,payload);continue
                    elif kind==COMMAND:
                        if room.state!=2:raise LobbyError('Match is not running')
                        if not 26<=len(payload)<=32768:raise LobbyError('Invalid command size')
                        if room.paused and not room.config['rules'][1]:raise LobbyError('Orders while paused are disabled by the host')
                        if len(room.commands)>=128 or sum(len(p)+5 for _,p in room.commands)+len(payload)>40000:
                            raise LobbyError('Command queue is full')
                        # Sender comes from the authenticated session. The native
                        # worker checks packet structure and ship ownership.
                        room.commands.append((member.slot,payload));continue
                    else:raise LobbyError('Unknown request')
                    await self.broadcast(room)
                except ValueError as error:
                    await self.send(member,ERROR,str(error).encode('utf-8')[:512])
        except (ConnectionClosed,TimeoutError,ValueError) as error:
            if isinstance(error,ValueError):
                with contextlib.suppress(Exception):await connection.send(envelope(ERROR,str(error).encode('utf-8')[:512]))
        finally:
            if member:await self.leave(member)
            self.connections.discard(connection)

    async def leave(self, member):
        room=member.room
        if not room:return
        if room.state in (1,2):await self.stop_match(room,f'{member.name} disconnected; match ended')
        room.remove_member(member)
        if room.members:await self.broadcast(room)
        else:
            if room.task and not room.task.done() and not room.task.cancelling():room.task.cancel()
            self.lobby.rooms.pop(room.code,None)

async def main():
    logging.basicConfig(level=logging.INFO)
    settings=json.loads(Path(os.environ.get('OPENHW_SERVER_CONFIG','server-config.json')).read_text())
    gateway=Gateway(settings)
    async with serve(gateway.handler,'127.0.0.1',settings.get('port',8767),compression=None,
        max_size=MAX_MESSAGE+HEADER.size,max_queue=16,write_limit=131072,
        ping_interval=15,ping_timeout=30,open_timeout=10,close_timeout=5):
        LOG.info('Dedicated gateway listening on loopback')
        await asyncio.Future()

if __name__=='__main__':asyncio.run(main())
