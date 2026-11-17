from pathlib import Path
import tempfile,unittest,json,sys,hashlib
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_updater as u
class LauncherRollback(unittest.TestCase):
 def fixture(self,r):
  (r/'setup-complete.json').write_text('{}');(r/'version.json').write_text('{"version":"1.1.12"}');(r/'OpenHW.exe').write_bytes(b'new-game');(r/'Launcher.exe').write_bytes(b'keep-new-launcher');(r/'Homeworld.cfg').write_bytes(b'player-settings');(r/'SavedGames').mkdir();(r/'SavedGames/mission').write_bytes(b'player-save');(r/'new.dll').write_bytes(b'new-file')
  t=r/'Development/Updater/Transactions/test';(t/'backup').mkdir(parents=True)
  rows=[]
  for name,data in [('OpenHW.exe',b'old-game'),('version.json',b'{"version":"1.1.11"}'),('Launcher.exe',b'old-launcher')]:
   (t/'backup'/name).write_bytes(data);rows.append(dict(path=name,existed=True,old_sha256=hashlib.sha256(data).hexdigest()))
  rows.append(dict(path='new.dll',existed=False));(t/'transaction.json').write_text(json.dumps(dict(schema=1,state='complete',prior='1.1.11',target='1.1.12',files=rows)))
  return t
 def test_restore_keeps_player_files_and_current_launcher(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);t=self.fixture(r)
   with patch.object(u,'stop_if_game_running'):
    self.assertEqual(u.previous_version(r)['version'],'1.1.11');self.assertEqual(u.restore_previous_version(r,'1.1.11',lambda *a:None),'1.1.11')
   self.assertEqual((r/'OpenHW.exe').read_bytes(),b'old-game');self.assertEqual((r/'Launcher.exe').read_bytes(),b'keep-new-launcher');self.assertEqual((r/'Homeworld.cfg').read_bytes(),b'player-settings');self.assertEqual((r/'SavedGames/mission').read_bytes(),b'player-save');self.assertFalse((r/'new.dll').exists());self.assertIsNone(u.previous_version(r))
 def test_corrupt_backup_and_player_file_injection_are_refused(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);t=self.fixture(r);(t/'backup/OpenHW.exe').write_bytes(b'corrupt');self.assertIsNone(u.previous_version(r))
   (t/'backup/OpenHW.exe').write_bytes(b'old-game');j=u.read_json(t/'transaction.json');j['files'].append(dict(path='Homeworld.cfg',existed=False));u.atomic_json(t/'transaction.json',j);self.assertIsNone(u.previous_version(r));self.assertEqual((r/'Homeworld.cfg').read_bytes(),b'player-settings')
 def test_mid_restore_failure_recovers_current_game(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);self.fixture(r);replace=u.os.replace;failed=[False]
   def fail_once(source,target):
    if Path(source).name=='version.json' and Path(source).parent.name=='new' and not failed[0]:failed[0]=True;raise OSError('interrupted restore')
    return replace(source,target)
   with patch.object(u,'stop_if_game_running'),patch.object(u.os,'replace',side_effect=fail_once):
    with self.assertRaises(OSError):u.restore_previous_version(r,'1.1.11',lambda *a:None)
   self.assertEqual(u.detect_version(r),'1.1.12');self.assertEqual((r/'OpenHW.exe').read_bytes(),b'new-game');self.assertTrue((r/'new.dll').is_file());self.assertIsNotNone(u.previous_version(r))
if __name__=='__main__':unittest.main()
