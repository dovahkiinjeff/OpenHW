import sys
import unittest
import struct
import tempfile
import hashlib
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from stage_kushan_native_materials import translate_surface, lif_rgba, neutral_vertex_surface, select_static_material_variant, stage_active_engine_variant, native_material_controls


class NativeMaterials(unittest.TestCase):
    def test_bentusi_glow_variant_keeps_rgb_emission_channels(self):
        diffuse=Image.new('RGBA',(2,2),(80,90,100,255))
        glow=Image.new('RGB',(2,2),(20,110,230))
        expected=translate_surface('shipglow',diffuse,glow)
        actual=translate_surface('shipglow_ns',diffuse,glow)
        self.assertEqual(set(actual),set(expected))
        for key in expected:self.assertEqual(actual[key].tobytes(),expected[key].tobytes())

    def test_material_controls_are_global_not_mission_overrides(self):
        root=Path(__file__).resolve().parents[1]
        source=(root/'src/Game/ModernMissionBackdrop.c').read_text()
        self.assertIn('if (i<9 || i>11) *mmbTuningBindings[i]',source)
        self.assertIn('if (index<9 || index>11) fprintf',source)
        self.assertIn('(row < 9 || row > 11)',source)
        ui=(root/'src/Game/ModernUI.c').read_text()
        panel=ui[ui.index('static sdword *muiTextureValue'):ui.index('bool32 modernUILightingEditorActive')]
        self.assertIn('opOptionsSaveSettings()',panel)
        self.assertNotIn('muiSaveAndExportMissionLighting',panel)

    def test_native_controls_separate_emission_and_specular(self):
        glow=Image.new('RGB',(2,1));glow.putdata([(255,0,255),(255,255,255)])
        result=native_material_controls('ship',(2,1),glow)
        self.assertEqual(result.getpixel((0,0))[2],224)
        self.assertEqual(result.getpixel((1,0))[2],0)
        self.assertEqual(result.getpixel((0,0))[::3],(255,255))

    def test_painted_material_has_broader_lobe_than_bare_material(self):
        glow=Image.new('RGB',(1,1),(255,0,255))
        painted=native_material_controls('ship',(1,1),glow,Image.new('RGB',(1,1),(0,255,0)))
        bare=native_material_controls('ship',(1,1),glow,Image.new('RGB',(1,1),(255,255,0)))
        self.assertGreater(painted.getpixel((0,0))[1],bare.getpixel((0,0))[1])
        self.assertLess(painted.getpixel((0,0))[2],bare.getpixel((0,0))[2])

    def test_rgb_glow_does_not_become_reflection(self):
        result=native_material_controls('shipglow',(1,1),Image.new('RGB',(1,1),'white'))
        self.assertEqual(result.getpixel((0,0))[2],0)

    def test_active_thruster_preserves_idle_and_copies_all_on_maps(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); cache=root/'cache'; output=root/'output'
            cache.mkdir(); output.mkdir()
            files={}
            for kind, suffix in [('diffuse','.dds'),('bootstrap','.lif'),('emissive','_emissive.dds'),('normal','_normal.dds')]:
                data=('On '+kind).encode(); (cache/(kind+suffix)).write_bytes(data)
                files[kind]={'path':kind+suffix,'sha256':hashlib.sha256(data).hexdigest()}
                (output/('c0_m4'+suffix)).write_bytes(b'idle')
            material={'shader':'thruster','variants':[{'state':'Off','files':{}},{'state':'On','files':files}]}
            stage_active_engine_variant(material,cache,output,'c0_m4')
            for kind,suffix in [('diffuse','.dds'),('bootstrap','.lif'),('emissive','_emissive.dds'),('normal','_normal.dds')]:
                self.assertEqual((output/('c0_m4'+suffix)).read_bytes(),b'idle')
                self.assertEqual((output/('c0_m4_engineOn'+suffix)).read_bytes(),('On '+kind).encode())
            (cache/files['diffuse']['path']).write_bytes(b'stale')
            with self.assertRaises(ValueError):
                stage_active_engine_variant(material,cache,output,'c0_m4')

    def test_non_engine_material_does_not_gain_active_variant(self):
        stage_active_engine_variant({'shader':'shipglow'},None,None,'unused')

    def test_thruster_defaults_to_idle_source_texture(self):
        variants=[{'state':'On'},{'state':'Off'}]
        self.assertEqual(select_static_material_variant(variants)['state'],'Off')
        self.assertEqual(select_static_material_variant([{'state':'Hull'}])['state'],'Hull')

    def test_derelict_matte_is_opaque_unpainted_and_uses_packed_glow(self):
        result=translate_surface('matte',Image.new('RGBA',(1,1),(100,80,60,0)),
            Image.new('RGB',(1,1),(255,128,255)),Image.new('RGB',(1,1),(0,0,255)))
        self.assertEqual(result['diffuse'].getpixel((0,0)),(100,80,60,255))
        self.assertEqual(result['teamEffect0'].getpixel((0,0)),0)
        self.assertEqual(result['teamEffect1'].getpixel((0,0)),0)
        self.assertEqual(result['emissive'].getpixel((0,0)),(50,40,30,255))

    def test_old_derelict_ship_shader_spelling_has_same_semantics(self):
        image=Image.new('RGB',(1,1),(100,80,60))
        a=translate_surface('Ship',image);b=translate_surface('ship',image)
        for key in a:self.assertEqual(a[key].tobytes(),b[key].tobytes())

    def test_authored_neutral_vertex_tint_is_not_emission_or_team_paint(self):
        result = neutral_vertex_surface([0xc8c8c8ff, 0xffffffff])
        self.assertEqual(result['diffuse'].size, (256, 1))
        self.assertEqual(result['diffuse'].getpixel((200, 0)), (200, 200, 200, 255))
        self.assertEqual(result['emissive'].getpixel((200, 0)), (0, 0, 0, 0))
        self.assertEqual(result['teamEffect0'].getextrema(), (0, 0))
        self.assertEqual(result['teamEffect1'].getextrema(), (0, 0))

    def test_untextured_colored_or_transparent_vertices_require_explicit_support(self):
        for colors in ([], [0xff0000ff], [0xc8c8c800]):
            with self.assertRaises(ValueError):
                neutral_vertex_surface(colors)

    def test_packed_glow_not_specular_red(self):
        result = translate_surface('ship', Image.new('RGB', (1, 1), (100, 80, 60)),
                                   Image.new('RGB', (1, 1), (255, 0, 255)))
        self.assertEqual(result['emissive'].getpixel((0, 0)), (0, 0, 0, 0))

    def test_emission_protects_only_glowing_pixels(self):
        glow = Image.new('RGB', (3, 1))
        glow.putdata([(0, 0, 0), (1, 0, 0), (20, 50, 80)])
        result = translate_surface('shipglow', Image.new('RGB', (3, 1), 'white'), glow)
        self.assertEqual(list(result['emissive'].getchannel('A').getdata()), [0, 255, 255])
        source = (Path(__file__).resolve().parents[1]/'src/Game/Mesh.c').read_text()
        self.assertIn('ubyte alpha = meshRemasteredEmissionCoverage(', source)

    def test_rgb_glow_preserved(self):
        result = translate_surface('shipglow', Image.new('RGB', (1, 1), 'white'),
                                   Image.new('RGB', (1, 1), (20, 50, 80)))
        self.assertEqual(result['emissive'].getpixel((0, 0)), (20, 50, 80, 255))

    def test_cutout_is_team_red_not_diffuse_alpha_or_paint(self):
        team = Image.new('RGB', (2, 1)); team.putdata([(127, 0, 0), (128, 0, 0)])
        result = translate_surface('matteScissor', Image.new('RGBA', (2, 1), (90, 80, 70, 17)), team=team)
        self.assertEqual(list(result['diffuse'].getchannel('A').getdata()), [0, 255])
        self.assertEqual(list(result['teamEffect0'].getdata()), [0, 0])
        self.assertEqual(list(result['teamEffect1'].getdata()), [0, 0])

    def test_badge_keeps_solid_body(self):
        result = translate_surface('badge', Image.new('RGBA', (1, 1), (10, 20, 30, 0)))
        self.assertEqual(result['diffuse'].getpixel((0, 0))[3], 255)
        self.assertEqual(result['teamEffect0'].getpixel((0, 0)), 0)
    def test_matte_alpha_is_not_team_paint_or_fake_emission(self):
        result=translate_surface('matteAlpha',Image.new('RGBA',(1,1),(0,0,0,123)))
        self.assertEqual(result['diffuse'].getpixel((0,0)),(0,0,0,123))
        self.assertEqual(result['teamEffect0'].getpixel((0,0)),0)
        self.assertEqual(result['emissive'].getpixel((0,0)),(0,0,0,0))

    def test_no_silent_untextured_or_unknown_fallback(self):
        for shader in ('default', 'innateSS', 'unknown'):
            with self.assertRaises(ValueError):
                translate_surface(shader, Image.new('RGB', (1, 1)))
        with self.assertRaises(ValueError):
            translate_surface('matteScissor', Image.new('RGB', (1, 1)))

    def test_rgba_bootstrap_layout_and_alpha(self):
        team = Image.new('RGB', (2, 1)); team.putdata([(0, 255, 0), (255, 255, 0)])
        surface = translate_surface('matteScissor', Image.new('RGB', (2, 1), 'red'), team=team)
        data = lif_rgba(surface)
        header = struct.unpack_from('<8s10I', data)
        self.assertEqual(header, (b'Willy 7\0', 0x104, 0x38, 2, 1, 0, 0, 48, 0, 56, 58))
        self.assertEqual(len(data), 60)
        self.assertEqual((data[51], data[55]), (0, 255))
        self.assertEqual(data[56:], bytes(4))


if __name__ == '__main__':
    unittest.main()
