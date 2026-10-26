from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]

class CloseupRenderCost(unittest.TestCase):
    def test_opaque_is_per_variant_and_invalidates_tlas_hash(self):
        source = (ROOT/'src/Modern/ModernRaytracing.cpp').read_text()
        self.assertIn('classifyOpaqueVariant(*variant);',source)
        self.assertIn('classifyOpaqueVariant(variant);',source)
        self.assertIn('D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE',source)
        self.assertIn('hashBytesLocal(contentHash, &instanceFlagsForHash',source)
        self.assertIn('record.minimumAlpha = std::min(record.minimumAlpha, surfaceRgba[index] >> 24u)',source)

    def test_cutout_anyhit_only_samples_coverage(self):
        source = (ROOT/'src/Modern/ModernRaytracing.hlsl').read_text()
        body = source.split('void SurfaceAnyHit(',1)[1].split('[shader("closesthit")]',1)[0]
        self.assertNotIn('EvaluateSurface(',body)
        self.assertIn('SampleSurfaceChannel(surface, uv, 0u).a',body)
        self.assertIn('IgnoreHit();',body)

    def test_zero_atmosphere_rejected_before_march(self):
        source = (ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        body = source.split('float3 AtmosphericSunScattering(',1)[1].split('float3 ApplyVolumetricDust(',1)[0]
        self.assertLess(body.index('if (directionalPhase <= 0.0)'),body.index('contrastLength +='))

    def test_empty_medium_keeps_independent_atmosphere_and_harvestable_detail(self):
        source = (ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        body = source.split('float4 FilteredDustScreen(',1)[1].split('float4 PresentPS(',1)[0]
        self.assertIn('if (!intersectsMedium)',body)
        self.assertIn('TryReuseClearAtmosphere(uv,cachedAtmosphere)',body)
        self.assertIn('return float4(AtmosphericSunScattering(',body)
        self.assertIn('ApplyVolumetricDust(uv,0.0,true,transmission)',body)
        self.assertIn('resolveNativeEdge',body)

if __name__ == '__main__': unittest.main()
