"""Exercise the actual construction-manager re-entry path without starting the game."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildManagerReopenTests(unittest.TestCase):
    def test_reuse_and_factory_focus(self):
        source = (ROOT / 'src/Game/ConsMgr.c').read_text()
        focus = source[source.index('bool32 cmShowSelectedFactory('):source.index('/* Region callback for opening the construction manager. */')]
        begin = source[source.index('sdword cmConstructionBegin('):]
        begin = begin[:begin.index('    cmFillInCarrierXs();')] + '    return 17;\n}\n'
        harness = r'''
#include <assert.h>
typedef int bool32; typedef int sdword; typedef unsigned udword; typedef long long smemsize;
typedef void *regionhandle;
typedef struct Ship {int visible;} Ship, *ShipPtr;
struct Player {Ship *PlayerMothership;};
struct {struct Player *curPlayerPtr;} universe;
struct {int bBuildManager;} tutEnable;
int cmActive,playPackets,universePause,opPauseOrders,tutorial,smZoomingIn,smZoomingOut;
int docked=1,updates,redraws,registered,normalized; Ship *cmDockSelectedFactory;
#define TRUE 1
#define FALSE 0
#define TUTORIAL_ONLY 1
int mdManagerDockEnabled(void){return docked;}
int cmDockFactoryVisible(const Ship *ship){return ship && ship->visible;}
void cmDockEnsureOwnedFactoriesRegistered(void){++registered;}
void cmDockFactoryNormalizeSelection(void){++normalized;}
void cmUpdateFactory(Ship *ship){assert(ship);++updates;}
void updateShipView(void){++redraws;}
'''
        checks = r'''
int main(void){
 Ship one={1},two={1},invalid={0}; struct Player player={&one}; universe.curPlayerPtr=&player;
 assert(cmConstructionBegin(0,(smemsize)&one,0,0)==17); /* first open */
 cmActive=1;cmDockSelectedFactory=&one;
 assert(cmConstructionBegin(0,(smemsize)&one,0,0)==1 && updates==0);
 assert(cmConstructionBegin(0,(smemsize)&two,0,0)==1 && updates==1 && redraws==1 && cmDockSelectedFactory==&two);
 assert(cmConstructionBegin(0,(smemsize)&invalid,0,0)==1 && updates==1);
 assert(cmConstructionBegin(0,0,0,0)==1 && updates==1);
 docked=0;assert(cmConstructionBegin(0,(smemsize)&one,0,0)==1 && updates==1);
 universePause=1;assert(cmConstructionBegin(0,(smemsize)&one,0,0)==0);
 return 0;
}
'''
        setup = r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat'
        with tempfile.TemporaryDirectory(prefix='openhw-build-reopen-') as tmp:
            folder = Path(tmp)
            (folder / 'reopen.c').write_text(harness + focus + begin + checks)
            (folder / 'compile.cmd').write_text(f'@echo off\ncall "{setup}" x64 >nul\ncl /nologo /TC reopen.c /Fe:reopen.exe\nif errorlevel 1 exit /b 1\nreopen.exe\n')
            result = subprocess.run(['cmd.exe', '/c', 'compile.cmd'], cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
