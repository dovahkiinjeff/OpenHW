"""Regression contract for stale Sensors screen closure during hyperspace."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SensorsCleanup(unittest.TestCase):
    def setUp(self):
        self.source = (ROOT / 'src/Game/Sensors.c').read_text()
        self.close = self.source.split('void smSensorsCloseForGood(void)', 1)[1].split('void smBlobSphereDraw', 1)[0]

    def test_no_untracked_region_deletion(self):
        self.assertIn('feScreenDeleteFlags(closingRegion, FE_DONT_DELETE_REGION_IF_SCREEN_NOT_FOUND)', self.close)
        self.assertNotIn('feScreenDelete(smBaseRegion)', self.close)
        frontend = (ROOT / 'src/Game/FEFlow.c').read_text()
        self.assertIn('if (!(flags & FE_DONT_DELETE_REGION_IF_SCREEN_NOT_FOUND))', frontend)

    def test_ownership_cleared_before_callbacks(self):
        deletion = self.close.index('feScreenDeleteFlags(')
        self.assertLess(self.close.index('smBaseRegion = NULL'), deletion)
        self.assertLess(self.close.index('smSensorsActive = FALSE'), deletion)
        self.assertIn('if (!smSensorsActive && smBaseRegion == NULL) return;', self.close)
        self.assertIn('smViewportRegion = NULL;', self.close)

    def test_scroll_storage_is_not_freed_twice(self):
        for side in ('Left', 'Right', 'Top', 'Bottom'):
            self.assertIn(f'memFree(smScrollList{side}); smScrollList{side} = NULL;', self.close)
        self.assertIn('smScrollCountBottom = 0;', self.close)

    def test_viewport_stops_after_animated_close(self):
        zoom = self.source.split('//...actually close the sensors manager', 1)[1]
        after = zoom.split('smSensorsCloseForGood();', 1)[1]
        self.assertIn('return;', after.split('}', 1)[0])

    def test_hyperspace_still_uses_shared_main_screen_cleanup(self):
        source = (ROOT / 'src/Game/SinglePlayer.c').read_text()
        main = source.split('void spMainScreen(void)', 1)[1].split('void spLockout', 1)[0]
        self.assertIn('if (smSensorsActive)', main)
        self.assertIn('smSensorsCloseForGood();', main)


if __name__ == '__main__':
    unittest.main()
