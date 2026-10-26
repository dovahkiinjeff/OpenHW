"""Synthetic tests: no installed or copyrighted game data required."""
from pathlib import Path
import io
import struct
import sys
import unittest

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from import_remastered_interceptor import geo, lif_palette, dds_rgba, remastered_surface_masks, remastered_paint_shade
from extract_vanilla_ship_textures import decode_lif
from classic_mesh_layout import translate, OBJECT


class RemasteredInterceptorTests(unittest.TestCase):
    def test_paint_shading_preserves_unpainted_hull_and_panel_detail(self):
        diffuse = Image.new('RGB', (3, 1), (100, 100, 100))
        diffuse.putpixel((1, 0), (50, 50, 50))
        retention = Image.new('RGB', (3, 1), (255, 0, 0))
        retention.putpixel((2, 0), (255, 255, 0))
        shade = remastered_paint_shade(diffuse, retention)
        self.assertLess(shade.getpixel((1, 0)), shade.getpixel((0, 0)))
        self.assertLess(shade.getpixel((0, 0)), 200)
        self.assertEqual(shade.getpixel((2, 0)), 255)
        # A fully yellow stripe becomes shaded yellow, never emissive white.
        yellow = np.array([255, 255, 0])*shade.getpixel((0, 0))/255
        self.assertLess(yellow[0], 200)
        self.assertEqual(yellow[2], 0)

    def test_remastered_packed_channels_and_inverse_paint_masks(self):
        glow = Image.new('RGB', (3, 1), (200, 0, 180))
        glow.putpixel((2, 0), (0, 128, 0))
        team = Image.new('RGB', (3, 1), (255, 255, 0))
        team.putpixel((1, 0), (0, 255, 0))
        team.putpixel((2, 0), (255, 0, 0))
        emission, masks = remastered_surface_masks(glow, team)
        self.assertEqual(list(masks.getdata()), [(0, 0, 0), (255, 0, 0), (0, 255, 0)])
        self.assertEqual(list(emission.getdata()), [(0, 0, 0), (0, 0, 0), (128, 128, 128)])

    def test_geometry_axes_normals_indices_uvs(self):
        source = np.zeros((3, 20), '<f4')
        source[:, :3] = [[0, 0, 0], [1, 0, 0], [0, 1, 0]]
        source[:, 5] = 1
        source[:, 16:18] = [[.1, .2], [.8, .3], [.4, .9]]
        raw, vertices, faces, uv, materials = geo([(0, source), (1, source)], 3)
        _, report = translate(raw)
        self.assertEqual(report['triangles'], 2)
        self.assertEqual(report['materials'], 2)
        np.testing.assert_allclose(vertices[:3], [[0, 0, 0], [0, -3, 0], [3, 0, 0]])
        np.testing.assert_allclose(uv[0].reshape(3, 2), source[:, 16:18])
        obj = OBJECT.unpack_from(raw, 68)
        self.assertEqual(obj[4], 6)
        self.assertEqual(obj[7], 2)
        self.assertEqual(obj[11:14], (0, 0, 0))
        self.assertEqual(materials.tolist(), [0, 1])
        source[:, 3:6] = 0
        with self.assertRaisesRegex(ValueError, 'normals'):
            geo([(0, source)], 1)

    def test_lif_and_dds_preserve_color_team_and_glow_policy(self):
        color = Image.new('RGB', (8, 8), (80, 90, 100))
        glow = Image.new('RGB', (8, 8))
        glow.putpixel((0, 0), (255, 255, 255))
        team = Image.new('RGB', (8, 8), (32, 128, 0))
        lif = lif_palette(color, glow, team)
        decoded, base, stripe, meta = decode_lif(lif)
        self.assertTrue(meta['paletted'])
        self.assertEqual(meta['flags'], 0x32)
        self.assertEqual(decoded.getpixel((0, 0)), (80, 90, 100, 255))
        self.assertEqual(base.getpixel((0, 0)), 32)
        self.assertEqual(stripe.getpixel((0, 0)), 128)
        offset = struct.unpack_from('<I', lif, 32)[0]
        self.assertLess(lif[offset], 64)
        self.assertGreaterEqual(lif[offset+1], 64)
        dds = Image.open(io.BytesIO(dds_rgba(color))).convert('RGBA')
        self.assertEqual(dds.size, (8, 8))
        self.assertEqual(dds.getpixel((0, 0)), (80, 90, 100, 255))

    def test_native_dds_size_does_not_overflow_legacy_lif_size_table(self):
        color = Image.new('RGB', (2048, 2048), (80, 90, 100))
        glow = Image.new('RGB', color.size)
        team = Image.new('RGB', color.size)
        image, _, _, _ = decode_lif(lif_palette(color, glow, team))
        self.assertEqual(image.size, (512, 512))
        dds = Image.open(io.BytesIO(dds_rgba(color)))
        self.assertEqual(dds.size, (2048, 2048))


if __name__ == '__main__':
    unittest.main()
