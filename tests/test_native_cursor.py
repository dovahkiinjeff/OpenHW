import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeCursorWiring(unittest.TestCase):
    def test_normal_windows_pointer_not_in_scene_render(self):
        mouse = (ROOT / 'src/SDL/mouse.c').read_text()
        draw = mouse.split('void mouseDraw(void)', 1)[1].split('#undef SX', 1)[0]
        self.assertIn('if (!mouseClip)\n        mouseRefreshSystemCursor();\n    else', draw)
        self.assertIn('if (softwareDraw) glDisable(GL_BLEND);', draw)
        self.assertIn('mouseCursorObjLast = mouseCursorObjPtr;', draw)
        self.assertIn('mouseholdexpire', draw)

    def test_capture_and_focus_refresh_visibility(self):
        mouse = (ROOT / 'src/SDL/mouse.c').read_text()
        main = (ROOT / 'src/SDL/main.c').read_text()
        self.assertIn('SDL_SetRelativeMouseMode(mouseClip);\n    mouseRefreshSystemCursor();', mouse)
        self.assertIn('mouseIsVisible = FALSE;\n    mouseRefreshSystemCursor();', mouse)
        self.assertIn('mouseIsVisible = TRUE;\n    mouseRefreshSystemCursor();', mouse)
        self.assertNotIn('SDL_ShowCursor(SDL_DISABLE)', main)
        self.assertEqual(main.count('mouseRefreshSystemCursor();'), 2)

    def test_cache_and_shutdown_never_free_active_cursor(self):
        mouse = (ROOT / 'src/SDL/mouse.c').read_text()
        self.assertIn('SDL_SetCursor(created);\n    if (entry->cursor) SDL_FreeCursor(entry->cursor);', mouse)
        self.assertIn('SDL_SetCursor(SDL_GetDefaultCursor());', mouse)
        self.assertIn('SDL_CreateColorCursor(surface, 0, 0)', mouse)
        self.assertIn('mouseLoadHDFile(fileName, &fileSize)', mouse)


if __name__ == '__main__':
    unittest.main()
