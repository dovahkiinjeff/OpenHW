from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ShipEditorContract(unittest.TestCase):
    def test_modding_entry_and_owned_screen_cleanup(self):
        editor = (ROOT / 'src/Game/MilitaryParadeEditor.inc').read_text()
        self.assertIn('"HOD EDITOR"', editor)
        self.assertIn('hodWorkshopOpen()', editor)
        self.assertNotIn('speBegin()', editor)
        self.assertNotIn('SHIP PROJECTS', editor)
        host = (ROOT / 'src/Game/HodWorkshop.inc').read_text()
        self.assertNotIn('CreateProcess', host)
        self.assertIn('hodWorkshopClose()', editor)

    def test_editor_is_packaged_inside_the_game(self):
        host = (ROOT / 'src/Game/HodWorkshop.inc').read_text()
        native = (ROOT / 'tools/hod-editor/src-tauri/src/game_window.rs').read_text()
        build = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('OpenHWHodEditor.dll', host)
        self.assertIn('OpenHWHodEditor.dll', build)
        self.assertIn('build_as_child', native)
        self.assertIn('hwLiveMapAuthoringEnabled()', host)

    def test_portable_assets_not_native_archive_writes(self):
        source = (ROOT / 'src/Game/ShipAuthoring.c').read_text()
        self.assertIn('Modding/Ships/%s/%s', source)
        self.assertIn('MOVEFILE_WRITE_THROUGH', source)
        self.assertIn('strstr(s,"..")', source)
        self.assertIn('OPENHW_SHIP 1', source)
        self.assertNotIn('LoadLibrary', source)
        self.assertNotIn('.big', source.lower())

    def test_preview_textures_do_not_depend_on_mission_loading(self):
        registry = (ROOT / 'src/SDL/texreg.c').read_text()
        preview = registry.split('static bool32 trPreparePreviewIndex', 1)[1].split('void trRegistryRefresh', 1)[0]
        self.assertIn('list.nAllocated=list.nTextures=1;', preview)
        self.assertIn('trMeshSortListLoad(&list);', preview)
        self.assertIn('trPreparePreviewIndex((sdword)reg->sharedFrom,depth+1)', preview)
        self.assertNotIn('HorseRaceBeginBar(', preview)
        self.assertNotIn('trRegistryRefresh();', preview)
        self.assertNotIn('trTextureDeleteAllUnregistered();', preview)

if __name__ == '__main__':
    unittest.main()
