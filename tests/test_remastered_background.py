"""Asset-independent safety tests for the Remastered translation boundary."""
import sys
import struct
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from translate_remastered_background import chunks, decompress_pool, clip, triangle
from inspect_remastered_scene import Reader


class RemasteredBackgroundTests(unittest.TestCase):
    def test_scene_reader_rejects_bad_lengths(self):
        with self.assertRaises(ValueError):
            Reader(struct.pack('<I', 5000)).string()
        with self.assertRaises(ValueError):
            Reader(struct.pack('<I', 3)+b'a').string()
        with self.assertRaises(ValueError):
            Reader(b'a').done()

    def test_literal_pool(self):
        self.assertEqual(decompress_pool(struct.pack('<I', 0x80000000)+b'hello', 5), b'hello')

    def test_reject_bad_pool(self):
        for data, length in ((b'', 1), (b'\0'*4+b'a', 1),
                             (struct.pack('<I', 0x80000001)+b'\xfc', 3)):
            with self.assertRaises(ValueError):
                decompress_pool(data, length)

    def test_zero_run(self):
        self.assertEqual(decompress_pool(struct.pack('<I', 0x80000001)+b'\0', 3), b'\0'*3)

    def test_iff_bounds_and_version(self):
        data = b'NRML'+struct.pack('>I', 8)+b'BMSH'+struct.pack('>I', 1401)
        self.assertEqual(list(chunks(data)), [(b'BMSH', struct.pack('>I', 1401))])
        for truncated in (data[:7], data[:-1], data+b'x'):
            with self.assertRaises(ValueError):
                list(chunks(truncated))

    def test_camera_plane_clipping(self):
        vertices = np.array([[-.2, -.2, 1, 1, 0, 0, 1],
                             [.2, -.2, 1, 0, 1, 0, 1],
                             [0, .2, -1, 0, 0, 1, 1]], dtype=float)
        result = clip(vertices)
        self.assertGreaterEqual(len(result), 3)
        self.assertTrue(np.isfinite(result).all())
        self.assertTrue((result[:, 2] >= 1e-5-1e-12).all())
        self.assertTrue((np.abs(result[:, :2]) <= result[:, 2:3]+1e-12).all())

    def test_reject_back_facing_cube_triangle(self):
        self.assertEqual(clip(np.array([[0, 0, -1], [1, 0, -1], [0, 1, -1]], float)), [])

    def test_triangle_does_not_fill_outside(self):
        image = np.zeros((32, 32, 3), np.float32)
        vertices = np.array([[-.5, -.5, 1, 1, 0, 0, 1],
                             [.5, -.5, 1, 1, 0, 0, 1],
                             [0, .5, 1, 1, 0, 0, 1]], float)
        triangle(image, vertices)
        self.assertEqual(float(image[0, 0].sum()), 0)
        self.assertGreater(float(image[14, 16, 0]), .9)
        self.assertTrue(np.isfinite(image).all())


if __name__ == '__main__':
    unittest.main()
