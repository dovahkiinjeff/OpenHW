"""Guard mission-local authoring assets and the production persistence routes."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MissionTuningContract(unittest.TestCase):
    def test_authored_first_two_missions_have_complete_independent_tuning(self):
        for mission in (1, 2):
            text = (ROOT / f"assets/Missions/Mission{mission:02d}.rtxlight").read_text()
            rows = dict((int(k), int(v)) for k, v in re.findall(
                r"^missionTuning (\d+) (\d+)$", text, re.M))
            # Material interpretation moved to global controls; missions now
            # persist only lighting rows. Do not reintroduce stale per-map
            # roughness/normal settings to satisfy the former 14-row layout.
            self.assertEqual(set(rows), set(range(9)) | {12, 13})
            self.assertEqual(rows[0], 160)
            self.assertEqual(rows[12], 100)

    def test_loading_a_mission_resets_before_parsing(self):
        source = (ROOT / "src/Game/ModernMissionBackdrop.c").read_text()
        loader = source.split("static void mmbLightingOverrideLoad(sdword mission)", 1)[1]
        loader = loader.split("static void mmbLightingOverrideEnsure", 1)[0]
        self.assertLess(loader.index("mmbLightingOverrideDefaults(mission)"),
                        loader.index("while (fileLineRead"))
        defaults = re.search(r"defaults\[14\] = \{([^}]+)\}", source).group(1)
        self.assertEqual(int(defaults.split(",")[0]), 100)
        self.assertIn('"missionTuning %d %d\\n"', source)

    def test_tuning_tab_saves_the_mission_not_global_options(self):
        source = (ROOT / "src/Game/ModernUI.c").read_text()
        footer = source.split("static void muiLightingHandleFooter", 1)[1]
        tuning = footer.split("else if (muiLighting.page == MUI_LIGHTING_PAGE_TUNING)", 1)[1]
        tuning = tuning.split("muiLightingSetPage(MUI_LIGHTING_PAGE_SHADOWS)", 1)[0]
        self.assertIn("muiSaveAndExportMissionLighting();", tuning)
        self.assertNotIn("opOptionsSaveSettings();", tuning)
        self.assertNotIn("[shift_f12_global_tuning]", source)
        self.assertIn('"MISSION TUNING"', source)

    def test_non_pt_glow_is_global_with_29_percent_default(self):
        mission = (ROOT / "src/Game/ModernMissionBackdrop.c").read_text()
        self.assertNotIn("mmbLightingOverride.rasterGlowBrightness", mission)
        self.assertNotIn('"rasterGlowBrightness %d', mission)
        self.assertNotIn("mainRasterGlowBrightnessPercent =", mission)
        main = (ROOT / "src/SDL/main.c").read_text()
        self.assertIn("mainRasterGlowBrightnessPercent = 29;", main)
        config = (ROOT / "src/SDL/utility.c").read_text()
        self.assertIn('{"rasterGlowBrightness", scriptSetUdwordCB, &mainRasterGlowBrightnessPercent}', config)
        ui = (ROOT / "src/Game/ModernUI.c").read_text()
        save = ui.split("static bool32 muiSaveAndExportMissionLighting(void)", 1)[1].split("static sdword", 1)[0]
        self.assertIn("opOptionsSaveSettings();", save)
        reset = ui.split("static void muiLightingReset(void)", 1)[1].split("static sdword", 1)[0]
        self.assertNotIn("mainRasterGlowBrightnessPercent =", reset)
        self.assertIn("GLOBAL / all missions", ui)


if __name__ == "__main__":
    unittest.main()
