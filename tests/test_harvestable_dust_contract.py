"""Regression guards for global dust authoring and independent scene submission."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class HarvestableDustContract(unittest.TestCase):
    def test_rope_candidate_culling_preserves_march_lattice(self):
        source=(ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        body=source[source.index('bool DustRayVolumeInterval('):source.index('float DustProceduralSunVisibility(')]
        self.assertIn('float support = volume.seedPadding;',body)
        self.assertIn('float3 tightHalf = min(halfSize * supportRadius',body)
        self.assertIn('entryDistance = tMin / directionScale;',body)
        self.assertIn('exitDistance = tMax / directionScale;',body)
        self.assertNotIn('entryDistance = tightMin',body)
        self.assertNotIn('exitDistance = tightMax',body)
        self.assertIn('gpu.seedPadding = hwdust::ropeSupportRadius',source)
        self.assertIn('hwdust::ropeSupportHalfSize(volume.size',source)

    def test_opaque_silhouettes_resolve_native_on_both_sides(self):
        source=(ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        body=source[source.index('float4 FilteredDustScreen('):source.index('float4 PresentPS(')]
        self.assertIn('((edgeDepth < 0.99990) != centerHasGeometry)',body)
        self.assertIn('sampleHasGeometry != centerHasGeometry',body)
        self.assertIn('centerHasGeometry && relativeDepthError >= 0.02',body)
        self.assertIn('if (!resolveNativeEdge && totalWeight > 0.0001)',body)
        self.assertIn('ApplyVolumetricDust(uv, 0.0, true, transmission)',body)
        self.assertNotIn('ApplyVolumetricDust(uv, 0.0, false, transmission)',body)

    def test_global_file_and_packaged_defaults_match_material_schema(self):
        lines=(ROOT/'assets/DustClouds/HarvestableDust.rtxdust').read_text().splitlines()
        self.assertEqual(lines[0],'RTXHARVESTABLEDUST 3')
        self.assertEqual(sum(line.startswith('PRESET STYLE ') for line in lines),9)
        values=lines[1].split()
        self.assertEqual(values[0],'STYLE')
        self.assertEqual(len(values),17)
        self.assertTrue(all(float(v)>=0 for v in values[1:]))
        source=(ROOT/'src/Game/ModernHarvestableDust.c').read_text()
        self.assertIn('modernHarvestableDustSave(void) {return FALSE;}',source)
        self.assertIn('hwModernGraphicsSetHarvestableDustVolumes(NULL,0)',source)
        self.assertNotIn('fileLoad',source)
        self.assertNotIn('.rtxmap',source)

    def test_stable_resource_shape_not_camera_or_time(self):
        source=(ROOT/'src/Game/RemasteredWeaponFX.c').read_text()
        body=source[source.index('bool32 rmFxDrawResourceDust'):source.index('void rmWeaponFxDrawTransient')]
        self.assertIn('HarvestableDust',body)
        self.assertIn('dust->resourcevalue<=0',body)
        self.assertIn('system->healthFactor',body)
        self.assertIn('SOF_Hide',body)
        self.assertNotIn('cameraDistance',body)
        self.assertNotIn('SDL_GetTicks',body)

    def test_independent_f11_tab_save_reload_and_single_color(self):
        source=(ROOT/'src/Game/ModernMapEditor.c').read_text()
        self.assertIn('#define MME_TAB_COUNT 4',source)
        self.assertNotIn('tabs[4]',source)
        build=(ROOT/'CMakeLists.txt').read_text()
        self.assertNotIn('install(DIRECTORY "${HW_SOURCE_DIR}/assets/DustClouds/"',build)
        retired=(ROOT/'src/Game/ModernHarvestableDust.c').read_text()
        self.assertNotIn('SetVolumetricDustVolumes',retired)
        self.assertNotIn('SetHarvestableNebulaVolumes',retired)

    def test_dust_alone_does_not_skip_presenter(self):
        source=(ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        lines=[s for s in source.splitlines() if 'volumetricDustVolumes.empty() && harvestableNebulaVolumes.empty()' in s]
        self.assertEqual(len(lines),5)
        for line in lines: self.assertIn('harvestableDustVolumes.empty()',line)
        self.assertIn('for (const HWModernVolumetricDustVolume &source : harvestableDustVolumes)',source)

if __name__=='__main__': unittest.main()
