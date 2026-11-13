"""Run the production mask/color functions without opening a graphics device."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from test_texture_lifecycle_contract import function

ROOT=Path(__file__).resolve().parents[1]

class SurfacePaint(unittest.TestCase):
    def test_modern_paint_matches_palette_masks_and_preserves_alpha(self):
        source=(ROOT/'src/SDL/texreg.c').read_text()
        harness=r'''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef uint32_t color,udword,trhandle;typedef int sdword,bool32;typedef float real32;typedef unsigned char ubyte;typedef size_t memsize;
typedef struct {color base,detail;}trcolorinfo;
typedef struct {int width,height;unsigned flags;ubyte *data,*teamEffect0,*teamEffect1;}lifheader;
typedef struct {int nPalettes;unsigned baseScalar,stripeScalar;char *fileName;}texreg;
#define TRUE 1
#define FALSE 0
#define PATH_MAX 512
#define TR_Invalid UINT32_MAX
#define TR_RegistrySize 1
#define TRF_Paletted 2
#define TRF_TeamColor0 16
#define TRF_TeamColor1 32
#define trIndex(h) ((h)&65535)
#define trPaletteIndex(h) ((h)>>16)
#define trStructureGet(h) (&textureSlot)
#define trPending(h) 0
#define bitTest(a,b) ((a)&(b))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define colRGBA(r,g,b,a) ((color)(r)|((color)(g)<<8)|((color)(b)<<16)|((color)(a)<<24))
#define colRed(c) ((c)&255)
#define colGreen(c) (((c)>>8)&255)
#define colBlue(c) (((c)>>16)&255)
#define colAlpha(c) (((c)>>24)&255)
#define colUbyteToReal(c) ((float)(c)/255.0f)
#define colRealToUbyte(c) ((ubyte)((c)*255.0f))
#define colUdwordToReal(c) ((float)(c)/65535.0f)
static unsigned long long trTeamColorCalls,trTeamColorPixels;static double trTeamColorMilliseconds;
static texreg textureSlot={1,0,0,"R1\\paint"};static trcolorinfo info={0xff0000ff,0xffff0000};
static int outstanding,explicitMasks,shading;
static trcolorinfo *trColorInfosPointer(int index){(void)index;return &info;}
static int StrToShipRace(char *name){(void)name;return -1;}
static void cpTeamEffectScalars(float*a,float*b,color c,color d,int race){*a=*b=1;(void)c;(void)d;(void)race;}
static void *memAlloc(size_t n,const char *name,int flags){(void)name;(void)flags;++outstanding;return malloc(n);}
static void memFree(void *p){if(p){--outstanding;free(p);}}
static bool32 trModernLoadLooseGrayPng(const char*name,int w,int h,ubyte **result){
    *result=NULL;
    if(explicitMasks && strstr(name,"teamEffect")) {
        *result=memAlloc(w*h,"mask",0);memset(*result,0,w*h);
        (*result)[strstr(name,"teamEffect0")?0:1]=255;return TRUE;
    }
    if(shading && strstr(name,"paintShade")) {
        *result=memAlloc(w*h,"shade",0);memset(*result,128,w*h);return TRUE;
    }
    return FALSE;
}
'''
        harness+=function(source,'void trBufferColorRGB(color *dest, color *source,')
        harness+=function(source,'static ubyte *trModernLoosePngEffectMap(')
        harness+='\n#include "TextureSurfacePaint.inc"\n'
        harness+=r'''
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    ubyte data[3]={0,1,2},base[3]={255,0,0},stripe[3]={0,255,0};
    lifheader lif={3,1,TRF_Paletted|TRF_TeamColor0|TRF_TeamColor1,data,base,stripe};
    color original[3]={0x11010203,0x22040506,0x33070809},pixels[3];
    memcpy(pixels,original,sizeof(pixels));
    CHECK(trColorizeSurfaceRgba(0,&lif,NULL,pixels,3,1));
    CHECK(pixels[0]==0x110000ff && pixels[1]==0x22ff0000 && pixels[2]==original[2]);
    CHECK(outstanding==0);
    explicitMasks=1;lif.width=1;lif.flags=TRF_TeamColor0|TRF_TeamColor1;
    lif.teamEffect0=lif.teamEffect1=NULL;memcpy(pixels,original,sizeof(pixels));
    CHECK(trColorizeSurfaceRgba(0,&lif,"hull.dds",pixels,3,1));
    CHECK(pixels[0]==0x110000ff && pixels[1]==0x22ff0000 && pixels[2]==original[2]);
    shading=1;memcpy(pixels,original,sizeof(pixels));
    CHECK(trColorizeSurfaceRgba(0,&lif,"hull.dds",pixels,3,1));
    CHECK(pixels[0]==0x11000080 && pixels[1]==0x22800000 && colAlpha(pixels[2])==0x33);
    explicitMasks=shading=0;memcpy(pixels,original,sizeof(pixels));
    CHECK(trColorizeSurfaceRgba(0,&lif,NULL,pixels,3,1));
    CHECK(!memcmp(pixels,original,sizeof(pixels)) && outstanding==0);
    CHECK(!trColorizeSurfaceRgba(TR_Invalid,&lif,NULL,pixels,3,1));
    CHECK(!trColorizeSurfaceRgba(1u<<16,&lif,NULL,pixels,3,1));
    puts("PASS: classic and native RM masks, paint shade, alpha, absent masks and invalid handles");return 0;
}
'''
        compilers=list(Path(os.environ.get('ProgramFiles','C:/Program Files')).glob(
            'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers,'MSVC required');compiler=sorted(compilers)[-1]
        vcvars=compiler.parents[7]/'VC/Auxiliary/Build/vcvars64.bat'
        with tempfile.TemporaryDirectory(prefix='hw-surface-paint-') as folder:
            folder=Path(folder);(folder/'paint.c').write_text(harness)
            command=f'call "{vcvars}" >nul && "{compiler}" /nologo /TC /I"{ROOT / "src/SDL"}" paint.c /Fe:paint.exe && paint.exe'
            result=subprocess.run(command,shell=True,cwd=folder,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

if __name__=='__main__':unittest.main()
