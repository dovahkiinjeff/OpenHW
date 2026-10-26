from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DetachedShipCleanup(unittest.TestCase):
    def test_production_cleanup(self):
        source = (ROOT / "src/Game/UnivUpdate.c").read_text()
        helper = "static bool32 univDetachDyingPassenger(" + source.split(
            "static bool32 univDetachDyingPassenger(", 1)[1].split(
            "/*-----------------------------------------------------------------------------", 1)[0]
        cache = (ROOT / "out/build/deploy-x64-clean/CMakeCache.txt").read_text()
        vs = Path(re.search(r"CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)", cache)[1])
        setup = vs / "Common7/Tools/VsDevCmd.bat"
        harness = r'''
#include <assert.h>
#include <string.h>
typedef int bool32;
#define TRUE 1
#define FALSE 0
#define dbgAssertOrIgnore assert
typedef struct Node { struct Node *next; void *belongto,*structptr; } Node;
typedef struct { Node *head; } List;
typedef struct { List insideList; } Hangar;
typedef struct Ship { Node objlink,shiplink,impactablelink; Hangar *shipsInsideMe; } Ship;
typedef struct { Node node; Ship *ship; } InsideShip;
typedef Ship SpaceObj;
struct { List ShipList; } universe;
struct { List ShipsInHyperspace; } singlePlayerGameInfo;
int freed,deleted,unlinked,wiped,notified;
void *listGetStructOfNode(Node *n) { return n->structptr; }
void dockRemoveShipFromInside(Ship *s,Ship *h) {
assert(h->shipsInsideMe->insideList.head->structptr);
h->shipsInsideMe->insideList.head=0; unlinked++;
}
void listRemoveNode(Node *n) { assert(n->belongto); n->belongto=0; n->structptr=0; }
void listDeleteNode(Node *n) { assert(n->belongto); assert(n->structptr); deleted++; }
void memFree(void *s) { assert(s); freed++; }
void nisObjectDied(SpaceObj *s) {}
void etgShipDied(Ship *s) { notified++; }
void univRemoveObjFromRenderList(SpaceObj *s) {}
void univFreeShipContents(Ship *s) { wiped++; }
'''
        cases = r'''
int main(void) {
Ship s={0},host={0}; Hangar hangar={0}; InsideShip passenger={0},hs={0}; Node hostNode={0};
/* Exact dump failure: all objlink fields cleared, no list membership. */
univWipeShipOutOfExistence(&s);
assert(freed==1 && deleted==0 && wiped==1 && notified==1);
/* Normal outside ship still uses list deletion. */
s.objlink.belongto=&universe; s.objlink.structptr=&s;
s.shiplink.belongto=&universe; s.impactablelink.belongto=&universe;
univWipeShipOutOfExistence(&s);
assert(deleted==1 && freed==1);
assert(!s.shiplink.belongto && !s.impactablelink.belongto);
/* A docked dying ship must not leave a dangling hangar entry. */
memset(&s,0,sizeof(s)); host.shipsInsideMe=&hangar;
passenger.ship=&s; passenger.node.structptr=&passenger;
hangar.insideList.head=&passenger.node;
hostNode.structptr=&host; universe.ShipList.head=&hostNode;
univWipeShipOutOfExistence(&s);
assert(unlinked==1 && !hangar.insideList.head && freed==2);
/* The same cleanup is required if the carrier has already hyperspaced. */
universe.ShipList.head=0; hs.ship=&host; hs.node.structptr=&hs;
singlePlayerGameInfo.ShipsInHyperspace.head=&hs.node;
hangar.insideList.head=&passenger.node;
univWipeShipOutOfExistence(&s);
assert(unlinked==2 && !hangar.insideList.head && freed==3);
return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="hw-detached-cleanup-") as tmp:
            folder = Path(tmp)
            (folder / "cleanup.c").write_text(harness + helper + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 cleanup.c /Fe:cleanup.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / "cleanup.exe")], cwd=folder, check=True)

if __name__ == "__main__":
    unittest.main()
