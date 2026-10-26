"""Keep the prototype isolated and the fleet loader confined to R1 visuals."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeHodScopeTests(unittest.TestCase):
    def test_single_interceptor_opt_in_and_legacy_fallback(self):
        text = (ROOT/'src/Game/Mesh.c').read_text()
        loader = text.split('static meshdata *meshTryNativeInterceptorHod', 1)[1].split('meshdata *meshLoad', 1)[0]
        self.assertIn('R1\\\\HeavyInterceptor\\\\RMTest\\\\HeavyInterceptor.geo', loader)
        self.assertIn('strcasecmp(fileName, experiment) != 0', loader)
        self.assertIn('FF_IgnoreBIG', loader)
        self.assertIn('using GEO fallback', loader)
        self.assertNotIn('fileSave', loader)
        self.assertNotIn('Somtaaw', loader)
        self.assertNotIn('shiptype', loader)
        self.assertIn('mesh = meshTryNativeInterceptorHod(inFileName)', text)
        self.assertIn('meshLoadAndConvertTo64Bit(fileName', text)

    def test_fleet_requires_local_package_and_original_r1_lod_path(self):
        text=(ROOT/'src/Game/Mesh.c').read_text()
        loader=text.split('static meshdata *meshTryNativeFleetPackage',1)[1].split('meshdata *meshLoad',1)[0]
        self.assertIn('strncasecmp(original,"R1\\\\",3)',loader)
        self.assertIn('Rl0\\\\LOD%u',loader)
        self.assertIn('visual.rmnative',loader)
        self.assertIn('FF_IgnoreBIG',loader)
        self.assertIn('retaining classic GEO',loader)
        self.assertNotIn('fileSave',loader)
        package=(ROOT/'src/Modern/RemasteredVisualPackage.cpp').read_text()
        self.assertIn('invalid/out-of-scope native R1 package',package)
        self.assertNotIn('"CataAcolyte"',package)
        self.assertNotIn('"P2Mothership"',package)


if __name__ == '__main__':
    unittest.main()
