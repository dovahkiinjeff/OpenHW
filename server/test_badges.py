import asyncio
import struct
import unittest
from websockets.asyncio.server import serve
from websockets.asyncio.client import connect
from coordinator import Gateway
from protocol import *

class BadgeTests(unittest.IsolatedAsyncioTestCase):
    def test_badge_bounds(self):
        image=struct.pack('<HH',96,96)+bytes(96*96*4)
        self.assertEqual(validate_badge(image),image)
        self.assertEqual(validate_badge(b''),b'')
        for bad in (b'x',image[:-1],image+b'x',struct.pack('<HH',65535,96)+image[4:]):
            with self.assertRaises(ValueError):validate_badge(bad)

    async def test_real_transport_join_and_leave_badges(self):
        gateway=Gateway(dict(maps={'Test%d':(2,8)},fingerprint='78'*32))
        async def receive(ws,kind,check=lambda p:True):
            async with asyncio.timeout(3):
                while True:
                    t,p=unpack(await ws.recv())
                    if t==ERROR:self.fail(p.decode())
                    if t==kind and check(p):return p
        async with serve(gateway.handler,'127.0.0.1',0,compression=None,max_size=MAX_MESSAGE+HEADER.size) as server:
            port=server.sockets[0].getsockname()[1];url=f'ws://127.0.0.1:{port}/multiplayer/v1'
            async with connect(url,compression=None) as a,connect(url,compression=None) as b:
                red=struct.pack('<HH',96,96)+bytes([255,0,0,128])*(96*96)
                green=struct.pack('<HH',96,96)+bytes([0,255,0,255])*(96*96)
                for ws,name,image in [(a,'Alpha',red),(b,'Beta',green)]:
                    await ws.send(envelope(HELLO_MSG,HELLO.pack(encode_text(name,48),bytes.fromhex('78'*32))))
                    await receive(ws,WELCOME);await ws.send(envelope(BADGE_UPLOAD,image))
                await a.send(envelope(CREATE));lobby=await receive(a,LOBBY);code=lobby[:8].split(b'\0')[0]
                self.assertEqual(await receive(a,BADGE,lambda p:p[0]==0),b'\0'+red)
                await b.send(envelope(JOIN,code.ljust(8,b'\0')))
                self.assertEqual(await receive(b,BADGE,lambda p:p[0]==0),b'\0'+red)
                self.assertEqual(await receive(b,BADGE,lambda p:p[0]==1),b'\1'+green)
                self.assertEqual(await receive(a,BADGE,lambda p:p[0]==1 and len(p)>1),b'\1'+green)
                await b.send(envelope(LEAVE));await receive(b,WELCOME)
                self.assertEqual(await receive(a,BADGE,lambda p:p[0]==1),b'\1')

if __name__=='__main__':unittest.main()

