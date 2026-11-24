import struct
import unittest
from copy import deepcopy
from lobby import Lobby, LobbyError
from protocol import *

class LobbyTests(unittest.TestCase):
    def setUp(self):
        self.lobby=Lobby({'GardenSalad%d':(2,8),'Small%d':(2,2)},b'x'*32)
        self.host=self.member('Host');self.guest=self.member('Guest')
        self.room=self.lobby.create(self.host)
    def member(self,name):return self.lobby.hello(HELLO.pack(encode_text(name,48),b'x'*32))
    def configure(self,config):self.room.configure(self.host,struct.pack('<I',self.room.revision)+pack_config(config))
    def test_protocol_rejects_bad_lengths_and_versions(self):
        for message in (b'',envelope(READY,b'1')+b'x',b'XXXX'+envelope(READY)[4:],HEADER.pack(b'OHMP',2,READY,0)):
            with self.assertRaises(ValueError):unpack(message)
    def test_add_ai_is_host_only_and_preserves_difficulty_team(self):
        self.room.add_member(self.guest)
        with self.assertRaises(LobbyError):self.room.add_ai(self.guest,b'\x01\x02\x02')
        self.room.add_ai(self.host,b'\x01\x02\x02')
        ai=self.room.config['slots'][2]
        self.assertEqual((ai['kind'],ai['race'],ai['difficulty'],ai['team']),(2,1,2,2))
        self.assertEqual(parse_config(pack_config(self.room.config),self.lobby.maps),self.room.config)
    def test_server_catalog_ignores_unused_native_slots(self):
        lobby=Lobby({'GardenSalad%d':(2,6),'Small%d':(2,2),'(null)':(0,0)},b'x'*32)
        data=lobby.map_catalog();count=struct.unpack_from('<I',data)[0]
        self.assertEqual(count,2)
        rows=[MAP_SUMMARY.unpack_from(data,4+i*MAP_SUMMARY.size) for i in range(count)]
        self.assertEqual([(text(name,63),lo,hi,reserved) for name,lo,hi,reserved in rows],
                         [('GardenSalad%d',2,6,0),('Small%d',2,2,0)])
        self.assertNotIn('(null)',lobby.maps)

    def test_host_changes_maps_and_guests_receive_new_revision(self):
        self.room.add_member(self.guest)
        before=self.room.revision
        c=deepcopy(self.room.config);c['map']='Small%d';self.configure(c)
        self.assertEqual(self.room.config['map'],'Small%d')
        self.assertEqual(self.room.revision,before+1)
        state=self.room.snapshot(self.guest)
        self.assertEqual(parse_config(state[LOBBY_PREFIX.size:],self.lobby.maps)['map'],'Small%d')
        self.assertFalse(self.room.config['slots'][0]['ready'])
        c['map']='GardenSalad%d'
        with self.assertRaises(LobbyError):self.room.configure(self.guest,struct.pack('<I',self.room.revision)+pack_config(c))

    def test_map_capacity(self):
        c=deepcopy(self.room.config);c['map']='Small%d';self.configure(c)
        self.room.add_ai(self.host,b'\0\1\0')
        with self.assertRaises(LobbyError):self.room.add_member(self.guest)
    def test_only_host_changes_rules_and_teams(self):
        self.room.add_member(self.guest)
        c=deepcopy(self.room.config);c['rules'][0]=1;c['slots'][0]['team']=1;c['slots'][1]['team']=2
        with self.assertRaises(LobbyError):self.room.configure(self.guest,struct.pack('<I',self.room.revision)+pack_config(c))
        self.configure(c)
        self.assertEqual(self.room.config['rules'][0],1)
        self.assertEqual(self.room.config['slots'][1]['team'],2)
    def test_changed_settings_invalidate_ready_and_stale_revision(self):
        self.room.ready(self.host,struct.pack('<IB',self.room.revision,1));old=self.room.revision
        self.room.add_ai(self.host,b'\0\1\0')
        self.assertFalse(self.room.config['slots'][0]['ready'])
        with self.assertRaises(LobbyError):self.room.ready(self.host,struct.pack('<IB',old,1))
    def test_requires_opponents_and_freezes_setup(self):
        self.room.add_ai(self.host,b'\1\2\1')
        c=deepcopy(self.room.config);c['slots'][0]['team']=1;self.configure(c)
        self.room.ready(self.host,struct.pack('<IB',self.room.revision,1))
        with self.assertRaises(LobbyError):self.room.begin(self.host)
        c=deepcopy(self.room.config);c['slots'][1]['team']=2;self.configure(c)
        self.room.ready(self.host,struct.pack('<IB',self.room.revision,1));self.room.begin(self.host)
        with self.assertRaises(LobbyError):self.room.add_ai(self.host,b'\0\1\0')
        with self.assertRaises(LobbyError):self.configure(c)
    def test_humans_compact_before_ai_without_switching_team(self):
        self.room.add_ai(self.host,b'\1\2\2');self.room.add_member(self.guest)
        c=deepcopy(self.room.config);c['slots'][2]['team']=1;self.configure(c)
        for m in (self.host,self.guest):self.room.ready(m,struct.pack('<IB',self.room.revision,1))
        self.room.begin(self.host)
        self.assertEqual(self.guest.slot,1)
        self.assertEqual([s['kind'] for s in self.room.config['slots'][:3]],[1,1,2])
        self.assertEqual([s['team'] for s in self.room.config['slots'][:3]],[0,1,2])
    def test_rejects_forged_human_slots_and_invalid_teams(self):
        c=deepcopy(self.room.config);c['slots'][0]['kind']=2
        with self.assertRaises(LobbyError):self.configure(c)
        for data in (bytes([0,4,0]),bytes([2,1,0]),bytes([0,1,9]),b''):
            with self.assertRaises(LobbyError):self.room.add_ai(self.host,data)
    def test_expert_survives_configuration_roundtrip(self):
        self.room.add_ai(self.host,bytes([1,3,2]))
        c=deepcopy(self.room.config)
        self.assertEqual(c['slots'][1]['difficulty'],3)
        self.configure(c)
        self.assertEqual(self.room.config['slots'][1]['difficulty'],3)
    def test_host_transfer_and_incompatible_build(self):
        with self.assertRaises(LobbyError):self.lobby.hello(HELLO.pack(encode_text('Other',48),b'y'*32))
        self.room.add_member(self.guest);self.room.remove_member(self.host)
        self.assertIs(self.room.host,self.guest)
        self.room.add_ai(self.guest,b'\0\1\1')

if __name__=='__main__':unittest.main()
