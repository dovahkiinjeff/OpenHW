"""Execute the actual production render-list builders with optional stock ranges."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FogOfWarTests(unittest.TestCase):
    def test_live_visibility_lists(self):
        source = (ROOT / 'src/Game/UnivUpdate.c').read_text()
        start = source.index('void univUpdateMinorRenderList()')
        end = source.index('void univCheckRegrowResources(', start)
        functions = source[start:end]
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        vs = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1])
        setup = vs / 'Common7/Tools/VsDevCmd.bat'
        harness = r'''
#include <assert.h>
#include <string.h>
typedef int bool32,sdword;
typedef unsigned int udword;
typedef float real32;
#define TRUE 1
#define FALSE 0
#define dbgAssertOrIgnore assert
#define bitTest(a,b) ((a)&(b))
#define HW_MODERN_INFINITE_DRAW_DISTANCE 1
enum {CLASS_Mothership,CLASS_HeavyCruiser,CLASS_Carrier,CLASS_Destroyer,
      CLASS_Frigate,CLASS_Corvette,CLASS_Fighter,CLASS_Resource,CLASS_NonCombat,NUM_CLASSES};
enum {OBJ_ShipType,OBJ_DerelictType,OBJ_Other};
enum {SOF_Hide=1,SOF_ForceVisible=2,SOF_AlwaysSortFront=4,SOF_BigObject=8};
#define RENDER_LIMIT_MOTHERSHIP 90000.f
#define RENDER_LIMIT_HEAVYCRUISER 80000.f
#define RENDER_LIMIT_CARRIER 70000.f
#define RENDER_LIMIT_DESTROYER 60000.f
#define RENDER_LIMIT_FRIGATE 50000.f
#define RENDER_LIMIT_CORVETTE 40000.f
#define RENDER_LIMIT_FIGHTER 30000.f
#define RENDER_LIMIT_RESOURCE 20000.f
#define RENDER_LIMIT_NONCOMBAT 10000.f
#define RENDER_VIEWABLE_DISTANCE_SQR 10000.f
#define RENDER_MAXVIEWABLE_DISTANCE_SQR 90000.f
typedef struct {real32 x,y,z;} vector;
#define vecSub(o,a,b) do {(o).x=(a).x-(b).x;(o).y=(a).y-(b).y;(o).z=(a).z-(b).z;}while(0)
#define vecMagnitudeSquared(v) ((v).x*(v).x+(v).y*(v).y+(v).z*(v).z)
typedef struct Node {struct Node *next;void *structptr;} Node;
typedef struct {Node *head,*tail;int num;} LinkedList;
typedef struct {sdword shipclass;real32 renderlistLimitSqr;} ShipStaticInfo,DerelictStaticInfo;
typedef sdword ShipClass;
typedef struct {Node link,renderlink;udword flags;sdword objtype;
    struct {vector position;} posinfo;void *staticinfo;
    vector cameraDistanceVector;real32 cameraDistanceSquared;} SpaceObj;
struct {LinkedList SpaceObjList,MinorSpaceObjList,RenderList,MinorRenderList;} universe;
struct Camera {vector lookatpoint,eyeposition;} camera,*mrCamera=&camera;
udword opRestoreFogOfWar;
void listVerify(LinkedList *l) {(void)l;}
void listInit(LinkedList *l) {memset(l,0,sizeof(*l));}
void listRemoveAll(LinkedList *l) {listInit(l);}
void listMergeSort2(LinkedList *l) {(void)l;}
void *listGetStructOfNode(Node *n) {return n->structptr;}
void listAddNode(LinkedList *l,Node *n,void *object) {
    n->next=0;n->structptr=object;
    if(l->tail)l->tail->next=n;else l->head=n;
    l->tail=n;l->num++;
}
'''
        cases = r'''
int main(void) {
    SpaceObj ship={0},wreck={0},minor={0},hidden={0},forced={0};
    ShipStaticInfo shipInfo={CLASS_Fighter,0};DerelictStaticInfo wreckInfo={0,0};
    ship.objtype=OBJ_ShipType;ship.staticinfo=&shipInfo;ship.posinfo.position.x=10000;
    wreck.objtype=OBJ_DerelictType;wreck.staticinfo=&wreckInfo;wreck.posinfo.position.x=10000;
    hidden.flags=SOF_Hide;hidden.staticinfo=&shipInfo;hidden.posinfo.position.x=1;
    forced.flags=SOF_ForceVisible;forced.posinfo.position.x=10000;
    listAddNode(&universe.SpaceObjList,&ship.link,&ship);
    listAddNode(&universe.SpaceObjList,&wreck.link,&wreck);
    listAddNode(&universe.SpaceObjList,&hidden.link,&hidden);
    listAddNode(&universe.SpaceObjList,&forced.link,&forced);
    minor.posinfo.position.x=10000;
    listAddNode(&universe.MinorSpaceObjList,&minor.link,&minor);
    opRestoreFogOfWar=0;univUpdateRenderList();univUpdateMinorRenderList();
    assert(universe.RenderList.num==3 && universe.MinorRenderList.num==1);
    opRestoreFogOfWar=1;univUpdateRenderList();univUpdateMinorRenderList();
    assert(universe.RenderList.num==1 && universe.MinorRenderList.num==0); /* forced remains */
    ship.posinfo.position.x=10;wreck.posinfo.position.x=10;minor.posinfo.position.x=10;
    univUpdateRenderList();univUpdateMinorRenderList();
    assert(universe.RenderList.num==3 && universe.MinorRenderList.num==1);
    shipInfo.renderlistLimitSqr=25;univUpdateRenderList();assert(universe.RenderList.num==2);
    /* Toggling back must restore distant objects immediately, with no mission reload. */
    ship.posinfo.position.x=wreck.posinfo.position.x=10000;
    opRestoreFogOfWar=0;univUpdateRenderList();assert(universe.RenderList.num==3);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='openhw-visibility-') as tmp:
            folder = Path(tmp)
            (folder / 'visibility.c').write_text(harness + functions + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 visibility.c /Fe:visibility.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'visibility.exe')], cwd=folder, check=True)


if __name__ == '__main__':
    unittest.main()
