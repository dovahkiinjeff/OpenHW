"""Run production background/metadata control flow without game assets or a GPU.

Only external asset I/O, graphics calls and the BTG drawing boundary are mocked.
The mission texture lifecycle, authoring availability and gameplay render gate
are extracted unchanged from the current production source and compiled as C.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class BackgroundAlwaysOn(unittest.TestCase):
    def test_removed_off_controls_cannot_restore_render_gate(self):
        for path in ('src/SDL/render.c', 'src/SDL/main.c',
                     'src/SDL/utility.c', 'src/Game/Options.c',
                     'src/Game/ModernUI.c'):
            source = (ROOT / path).read_text()
            source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
            self.assertNotRegex(source, r'\bshowBackgrounds\b', path)
        main = (ROOT / 'src/SDL/main.c').read_text()
        self.assertNotIn('entryVr("/noBG"', main)

    def test_production_background_and_metadata_lifecycle(self):
        backdrop = (ROOT / 'src/Game/ModernMissionBackdrop.c').read_text()
        render = (ROOT / 'src/SDL/render.c').read_text()
        header = (ROOT / 'src/Game/ModernMissionBackdrop.h').read_text()
        header = header.replace('#include "Types.h"', '')
        globals_start = backdrop.index('static GLuint mmbTexture;')
        globals_end = backdrop.index('static real32 mmbClamp(', globals_start)
        harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int bool32, sdword, GLint;
typedef uint32_t udword, GLuint;
typedef unsigned char ubyte;
typedef float real32;
#define HW_DIAG_FPRINTF fprintf
#define TRUE 1
#define FALSE 0
#define MISSION_1_KHARAK_SYSTEM 1
#define MISSION_3_RETURN_TO_KHARAK 3
#define MMB_DXGI_FORMAT_BC6H_UF16 95
#define GL_MAX_TEXTURE_SIZE 1
#define GL_TEXTURE_2D 2
#define GL_TEXTURE_WRAP_S 3
#define GL_TEXTURE_WRAP_T 4
#define GL_CLAMP_TO_EDGE 5
#define GL_TEXTURE_MAG_FILTER 6
#define GL_TEXTURE_MIN_FILTER 7
#define GL_LINEAR 8
#define GL_LINEAR_MIPMAP_LINEAR 9
#define GL_PROJECTION_MATRIX 10
#define GL_PROJECTION 11
#define GL_MODELVIEW 12
'''
        harness += '#include "' + (ROOT/'src/Game/BackdropMapPolicy.h').as_posix() + '"\n'
        harness += header + '\n' + backdrop[globals_start:globals_end]
        harness += r'''
static int currentMission, gameIsRunning, showBackgrounds;
static int singlePlayerGame = TRUE;
static char btgLastBackground[128];
static int loads, metadataLoads, uploads, btgDraws, modernDraws, starDraws;
static int missingAsset, uploadFailure, invalidAsset, maxTextureSize = 16384;
static int lastAssetMission;
static GLuint nextTexture = 1;
static real32 btgFieldOfView = 90, rndAspectRatio = 1.5f;
typedef struct { real32 clipPlaneNear, clipPlaneFar; } Camera;
static sdword mmbCurrentEnvironment(void) { return currentMission; }
static void glDeleteTextures(int n, const GLuint *p) { (void)n; (void)p; }
static void trClearCurrent(void) {}
static void glBindTexture(int target, GLuint id) { (void)target; (void)id; }
static void glTexParameteri(int a, int b, int c) {(void)a;(void)b;(void)c;}
static void glGenTextures(int n, GLuint *id) { (void)n; *id = nextTexture++; }
static void glGetIntegerv(int name, GLint *value) {(void)name;*value=maxTextureSize;}
static void glGetFloatv(int name, real32 *p) {(void)name;memset(p,0,16*sizeof(*p));}
static void glMatrixMode(int mode) { (void)mode; }
static void glLoadIdentity(void) {}
static void glLoadMatrixf(real32 *p) { (void)p; }
static real32 rndNear(real32 value) { return value; }
static void rgluPerspective(real32 a,real32 b,real32 c,real32 d) {(void)a;(void)b;(void)c;(void)d;}
static void rndTextureEnable(int value) { (void)value; }
static void rndAdditiveBlends(int value) { (void)value; }
static int remasteredBackdropActive(void) { return FALSE; }
static void rndSoftStarBillboardsDraw(Camera *camera) {(void)camera;++starDraws;}
static void hwModernGraphicsSetMissionAuthoringReplacesMapLights(int value) {(void)value;}
static void hwModernGraphicsSetMissionAuthoringLights(void *p,int n) {(void)p;(void)n;}
static void *mmbLoadAsset(const char *path, size_t *size) {
    ++loads; sscanf(path,"Missions/mission%d_sky.dds",&lastAssetMission);
    *size=16; return missingAsset ? NULL : malloc(*size);
}
static void SDL_free(void *p) { free(p); }
static int mmbValidateDds(const ubyte *p,size_t n,sdword *w,sdword *h,
                          sdword *mips,udword *format) {
    (void)p;(void)n;*w=4096;*h=2048;*mips=13;*format=MMB_DXGI_FORMAT_BC6H_UF16;
    return !invalidAsset;
}
static int mmbUploadBptcDds(const ubyte *p,size_t n,sdword w,sdword h,
                            sdword mips,udword format) {
    (void)p;(void)n;(void)w;(void)h;(void)mips;(void)format;++uploads;
    return !uploadFailure;
}
static int mmbLoadHdrMetadata(sdword mission,sdword assetMission) {
    (void)assetMission;++metadataLoads;mmbGodRaySourceMission=mission;
    mmbGodRaySourceValid=TRUE;mmbGodRaySourceDirection[0]=(real32)mission;
    return TRUE;
}
static void mmbAnalyzeGodRaySource(sdword m,sdword w,sdword h,sdword n) {
    (void)m;(void)w;(void)h;(void)n;abort();
}
static void mmbLightingOverrideEnsure(sdword mission) {
    if(mmbLightingOverride.mission != mission) {
        memset(&mmbLightingOverride,0,sizeof(mmbLightingOverride));
        mmbLightingOverride.mission=mission;
    }
}
static void mmbDefaultAmbientTint(real32 *color) {color[0]=color[1]=color[2]=1;}
static int hwModernGraphicsRenderMissionSky(udword texture,real32 fade) {
    (void)fade;if(!texture)abort();++modernDraws;return TRUE;
}
static void mmbSubmitGodRayProjection(void) {}
'''
        for signature in ('static void mmbReleaseTexture(void)',
                          'static bool32 mmbEnsureTexture(sdword mission)',
                          'bool32 modernMissionBackdropLightingAuthoringGet(',
                          'bool32 modernMissionBackdropActive(void)',
                          'void modernMissionBackdropPrepare(void)',
                          'bool32 modernMissionBackdropRender(real32 fade)'):
            harness += '\n' + function(backdrop, signature) + '\n'
        harness += r'''
/* BTG's graphics boundary is mocked, but its production initialization and
   modern/fallback dispatch order are separately verified in Python below. */
static void btgRender(void) {
    modernMissionBackdropPrepare();
    if(!modernMissionBackdropRender(1.0f)) ++btgDraws;
}
'''
        harness += function(render, 'void rndBackgroundRender(real32 radius,')
        harness += r'''
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    Camera camera={1,1000000}; ModernMissionLightingAuthoringState authoring;
    int off,mission,before;
    gameIsRunning=TRUE;
    /* A legacy off setting cannot suppress any campaign sky or its lighting.
       The decoy intentionally remains zero during the first complete pass. */
    for(off=0;off<2;++off) for(mission=1;mission<=16;++mission) {
        showBackgrounds=off;currentMission=mission;mmbPackedSkyVisualsEnabled=FALSE;
        before=btgDraws;loads=metadataLoads=0;
        CHECK(!modernMissionBackdropLightingAuthoringGet(&authoring));
        rndBackgroundRender(1,&camera,TRUE);
        CHECK(btgDraws==before+1 && loads==1 && metadataLoads==1);
        CHECK(lastAssetMission==(mission==3?1:mission));
        CHECK(modernMissionBackdropLightingAuthoringGet(&authoring));
        CHECK(authoring.available && authoring.mission==mission && authoring.sourceValid);
        CHECK(authoring.baseDirection[0]==(real32)mission && authoring.sunIntensity==1);
        rndBackgroundRender(1,&camera,FALSE);
        CHECK(loads==1 && metadataLoads==1 && btgDraws==before+2);
        CHECK(starDraws==0);
    }
    /* The optional packed visual path also initializes the same metadata. */
    currentMission=1;mmbPackedSkyVisualsEnabled=TRUE;before=modernDraws;
    rndBackgroundRender(1,&camera,TRUE);
    CHECK(modernDraws==before+1 && modernMissionBackdropLightingAuthoringGet(&authoring));
    /* First-use failures retain the authentic BTG; preparation is not retried
       every frame and never falsely advertises authoring readiness. */
    for(mission=2;mission<=5;++mission) {
        currentMission=mission;missingAsset=mission==2;invalidAsset=mission==3;
        uploadFailure=mission==4;maxTextureSize=mission==5?1024:16384;
        loads=0;before=btgDraws;
        rndBackgroundRender(1,&camera,TRUE);
        CHECK(btgDraws==before+1 && loads==1);
        CHECK(!modernMissionBackdropLightingAuthoringGet(&authoring));
        CHECK(!authoring.available && authoring.mission==mission);
        rndBackgroundRender(1,&camera,TRUE);
        CHECK(btgDraws==before+2 && loads==1);
    }
    missingAsset=invalidAsset=uploadFailure=0;maxTextureSize=16384;
    /* Multiplayer loads the named sky but retains a distinct authoring key
       for each map, even when both maps share the same sky resource. */
    singlePlayerGame=FALSE;strcpy(btgLastBackground,"BTG/ez08.btg");
    currentMission=hwBackdropContext(0,1,"map-a",btgLastBackground);
    before=modernDraws;loads=metadataLoads=0;
    rndBackgroundRender(1,&camera,TRUE);
    CHECK(modernDraws==before+1 && lastAssetMission==8 && loads==1 && metadataLoads==1);
    CHECK(modernMissionBackdropLightingAuthoringGet(&authoring));
    CHECK(authoring.mission==currentMission && authoring.available);
    currentMission=hwBackdropContext(0,1,"map-b",btgLastBackground);
    rndBackgroundRender(1,&camera,TRUE);
    CHECK(loads==2 && metadataLoads==2 && lastAssetMission==8);
    CHECK(modernMissionBackdropLightingAuthoringGet(&authoring) && authoring.mission==currentMission);
    singlePlayerGame=TRUE;
    /* Main menu has no world draw; unknown custom environments retain BTG. */
    before=btgDraws+modernDraws;gameIsRunning=FALSE;currentMission=6;loads=0;
    rndBackgroundRender(1,&camera,TRUE);
    CHECK(btgDraws+modernDraws==before && loads==0);
    gameIsRunning=TRUE;currentMission=0;before=btgDraws;
    rndBackgroundRender(1,&camera,TRUE);
    CHECK(btgDraws==before+1 && loads==0);
    CHECK(!modernMissionBackdropLightingAuthoringGet(&authoring));
    puts("PASS: all 16 campaign backgrounds, legacy off immunity, cached lighting metadata, BTG failure fallback, menu/noncampaign lifecycle");
    return 0;
}
'''
        # Keep the mock BTG boundary tied to the actual initialization order.
        btg = function((ROOT / 'src/Game/BTG.c').read_text(), 'void btgRender(void)')
        self.assertLess(btg.index('modernMissionBackdropPrepare();'),
                        btg.index('remasteredBackdropRender('))
        self.assertLess(btg.index('modernMissionBackdropPrepare();'),
                        btg.index('modernMissionBackdropRender('))
        compilers = list(Path(os.environ.get('ProgramFiles', 'C:/Program Files')).glob(
            'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers, 'MSVC compiler required for CPU background lifecycle test')
        compiler = sorted(compilers)[-1]
        vcvars = compiler.parents[7] / 'VC/Auxiliary/Build/vcvars64.bat'
        with tempfile.TemporaryDirectory(prefix='hw-background-lifecycle-') as directory:
            directory = Path(directory)
            (directory / 'background.c').write_text(harness)
            command = (f'call "{vcvars}" >nul && "{compiler}" /nologo /TC '
                       '/W3 background.c /Fe:background.exe && background.exe')
            result = subprocess.run(command, shell=True, cwd=directory,
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout)


if __name__ == '__main__':
    unittest.main()
