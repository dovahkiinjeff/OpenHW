import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_launcher_ui as ui
import openhw_updater as updater


class FullInstallHandoff(unittest.TestCase):
    def test_installer_handoff_does_not_reopen_launcher(self):
        self.assertEqual(ui.update_result_kind('full-install'),'restart')
        self.assertEqual(ui.update_result_kind('restart'),'restart')
        self.assertEqual(ui.update_result_kind('1.1.16'),'done')

    def test_installer_waits_for_asynchronous_shutdown_in_its_folder(self):
        root=Path('test-install'); progress=Mock()
        with patch.object(updater,'launcher_running',side_effect=[True,True,False]) as running, \
             patch.object(updater.time,'sleep') as sleep:
            updater.wait_for_install_launcher(root,progress)
        self.assertEqual(sleep.call_count,2)
        self.assertEqual(running.call_args.args,(root,))
        progress.assert_called_once()

    def test_stuck_launcher_times_out_before_installation(self):
        with patch.object(updater,'launcher_running',return_value=True), \
             patch.object(updater.time,'monotonic',side_effect=[0,121]):
            with self.assertRaisesRegex(updater.UpdateError,'files have not been changed'):
                updater.wait_for_install_launcher(Path('test-install'),Mock())

    def test_closed_launcher_requires_no_delay(self):
        progress=Mock()
        with patch.object(updater,'launcher_running',return_value=False),patch.object(updater.time,'sleep') as sleep:
            updater.wait_for_install_launcher(Path('test-install'),progress)
        progress.assert_not_called();sleep.assert_not_called()


if __name__=='__main__':unittest.main()
