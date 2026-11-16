from pathlib import Path
import json,sys,unittest,tempfile
from unittest.mock import patch,Mock
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_updater as u
import openhw_launcher as l
import openhw_launcher_ui as ui
class UpdatePaths(unittest.TestCase):
 def setUp(self):self.feed=json.loads((Path(__file__).resolve().parents[1]/'updates/stable.json').read_text())
 def test_every_published_version_has_a_route_or_explicit_base_boundary(self):
  versions={v for p in self.feed['patches'] for v in p['from']}|{p['to'] for p in self.feed['patches']}
  for v in versions:
   with self.subTest(version=v):
    if u.compare_versions(v,'1.1')<0:
     with self.assertRaisesRegex(u.FreshInstallRequired,'separate empty folder'):u.update_plan(self.feed,v)
    elif u.compare_versions(v,self.feed['latest'])<0:self.assertEqual(u.update_plan(self.feed,v)[-1]['to'],self.feed['latest'])
    else:self.assertEqual(u.update_plan(self.feed,v),[])
 def test_boundary_does_not_hide_a_broken_supported_chain(self):
  self.feed['patches']=[p for p in self.feed['patches'] if p['to']!='1.1.6']
  with self.assertRaises(u.UpdateError) as result:u.update_plan(self.feed,'1.1.5')
  self.assertNotIsInstance(result.exception,u.FreshInstallRequired)
 def test_launcher_old_folder_fails_before_downloading_or_replacing_files(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);original={'version.json':b'{"version":"1.0.7"}','setup-complete.json':b'{}','OpenHW.exe':b'old','Homeworld.cfg':b'settings'}
   for n,b in original.items():(r/n).write_bytes(b)
   with patch.object(u,'fetch_manifest',return_value=self.feed),patch.object(u,'stop_if_game_running'),patch.object(u,'download') as download:
    with self.assertRaises(u.FreshInstallRequired):l.prepare_launcher_update(r,lambda *a:None)
    download.assert_not_called()
   for n,b in original.items():self.assertEqual((r/n).read_bytes(),b)
 def test_ticker_old_folder_offers_new_installer(self):
  app=SimpleNamespace(api=l,version='1.0.7',channel_status=Mock(),update_now=Mock(),update=Mock())
  ui.App.receive_channel(app,self.feed)
  self.assertIn('NEW INSTALLATION REQUIRED',app.channel_status.set.call_args.args[0])
  self.assertEqual(app.update_now.configure.call_args.kwargs['text'],'GET NEW INSTALLER')
if __name__=='__main__':unittest.main()
