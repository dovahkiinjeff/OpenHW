from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class MissionStartLaunchContract(unittest.TestCase):
    def test_only_successful_campaign_arrival_uses_option(self):
        source = (ROOT / "src/Game/SinglePlayer.c").read_text()
        self.assertEqual(source.count("clMissionStartLaunch("), 1)
        arrival = source.split("case HYPERSPACE_SHIPSARRIVING:", 1)[1].split("hyperspacefailed:", 1)[0]
        self.assertIn("opMissionStartLaunch", arrival)
        failed = source.split("case HYPERSPACE_FAILED:", 1)[1]
        self.assertNotIn("clMissionStartLaunch", failed)

    def test_filter_uses_existing_manual_launch_flag_and_utilities(self):
        source = (ROOT / "src/Game/CommandLayer.c").read_text()
        helper = source.split("void clMissionStartLaunch(", 1)[1].split("void clSetAlliance(", 1)[0]
        for ship in ("ResourceCollector", "RepairCorvette", "SalCapCorvette"):
            self.assertIn("ship->shiptype == " + ship, helper)
        self.assertIn("SPECIAL_STAY_TILL_EXPLICITLAUNCH", helper)
        self.assertIn("player->autoLaunch = autoLaunch", helper)
        self.assertIn("LaunchAllInternalShipsOfPlayerThatMustBeLaunched(player)", helper)
        dock = (ROOT / "src/Game/Dock.c").read_text()
        launch = dock.split("void RemoveShipFromLaunching(", 1)[1].split("\n}", 1)[0]
        self.assertIn("bitClear(ship->specialFlags,SPECIAL_STAY_TILL_EXPLICITLAUNCH", launch)
        manual = source.split("void clLaunchShip(", 1)[1].split("void clLaunchMultipleShips(", 1)[0]
        self.assertNotIn("SPECIAL_STAY_TILL_EXPLICITLAUNCH", manual)

    def test_setting_is_saved_and_cancel_restores_it(self):
        ui = (ROOT / "src/Game/ModernUI.c").read_text()
        self.assertIn("savedMissionStartLaunch = opMissionStartLaunch", ui)
        self.assertIn("opMissionStartLaunch = muiGameplay.savedMissionStartLaunch", ui)
        self.assertIn('"MissionStartLaunch",             scriptSetUdwordCB, &opMissionStartLaunch',
                      (ROOT / "src/SDL/utility.c").read_text())

if __name__ == "__main__":
    unittest.main()
