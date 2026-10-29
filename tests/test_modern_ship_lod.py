from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ModernShipLOD(unittest.TestCase):
    def test_ghost_ship_missing_scheme_uses_native_texture(self):
        source = (ROOT / 'src/Game/Mesh.c').read_text()
        start = source.index('            trhandle selected = iColorScheme >= 0')
        end = source.index('            trMakeCurrent(selected);', start)
        logic = source[start:end] + 'return selected;\n}\n'
        harness = r'''
#include <assert.h>
#include <string.h>
#define strncasecmp _strnicmp
#define TR_Invalid -1
#define TR_RegistrySize 20
#define MAX_MULTIPLAYER_PLAYERS 8
#define TE_DerelictDefaultColorScheme 7
typedef int trhandle;
typedef struct {char *fileName;} texreg;
static texreg textures[20];
static int trIndex(int handle){return handle;}
static texreg *trStructureGet(int handle){return &textures[handle];}
static int choose(trhandle *handles,int iColorScheme){
'''
        cases = r'''
int main(void){
 int h[8]={-1,-1,-1,-1,-1,-1,-1,3};
 textures[3].fileName="Traders\\Ghostship\\RMNative\\c0_m0";
 assert(choose(h,1)==3);assert(choose(h,-1)==3);
 h[1]=4;assert(choose(h,1)==4); /* never override a valid authored scheme */
 h[1]=-1;textures[3].fileName="TRADERS\\OTHER\\RMNative\\c0_m0";
 assert(choose(h,1)==TR_Invalid);
 textures[3].fileName="DERELICTS\\WRECK\\part";assert(choose(h,1)==3);
 h[7]=TR_Invalid;assert(choose(h,1)==TR_Invalid);
 return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-ghost-material-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + logic + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_authored_distance_bands(self):
        header = (ROOT / 'src/Game/ModernShipLOD.h').read_text()
        cases = r'''
#include <assert.h>
int main(void) {
    float bounds[]={100.f,400.f,900.f,1600.f};
    assert(hwModernShipLODSelect(0.f,bounds,5)==0);
    assert(hwModernShipLODSelect(100.f,bounds,5)==0);
    assert(hwModernShipLODSelect(101.f,bounds,5)==1);
    assert(hwModernShipLODSelect(401.f,bounds,5)==2);
    assert(hwModernShipLODSelect(901.f,bounds,5)==3);
    assert(hwModernShipLODSelect(1601.f,bounds,5)==4);
    assert(hwModernShipLODSelect(1601.f,bounds,4)==3);
    assert(hwModernShipLODSelect(1601.f,bounds,6)==4);
    assert(hwModernShipLODSelect(1601.f,bounds,1)==0);
    assert(hwModernShipLODSelect(NAN,bounds,5)==0);
    assert(hwModernShipLODSelect(1000.f,0,5)==0);
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
        self.assertNotIn('MAIN_WindowHeight', helper)
        self.assertNotIn('fieldofview', helper)
        self.assertNotIn('lodScaleFactor', helper)
        self.assertIn('info->level[index].bOff', helper)
        self.assertIn('shiptype == Ghostship', helper)
        self.assertIn('meshNativeVisualParameters(info->level[0].pData', helper)
        panic = source[source.index('lod* lodPanicLevelGet('):]
        self.assertIn('return lodLevelGet(spaceObj, camera, ship);', panic)


if __name__ == '__main__':
    unittest.main()
