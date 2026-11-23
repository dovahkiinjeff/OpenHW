"""Exercise real hidden Tk widgets; mocks cannot catch unsupported Tcl options."""
import json,sys,unittest,tkinter as tk,tempfile,queue,threading,time,contextlib
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock,patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_launcher as launcher
import openhw_launcher_ui as ui

class RealLauncherControls(unittest.TestCase):
 def setUp(self):
  self.window=tk.Tk();self.window.withdraw()
 def tearDown(self):self.window.destroy()
 def test_custom_button_accepts_label_callback_and_canvas_changes(self):
  original=Mock();replacement=Mock();button=ui.Button(self.window,'OLD',original)
  button.configure(text='UPDATE NOW',command=replacement,width=250)
  self.assertEqual(button.text,'UPDATE NOW');self.assertEqual(int(button.cget('width')),250)
  button.invoke();replacement.assert_called_once();original.assert_not_called()
  button.config({'text':'RETRY UPDATE'});self.assertEqual(button.text,'RETRY UPDATE')
 def test_fresh_115_ticker_uses_real_canvas_button(self):
  feed=json.loads((Path(__file__).resolve().parents[1]/'updates/stable.json').read_text())
  callback=Mock();button=ui.Button(self.window,'WAIT',Mock())
  status=tk.StringVar(self.window)
  app=SimpleNamespace(api=launcher,version='1.1.15',channel_fetching=True,
      channel_status=status,update_now=button,update=callback)
  ui.App.receive_channel(app,feed)
  self.assertEqual(button.text,'UPDATE NOW');self.assertIn('UPDATE AVAILABLE',status.get())
  button.invoke();callback.assert_called_once()
 def test_current_version_shows_up_to_date_without_an_update_button(self):
  status=tk.StringVar(self.window);button=ui.Button(self.window,'WAIT',Mock())
  app=SimpleNamespace(api=launcher,version='1.1.16',channel_fetching=True,
      channel_status=status,update_now=button,update=Mock())
  ui.App.receive_channel(app,{'latest':'1.1.16','patches':[],
      'launcher':{'version':launcher.VERSION}})
  self.assertEqual(status.get(),"YOU'RE UP TO DATE / OpenHW 1.1.16")
  self.assertFalse(button.place_info());self.assertFalse(app.channel_fetching)
 def test_frozen_utilities_prefer_their_own_font_files(self):
  with tempfile.TemporaryDirectory() as temporary:
   root=Path(temporary);private=root/'Runtime/Theme';private.mkdir(parents=True)
   (private/'Rajdhani-Regular.ttf').write_bytes(b'font path fixture')
   with patch.object(ui.sys,'executable',str(root/'Runtime/UpdaterEngine.exe')):
    self.assertEqual(ui.utility_font_folder(root),private)
 def test_check_does_not_run_game_process_probe_on_ui_thread(self):
  entered=threading.Event();release=threading.Event();messages=queue.Queue()
  def prepare(*args):
   entered.set();release.wait(3)
   raise launcher.updater.UpdateError('Close OpenHW before updating.')
  api=SimpleNamespace(prepare_launcher_update=prepare,save_mods=Mock(),
      updater=SimpleNamespace(stop_if_game_running=Mock(side_effect=AssertionError('UI thread blocked'))))
  app=SimpleNamespace(api=api,root=Path('.'),mods=[],save=Mock(),busy=False,
      status=tk.StringVar(self.window),messages=messages,version='1.1.17')
  try:
   ui.App.update(app);self.assertTrue(entered.wait(1))
   heartbeat=[];self.window.after(0,lambda:heartbeat.append(True));self.window.update()
   self.assertEqual(heartbeat,[True]);self.assertTrue(app.busy)
  finally:release.set()
  kind,text,value=messages.get(timeout=2)
  app.channel_status=tk.StringVar(self.window);app.update_now=ui.Button(self.window,'UPDATE',Mock())
  ui.App.finish_update(app,kind,text,value)
  self.assertFalse(app.busy);self.assertIn('Close OpenHW',app.status.get())
 def test_no_update_is_allowed_while_game_is_running(self):
  feed={'latest':'1.1.17','patches':[],'launcher':{'version':launcher.VERSION}}
  with patch.object(launcher.updater,'update_lock',return_value=contextlib.nullcontext()), \
       patch.object(launcher.updater,'fetch_manifest',return_value=feed), \
       patch.object(launcher.updater,'detect_version',return_value='1.1.17'), \
       patch.object(launcher.updater,'stop_if_game_running',side_effect=AssertionError('Unneeded process check')):
   self.assertIsNone(launcher.prepare_launcher_update(Path('.'),Mock()))
 def test_letter_hotfix_is_displayed_with_legacy_numeric_update_version(self):
  status=tk.StringVar(self.window);button=ui.Button(self.window,'WAIT',Mock())
  app=SimpleNamespace(api=launcher,version='1.1.17.1',channel_fetching=True,
      channel_status=status,update_now=button,update=Mock())
  ui.App.receive_channel(app,{'latest':'1.1.17.1','patches':[],
      'launcher':{'version':launcher.VERSION}})
  self.assertEqual(status.get(),"YOU'RE UP TO DATE / OpenHW 1.1.17a")
  self.assertGreater(launcher.updater.compare_versions('1.1.17.1','1.1.17'),0)

if __name__=='__main__':unittest.main()
