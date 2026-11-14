import sys,struct,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'server'))
from lobby import Lobby,Member,LobbyError
from protocol import ROOM_SUMMARY

class DirectoryTests(unittest.TestCase):
 def setUp(self):self.lobby=Lobby({'test-map':('Test map',8)},b'x'*32)
 def rows(self):
  raw=self.lobby.directory();count=struct.unpack_from('<I',raw)[0]
  self.assertEqual(len(raw),4+count*ROOM_SUMMARY.size)
  return [ROOM_SUMMARY.unpack_from(raw,4+i*ROOM_SUMMARY.size) for i in range(count)]
 def test_empty_directory(self):self.assertEqual(self.rows(),[])
 def test_all_sixteen_rooms_are_listed_with_state_and_counts(self):
  for i in range(16):self.lobby.create(Member('Host'+str(i),b'x'*32))
  self.assertEqual(len(self.rows()),16)
  self.assertEqual([r[0] for r in self.rows()],sorted(r[0] for r in self.rows()))
  with self.assertRaises(LobbyError):self.lobby.create(Member('Too many',b'x'*32))
 def test_capacity_and_loading_are_taken_from_room(self):
  self.lobby=Lobby({'small-map':('Small map',2)},b'x'*32)
  room=self.lobby.create(Member('Host',b'x'*32));room.add_member(Member('Guest',b'x'*32));room.state=1
  row=self.rows()[0];self.assertEqual(row[3:],(2,0,1,2));self.assertEqual(ROOM_SUMMARY.size,124)
 def test_room_removal_updates_next_snapshot(self):
  room=self.lobby.create(Member('Host',b'x'*32));self.assertEqual(len(self.rows()),1)
  del self.lobby.rooms[room.code];self.assertEqual(self.rows(),[])
if __name__=='__main__':unittest.main()
