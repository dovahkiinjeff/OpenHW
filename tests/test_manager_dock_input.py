"""Docked managers preserve the world input path; Classic tutorial stays modal."""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class DockInputTests(unittest.TestCase):
    def test_base_is_non_modal(self):
        text=(ROOT/'src/Game/ManagerDock.c').read_text()
        self.assertIn('bitClear(atom->flags, FAF_Modal | FAF_Popup | FAF_Draggable)',text)
        self.assertIn('regFilterGet(baseRegion) & ~RPE_ModalBreak',text)
        self.assertIn('return tutorial != TUTORIAL_ONLY',text)

    def test_all_three_toggle_and_switch(self):
        text=(ROOT/'src/SDL/mainrgn.c').read_text()
        for active,close,kind in [('cmActive','cmCloseIfOpen','BUILD'),
                                 ('lmActive','lmCloseIfOpen','LAUNCH'),
                                 ('rmGUIActive','rmCloseIfOpen','RESEARCH')]:
            self.assertIn('mdManagerDockEnabled() && '+active,text)
            self.assertIn(close+'();',text)
            self.assertIn('mdCloseOtherManagers(HW_MANAGER_DOCK_'+kind+')',text)

if __name__=='__main__': unittest.main()
