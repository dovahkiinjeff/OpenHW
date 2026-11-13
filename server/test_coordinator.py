"""Gateway lifecycle checks; worker simulation is covered by live replay."""
import asyncio
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from coordinator import Gateway
from protocol import WORKER_READY


class LifecycleTests(unittest.IsolatedAsyncioTestCase):
    async def test_cancellation_during_worker_cleanup_releases_match_capacity(self):
        waiting=asyncio.Event()

        class Worker:
            returncode=None
            stdout=None
            stdin=SimpleNamespace(write=lambda data:None)
            async def drain(self):pass
            async def wait(self):
                waiting.set()
                await asyncio.Future()
            def terminate(self):pass
            def kill(self):pass

        worker=Worker();worker.stdin.drain=worker.drain
        room=SimpleNamespace(code='TEST01',state=0,members={},config={'slots':[1,2]},
            worker=None,load_payload=lambda:b'')
        with tempfile.TemporaryDirectory(dir='.') as temporary:
            gateway=Gateway(dict(maps={'Test%d':(2,8)},fingerprint='78'*32,
                log_dir=temporary,worker_command=['unused'],game_directory='.'))
            async def ready(stream,timeout):
                return WORKER_READY,struct.pack('<10IQ',0,2,*([0]*8),1)
            async def spawn(*args,**kwargs):return worker
            with patch('coordinator.asyncio.create_subprocess_exec',spawn),patch('coordinator.read_worker',ready):
                task=asyncio.create_task(gateway.match(room))
                await asyncio.wait_for(waiting.wait(),1)
                self.assertEqual(gateway.active,1)
                task.cancel()
                with self.assertRaises(asyncio.CancelledError):await task
                self.assertEqual(gateway.active,0)


if __name__=='__main__':unittest.main()
