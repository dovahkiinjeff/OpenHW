from pathlib import Path
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "src/Game/ModernUI.c").read_text()

class ControlGroupTabsContract(unittest.TestCase):
    def test_all_slots_render_including_unassigned_groups(self):
        draw = SOURCE.split("static void muiControlGroupsDraw(", 1)[1].split("static udword muiControlGroupsProcess", 1)[0]
        self.assertIn("slot < HW_CONTROL_GROUP_TAB_COUNT", draw)
        self.assertIn("selHotKeyString[group]", draw)
        self.assertIn("assigned ? muiCool : muiDisabled", draw)
        self.assertNotIn("continue;", draw)

    def test_uses_vanilla_selection_and_empty_slots_are_noop(self):
        handler = SOURCE.split("static udword muiControlGroupsProcess(", 1)[1].split("static void muiCommandRuleDraw", 1)[0]
        self.assertIn("if (selHotKeyGroup[group].numShips > 0)", handler)
        self.assertIn("selSelectHotKeyGroup(&selHotKeyGroup[group])", handler)
        self.assertIn("selHotKeyNumbersSet(group)", handler)
        self.assertIn("ioUpdateShipTotals()", handler)
        self.assertNotIn("ccFocus", handler)
        self.assertNotIn("selSelected.numShips = 0", handler)

    def test_region_registered_reordered_and_deleted(self):
        self.assertIn("regFunctionSet(muiCommand.controlGroupRegion, muiControlGroupsProcess)", SOURCE)
        self.assertIn("regSiblingMoveToFront(muiCommand.controlGroupRegion)", SOURCE)
        self.assertIn("regionhandle groups = muiCommand.controlGroupRegion", SOURCE)
        self.assertIn("if (groups != NULL) regRegionDelete(groups)", SOURCE)

if __name__ == "__main__":
    unittest.main()
