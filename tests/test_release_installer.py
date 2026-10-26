"""Release presets, containment and reviewed-model import contracts."""
import json,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from stage_release import defaults
from release_setup import verify_payload,local_environment,root_directory
from release_import import SPECIALS

class ReleaseInstallerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/'tmp').mkdir(exist_ok=True)

    def test_presets_keep_preferences_without_developer_display_or_paths(self):
        source='[init options]\nHomeworldDataPath F:/OpenHWWIP/Data\nscreenWidth 2560\nscreenHeight 1440\nscreenDepth 24\nrefreshRate 240\ndeviceCRC 123\nraytracing 1\nrasterGlowBrightness 29\nframeGeneration 3\nshowFPS 1\nUnlimitedRUs 0\n'
        output=defaults(source)
        for key in ('screenWidth','screenHeight','screenDepth','refreshRate','HomeworldDataPath','deviceCRC'):
            self.assertNotIn(key,output)
        for row in ('raytracing 1','rasterGlowBrightness 29','frameGeneration 3','showFPS 1','UnlimitedRUs 0'):
            self.assertIn(row,output)
        self.assertIn('detectDisplayOnFirstBoot    1',output)

    def test_frozen_installer_uses_its_own_folder_not_working_directory(self):
        with patch.object(sys,'frozen',True,create=True),patch.object(sys,'executable',str(ROOT/'Installer/OpenHW-Setup.exe')):
            self.assertEqual(root_directory(),(ROOT/'Installer').resolve())
        with patch.object(sys,'frozen',True,create=True),patch.object(sys,'executable',str(ROOT/'Development/Installer/OpenHW-SetupEngine.exe')):
            self.assertEqual(root_directory(),ROOT.resolve())

    def test_temporary_and_cache_paths_stay_in_install_folder(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'tmp') as folder,patch.dict(os.environ,{}):
            root=Path(folder);local_environment(root)
            for key in ('TEMP','TMP','TMPDIR'):
                self.assertEqual(Path(os.environ[key]),root/'Development/Temp')
            self.assertEqual(Path(os.environ['HW_PORTABLE_ROOT']),root)
            self.assertEqual(Path(os.environ['HW_DEVELOPMENT_ROOT']),root/'Development')

    def test_payload_rejects_paths_outside_install_folder(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'tmp') as folder:
            root=Path(folder);(root/'Development').mkdir()
            (root/'Development/release.json').write_text(json.dumps({'schema':1,'version':'1.0','content_policy':2,'files':[{'path':'../escape','size':0,'sha256':'0'*64}]}))
            with self.assertRaisesRegex(ValueError,'Unsafe'):verify_payload(root)

    def test_all_later_special_models_have_local_import_recipes(self):
        self.assertEqual({row[1] for row in SPECIALS},{'traders/floatingcity','traders/ghostship','traders/researchstation','traders/junkyardhq','traders/miningbase','p3/p3megaship'})

    def test_preset_config_does_not_skip_first_boot_display_detection(self):
        main=(ROOT/'src/SDL/main.c').read_text()
        self.assertIn('(!hasSavedOptions || mainDetectDisplayOnFirstBoot) && !selectedRES',main)
        self.assertIn('if (mainDetectInitialDisplayMode()) mainDetectDisplayOnFirstBoot = 0;',main)
        self.assertIn('else if (selectedRES) mainDetectDisplayOnFirstBoot = 0;',main)

if __name__=='__main__':unittest.main()
