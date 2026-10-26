"""Run production RM dust persistence with minimal SDL/campaign stubs."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DustPerMissionTests(unittest.TestCase):
    def test_saved_tints_switch_and_legacy_is_preserved(self):
        source = (ROOT / 'src/Game/RemasteredWeaponFX.c').read_text()
        production = 'static RmResourceDustStyle resourceDustStyle=' + source.split(
            'static RmResourceDustStyle resourceDustStyle=', 1)[1].split(
            'static FILE *diagnosticFile;', 1)[0]
        harness = r'''
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
typedef int bool32, sdword;
#define TRUE 1
#define FALSE 0
#define SDL_free free
typedef struct { float opacity,brightness,size,phase,tint[3]; } RmResourceDustStyle;
int singlePlayerGame=1, mission=1;
int spGetCurrentMission(void) { return mission; }
char *SDL_GetBasePath(void) { char *p=malloc(3); p[0]='.';p[1]='/';p[2]=0;return p; }
'''
        cases = r'''
int main(void) {
    FILE *fp=fopen("RMHarvestableDust.cfg","wb");
    RmResourceDustStyle *s;
    fprintf(fp,"RM_DUST_V1 2 1 1 .5 .5 1 1\n"); fclose(fp);
    s=rmFxResourceDustStyle(); assert(s->opacity==2 && s->tint[0]==.5f);
    s->tint[0]=3; s->opacity=4; assert(rmFxResourceDustSave());
    mission=2; s=rmFxResourceDustStyle();
    assert(s->tint[0]==.5f && s->opacity==2);
    s->tint[0]=1; s->tint[2]=3; assert(rmFxResourceDustSave());
    mission=1; s=rmFxResourceDustStyle(); assert(s->tint[0]==3 && s->opacity==4);
    rmFxResourceDustReset(); assert(rmFxResourceDustStyle()->opacity==1);
    rmFxResourceDustReload(); assert(rmFxResourceDustStyle()->opacity==4);
    mission=2; s=rmFxResourceDustStyle(); assert(s->tint[0]==1 && s->tint[2]==3);
    mission=3; s=rmFxResourceDustStyle(); assert(s->opacity==2 && s->tint[0]==.5f);
    remove("RMHarvestableDust.cfg"); mission=4;
    assert(rmFxResourceDustStyle()->opacity==1);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-rm-dust-maps-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + production + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)
        editor = (ROOT / 'src/Game/ModernMapEditor.c').read_text()
        self.assertNotIn('SAVE GLOBAL', editor)
        self.assertIn('RM DUST RELOADED / CURRENT MAP', editor)


if __name__ == '__main__':
    unittest.main()
