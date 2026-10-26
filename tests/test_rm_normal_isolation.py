import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class NativeNormals(unittest.TestCase):
    def test_native_materials_are_explicitly_tagged(self):
        source=(ROOT/'src/Modern/ModernRaytracing.cpp').read_text()
        self.assertIn('record.nativeNormal = namedKey.find("/rmnative/")',source)
        self.assertIn('record.nativeNormal ? 0xffff8080u : generateSurfaceNormal',source)
        self.assertIn('texture->second.nativeNormal ? gpuSurfaceNativeNormal',source)
    def test_authored_normals_bypass_generated_strength_and_relief(self):
        source=(ROOT/'src/Modern/ModernRaytracing.hlsl').read_text()
        sample=source[source.index('float3 SampleGeneratedNormal'):source.index('void EvaluateSurface')]
        self.assertLess(sample.index('SURFACE_NATIVE_NORMAL'),sample.index('slope *= NormalDetailStrength'))
        self.assertIn('return normalize(normalSample.xyz * 2.0 - 1.0)',sample)
        self.assertIn('(surface.flags & SURFACE_NATIVE_NORMAL) == 0u && NormalDetailStrength <= 0.0001',source)
        self.assertIn('(surface.flags & SURFACE_NATIVE_NORMAL) == 0u)\n    {\n        lighting *= GeneratedReliefResponse',source)
if __name__=='__main__':unittest.main()
