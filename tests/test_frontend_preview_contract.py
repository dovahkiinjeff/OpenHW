"""Execute production preview ownership/scoping functions without a game window."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
def function(source,signature):
    start=source.index(signature); opening=source.index('{',start); end=opening+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

def run_native(code,suffix='c'):
    compiler=sorted(Path(os.environ.get('ProgramFiles','C:/Program Files')).glob(
        'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))[-1]
    vcvars=compiler.parents[7]/'VC/Auxiliary/Build/vcvars64.bat'
    with tempfile.TemporaryDirectory(prefix='openhw-preview-') as folder:
        folder=Path(folder);(folder/f'check.{suffix}').write_text(code)
        result=subprocess.run(f'call "{vcvars}" >nul && "{compiler}" /nologo /I"{ROOT/"src/Modern"}" check.{suffix} /Fe:check.exe && check.exe',
            cwd=folder,shell=True,text=True,capture_output=True)
        if result.returncode:raise AssertionError(result.stdout+result.stderr)
        print(result.stdout)

class PreviewContract(unittest.TestCase):
    def test_menu_ship_is_horizontal_without_rotating_game_models(self):
        code='#include "'+(ROOT/'src/Game/FrontendPreviewPose.h').as_posix()+'"\n'
        code+=r'''
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int main(void){
    const float *m=hwFrontendPreviewBasis;int i,j,k;
    // Every view rotation must preserve an orthonormal, right-handed model.
    for(i=0;i<3;++i)for(j=0;j<3;++j){float dot=0;for(k=0;k<3;++k)dot+=m[i*4+k]*m[j*4+k];if(fabsf(dot-(i==j))>.00001f)abort();}
    if(m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2])<.999f)abort();
    // Engine-model forward (+Z) should project mostly horizontally under the
    // actual default menu yaw/pitch; its up (+X) should remain above the ship.
    {float yaw=42*.01745329252f,pitch=-18*.01745329252f;
     float x=cosf(yaw)*m[8]+sinf(yaw)*m[10];
     float z=-sinf(yaw)*m[8]+cosf(yaw)*m[10];
     float y=cosf(pitch)*m[9]-sinf(pitch)*z;
     if(x<=0 || fabsf(x)<3*fabsf(y) || m[1]<.99f)abort();}
    puts("PASS: menu interceptor faces horizontally with an upright, right-handed model basis");return 0;
}
'''
        run_native(code)
    def test_registry_palettes_cannot_alias_a_live_ship(self):
        source=(ROOT/'src/SDL/texreg.c').read_text()
        harness=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef int sdword,bool32;typedef uintptr_t trhandle;typedef unsigned udword,crc32;
typedef unsigned char ubyte;typedef unsigned short uword;
typedef struct {unsigned base,detail;} trcolorinfo;
typedef struct {int nUsageCount,nPalettes,currentPalette,sharedFrom;unsigned flags,paletteCRC,handle;
    void *meshReference;char *fileName;ubyte *palettes;uword baseScalar,stripeScalar;} texreg;
#define TRUE 1
#define FALSE 0
#define TR_RegistrySize 8
#define TR_NumPalettesPerTexture 8
#define TR_NumPaletteSize (sizeof(trcolorinfo)*8)
#define TR_UnusedColorInfo 0
#define TR_NotShared -1
#define TR_Invalid ((trhandle)-1)
#define TR_InvalidInternalHandle -1
#define TR_BadCRC 0
#define TRF_Pending 1
#define TRF_Allocated 4
#define NonVolatile 0
#define SDWORD_Max 0x7fffffff
#define trHandleMake(i,p) ((trhandle)((i)|((p)<<16)))
#define trIndex(h) ((h)&65535)
#define trPaletteIndex(h) ((h)>>16)
#define trStructure(h) (&trTextureRegistry[trIndex(h)])
#define trAllocated(i) (trTextureRegistry[i].flags&4)
#define trPending(i) (trTextureRegistry[i].flags&1)
#define trSetPending(i) (trTextureRegistry[i].flags|=1)
#define trSetAllocated(i) (trTextureRegistry[i].flags|=4)
#define max(a,b) ((a)>(b)?(a):(b))
#define colRealToUdword(v) 255
#define dbgAssertOrIgnore(x) do {if(!(x))abort();}while(0)
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static texreg trTextureRegistry[8];static crc32 trNameCRCs[8];static int trHighestAllocated=-1,trLowestFree;
static ubyte trPreviewPrivate[8];static bool32 trRegisteringPreview;
static trcolorinfo trDefaultColorInfo={0xffffffff,0xffffffff};static float trBaseColorScalar,trStripeColorScalar;
static int retired,deletedGpu;
static void *memAlloc(size_t n,const char *name,int flags){return calloc(1,n);}
static void memClearDword(void *p,unsigned v,size_t n){memset(p,0,n*4);}
static crc32 crc32Compute(ubyte *p,int n){crc32 r=0;while(n--)r=r*31+*p++;return r;}
static trcolorinfo *trColorInfosPointer(int i){return (trcolorinfo *)trTextureRegistry[i].palettes;}
static int trUnusedInfo(trcolorinfo *c){return !c->base && !c->detail;}
static int trColorsEqual(trcolorinfo *c,int i){int j;for(j=0;j<trTextureRegistry[i].nPalettes;++j)if(!memcmp(c,&trColorInfosPointer(i)[j],sizeof(*c)))return j;return -1;}
static int trColorsIndexAlloc(trcolorinfo *c,int i){texreg *r=&trTextureRegistry[i];if(r->nPalettes>=8)abort();trColorInfosPointer(i)[r->nPalettes]=*c;return r->nPalettes++;}
static void trInternalTexturesDelete(trhandle h){++deletedGpu;free(trStructure(h)->palettes);}
static int trTextureDelete(trhandle h){texreg *r=trStructure(h);++retired;free(r->palettes);free(r->fileName);r->flags=0;trPreviewPrivate[trIndex(h)]=0;return 0;}
'''
        for signature in ('trhandle trTextureRegister(', 'trhandle trTextureRegisterPreview(',
                          'void trTextureReleasePreview(', 'void trTextureColorsUpdate('):
            harness+=function(source,signature)+'\n'
        harness+=r'''
int main(void){
    int liveOwner=1,menuOwner=2;trcolorinfo original={0x112233,0x445566},edited={0xabcdef,0x123456};
    int i;for(i=0;i<8;++i)trTextureRegistry[i].sharedFrom=TR_NotShared;
    trhandle live=trTextureRegister("shared_hull",&original,&liveOwner);
    trhandle menu=trTextureRegisterPreview("shared_hull",&original,&menuOwner);
    CHECK(live!=menu && trIndex(live)!=trIndex(menu));
    CHECK(trTextureRegister("shared_hull",&original,&liveOwner)==live);
    trTextureColorsUpdate(menu,&edited);
    CHECK(!memcmp(trColorInfosPointer(trIndex(live)),&original,sizeof(original)));
    CHECK(!memcmp(trColorInfosPointer(trIndex(menu)),&edited,sizeof(edited)));
    // The GPU-loaded palette transition also stays private.
    trStructure(menu)->flags&=~TRF_Pending;trTextureColorsUpdate(menu,&original);
    CHECK(deletedGpu==1 && trPending(trIndex(menu)));
    CHECK(!memcmp(trColorInfosPointer(trIndex(live)),&original,sizeof(original)));
    trTextureReleasePreview(live);trTextureReleasePreview(menu);CHECK(retired==0);
    trStructure(menu)->nUsageCount=0;trTextureReleasePreview(menu);CHECK(retired==1);
    CHECK(trAllocated(trIndex(live)) && trStructure(live)->nUsageCount==2);
    trTextureDelete(live);
    puts("PASS: equal-colour preview isolation, loaded palette changes, scoped retirement");return 0;
}
'''
        run_native(harness)

    def test_trail_scope_never_requires_or_replaces_game_camera(self):
        source=(ROOT/'src/Game/Trails.c').read_text()
        code=r'''
#include <stdio.h>
#include <stdlib.h>
typedef int sdword;typedef float real32;typedef unsigned trhandle;
typedef struct {float x,y,z;} vector;typedef struct {int id;} Ship;
typedef struct {void *vship;} shiptrail;typedef struct {vector eyeposition;} Camera;
#define MAX2(a,b) ((a)>(b)?(a):(b))
#define TR_InvalidHandle ((unsigned)-1)
static Camera *mrCamera;static const Ship *trailMenuOwner;static real32 trailMenuMaxSpeed;static vector trailMenuEye;
static unsigned g_glowHandle=3;static int draws,updates,tactics,prepares;
static float tacticsGetShipsMaxVelocity(Ship *ship){++tactics;return 73;}
static int trPreparePreviewTexture(trhandle handle){++prepares;return 1;}
'''
        code+=function(source,'static const vector *trailViewerEye(')
        code+=function(source,'static real32 trailOwnerMaxSpeed(')
        code+=r'''
static void trailDraw(vector *p,shiptrail *t,int lod,int team){
    if(mrCamera || trailOwnerMaxSpeed(t->vship)!=1000 || trailViewerEye()->x!=123)abort();++draws;
}
static void trailUpdate(shiptrail *t,vector *p){if(trailOwnerMaxSpeed(t->vship)!=1000)abort();++updates;}
'''
        code+=function(source,'void trailDrawMenuPreview(')
        code+=function(source,'void trailUpdateMenuPreview(')
        code+=r'''
int main(void){Ship menu={1},live={2};shiptrail t={&menu};vector p={0},eye={123,4,5};
    trailMenuMaxSpeed=9;trailMenuEye.x=88;
    trailDrawMenuPreview(&p,&t,1000,&eye);trailUpdateMenuPreview(&t,&p,1000);
    if(draws!=1 || updates!=1 || prepares!=1 || tactics || mrCamera || trailMenuOwner || trailMenuMaxSpeed!=9 || trailMenuEye.x!=88)abort();
    if(trailOwnerMaxSpeed(&live)!=73 || tactics!=1)abort();
    puts("PASS: preview trail camera/speed isolation and live tactics restoration");return 0;}
'''
        run_native(code)

    def test_preview_texture_cap_does_not_change_game_texture_cap(self):
        source=(ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        code='''#include <stdio.h>\n#include <stdlib.h>\n#include "TextureResolution.h"\n#define HW_DIAG_FPRINTF(...) ((void)0)\nstatic int observedModelTextureCap;\n'''
        code+=function(source,'extern "C" unsigned int hwModernGraphicsModelTextureCap(')
        code+='''\nint main(){if(hwModernGraphicsModelTextureCap(3)!=0 || hwModernGraphicsModelTextureCap(1)!=1024 || hwModernGraphicsModelTextureCap(3)!=0 || hwModernGraphicsModelTextureCap(0)!=512)abort();puts("PASS: preview texture cap cannot pin gameplay to its first texture quality");}\n'''
        run_native(code,'cpp')

    def test_model_bounds_follow_parent_transforms(self):
        source=(ROOT/'src/Game/FrontendShipPreview.inc').read_text()
        code=r'''
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef int sdword;typedef struct polygonobject {float localMatrix[16];struct polygonobject *pMother;} polygonobject;
'''
        code+=function(source,'static void muiPreviewObjectMatrix(')
        code+=r'''
int main(void){polygonobject parent={0},child={0};float m[16];int i;
    for(i=0;i<4;++i)parent.localMatrix[i*5]=child.localMatrix[i*5]=1;
    parent.localMatrix[0]=0;parent.localMatrix[1]=1;parent.localMatrix[4]=-1;parent.localMatrix[5]=0;
    parent.localMatrix[12]=5;child.localMatrix[12]=3;child.localMatrix[13]=2;child.pMother=&parent;
    muiPreviewObjectMatrix(&child,m,0);
    if(m[12]!=3 || m[13]!=3 || m[0]!=0 || m[1]!=1 || m[15]!=1)abort();
    puts("PASS: interceptor viewport framing follows nested model transforms");return 0;}
'''
        run_native(code)

if __name__=='__main__':unittest.main()
