"""Compile the actual registry resize/delete functions with tracked GPU/heap stubs."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

class TextureLifecycle(unittest.TestCase):
    def test_production_resize_and_retirement(self):
        source = (ROOT/'src/SDL/texreg.c').read_text()
        harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ReplacementTextureStorage.h"
#define HW_ENABLE_D3D12_BACKEND 1
typedef uint32_t udword, GLuint, trhandle, color;
typedef int sdword;
typedef unsigned char ubyte;
typedef struct { uint32_t base,detail; } trcolorinfo;
typedef struct { unsigned flags; int sharedFrom,nPalettes; ubyte *palettes;
    udword handle; int scaledWidth,scaledHeight,nUsageCount,nSharedTo; char *fileName; } texreg;
#define TR_NotShared -1
#define TRF_Paletted 2
#define TR_InvalidInternalHandle UINT32_MAX
#define TR_NumPaletteSize 64
#define TR_PaletteSize 1024
#define NonVolatile 8
#define trIndex(h) (h)
#define trStructure(h) (&trTextureRegistry[h])
#define trPending(h) 0
#define trAllocated(h) (trTextureRegistry[h].flags & 4)
#define trSetFree(h) ((void)0)
#define FALSE 0
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#define trSetPending(h) ((void)0)
#define bitTest(f,b) ((f)&(b))
#define dbgAssertOrIgnore(x) do { if(!(x)) abort(); } while(0)
static texreg trTextureRegistry[1];
static HWReplacementTextureStorage trReplacementStorage[1];
static int trLowestFree,trHighestAllocated;
static unsigned trNameCRCs[1];
static void trAllSharedFromDelete(int index,int remove) { abort(); }
static int allocations, releases, deletes;
static void *memAlloc(int size, const char *name, int flags) {
    ++allocations; return malloc(size);
}
static void memFree(void *p) {
    if(p == trReplacementStorage[0].words) abort();
    ++releases; free(p);
}
static void glDeleteTextures(int n,const GLuint *handles) { deletes += n; }
static void trNoPalTextureDelete(udword h) { ++deletes; }
'''
        harness += function(source,'void trInternalTexturesDelete(trhandle handle)')
        harness += function(source,'void trPreLoadedTextureScale(sdword handle,')
        harness += function(source,'sdword trTextureDelete(trhandle handle)')
        harness += r'''
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    trcolorinfo colors[8]; int n,i,transition;
    for(i=0;i<8;++i) {colors[i].base=100+i;colors[i].detail=200+i;}
    for(n=1;n<=8;++n) {
        texreg *r=&trTextureRegistry[0];
        r->flags=0;r->sharedFrom=TR_NotShared;r->nPalettes=n;
        r->scaledWidth=512;r->scaledHeight=512;r->handle=123;
        r->palettes=hwReplacementStorageBegin(&trReplacementStorage[0],n,colors);
        for(i=0;i<n;++i) trReplacementStorage[0].words[i]=123+i;
        allocations=releases=deletes=0;
        for(transition=0;transition<32;++transition) trPreLoadedTextureScale(0,128,128);
        CHECK(r->scaledWidth==512 && r->scaledHeight==512);
        CHECK(allocations==0 && releases==0 && deletes==0);
        trInternalTexturesDelete(0);
        CHECK(deletes==n && allocations==1 && releases==0);
        CHECK(!trReplacementStorage[0].resident);
        CHECK(r->palettes!=(void *)trReplacementStorage[0].words);
        CHECK(memcmp(r->palettes,colors,n*sizeof(trcolorinfo))==0);
        memFree(r->palettes); CHECK(releases==1);
        // Full production slot retirement must release only the new pending
        // colors and filename, never the slot-owned resident backing store.
        r->palettes=hwReplacementStorageBegin(&trReplacementStorage[0],n,colors);
        for(i=0;i<n;++i) trReplacementStorage[0].words[i]=123+i;
        r->flags=4;r->handle=123;r->nSharedTo=0;r->nUsageCount=2;
        r->fileName=malloc(16); strcpy(r->fileName,"test_texture");
        allocations=releases=deletes=0;
        CHECK(trTextureDelete(0)==2);
        CHECK(allocations==1 && releases==2 && deletes==n);
        CHECK(r->palettes==NULL && !trReplacementStorage[0].resident);
    }
    // Vanilla RGB still resizes; its existing metadata is freed exactly once.
    texreg *r=&trTextureRegistry[0];r->nPalettes=1;r->handle=123;
    r->palettes=malloc(4+sizeof(colors));
    memcpy(r->palettes+4,colors,sizeof(colors));
    allocations=releases=deletes=0;
    trPreLoadedTextureScale(0,128,64);
    CHECK(r->scaledWidth==128 && r->scaledHeight==64);
    CHECK(allocations==1 && releases==1 && deletes==1);
    CHECK(memcmp(r->palettes,colors,sizeof(trcolorinfo))==0);
    memFree(r->palettes);
    puts("PASS: production replacement retention/retirement (1-8 palettes), vanilla resize, no duplicate allocation");
    return 0;
}
'''
        compilers = list(Path(os.environ.get('ProgramFiles','C:/Program Files')).glob(
            'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers,'MSVC compiler required for native registry lifecycle check')
        compiler = sorted(compilers)[-1]
        vs = compiler.parents[7]
        vcvars = vs/'VC/Auxiliary/Build/vcvars64.bat'
        with tempfile.TemporaryDirectory(prefix='hw-texture-lifecycle-') as directory:
            directory = Path(directory)
            (directory/'lifecycle.c').write_text(harness)
            command = f'call "{vcvars}" >nul && "{compiler}" /nologo /TC /I"{ROOT / "src/Modern"}" lifecycle.c /Fe:lifecycle.exe && lifecycle.exe'
            result = subprocess.run(command,shell=True,cwd=directory,
                                    text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout)

if __name__ == '__main__': unittest.main()
