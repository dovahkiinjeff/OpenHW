"""Keep the Mission 6 asteroid shortcut out of normal mission completion."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Mission6SkipContract(unittest.TestCase):
    def setUp(self):
        self.source = (ROOT / 'src/Game/SinglePlayer.c').read_text()
        self.available = self.source.split('bool32 spMission6SkipAvailable(void)', 1)[1].split('bool32 spSkipMission6Asteroids', 1)[0]
        self.action = self.source.split('bool32 spSkipMission6Asteroids(void)', 1)[1].split('MissionEnum spGetNextMission', 1)[0]

    def test_mission_scope_and_saved_one_shot(self):
        self.assertIn('gameIsRunning && singlePlayerGame', self.available)
        self.assertIn('MISSION_6_DIAMOND_SHOALS', self.available)
        self.assertIn('NO_HYPERSPACE', self.available)
        self.assertIn('!aivarValueGet(aivarFind("M06AsteroidSkipUsed"))', self.available)
        self.assertIn('!spMission6SkipAvailable() || nisIsRunning || universePause', self.action)
        self.assertLess(self.action.index('aivarValueSet(used, TRUE)'), self.action.index('resourceUnits += 5000'))

    def test_deletion_is_deferred_no_fragments_no_regrowth(self):
        self.assertIn('universe.ResourceList.head', self.action)
        self.assertIn('universe.MinorSpaceObjList.head', self.action)
        self.assertIn('resource->objtype != OBJ_AsteroidType', self.action)
        self.assertIn('volume->resourceObjType == OBJ_AsteroidType', self.action)
        self.assertIn('volume->number = 0', self.action)
        self.assertIn('!bitTest(resource->flags, SOF_Dead)', self.action)
        self.assertLess(self.action.index('resource->resourcevalue = 0'), self.action.index('AddResourceToDeleteResourceList(resource)'))
        self.assertNotIn('univReallyDelete', self.action)
        self.assertNotIn('memFree', self.action)

    def test_bentusi_and_hyperspace_remain_owned_by_original_script(self):
        self.assertIn('aivarFind("PlayerHasClearedField")', self.action)
        self.assertIn('aivarValueSet(cleared, TRUE)', self.action)
        self.assertNotIn('playerCanHyperspace =', self.action)
        self.assertNotIn('singlePlayerMissionCompleteCB(', self.action)
        self.assertNotIn('kasfMissionCompleted(', self.action)
        mission = (ROOT / 'src/Missions/SinglePlayer/Mission06.kas').read_text(encoding='latin1')
        self.assertIn('VarGet ("G_PlayerHasClearedField")', mission)
        self.assertIn('TimerCreateSetStart ("G_PlayTraderArrival", 15)', mission)
        self.assertIn('FSMCREATE (Traders, TEAM_Traders)', mission)
        self.assertIn('VarGet ("G_ModuleIsCharged")', mission)

    def test_visible_button_and_click_share_layout(self):
        ui = (ROOT / 'src/Game/ModernUI.c').read_text()
        draw = ui.split('static void muiControlGroupsDraw(', 1)[1].split('static udword muiControlGroupsProcess', 1)[0]
        click = ui.split('static udword muiControlGroupsProcess(', 1)[1].split('static void muiCommandRuleDraw', 1)[0]
        for block in (draw, click):
            self.assertIn('spMission6SkipAvailable()', block)
            self.assertIn('muiMission6SkipRect(&layout)', block)
        self.assertIn('SKIP MISSION 6 (+5,000 RU)', draw)
        self.assertIn('region->rect.y0 = skip.y0', draw)
        self.assertIn('spSkipMission6Asteroids()', click)


if __name__ == '__main__':
    unittest.main()
