from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GpuWorkReuse(unittest.TestCase):
    def test_atmosphere_cache_rejects_medium_and_depth_boundaries(self):
        text = (ROOT / 'src/Modern/ModernGraphics.cpp').read_text()
        body = text.split('bool TryReuseClearAtmosphere(', 1)[1].split('float4 FilteredDustScreen(', 1)[0]
        self.assertIn('if (value.a < 1.0) return false;', body)
        self.assertIn('sampleGeometry != centerGeometry', body)
        self.assertIn('relativeError >= 0.001', body)
        self.assertIn('GodRaySourceExternal & 16u', body)
        self.assertIn('float2(uv.x,1.0-uv.y)', body)
        self.assertIn('scattering += value.rgb*weight', body)
        # Cache is current-frame and never accepts an incomplete footprint.
        self.assertNotIn('History', body)
        self.assertIn('TryReuseClearAtmosphere(uv,cachedAtmosphere)', text)
        self.assertIn('HW_OPT_ATMOS_CACHE', text)

    def test_exact_zero_contribution_skips(self):
        text = (ROOT / 'src/Modern/ModernGraphics.cpp').read_text()
        body = text.split('float3 AtmosphericSunScattering(', 1)[1].split('float3 ApplyVolumetricDust(', 1)[0]
        self.assertLess(body.index('if (envelope <= 0.0) continue;'), body.index('float visibility ='))
        self.assertLess(body.index('if (visibility <= 0.0) continue;'), body.index('float reference ='))
        self.assertIn('if (any(cachedDirectSunLighting > 0.0))', text)
        # Algebra: max(0, visibility-reference) is exactly zero whenever
        # visibility is zero and the reference visibility is nonnegative.
        for reference in (0, .1, .25, .5, 1):
            self.assertEqual(max(0, 0-reference), 0)

    def test_one_footprint_for_all_hit_material_channels(self):
        shader = (ROOT / 'src/Modern/ModernRaytracing.hlsl').read_text()
        evaluation = shader.split('void EvaluateSurface(', 1)[1].split('uint Hash(', 1)[0]
        self.assertEqual(evaluation.count('SurfaceMipLevel(surface)'), 1)
        self.assertIn('SampleSurfaceChannelLod(surface, uv, 0u, surfaceMip)', evaluation)
        self.assertIn('SampleSurfaceChannelLod(surface, uv, 1u, surfaceMip)', evaluation)
        self.assertIn('SampleSurfaceChannelLod(surface, uv, 3u, surfaceMip)', shader)
        normal = shader.split('float3 SampleGeneratedNormal(', 1)[1].split('void EvaluateSurface(', 1)[0]
        self.assertNotIn('SurfaceMipLevel(surface)', normal)
        self.assertIn('max(surfaceMip, log2(max(1.0, filterTexels)))', normal)


if __name__ == '__main__': unittest.main()
