from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('sdword ' + name + '(')
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class Mission8Discovery(unittest.TestCase):
    def test_production_distance_and_intro_guard(self):
        lod = (ROOT / 'src/Game/LOD.c').read_text()
        kas = (ROOT / 'src/Game/KASFunc.c').read_text()
        harness = r'''
#include <assert.h>
#include <string.h>
typedef int sdword,DerelictType;
#define LOD_NumberLevels 6
#define MISSION_8_THE_CATHEDRAL_OF_KADESH 8
#define PrisonShipOld 42
#define bitTest(v,m) ((v)&(m))
typedef struct { float bOff; } lod;
typedef struct { int nLevels; lod level[6]; } lodinfo;
typedef struct { struct { lodinfo *LOD; } staticheader; } StaticInfo;
typedef struct { StaticInfo *staticinfo; float cameraDistanceSquared; int currentLOD; } SpaceObj;
typedef struct { SpaceObj obj; int derelicttype,renderedLODs; } Derelict;
typedef struct Node { struct Node *next; void *structptr; } Node;
struct { struct { Node *head; } DerelictList; } universe;
struct Team { char *kasFSMName,*kasStateName; } team,*CurrentTeamP=&team;
float lodScaleFactor=0.6f;
int singlePlayerGame=1,mission=8,attackObjective=0;
int spGetCurrentMission(void) { return mission; }
void *objectiveFind(char *name) { assert(!strcmp(name,"DestroyAtt")); return attackObjective ? &team:0; }
DerelictType StrToDerelictType(char *name) { return !strcmp(name,"PrisonShipOld")?PrisonShipOld:7; }
void *listGetStructOfNode(Node *n) { return n->structptr; }
'''
        cases = r'''
int main(void) {
    lodinfo info={6,{{12866569.f},{0.f},{210975625.f},{1078399921.f},{10353547900.f},{99999999999.f}}};
    StaticInfo stat={{&info}};
    Derelict wreck={{&stat,3600000000.f,0},PrisonShipOld,0};
    Node node={0,&wreck}; universe.DerelictList.head=&node;
    /* Modern rendering still uses geometry LOD0; far wreck records logical4. */
    assert(lodDiscoveryLevelGet(&wreck.obj)==4 && wreck.obj.currentLOD==0);
    wreck.renderedLODs|=1<<lodDiscoveryLevelGet(&wreck.obj);
    team.kasFSMName="Events"; team.kasStateName="NullState"; attackObjective=1;
    for(int i=0;i<4;i++) assert(!kasfRenderedDerelicts("PrisonShipOld",i));
    /* Close view is remembered, but cannot interrupt the opening dialogue. */
    wreck.obj.cameraDistanceSquared=100.f;
    wreck.renderedLODs|=1<<lodDiscoveryLevelGet(&wreck.obj);
    attackObjective=0;
    assert(!kasfRenderedDerelicts("PrisonShipOld",0));
    attackObjective=1;
    char *opening[]={"LocationCard","ImLost","Inhibitors","LastChance","WereTheSame","YoullFail","AttackBegun"};
    for(int i=0;i<7;i++) { team.kasStateName=opening[i]; assert(!kasfRenderedDerelicts("PrisonShipOld",0)); }
    team.kasStateName="NullState";
    assert(kasfRenderedDerelicts("PrisonShipOld",0));
    team.kasStateName="EnemyGoingHere";
    assert(!kasfRenderedDerelicts("PrisonShipOld",0));
    /* Other missions and wreck types retain their original query. */
    mission=2; assert(kasfRenderedDerelicts("PrisonShipOld",0));
    mission=8; wreck.derelicttype=7; assert(kasfRenderedDerelicts("Other",0));
    info.nLevels=0; assert(lodDiscoveryLevelGet(&wreck.obj)==5);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-m8-discovery-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + function(lod, 'lodDiscoveryLevelGet') + function(kas, 'kasfRenderedDerelicts') + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_renderer_records_discovery_not_geometry_for_derelicts(self):
        render = (ROOT / 'src/SDL/render.c').read_text()
        self.assertIn('? lodDiscoveryLevelGet(spaceobj)', render)


if __name__ == '__main__':
    unittest.main()
