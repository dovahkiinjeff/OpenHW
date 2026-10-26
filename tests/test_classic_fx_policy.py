import json
import re
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from build_weapon_fx_textures import fx_dimensions


class ClassicFxPolicy(unittest.TestCase):
    def test_small_sprites_and_wide_frames_meet_both_bounds(self):
        for source, expected in [((8, 8), (256, 256)),
                                 ((16, 4), (1024, 256)),
                                 ((64, 8), (2048, 256)),
                                 ((128, 128), (512, 512)),
                                 ((256, 256), (1024, 1024)),
                                 ((1024, 1024), (2048, 2048))]:
            size = fx_dimensions(*source)
            self.assertEqual(size, expected)
            self.assertEqual(size[0] * source[1], size[1] * source[0])
        with self.assertRaises(ValueError): fx_dimensions(8192, 1)

    def test_classic_fallback_keeps_dust_and_upgraded_engines(self):
        rm = (ROOT / 'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertIn('static bool32 useClassicShipFx=TRUE', rm)
        self.assertEqual(rm.count('if (useClassicShipFx) return NULL;'), 2)
        dust = rm.split('bool32 rmFxDrawResourceDust(')[1].split('void rmWeaponFxDrawTransient')[0]
        self.assertIn('"HarvestableDust"', dust)
        self.assertNotIn('useClassicShipFx', dust)
        trails = (ROOT / 'src/Game/Trails.c').read_text()
        self.assertIn('hwglEngineGlow(', trails)
        self.assertNotIn('rmFxDrawEngine(', trails)

    def test_muzzles_and_beams_do_not_follow_hull_lod(self):
        gun = (ROOT / 'src/Game/Gun.c').read_text()
        self.assertIn('if (bullet->bulletType == BULLET_Beam) LOD = 0;', gun)
        for path in ('src/Game/Gun.c', 'src/Ships/DefenseFighter.c'):
            text = (ROOT / path).read_text()
            text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
            text = re.sub(r'//[^\n]*', '', text)
            for block in text.split('EGT_GunFire];')[1:]:
                block = block.split('stat = etgLOD->level[LOD];')[0]
                self.assertIn('LOD = 0;', block)
                self.assertNotIn('LOD = ship->currentLOD;', block)
        registry = (ROOT / 'src/SDL/texreg.c').read_text()
        self.assertIn('{ textureQuality = 3; break; }', registry)
        self.assertIn('pngName, textureQuality,', registry)

    def test_generated_dds_dimensions_match_manifest_and_bounds(self):
        work = ROOT / 'out/classic-fx-256-2048'
        manifest = work / 'weapon-fx-manifest.json'
        if not manifest.exists(): self.skipTest('Owned HW1 texture outputs unavailable')
        records = json.loads(manifest.read_text())
        self.assertEqual(len(records), 425)
        for record in records:
            with (work / 'dds' / record['dds']).open('rb') as stream:
                header = stream.read(128)
            self.assertEqual(header[:4], b'DDS ')
            height, width = struct.unpack_from('<II', header, 12)
            self.assertEqual([width, height], record['output_size'])
            self.assertGreaterEqual(min(width, height), 256)
            self.assertLessEqual(max(width, height), 2048)
            self.assertFalse(record['normal_generated'])


if __name__ == '__main__': unittest.main()
