"""Integration wiring guards, paired with compiled dimension-policy tests."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class TextureResolution(unittest.TestCase):
    def test_ui_persistence_and_cancel(self):
        ui = (ROOT / 'src/Game/ModernUI.c').read_text()
        options = (ROOT / 'src/Game/Options.c').read_text()
        config = (ROOT / 'src/SDL/utility.c').read_text()
        self.assertIn('"TEXTURE RESOLUTION"', ui)
        self.assertEqual(ui.count('muiVideo.tab >= 2 ? MUI_VIDEO_ROW_COUNT'), 2)
        layout = ui.split('static void muiBuildVideoLayout(', 1)[1].split('static void muiDrawVideo(', 1)[0]
        self.assertIn('if (muiVideo.tab < 2) return;', layout)
        self.assertIn('index < MUI_VIDEO_ROW_COUNT', layout)
        self.assertIn('"textureResolution",   scriptSetUdwordCB, &mainTextureResolution', config)
        self.assertIn('mainTextureResolution = opSaveTextureResolution;', options)
        self.assertIn('opSaveTextureResolution = mainTextureResolution;', options)

    def test_raster_masks_use_diffuse_not_bootstrap_dimensions(self):
        raster = (ROOT / 'src/SDL/texreg.c').read_text()
        self.assertIn('trModernLoadLooseGrayPng(maskName, width, height,', raster)
        self.assertNotIn('trModernLoadLooseGrayPng(maskName, sourceWidth, sourceHeight,', raster)
        self.assertIn('sourceWidth = width;\n        sourceHeight = height;', raster)
        self.assertIn('name, mainTextureResolution,', raster)

    def test_all_dxr_channels_share_quality(self):
        mesh = (ROOT / 'src/Game/Mesh.c').read_text()
        self.assertIn('pngName, mainTextureResolution,', mesh)
        self.assertIn('normalName, mainTextureResolution,', mesh)
        self.assertIn('emissionName, mainTextureResolution,', mesh)

    def test_rgba_dds_not_rejected_by_bc7_fast_path(self):
        renderer = (ROOT / 'src/Modern/ModernGraphics.cpp').read_text()
        upload = renderer.split('extern "C" int hwModernGraphicsUploadBoundDds(', 1)[1]
        self.assertIn('metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM', upload)
        self.assertIn('hwglTexImage2D(', upload)
        self.assertIn('DirectX::TEX_FILTER_FANT', renderer)


if __name__ == '__main__':
    unittest.main()
