from pathlib import Path
import math
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PresentationContracts(unittest.TestCase):
    def test_rm_dust_editor_is_reachable_and_uses_new_style(self):
        editor = (ROOT / 'src/Game/ModernMapEditor.c').read_text()
        self.assertIn('#define MME_TAB_COUNT 5', editor)
        self.assertIn('mmeDrawButton(&tabs[4], "RM DUST"', editor)
        edit = editor.split('static bool32 mmeAdjustInspector', 1)[-1]
        self.assertIn('s->opacity=mmeClampF', edit)
        self.assertIn('bool32 saved=rmFxResourceDustSave()', editor)
        self.assertIn('"EFFECT AGE"', editor)
        fx = (ROOT / 'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertIn('drawingResourceDust=1;', fx)
        self.assertIn('drawingResourceDust=0;', fx)
        self.assertIn('blend==2?resourceDustStyle.opacity:1', fx)

    def test_screen_height_round_trip(self):
        # Perspective screen Y = (a*z+b)/(c*z+d). The new inverse must
        # preserve screen position at any camera pitch and starting height.
        for a, b, c, d in ((1, 20, .01, 500), (-2, 10, .1, 600), (.01, 3, -.2, 1000)):
            for z in (-100, -20, 0, 45, 200):
                y = (a*z+b)/(c*z+d)
                recovered = (b-y*d)/(y*c-a)
                self.assertTrue(math.isclose(z, recovered, abs_tol=1e-8))
        pie = (ROOT / 'src/Game/PiePlate.c').read_text()
        z_branch = pie.split('/* Match screen Y', 1)[1].split('//recompute the on-screen', 1)[0]
        self.assertNotIn('mousePositionSet', z_branch)
        self.assertNotIn('pieVerticalMouseSensitivity', pie)
        render = (ROOT / 'src/SDL/render.c').read_text()
        tail = render.split('rmWeaponFxDrawTransient(camera);', 1)[1]
        self.assertLess(tail.index('hwModernGraphicsCaptureOpenGLWorld()'), tail.index('rndPostObjectCallback();'))

    def test_hyperspace_rejects_all_materials_and_shadows(self):
        cpp = (ROOT / 'src/Modern/ModernRaytracing.cpp').read_text()
        shader = (ROOT / 'src/Modern/ModernRaytracing.hlsl').read_text()
        self.assertIn('sizeof(GpuInstanceMotion) == 72', cpp)
        self.assertIn('if (!instance.clipEnabled && instance.surfaceVariant', cpp)
        self.assertIn('D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE', cpp)
        body = shader.split('void SurfaceAnyHit(', 1)[1].split('[shader("closesthit")]', 1)[0]
        self.assertLess(body.index('instance.clipEnabled'), body.index('SampleSurfaceChannel'))
        self.assertIn('WorldRayOrigin()+RayTCurrent()*WorldRayDirection()', body)
        self.assertIn('IgnoreHit();', body)

    def test_manager_keys_route_once_before_regions(self):
        main = (ROOT / 'src/SDL/main.c').read_text()
        body = main.split('case SDL_KEYDOWN:', 1)[1].split('case SDL_USEREVENT:', 1)[0]
        self.assertLess(body.index('mdHandleActiveManagerKey'), body.index('keyPressDown'))
        self.assertIn('pEvent->key.repeat && pEvent->key.keysym.scancode == managerConsumedKey', body)
        dock = (ROOT / 'src/Game/ManagerDock.c').read_text()
        body = dock.split('bool32 mdHandleActiveManagerKey', 1)[1].split('void mdInstall', 1)[0]
        for key, command in [('BKEY', 'mrBuildShips'), ('LKEY', 'mrLaunch'), ('RKEY', 'mrResearch')]:
            self.assertIn(f'case {key}: {command}(NULL,NULL); return TRUE;', body)

if __name__ == '__main__':
    unittest.main()
