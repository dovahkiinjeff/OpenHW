from pathlib import Path
import sys,json,tempfile,unittest,hashlib,queue
from types import SimpleNamespace
from unittest.mock import Mock,patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_launcher as l
import openhw_launcher_ui as ui
class IntegratedUpdates(unittest.TestCase):
 def test_preparation_downloads_before_handoff_without_replacing_launcher(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);(root/'setup-complete.json').write_text('{}');(root/'OpenHW.exe').write_bytes(b'game');(root/'version.json').write_text('{"version":"1.1"}');(root/'Launcher.exe').write_bytes(b'locked launcher')
   candidate=root/'candidate';candidate.write_bytes(b'verified helper')
   own=dict(version='1.1.3',url='https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.1.1/Updater.exe',size=15,sha256=hashlib.sha256(candidate.read_bytes()).hexdigest())
   feed=dict(latest='1.1.1',patches=[{'to':'1.1.1','from':['1.1']}],updater=own)
   with patch.object(l.updater,'fetch_manifest',return_value=feed),patch.object(l.updater,'download',return_value=candidate) as download,patch.object(l.updater,'stop_if_game_running'):
    command=l.prepare_launcher_update(root,lambda *a:None)
   self.assertEqual((root/'Launcher.exe').read_bytes(),b'locked launcher')
   self.assertEqual((root/'Updater.exe').read_bytes(),candidate.read_bytes());self.assertIn('--launcher-update',command)
   self.assertEqual(download.call_count,2)
 def test_ticker_shows_button_for_game_or_launcher_revision(self):
  app=SimpleNamespace(api=l,version='1.1.1',channel_fetching=True,channel_status=Mock(),update_now=Mock())
  ui.App.receive_channel(app,{'latest':'1.1.1','launcher':{'version':'9.0'}})
  app.update_now.place.assert_called_once();self.assertFalse(app.channel_fetching)
  app.update_now.reset_mock();ui.App.receive_channel(app,{'latest':'1.1.1','launcher':{'version':l.VERSION}})
  app.update_now.place_forget.assert_called_once()
 def test_current_version_component_can_be_repaired_without_game_patch(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);(root/'Launcher.exe').write_bytes(b'old');candidate=root/'candidate';candidate.write_bytes(b'new launcher')
   row=dict(version='1.1.3',url='https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.1.1/Launcher.exe',size=12,sha256=hashlib.sha256(candidate.read_bytes()).hexdigest())
   with patch.object(l.updater,'launcher_running',return_value=False),patch.object(l.updater,'download',return_value=candidate):l.updater.install_launcher_component(root,{'launcher':row},lambda *a:None)
   self.assertEqual((root/'Launcher.exe').read_bytes(),candidate.read_bytes());self.assertTrue(list((root/'Development/Updater/ComponentBackups').rglob('Launcher.exe')))
if __name__=='__main__':unittest.main()
