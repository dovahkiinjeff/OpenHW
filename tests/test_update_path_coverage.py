from pathlib import Path
import json,sys,unittest,tempfile,hashlib
from unittest.mock import patch,Mock
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_updater as u
import openhw_launcher as l
import openhw_launcher_ui as ui
class UpdatePaths(unittest.TestCase):
 def setUp(self):self.feed=json.loads((Path(__file__).resolve().parents[1]/'updates/stable.json').read_text())
 def test_every_published_version_reaches_latest_via_baseline(self):
  versions={v for p in self.feed['patches'] for v in p['from']}|{p['to'] for p in self.feed['patches']}
  for v in versions:
   with self.subTest(version=v):
    current=v
    for _ in range(3):
     route=u.update_plan(self.feed,current)
     if not route:break
     current=route[-1]['to']
    self.assertEqual(current,self.feed['latest'])
 def test_boundary_does_not_hide_a_broken_supported_chain(self):
  self.feed['patches']=[p for p in self.feed['patches'] if p['to']!=self.feed['latest']]
  with self.assertRaises(u.UpdateError) as result:u.update_plan(self.feed,self.feed['full_install']['to'])
  self.assertNotIsInstance(result.exception,u.FreshInstallRequired)
 def test_launcher_old_folder_prepares_handoff_without_replacing_player_files(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);original={'version.json':b'{"version":"1.0.7"}','setup-complete.json':b'{}','OpenHW.exe':b'old','Homeworld.cfg':b'settings'}
   for n,b in original.items():(r/n).write_bytes(b)
   helper=r/'helper.exe';helper.write_bytes(b'verified updater')
   self.feed['updater']['sha256']=hashlib.sha256(helper.read_bytes()).hexdigest()
   with patch.object(u,'fetch_manifest',return_value=self.feed),patch.object(u,'stop_if_game_running'),patch.object(u,'download',return_value=helper):
    command=l.prepare_launcher_update(r,lambda *a:None)
    self.assertIn('--launcher-update',command)
   for n,b in original.items():self.assertEqual((r/n).read_bytes(),b)
 def test_ticker_old_folder_offers_new_installer(self):
  app=SimpleNamespace(api=l,version='1.0.7',channel_status=Mock(),update_now=Mock(),update=Mock())
  ui.App.receive_channel(app,self.feed)
  self.assertIn('UPDATE AVAILABLE',app.channel_status.set.call_args.args[0])
  self.assertNotIn('NEW INSTALLATION REQUIRED',app.channel_status.set.call_args.args[0])
if __name__=='__main__':unittest.main()
