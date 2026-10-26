from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name, prefix='void '):
    start = source.index(prefix + name + '(')
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class SwarmDocking(unittest.TestCase):
    def test_production_replacement_and_docking(self):
        moves = (ROOT / 'src/Game/AIMoves.c').read_text()
        utilities = (ROOT / 'src/Game/AIUtilities.c').read_text()
        self.assertIn('aimSwarmDefenseReplacePods(params, newPodTeam->shipList.selection);', moves)
        harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef int sdword; typedef size_t memsize; typedef float real32;
typedef struct { float x,y,z; } vector;
typedef struct { int busyness; } DockInfo;
typedef struct { int canHandleNumShipsDocking; } StaticInfo;
typedef struct Ship { struct { vector position; } posinfo; DockInfo *dockInfo; StaticInfo *staticinfo; int disabled,id; } Ship,*ShipPtr;
typedef struct { int numShips; Ship *ShipPtr[1]; } SelectCommand,MaxSelection;
typedef struct { SelectCommand *Pods; } paramsSwarmDefense;
#define REALlyBig 1.e30f
#define Pyrophoric 0
#define DOCK_AT_SPECIFIC_SHIP 1
#define aiuShipIsntSelectable(s) ((s)->disabled)
#define sizeofSelectCommand(n) (sizeof(SelectCommand)+((n)-1)*sizeof(Ship *))
#define aiumemFree(p) do { memFree(p); (p)=0; } while(0)
#define aiuNewPyroSelection(p,n,label) do { (p)=memAlloc(sizeofSelectCommand(n),label,0); (p)->numShips=0; } while(0)
#define vecSub(d,a,b) do { (d).x=(a).x-(b).x; (d).y=(a).y-(b).y; (d).z=(a).z-(b).z; } while(0)
#define vecMagnitudeSquared(v) ((v).x*(v).x+(v).y*(v).y+(v).z*(v).z)
#define fsqrt sqrtf
real32 BUSY_ADD_TO_DISTANCE=100;
int allocations=0,assigned[64],calls=0;
void *memAlloc(size_t n,char *label,int flags) { allocations++; return calloc(1,n); }
void memFree(void *p) { if(p) { allocations--; free(p); } }
SelectCommand *selectMemDupSelection(SelectCommand *s,char *label,int flags) {
    SelectCommand *r=memAlloc(sizeofSelectCommand(s->numShips),label,flags);
    memcpy(r,s,sizeofSelectCommand(s->numShips)); return r;
}
void selSelectionAddSingleShip(MaxSelection *s,Ship *ship) { s->ShipPtr[s->numShips++]=ship; }
void aiuWrapDock(SelectCommand *s,int type,Ship *pod) {
    assert(s->numShips>0 && pod && pod->dockInfo); calls++;
    for(int i=0;i<s->numShips;i++) assigned[s->ShipPtr[i]->id]=pod->id;
}
'''
        cases = r'''
int main(void) {
    Ship pod[32]={0},swarm[3]={0}; DockInfo docks[32]={0}; StaticInfo stat={6};
    SelectCommand *pods=memAlloc(sizeofSelectCommand(32),"pods",0);
    SelectCommand *ships=memAlloc(sizeofSelectCommand(3),"ships",0);
    pods->numShips=32; ships->numShips=3;
    for(int i=0;i<32;i++) { pod[i].id=i; pod[i].dockInfo=&docks[i]; pod[i].staticinfo=&stat; pod[i].posinfo.position.x=i*100; pods->ShipPtr[i]=&pod[i]; }
    for(int i=0;i<3;i++) { swarm[i].id=i; ships->ShipPtr[i]=&swarm[i]; }
    swarm[0].posinfo.position.x=0; swarm[1].posinfo.position.x=3100; swarm[2].posinfo.position.x=900;
    /* Old group had one allocated slot. Replacement now safely contains32. */
    paramsSwarmDefense params={memAlloc(sizeofSelectCommand(1),"old",0)};
    aimSwarmDefenseReplacePods(&params,pods);
    assert(params.Pods->numShips==32 && params.Pods->ShipPtr[31]==&pod[31]);
    int before=allocations;
    aiuSwarmDock(ships,params.Pods);
    assert(assigned[0]==0 && assigned[1]==31 && assigned[2]==9);
    assert(allocations==before && calls==3);
    /* Busy pods are penalized per ship; each search starts independently. */
    docks[9].busyness=100; aiuSwarmDock(ships,params.Pods); assert(assigned[2]!=9);
    /* Empty/missing/disabled hosts must not issue a command or leak. */
    for(int i=0;i<32;i++) pod[i].disabled=1;
    before=calls; aiuSwarmDock(ships,params.Pods); assert(calls==before);
    params.Pods->numShips=1; params.Pods->ShipPtr[0]=0;
    aiuSwarmDock(ships,params.Pods); assert(calls==before);
    params.Pods->numShips=0; aiuSwarmDock(ships,params.Pods);
    aiuSwarmDock(0,params.Pods); aiuSwarmDock(ships,0);
    memFree(params.Pods); memFree(pods); memFree(ships); assert(!allocations);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-swarm-dock-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + function(moves, 'aimSwarmDefenseReplacePods', 'static void ') + function(utilities, 'aiuSwarmDock') + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)


if __name__ == '__main__':
    unittest.main()
