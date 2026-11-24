"""Exercise map discovery and host map changes over the real lobby transport."""
import asyncio
from copy import deepcopy
import struct
import unittest
from websockets.asyncio.server import serve
from websockets.asyncio.client import connect
from coordinator import Gateway
from protocol import *

class MapCatalogTransportTests(unittest.IsolatedAsyncioTestCase):
    async def test_host_selects_server_map_and_guest_cannot_change_it(self):
        maps={'Large%d':(2,8),'Small%d':(2,2),'(null)':(0,0)}
        gateway=Gateway(dict(maps=maps,fingerprint='78'*32))
        async def receive(ws,kind,predicate=lambda p:True):
            async with asyncio.timeout(3):
                while True:
                    t,payload=unpack(await ws.recv())
                    if t==ERROR and kind!=ERROR:self.fail(payload.decode())
                    if t==kind and predicate(payload):return payload
        def config(payload):return parse_config(payload[LOBBY_PREFIX.size:],maps)
        async with serve(gateway.handler,'127.0.0.1',0,compression=None,max_size=MAX_MESSAGE+HEADER.size) as server:
            port=server.sockets[0].getsockname()[1]
            async with connect(f'ws://127.0.0.1:{port}/multiplayer/v1',compression=None) as host,connect(f'ws://127.0.0.1:{port}/multiplayer/v1',compression=None) as guest:
                for ws,name in ((host,'Host'),(guest,'Guest')):
                    await ws.send(envelope(HELLO_MSG,HELLO.pack(encode_text(name,48),bytes.fromhex('78'*32))))
                    await receive(ws,WELCOME)
                    catalog=await receive(ws,MAP_CATALOG)
                    self.assertEqual(struct.unpack_from('<I',catalog)[0],2)
                    self.assertEqual(text(MAP_SUMMARY.unpack_from(catalog,4)[0],63),'Large%d')
                await host.send(envelope(CREATE));state=await receive(host,LOBBY)
                await guest.send(envelope(JOIN,state[:8]));await receive(guest,LOBBY)
                state=await receive(host,LOBBY,lambda p:sum(s is not None for s in config(p)['slots'])==2)
                selected=deepcopy(config(state));selected['map']='Small%d'
                revision=LOBBY_PREFIX.unpack_from(state)[1]
                await host.send(envelope(CONFIGURE,struct.pack('<I',revision)+pack_config(selected)))
                updated=await receive(guest,LOBBY,lambda p:config(p)['map']=='Small%d')
                self.assertGreater(LOBBY_PREFIX.unpack_from(updated)[1],revision)
                self.assertFalse(any(s['ready'] for s in config(updated)['slots'] if s))
                selected['map']='Large%d'
                await guest.send(envelope(CONFIGURE,struct.pack('<I',revision+1)+pack_config(selected)))
                self.assertIn(b'Only the host',await receive(guest,ERROR))

if __name__=='__main__':unittest.main()
