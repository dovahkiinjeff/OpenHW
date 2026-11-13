"""The production gateway must advance game time at real-time speed."""
import asyncio
from pathlib import Path
import re
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch
import coordinator
from protocol import WORKER_READY, WORKER_SYNC


class ClockTests(unittest.IsolatedAsyncioTestCase):
    async def test_production_step_loop_tracks_engine_seconds_including_work_time(self):
        now=1000.0
        frames=0
        initialized=False
        sleeps=[]
        worker=SimpleNamespace(returncode=0,stdout=None,
            stdin=SimpleNamespace(write=lambda _:None,drain=AsyncMock()))
        member=SimpleNamespace(loaded=True,signature=struct.pack('<Q',1),
            connection=SimpleNamespace(send=AsyncMock()))
        room=SimpleNamespace(code='CLOCK1',state=1,members={0:member},
            config={'slots':[1,2]},worker=None,load_payload=lambda *args:b'',
            paused=False,commands=[])
        async def read_worker(*args):
            nonlocal initialized,now,frames
            if not initialized:
                initialized=True
                return WORKER_READY,struct.pack('<10IQ',0,2,*([0]*8),1)
            # Include simulated CPU/pipe work in the wall clock. Waiting a fixed
            # period AFTER work would quietly slow the game as CPU cost rises.
            now+=.005
            frames+=1
            if frames==128:room.state=3
            return WORKER_SYNC,bytes(24)
        async def sleep(seconds):
            nonlocal now
            self.assertGreater(seconds,0)
            sleeps.append(seconds);now+=seconds
        with tempfile.TemporaryDirectory() as folder:
            gateway=coordinator.Gateway(dict(maps={'Test%d':(2,8)},fingerprint='78'*32,
                log_dir=folder,worker_command=['unused'],game_directory='.'))
            gateway.broadcast=AsyncMock()
            with patch.object(coordinator.asyncio,'create_subprocess_exec',AsyncMock(return_value=worker)), \
                 patch.object(coordinator,'read_worker',read_worker), \
                 patch.object(coordinator,'time',SimpleNamespace(monotonic=lambda:now,time=lambda:now)), \
                 patch.object(coordinator.asyncio,'sleep',sleep):
                await gateway.match(room)
        simulated_seconds=frames*2/64
        self.assertEqual(frames,128)
        self.assertAlmostEqual(now-1000.0,simulated_seconds,places=8)
        self.assertEqual(len(sleeps),128)
        self.assertEqual(gateway.active,0)

    def test_packet_schedule_matches_current_engine_physics_contract(self):
        repo=Path(__file__).resolve().parents[1]
        header=(repo/'src/Game/Universe.h').read_text(encoding='utf-8')
        source=(repo/'src/Game/Universe.c').read_text(encoding='utf-8')
        base=int(re.search(r'#define\s+UNIVERSE_UPDATE_RATE_ORIG\s+(\d+)',header)[1])
        shift=int(re.search(r'UNIVERSE_UPDATE_RATE_SHIFT\s*=\s*(\d+)',source)[1])
        self.assertEqual(coordinator.PHYSICS_TICKS_PER_SECOND,base*(1<<shift))
        self.assertEqual(coordinator.PHYSICS_TICKS_PER_SYNC,2)
        self.assertAlmostEqual(coordinator.SYNC_PERIOD,1/32)


if __name__=='__main__':unittest.main()
