from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('void ' + name + '(')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class CaptureEffectCleanup(unittest.TestCase):
    def test_bullet_retirement_detaches_all_auxiliary_effects(self):
        univ = (ROOT / 'src/Game/UnivUpdate.c').read_text()
        etg = (ROOT / 'src/Game/ETG.c').read_text()
        harness = r'''
#include <assert.h>
#include <string.h>
typedef struct Ship { int unused; } Ship;
typedef struct vector { float x,y,z; } vector;
typedef struct Bullet { struct { vector position,velocity; } posinfo; vector bulletheading; } Bullet;
typedef struct Node { struct Node *next; void *structptr; } Node;
typedef struct Effect { Ship *owner; } Effect;
struct { struct { Node *head; } effectList; } universe;
void *listGetStructOfNode(Node *n) { return n->structptr; }
static unsigned releases;
void rmPresentationReleaseProjectile(const void *bullet,const float *p,const float *h,const float *v) {
    Node *n; (void)p;(void)h;(void)v;
    for(n=universe.effectList.head;n;n=n->next)
        assert(((Effect *)n->structptr)->owner != (const Ship *)bullet);
    ++releases;
}
'''
        cases = r'''
int main(void) {
    Bullet bullet={0}; Ship other={0};
    Effect beam={(Ship *)&bullet},flash={(Ship *)&bullet},spawned={(Ship *)&bullet},unrelated={&other};
    Node n4={0,&unrelated},n3={&n4,&spawned},n2={&n3,&flash},n1={&n2,&beam};
    universe.effectList.head=&n1;
    univFreeBulletContents(&bullet);
    assert(!beam.owner && !flash.owner && !spawned.owner && unrelated.owner==&other);
    memset(&bullet,0x3f,sizeof(bullet));
    assert(!beam.owner && !flash.owner && !spawned.owner);
    univFreeBulletContents(&bullet);
    assert(releases==2 && unrelated.owner==&other);
    universe.effectList.head=0;
    univFreeBulletContents(&bullet);
    assert(releases==3);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        vs = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1])
        setup = vs / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-bullet-effect-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + function(etg, 'etgShipDied') +
                function(univ, 'univFreeBulletContents') + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /O2 /W3 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_capture_detaches_effect_before_free(self):
        univ = (ROOT / 'src/Game/UnivUpdate.c').read_text()
        etg = (ROOT / 'src/Game/ETG.c').read_text()
        cleanup = function(univ, 'univFreeShipContents')
        self.assertLess(cleanup.index('etgShipDied(ship);'),
                        cleanup.index('univRemoveShipReferences(ship);'))
        # Execute the production cleanup entry, owner detachment and capture's
        # inside-ship deletion. Poison the old ship exactly as in the dump.
        entry = cleanup[:cleanup.index('univRemoveShipReferences(ship);')] + '}'
        harness = r'''
#include <assert.h>
#include <string.h>
typedef unsigned int udword;
typedef struct Ship { void *staticinfo; } Ship;
typedef struct Node { struct Node *next; void *structptr; } Node;
typedef struct Effect { Ship *owner; } Effect;
struct { struct { Node *head; } effectList; } universe;
void *listGetStructOfNode(Node *n) { return n->structptr; }
void memFree(Ship *ship) {
    Node *n;
    for(n=universe.effectList.head; n; n=n->next)
        assert(((Effect *)n->structptr)->owner != ship);
    memset(ship, 0, sizeof(*ship));
}
'''
        cases = r'''
int main(void) {
    Ship prize={(void *)1},other={(void *)2};
    Effect gate={&prize},damage={&prize},unrelated={&other},detached={0};
    Node n4={0,&detached},n3={&n4,&unrelated},n2={&n3,&damage},n1={&n2,&gate};
    universe.effectList.head=&n1;
    univDeleteWipeInsideShipOutOfExistence(&prize);
    assert(!gate.owner && !damage.owner && !detached.owner);
    assert(unrelated.owner==&other && !prize.staticinfo);
    univDeleteWipeInsideShipOutOfExistence(&prize);
    assert(unrelated.owner==&other);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        vs = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1])
        setup = vs / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-capture-effect-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(
                harness + function(etg, 'etgShipDied') + entry +
                function(univ, 'univDeleteWipeInsideShipOutOfExistence') + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_owner_local_render_requires_owner(self):
        draw = function((ROOT / 'src/Game/ETG.c').read_text(), 'etgEffectDraw')
        branch = draw.split('else if (bitTest(effect->flags, EAF_OwnerCoordSys))', 1)[1]
        self.assertRegex(branch, r'if \(effect->owner == NULL\)\s*\{\s*continue;\s*\}')
        self.assertLess(branch.index('if (effect->owner == NULL)'),
                        branch.index('effect->owner->rotinfo.coordsys'))


if __name__ == '__main__':
    unittest.main()
