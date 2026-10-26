"""Run the production event dispatcher with small engine/script stubs."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Mission7HyperspaceEvents(unittest.TestCase):
    def test_dispatcher(self):
        source = (ROOT / "src/Game/SinglePlayer.c").read_text()
        helper = "bool32 singlePlayerMissionEventsUpdate(" + source.split(
            "bool32 singlePlayerMissionEventsUpdate(", 1)[1].split(
            "\nvoid singlePlayerGameUpdate(void)", 1)[0]
        cache = (ROOT / "out/build/deploy-x64-clean/CMakeCache.txt").read_text()
        vs = Path(re.search(r"CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)", cache)[1])
        setup = vs / "Common7/Tools/VsDevCmd.bat"
        harness = r'''
#include <assert.h>
#include <string.h>
typedef int bool32;
#define TRUE 1
#define FALSE 0
#define MISSION_7_THE_GARDENS_OF_KADESH 7
#define NO_HYPERSPACE 0
#define LOCKED_FOCUS_FOR_HOP 1
struct AITeam { char kasFSMName[48], kasStateName[48]; } team;
typedef struct { int value; char label[48]; } AIVar;
typedef struct { int status; } Objective;
struct { int currentMission, hyperspaceState, hyperspaceSubState;
float hyperspaceTimeStamp; int hyperspaceFails, playerCanHyperspace; } singlePlayerGameInfo;
int singlePlayerGame, count, jumps, hasHyper, hasDefend;
AIVar vars[8]; Objective hyper, defend;
AIVar *aivarFind(char *s) { int i; for(i=0;i<count;i++)
if(!strcmp(vars[i].label,s)) return &vars[i]; return 0; }
AIVar *aivarCreate(char *s) { AIVar *v; if(aivarFind(s)) return 0;
v=&vars[count++]; strcpy(v->label,s); v->value=0; return v; }
int aivarValueGet(AIVar *v) { return v ? v->value : 0; }
void aivarValueSet(AIVar *v,int n) { if(v) v->value=n; }
Objective *objectiveFind(char *s) { return !strcmp(s,"Hyperspace") ?
(hasHyper ? &hyper : 0) : (hasDefend ? &defend : 0); }
void Init_Mission07_Events_HyperdriveFail(void) {}
void Watch_Mission07_Events_HyperdriveFail(void) {}
void kasJump(char *s,void (*init)(void),void (*watch)(void)) {
strcpy(team.kasStateName,s); jumps++; init(); }
void reset(void) { memset(vars,0,sizeof(vars)); count=jumps=0;
singlePlayerGame=1; singlePlayerGameInfo.currentMission=7;
singlePlayerGameInfo.hyperspaceState=0; singlePlayerGameInfo.hyperspaceSubState=1;
singlePlayerGameInfo.hyperspaceTimeStamp=100;
singlePlayerGameInfo.hyperspaceFails=singlePlayerGameInfo.playerCanHyperspace=0;
hasHyper=hasDefend=1; defend.status=1;
strcpy(team.kasFSMName,"Events"); strcpy(team.kasStateName,"Engage"); }
'''
        cases = r'''
int main(void) {
int i; const char *late[]={"ContinueProtecting","HyperspaceBack","JumpAway","FinalState"};
/* Fresh failure survives an interrupted Events state and a full UI queue. */
reset(); strcpy(team.kasStateName,"ProtectResourcers");
aivarValueSet(aivarCreate("M07HyperspaceFailurePending"),1);
assert(singlePlayerMissionEventsUpdate(&team)); assert(jumps==1);
assert(!strcmp(team.kasStateName,"HyperdriveFail"));
/* Objective replay cannot interrupt the dialogue; never deliver twice. */
assert(singlePlayerMissionEventsUpdate(&team)); assert(jumps==1);
strcpy(team.kasStateName,"ContinueProtecting");
assert(singlePlayerMissionEventsUpdate(&team));
assert(!aivarValueGet(aivarFind("M07HyperspaceFailurePending")));
assert(!singlePlayerMissionEventsUpdate(&team)); assert(jumps==1);
/* Recover the existing stuck save, without requiring another button click. */
reset(); strcpy(team.kasStateName,"NullState");
assert(singlePlayerMissionEventsUpdate(&team)); assert(jumps==1);
/* Don't mistake roll call, charging, abort, or the final exit for failure. */
reset(); singlePlayerGameInfo.hyperspaceState=2;
assert(!singlePlayerMissionEventsUpdate(&team));
reset(); singlePlayerGameInfo.hyperspaceFails=1;
assert(!singlePlayerMissionEventsUpdate(&team));
reset(); singlePlayerGameInfo.playerCanHyperspace=1;
assert(!singlePlayerMissionEventsUpdate(&team));
reset(); singlePlayerGameInfo.hyperspaceTimeStamp=0;
assert(!singlePlayerMissionEventsUpdate(&team));
reset(); hasHyper=0; assert(!singlePlayerMissionEventsUpdate(&team));
reset(); defend.status=0; assert(!singlePlayerMissionEventsUpdate(&team));
for(i=0;i<4;i++) { reset(); strcpy(team.kasStateName,late[i]);
assert(!singlePlayerMissionEventsUpdate(&team)); }
reset(); singlePlayerGameInfo.currentMission=8;
assert(!singlePlayerMissionEventsUpdate(&team));
reset(); singlePlayerGame=0; assert(!singlePlayerMissionEventsUpdate(&team));
reset(); strcpy(team.kasFSMName,"P2Mothership");
assert(!singlePlayerMissionEventsUpdate(&team));
/* An existing false variable must be set, not recreated (Create returns NULL). */
reset(); aivarCreate("M07HyperspaceFailureHandled");
aivarValueSet(aivarCreate("M07HyperspaceFailurePending"),1);
assert(singlePlayerMissionEventsUpdate(&team));
assert(aivarValueGet(aivarFind("M07HyperspaceFailureHandled"))==1);
return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="hw-m7-events-") as tmp:
            folder = Path(tmp)
            (folder / "events.c").write_text(harness + helper + cases)
            compile_result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 events.c /Fe:events.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
            subprocess.run([str(folder / "events.exe")], cwd=folder, check=True)

    def test_dispatch_order_and_persistent_delivery(self):
        kas = (ROOT / "src/Game/KAS.c").read_text()
        self.assertEqual(kas.count("!singlePlayerMissionEventsUpdate(CurrentTeamP) &&"), 2)
        single = (ROOT / "src/Game/SinglePlayer.c").read_text()
        failed = single.split("case HYPERSPACE_FAILED:", 1)[1].split("\nstatic void", 1)[0]
        self.assertIn('aivarFind("M07HyperspaceFailurePending")', failed)
        self.assertIn("aivarValueSet(pending, TRUE)", failed)
        self.assertIn('tutGameMessage("Game_HyperspaceFailed")', failed)

if __name__ == "__main__":
    unittest.main()
