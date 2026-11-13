import sys,unittest,queue
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_launcher_ui as ui
class UpdateCompletion(unittest.TestCase):
 def test_error_clears_busy_without_grabbing_modal(self):
  app=SimpleNamespace(busy=True,amount=None,status=Mock(),dialog=Mock())
  ui.App.finish_update(app,'error','Network unavailable',0)
  self.assertFalse(app.busy);self.assertEqual(app.amount,0);app.dialog.assert_not_called()
  self.assertIn('retry',app.status.set.call_args[0][0])
 def test_poll_keeps_scheduling_after_completion_error(self):
  app=SimpleNamespace(tick=0,messages=queue.Queue(),window=Mock(),status=Mock(),busy=True,amount=None,draw_progress=Mock())
  app.messages.put(('done','Done',1));app.finish_update=Mock(side_effect=RuntimeError('reload error'))
  app.poll=lambda:None
  ui.App.poll(app)
  self.assertFalse(app.busy);app.window.after.assert_called_once_with(100,app.poll)
if __name__=='__main__':unittest.main()
