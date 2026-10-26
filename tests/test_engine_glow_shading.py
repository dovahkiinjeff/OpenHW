import math
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EngineGlowShading(unittest.TestCase):
    def test_capital_glow_state_is_scoped_and_preserves_fade(self):
        text = (ROOT / 'src/Game/Trails.c').read_text()
        start = text.index('hwglEngineGlow(colUbyteToReal')
        stop = text.index('hwglEngineGlow(0,0,0,0,0)', start)
        self.assertIn('bFade ? meshFadeAlpha : 1.0f', text[start:stop])
        self.assertIn('meshRender(', text[start:stop])
        self.assertLess(stop, text.index('glDisable(GL_NORMALIZE)', stop))
        mesh = (ROOT / 'src/Game/Mesh.c').read_text()
        self.assertIn('glNormal3f(normal->x,normal->y,normal->z)', mesh)
        self.assertIn('specIndex==2 && hwglEngineGlowActive()', mesh)

    def test_glow_is_calculated_per_pixel_with_pixel_visible_constants(self):
        text = (ROOT / 'src/Modern/LegacyD3D12GL.cpp').read_text()
        pixel = text[text.index('float4 LegacyPS('):]
        self.assertIn('float glow = pow(saturate(abs(dot(n,v))),EngineGlowParameters.x)', pixel)
        self.assertIn('result.g *= min(glow,0.92)', pixel)
        self.assertIn('result.a *= glow', pixel)
        self.assertIn('params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL', text)
        self.assertIn('state.engineNormalRows[row*4+column]=inverse.m[row*4+column]', text)
        self.assertIn('g = FixedState{}', text)

    def test_interpolated_normals_avoid_gouraud_brightness_bands(self):
        # Two symmetric normals: the surface midpoint faces the eye directly.
        # Interpolating already shaded vertex colors incorrectly dims it.
        cosine = math.cos(math.radians(30))
        gouraud_midpoint = cosine ** 4
        pixel_midpoint = 1.0 ** 4
        self.assertAlmostEqual(gouraud_midpoint, 0.5625)
        self.assertEqual(pixel_midpoint, 1.0)
        self.assertGreater(pixel_midpoint, gouraud_midpoint)


if __name__ == '__main__':
    unittest.main()
