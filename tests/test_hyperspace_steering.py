"""Hyperspace/parade steering ownership and captured-mothership regressions."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HyperspaceSteeringTests(unittest.TestCase):
    def test_auto_combat_policy_and_call_sites(self):
        source = (ROOT / 'src/Game/SinglePlayer.c').read_text()
        helper = 'bool32 singlePlayerHyperspaceSuppressesAutoCombat(' + source.split(
            'bool32 singlePlayerHyperspaceSuppressesAutoCombat(', 1)[1].split(
            '\n// Parade slots', 1)[0]
        harness = r'''
#include <assert.h>
#include <stddef.h>
typedef int bool32, sdword;
enum { NO_HYPERSPACE, HYPERSPACE_PREWAIT, HYPERSPACE_WAITINGROLLCALL,
       HYPERSPACE_WAITINGFORFLEETCOMMAND, HYPERSPACE_SHIPSLEAVING,
       HYPERSPACE_WHITEOUT, HYPERSPACE_LOADDONE, HYPERSPACE_SHIPSARRIVING,
       HYPERSPACE_FAILED };
typedef struct { void *playerowner; } Ship;
int singlePlayerGame=1;
struct { int hyperspaceState; } singlePlayerGameInfo;
struct { void *curPlayerPtr; } universe;
'''
        cases = r'''
int main(void) {
    int player, enemy, state;
    Ship own={&player}, hostile={&enemy};
    universe.curPlayerPtr=&player;
    for(state=NO_HYPERSPACE;state<=HYPERSPACE_FAILED;state++) {
        singlePlayerGameInfo.hyperspaceState=state;
        assert(!!singlePlayerHyperspaceSuppressesAutoCombat(&own) ==
               (state==HYPERSPACE_WAITINGROLLCALL ||
                state==HYPERSPACE_WAITINGFORFLEETCOMMAND ||
                state==HYPERSPACE_SHIPSLEAVING));
        assert(!singlePlayerHyperspaceSuppressesAutoCombat(&hostile));
        assert(!singlePlayerHyperspaceSuppressesAutoCombat(NULL));
    }
    singlePlayerGameInfo.hyperspaceState=HYPERSPACE_WAITINGROLLCALL;
    singlePlayerGame=0;
    assert(!singlePlayerHyperspaceSuppressesAutoCombat(&own));
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-hyperspace-combat-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + helper + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)
        update = (ROOT / 'src/Game/UnivUpdate.c').read_text()
        guard = update.index('if (singlePlayerHyperspaceSuppressesAutoCombat(ship))')
        self.assertLess(guard, update.index('tacticsUpdate(ship)', guard))
        self.assertLess(guard, update.index('tacticsDelegateAttackCommand(ship', guard))
        self.assertIn('ClearPassiveAttacking(command);', update[guard:guard+650])
        parade = (ROOT / 'src/Game/CommandLayer.c').read_text().split(
            'case COMMAND_MILITARY_PARADE:\n                    passiveAttacked = FALSE;', 1)[1]
        self.assertLess(parade.index('singlePlayerHyperspaceSuppressesAutoCombat'),
                        parade.index('processPassiveAttackToDo(command)'))

    def test_production_ownership_policy(self):
        source = (ROOT / 'src/Game/SinglePlayer.c').read_text()
        helper = 'bool32 singlePlayerHyperspaceOwnsSteering(' + source.split(
            'bool32 singlePlayerHyperspaceOwnsSteering(', 1)[1].split(
            '\nbool32 OrientHyperspacingShips(', 1)[0]
        harness = r'''
#include <assert.h>
typedef int bool32, sdword;
enum { NO_HYPERSPACE, HYPERSPACE_PREWAIT, HYPERSPACE_WAITINGROLLCALL,
       HYPERSPACE_WAITINGFORFLEETCOMMAND, HYPERSPACE_SHIPSLEAVING,
       HYPERSPACE_WHITEOUT, HYPERSPACE_SHIPSARRIVING, HYPERSPACE_FAILED };
enum { FOCUSING_ON_MOTHERSHIP, LOCKED_FOCUS_FOR_HOP };
#define SOF_Disabled 1
#define SPECIAL_SinglePlayerWithinHyperSpaceRange 128
typedef struct { int flags, specialFlags, capital, ally; } Ship;
int singlePlayerGame=1;
struct { int hyperspaceState, hyperspaceSubState; } singlePlayerGameInfo;
struct { void *curPlayerPtr; } universe;
#define ShipCanMakeHyperspace(s) ((s)->capital && ((s)->specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange))
int allianceIsShipAlly(Ship *s, void *p) { return s->ally; }
'''
        cases = r'''
int main(void) {
    Ship ship={0,128,1,1};
    int state;
    for(state=0;state<=HYPERSPACE_FAILED;state++) {
        singlePlayerGameInfo.hyperspaceState=state;
        singlePlayerGameInfo.hyperspaceSubState=FOCUSING_ON_MOTHERSHIP;
        assert(!!singlePlayerHyperspaceOwnsSteering(&ship) ==
               (state==HYPERSPACE_WAITINGROLLCALL || state==HYPERSPACE_WAITINGFORFLEETCOMMAND || state==HYPERSPACE_SHIPSLEAVING));
    }
    singlePlayerGameInfo.hyperspaceState=HYPERSPACE_SHIPSLEAVING;
    singlePlayerGameInfo.hyperspaceSubState=LOCKED_FOCUS_FOR_HOP;
    assert(!singlePlayerHyperspaceOwnsSteering(&ship));
    singlePlayerGameInfo.hyperspaceState=HYPERSPACE_WAITINGROLLCALL;
    ship.specialFlags=0; assert(!singlePlayerHyperspaceOwnsSteering(&ship));
    ship.specialFlags=128; ship.flags=SOF_Disabled; assert(!singlePlayerHyperspaceOwnsSteering(&ship));
    ship.flags=0; ship.ally=0; assert(!singlePlayerHyperspaceOwnsSteering(&ship));
    ship.ally=1; ship.capital=0; assert(!singlePlayerHyperspaceOwnsSteering(&ship));
    ship.capital=1; singlePlayerGame=0; assert(!singlePlayerHyperspaceOwnsSteering(&ship));
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-hyperspace-steering-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + helper + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_parade_yields_and_only_actual_flagship_is_exempt(self):
        source = (ROOT / 'src/Game/SinglePlayer.c').read_text()
        parade = (ROOT / 'src/Game/Formation.c').read_text().split(
            'void processMilitaryParadeToDo(', 1)[1].split('\nbool32 shipInMilitaryParade(', 1)[0]
        self.assertRegex(parade, r'if \(singlePlayerHyperspaceOwnsSteering\(ship\)\)\s*\{\s*aitrackSteadyShipDriftOnly\(ship\);\s*continue;')
        for start, end in [('void spGoNowHyperspaceCB(', 'void CalculateRollCall('),
                           ('void CalculateRollCall(', 'void TellShipsToDockForHyperspace('),
                           ('void TellHyperspacingShipsToReady(', 'sdword HyperspaceRollCallBegin('),
                           ('bool32 OrientHyperspacingShips(', 'void PutShipOutsideAt(')]:
            body = source.split('\n' + start, 1)[1].split(end, 1)[0]
            self.assertNotIn('ship->shiptype != Mothership', body)
            self.assertNotIn('ship->shiptype == Mothership', body)
        self.assertIn('[HyperspaceBlocker]', source)


if __name__ == '__main__':
    unittest.main()
