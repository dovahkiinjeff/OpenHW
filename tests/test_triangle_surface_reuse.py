"""Execute the actual mesh submission before/after allocation policy change."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('static void ' + name + '(')
    brace = source.index('{', start)
    level = 1
    end = brace + 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class TriangleSurfaceReuse(unittest.TestCase):
    def test_identical_submissions_reuse_and_growth_failure(self):
        source = (ROOT / 'src/Game/Mesh.c').read_text()
        current = function(source, 'meshSubmitModernRaytracingObjectEx')
        # Reconstruct the previous allocation policy while keeping all actual
        # material/geometry code identical, so the oracle cannot drift.
        original = current.replace('meshSubmitModernRaytracingObjectEx', 'originalSubmit')
        start = original.index('    if ((size_t)object->nPolygons >')
        end = original.index('    memset(surfaces, 0,', start)
        original = original[:start] + '''    surfaces = memAlloc(
            (memsize)object->nPolygons * sizeof(HWModernTriangleSurface),
            "Modern triangle surfaces", Pyrophoric);
''' + original[end:]
        original = original.replace('if (temporarySurfaces) memFree(surfaces);', 'memFree(surfaces);')
        header = (ROOT / 'src/Modern/ModernGraphics.h').read_text()
        surface = re.search(r'typedef struct HWModernTriangleSurface\s*\{.*?\} HWModernTriangleSurface;', header, re.S)[0]
        preamble = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int bool32; typedef int sdword; typedef size_t memsize; typedef float real32;
typedef uint32_t color; typedef unsigned char GLboolean;
#define TRUE 1
#define FALSE 0
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_DEPTH_WRITEMASK 1
#define GL_MODELVIEW_MATRIX 2
#define GL_BLEND 3
#define Pyrophoric 0
#define MDF_SelfIllum 1
#define bitTest(f,b) ((f)&(b))
#define colRed(c) ((c)&255)
#define colGreen(c) (((c)>>8)&255)
#define colBlue(c) (((c)>>16)&255)
#define colUbyteToReal(c) ((float)(c)/255.f)
typedef struct {float x,y,z;} vertexentry;
typedef struct {unsigned short iV0,iV1,iV2,iMaterial; float s0,t0,s1,t1,s2,t2;} polyentry;
typedef struct {int nVertices,nPolygons; vertexentry *pVertexList; polyentry *pPolygonList;} polygonobject;
typedef struct {color diffuse,ambient; unsigned flags,nFullAmbient;} materialentry;
static int sceneOpen=1, bFade, g_WireframeHack, blend, depth=1;
static float meshEngineBlend; static uint64_t meshMotionOwner=61;
static unsigned allocations,frees,growths; static int failGrowth,active;
static materialentry activeMaterial;
static void *memAlloc(size_t n,const char *name,int flags) {++allocations;return malloc(n);}
static void memFree(void *p){++frees;free(p);}
static void *testRealloc(void *p,size_t n){++growths;return failGrowth?NULL:realloc(p,n);}
static int hwModernGraphicsIsRaytracingSceneOpen(void){return sceneOpen;}
static void glGetBooleanv(int e,GLboolean *p){*p=(GLboolean)depth;}
static int glIsEnabled(int e){return blend;}
static void glGetFloatv(int e,float *p){int i;for(i=0;i<16;i++)p[i]=(float)i*.37f;}
static materialentry *meshEngineMaterial(materialentry *p){return active?&activeMaterial:p;}
static color meshModernMaterialDiffuse(const materialentry *p,int scheme){return p->diffuse^(scheme*0x00010203u);}
'''
        storage = r'''
static HWModernTriangleSurface *meshModernTriangleSurfaces;
static size_t meshModernTriangleSurfaceCapacity;
static HWModernTriangleSurface expected[2048];
static unsigned char expectedMeta[512];
static size_t expectedMetaSize;
static unsigned calls; static int verify;
static void hwModernGraphicsSubmitTriangleGeometryMaskedIdentified(
 const void *id,const float *v,unsigned vc,unsigned vs,const unsigned short *t,
 unsigned tc,unsigned ts,unsigned variant,const HWModernTriangleSurface *s,
 unsigned ss,const float m[16],const float base[3],unsigned mask,
 unsigned long long owner,unsigned long long part)
{
 unsigned char meta[512];size_t n=0;
#define RECORD(x) memcpy(meta+n,&(x),sizeof(x));n+=sizeof(x)
 RECORD(id);RECORD(v);RECORD(vc);RECORD(vs);RECORD(t);RECORD(tc);RECORD(ts);
 RECORD(variant);RECORD(ss);RECORD(mask);RECORD(owner);RECORD(part);
 memcpy(meta+n,m,64);n+=64;memcpy(meta+n,base,12);n+=12;
 assert(tc<=2048);++calls;
 if(!verify){memcpy(expected,s,tc*sizeof(*s));memcpy(expectedMeta,meta,n);expectedMetaSize=n;}
 else {assert(n==expectedMetaSize);assert(!memcmp(meta,expectedMeta,n));assert(!memcmp(s,expected,tc*sizeof(*s)));}
}
'''
        main = r'''
int main(void){
 vertexentry vertices[3]={{0,0,0},{1,0,0},{0,1,0}};
 polyentry polygons[2048];materialentry material[2]={{0x3A19C2,0x743269,0,0},{0,0x84F1A3,1,7}};
 polygonobject object={3,1,vertices,polygons};unsigned i,k,oldAlloc;
 const unsigned sizes[]={1,12,7,128,17,1024};
 memset(polygons,0,sizeof(polygons));
 for(i=0;i<2048;i++){polygons[i].iV1=1;polygons[i].iV2=2;polygons[i].iMaterial=i%2;polygons[i].s0=i*.001f;polygons[i].t2=.75f;}
 activeMaterial=material[1];
 for(i=0;i<12000;i++){
   object.nPolygons=sizes[i%6];active=i%2;meshEngineBlend=(i%101)*.01f;
   meshMotionOwner=61+i;verify=0;originalSubmit(&object,material,i%8,3,0);
   verify=1;meshSubmitModernRaytracingObjectEx(&object,material,i%8,3,0);
 }
 assert(calls==24000);assert(allocations==12000);assert(frees==12000);assert(growths==4);
 /* Reused storage is not a per-ship cache. Growing failure retains the old
    buffer and executes the old allocation/free path, with identical bytes. */
 object.nPolygons=2048;failGrowth=1;oldAlloc=allocations;
 verify=0;originalSubmit(&object,material,3,1,1);
 verify=1;meshSubmitModernRaytracingObjectEx(&object,material,3,1,1);
 assert(allocations==oldAlloc+2);assert(meshModernTriangleSurfaceCapacity==1024);
 failGrowth=0;meshSubmitModernRaytracingObjectEx(&object,material,3,1,1);
 assert(meshModernTriangleSurfaceCapacity==2048);
 /* Existing translucent/depth/closed-scene guards still prevent submission. */
 k=calls;blend=1;meshSubmitModernRaytracingObjectEx(&object,material,3,3,0);assert(calls==k);
 blend=0;depth=0;meshSubmitModernRaytracingObjectEx(&object,material,3,3,0);assert(calls==k);
 depth=1;sceneOpen=0;meshSubmitModernRaytracingObjectEx(&object,material,3,3,1);assert(calls==k);
 free(meshModernTriangleSurfaces);meshModernTriangleSurfaces=NULL;meshModernTriangleSurfaceCapacity=0;
 puts("PASS: 12000 byte-identical submissions; 12000 legacy heap cycles replaced by 4 scratch growths; allocation failure fallback and guards passed.");return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='hw-triangle-scratch-') as tmp:
            folder = Path(tmp)
            (folder/'test.c').write_text(preamble + surface + storage + original + '\n#define realloc testRealloc\n' + current + '\n#undef realloc\n' + main)
            cache = (ROOT/'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
            vs = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)',cache)[1])/'Common7/Tools/VsDevCmd.bat'
            cmd = f'cmd.exe /d /s /c ""{vs}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"'
            result = subprocess.run(cmd,cwd=folder,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result = subprocess.run([str(folder/'test.exe')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout.strip())


if __name__ == '__main__':
    unittest.main()
