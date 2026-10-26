from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ModernShipLOD(unittest.TestCase):
    def test_distance_bands_and_hysteresis(self):
        header = (ROOT / 'src/Game/ModernShipLOD.h').read_text()
        cases = r'''
#include <assert.h>
static int pixels(float p, int current, int available) {
    float d=100.f*1080.f/(p*tanf(45.f*0.00872664626f));
    return hwModernShipLODSelect(d*d,100.f,45.f,1080,current,available);
}
int main(void) {
    float p[]={500.f,240.f,120.f,60.f,20.f};
    int current=0;
    for(int i=0;i<5;i++) { current=pixels(p[i],current,5); assert(current==i); }
    for(int i=4;i>=0;i--) { current=pixels(p[i],current,5); assert(current==i); }
    assert(pixels(300.f,0,5)==0);
    assert(pixels(340.f,1,5)==1);
    assert(pixels(280.f,0,5)==1);
    assert(pixels(360.f,1,5)==0);
    assert(pixels(0.001f,0,5)==4);
    assert(pixels(0.001f,0,4)==3);
    assert(pixels(0.001f,0,6)==4);
    assert(pixels(100.f,0,1)==0);
    assert(hwModernShipLODSelect(0.f,100.f,45.f,1080,4,5)==0);
    assert(hwModernShipLODSelect(100.f,0.f,45.f,1080,4,5)==0);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-ship-lod-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(header + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_ship_only_wiring_and_missing_mesh_guard(self):
        source = (ROOT / 'src/Game/LOD.c').read_text()
        entry = source[source.index('lod *lodLevelGet('):source.index('lod* lodPanicLevelGet(')]
        self.assertLess(entry.index('return lodModernShipLevelGet'), entry.index('if (opNoLODVal)'))
        self.assertIn('if (obj->objtype == OBJ_ShipType)', entry)
        self.assertIn('obj->currentLOD = 0;', entry)
        helper = source[source.index('static lod *lodModernShipLevelGet'):source.index('lod *lodLevelGet(')]
        self.assertIn('available < 5', helper)
        self.assertIn('== LT_Mesh', helper)
        self.assertIn('pData != NULL', helper)
        self.assertIn('pData != defaultmesh', helper)
        self.assertIn('MAIN_WindowHeight', helper)
        panic = source[source.index('lod* lodPanicLevelGet('):]
        self.assertIn('return lodLevelGet(spaceObj, camera, ship);', panic)


if __name__ == '__main__':
    unittest.main()
