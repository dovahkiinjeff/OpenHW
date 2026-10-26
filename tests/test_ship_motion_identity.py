"""Compile the production renderer-only lifetime registry as C and C++."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ShipMotionIdentityTests(unittest.TestCase):
    def test_runtime_lifetime_scenarios(self):
        harness = r'''
#include "ShipMotionIdentity.h"
#include <assert.h>
static ShipMotionIdentityRegistry registry;
static uint64_t tokens[65536];
int main(void)
{
    int first=0, replacement=0;
    unsigned int i;
    uint64_t firstToken, secondToken, thirdToken, lastToken;
    assert(shipMotionIdentityAcquire(&registry, NULL, 0)==0);
    assert(shipMotionIdentityRegister(&registry, &first, 65536u)==0);
    firstToken=shipMotionIdentityRegister(&registry, &first, 9);
    assert(firstToken!=0);
    /* Draw order, material changes and repeated passes cannot advance identity. */
    for(i=0;i<1000;++i)
        assert(shipMotionIdentityAcquire(&registry, &first, 9)==firstToken);
    /* The network number wraps/reuses; a different owner gets another token. */
    secondToken=shipMotionIdentityRegister(&registry, &replacement, 9);
    assert(secondToken>firstToken);
    /* Real campaign saves contain concurrent live ships sharing a number.
       Alternating their draw scopes must not continually recreate tokens. */
    for(i=0;i<10000;++i) {
        assert(shipMotionIdentityAcquire(&registry, &first, 9)==firstToken);
        assert(shipMotionIdentityAcquire(&registry, &replacement, 9)==secondToken);
    }
    shipMotionIdentityRetire(&registry, &first, 9);
    assert(shipMotionIdentityAcquire(&registry, &replacement, 9)==secondToken);
    shipMotionIdentityRetire(&registry, &replacement, 9);
    assert(shipMotionIdentityFindSlot(registry.slots,&replacement,9,0)==NULL);
    /* Even the same allocation address + ID must not inherit a dead ship. */
    thirdToken=shipMotionIdentityRegister(&registry, &replacement, 9);
    assert(thirdToken>secondToken);
    /* Loading into the same allocation likewise starts a fresh lifetime. */
    lastToken=shipMotionIdentityRegister(&registry, &replacement, 9);
    assert(lastToken>thirdToken);
    /* A valid but previously unregistered scope gets one stable lazy token. */
    firstToken=shipMotionIdentityAcquire(&registry, &first, 0);
    assert(firstToken>lastToken);
    assert(shipMotionIdentityAcquire(&registry, &first, 0)==firstToken);
    /* All 16-bit IDs are in bounds and independent, including 0 and 65535. */
    lastToken=firstToken;
    shipMotionIdentityClear(&registry);
    for(i=0;i<65536u;++i)
    {
        uint64_t token=shipMotionIdentityRegister(&registry, &first, i);
        assert(token>lastToken);
        tokens[i]=token;
        lastToken=token;
    }
    for(i=0;i<65536u;++i)
        assert(shipMotionIdentityAcquire(&registry, &first, i)==tokens[i]);
    /* Full tables fail without replacing any live owner's history. Retiring
       one key permits safe tombstone reuse even without an empty bucket. */
    assert(shipMotionIdentityRegister(&registry,&replacement,9)==0);
    shipMotionIdentityRetire(&registry,&first,9);
    assert(shipMotionIdentityRegister(&registry,&replacement,9)>lastToken);
    assert(shipMotionIdentityAcquire(&registry,&first,10)==tokens[10]);
    lastToken=registry.sequence;
    /* Level/renderer reset drops slots without reusing historical tokens. */
    shipMotionIdentityClear(&registry);
    for(i=0;i<65536u;++i)
        assert(registry.slots[i].owner==NULL && registry.slots[i].token==0);
    assert(shipMotionIdentityAcquire(&registry, &first, 65535u)>lastToken);
    /* Ships and asteroids can share every ID/address without sharing history. */
    firstToken=shipMotionIdentityAcquire(&registry, &first, 9);
    secondToken=asteroidMotionIdentityRegister(&registry, &first, 9);
    assert(secondToken>firstToken);
    for(i=0;i<10000;++i) {
        assert(shipMotionIdentityAcquire(&registry, &first, 9)==firstToken);
        assert(asteroidMotionIdentityAcquire(&registry, &first, 9)==secondToken);
    }
    thirdToken=asteroidMotionIdentityRegister(&registry, &replacement, 9);
    for(i=0;i<10000;++i) {
        assert(asteroidMotionIdentityAcquire(&registry,&first,9)==secondToken);
        assert(asteroidMotionIdentityAcquire(&registry,&replacement,9)==thirdToken);
    }
    asteroidMotionIdentityRetire(&registry, &first, 9);
    assert(asteroidMotionIdentityAcquire(&registry, &replacement, 9)==thirdToken);
    assert(shipMotionIdentityAcquire(&registry, &first, 9)==firstToken);
    asteroidMotionIdentityRetire(&registry, &replacement, 9);
    assert(shipMotionIdentityFindSlot(registry.asteroidSlots,&replacement,9,0)==NULL);
    assert(asteroidMotionIdentityRegister(&registry, &replacement, 9)>thirdToken);
    assert(asteroidMotionIdentityRegister(&registry, NULL, 0)==0);
    assert(asteroidMotionIdentityAcquire(&registry, &first, 65536u)==0);
    lastToken=registry.sequence;
    shipMotionIdentityClear(&registry);
    for(i=0;i<65536u;++i) {
        uint64_t token=asteroidMotionIdentityRegister(&registry, &first, i);
        assert(token>lastToken);lastToken=token;
    }
    shipMotionIdentityClear(&registry);
    for(i=0;i<65536u;++i)assert(registry.asteroidSlots[i].owner==NULL && registry.asteroidSlots[i].token==0);
    assert(asteroidMotionIdentityAcquire(&registry, &first, 65535u)>lastToken);
    /* Dense graveyards reuse mesh parts and can share saved numbers. Their
       lifetime space must also be disjoint from ships and resource objects. */
    firstToken=shipMotionIdentityAcquire(&registry,&first,9);
    secondToken=asteroidMotionIdentityAcquire(&registry,&first,9);
    thirdToken=derelictMotionIdentityRegister(&registry,&first,9);
    lastToken=derelictMotionIdentityRegister(&registry,&replacement,9);
    assert(thirdToken!=firstToken && thirdToken!=secondToken && lastToken>thirdToken);
    for(i=0;i<10000;++i) {
        assert(derelictMotionIdentityAcquire(&registry,&first,9)==thirdToken);
        assert(derelictMotionIdentityAcquire(&registry,&replacement,9)==lastToken);
    }
    derelictMotionIdentityRetire(&registry,&first,9);
    assert(derelictMotionIdentityAcquire(&registry,&replacement,9)==lastToken);
    assert(derelictMotionIdentityRegister(&registry,&first,9)>lastToken);
    assert(derelictMotionIdentityAcquire(&registry,NULL,0)==0);
    assert(derelictMotionIdentityRegister(&registry,&first,65536u)==0);
    lastToken=registry.sequence;
    shipMotionIdentityClear(&registry);
    for(i=0;i<65536u;++i)
        assert(registry.derelictSlots[i].owner==NULL && registry.derelictSlots[i].token==0);
    for(i=0;i<65536u;++i) {
        assert(derelictMotionIdentityRegister(&registry,&first,i)>lastToken);
    }
    assert(derelictMotionIdentityRegister(&registry,&replacement,9)==0);
    derelictMotionIdentityRetire(&registry,&first,9);
    assert(derelictMotionIdentityRegister(&registry,&replacement,9)>lastToken);
    lastToken=registry.sequence;
    shipMotionIdentityClear(&registry);
    assert(derelictMotionIdentityAcquire(&registry,&first,65535u)>lastToken);
    /* The theoretical 64-bit exhaustion is safe: never wrap to an old token. */
    registry.sequence=UINT64_MAX-1;
    assert(shipMotionIdentityRegister(&registry, &first, 0)==UINT64_MAX);
    assert(shipMotionIdentityRegister(&registry, &replacement, 1)==0);
    assert(shipMotionIdentityAcquire(&registry, &replacement, 1)==0);
    assert(asteroidMotionIdentityRegister(&registry, &first, 0)==0);
    assert(derelictMotionIdentityRegister(&registry, &first, 0)==0);
    assert(registry.sequence==UINT64_MAX);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        instance = re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]
        setup = Path(instance) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-motion-identity-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness)
            (folder / 'test.cpp').write_text(harness)
            command = (
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && '
                f'cl /nologo /std:c11 /W4 /O2 /I"{ROOT / "src/Game"}" '
                'test.c /Fe:c-test.exe && '
                f'cl /nologo /std:c++17 /W4 /O2 /I"{ROOT / "src/Game"}" '
                'test.cpp /Fe:cpp-test.exe"')
            result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for executable in ('c-test.exe', 'cpp-test.exe'):
                subprocess.run([str(folder / executable)], cwd=folder, check=True)


if __name__ == '__main__':
    unittest.main()
