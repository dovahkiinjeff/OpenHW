import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class DX11RendererContract(unittest.TestCase):
    def test_independent_device_and_shader_model(self):
        source = (ROOT / 'src/Modern/LegacyD3D11Renderer.inc').read_text()
        self.assertIn('D3D11CreateDeviceAndSwapChain', source)
        self.assertIn('D3D_FEATURE_LEVEL_11_0', source)
        self.assertNotIn('D3D12CreateDevice', source)
        for entry in ('"VS","vs_5_0"', '"PS","ps_5_0"', '"ShadowVS","vs_5_0"'):
            self.assertIn(entry, source)

    def test_lit_material_and_clipping_path(self):
        source = (ROOT / 'src/Modern/LegacyD3D11Renderer.inc').read_text()
        for feature in ('Normal.Sample', 'Emission.Sample', 'ORM.Sample',
                        'SV_ClipDistance', 'SampleCmpLevelZero', 'renderShadows()',
                        'setFrameLighting', 'GenerateMips', 'backgroundPrograms'):
            self.assertIn(feature, source)
        render = (ROOT / 'src/SDL/render.c').read_text()
        self.assertIn('usingShader = hwModernGraphicsIsD3D11() ? FALSE : TRUE;', render)

    def test_explicit_option_and_no_saved_setting_rewrite(self):
        main = (ROOT / 'src/SDL/main.c').read_text()
        self.assertIn('entryVr("/dx11"', main)
        renderer = (ROOT / 'src/Modern/ModernGraphics.cpp').read_text()
        self.assertIn('if(requestDX11)return hwlegacyd3d11::initialize', renderer)
        self.assertNotIn('mainDlaa = HW_MODERN_AA_OFF', renderer)
        self.assertNotIn('mainRaytracing = 0', renderer)

    def test_direct_presentation_orientation(self):
        source = (ROOT / 'src/Modern/LegacyD3D11Renderer.inc').read_text()
        self.assertIn('o.p=float4(p.x,p.y,(p.z+p.w)*0.5,p.w)', source)
        self.assertIn('o.p=float4(i.p.x,-i.p.y,i.p.z,i.p.w)', source)
        self.assertIn('FrontCounterClockwise=TRUE', source)
        self.assertIn('DX11SkyVS', source)
        self.assertIn('DX11BackgroundVS', source)
        self.assertIn('drawState.cullFace=false', source)

if __name__ == '__main__':
    unittest.main()
