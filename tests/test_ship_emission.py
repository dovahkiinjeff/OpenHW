from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class ShipEmission(unittest.TestCase):
    def test_production_key_separates_emissive_policy(self):
        mesh=(ROOT/'src/Game/Mesh.c').read_text()
        start=mesh.index('static void meshModernSurfaceTextureKey(')
        end=mesh.index('\n}',start)+2
        function=mesh[start:end]
        harness=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define FL_Path 260
#define FF_IgnoreBIG 1
#define strcasecmp _stricmp
int fileExists(const char *name, int flags) { (void)name; (void)flags; return 0; }
#define MDF_SelfIllum 64
#define bitTest(v,m) ((v)&(m))
typedef struct { unsigned int flags,nFullAmbient; } materialentry;
'''
        cases=r'''
int main(void) {
    char hull[512],lamp[512],smooth[512],palette[512];
    materialentry m={0,0};
    meshModernSurfaceTextureKey(hull,sizeof(hull),"r1/ship/lod0/page0.dds",&m);
    m.flags=64;
    meshModernSurfaceTextureKey(lamp,sizeof(lamp),"r1/ship/lod0/page0.dds",&m);
    assert(strcmp(hull,lamp)!=0);
    m.flags=64|2;
    meshModernSurfaceTextureKey(smooth,sizeof(smooth),"r1/ship/lod0/page0.dds",&m);
    assert(strcmp(lamp,smooth)==0);
    m.flags=0;m.nFullAmbient=12;
    meshModernSurfaceTextureKey(palette,sizeof(palette),"r1/ship/lod0/page0.dds",&m);
    assert(strcmp(hull,palette)!=0 && strcmp(lamp,palette)!=0);
    return 0;
}
'''
        cache=(ROOT/'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup=Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)',cache)[1])/'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-emission-') as tmp:
            folder=Path(tmp)
            (folder/'test.c').write_text(harness+function+cases)
            result=subprocess.run(f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',cwd=folder,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            subprocess.run([str(folder/'test.exe')],cwd=folder,check=True)
        self.assertEqual(mesh.count('meshModernSurfaceTextureKey(textureKey,'),3)
        self.assertNotIn('hwModernGraphicsTryAliasSurfaceTexture(material, fullName)',mesh)
        self.assertNotIn('material, pngName, (unsigned int)width',mesh)

    def test_primary_mask_and_compositor_contract(self):
        shader=(ROOT/'src/Modern/ModernRaytracing.hlsl').read_text()
        self.assertIn('saturate(primaryEmissiveMask)',shader)
        self.assertIn('SampleSurfaceChannelLod(surface, uv, 1u, surfaceMip).a : 0.0',shader)
        self.assertEqual(shader.count('primaryEmissiveMask = 0.0;'),4)
        render=(ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        self.assertIn('emissiveSurfaceMask = FilteredEmissionProtection(guideUv)',render)
        self.assertIn('abs(depth-centerDepth)',render)
        self.assertIn('max(emissiveFxProtection,emissiveSurfaceMask)',render)
        self.assertNotIn('emissiveSurfaceColor',render)
        self.assertLess(render.index('max(emissiveFxProtection,emissiveSurfaceMask)'),render.index('linearScene * transmission + medium'))
        host=(ROOT/'src/Modern/ModernRaytracing.cpp').read_text()
        self.assertIn('shaderConfig.MaxPayloadSizeInBytes = 104;',host)


if __name__=='__main__': unittest.main()
