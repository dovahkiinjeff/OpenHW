"""CPU regressions for conservative camera-inside cloud-field culling."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DustFieldVisibilityTests(unittest.TestCase):
    def test_actual_support_sphere_frustum_helper(self):
        harness = r'''
#include "DustTileBounds.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>

static bool oldConeVisible(const float p[3], float radius,
                           const float camera[3], const float forward[3], float coneCos) {
    float d[3],lengthSquared=0,dot=0;
    for(int k=0;k<3;++k){d[k]=p[k]-camera[k];lengthSquared+=d[k]*d[k];dot+=d[k]*forward[k];}
    return !(dot+radius<std::sqrt(lengthSquared)*coneCos);
}
static void rotate(float p[3], const float sx[2],const float sy[2],const float sz[2]) {
    hwdust::localToWorld(p,sx,sy,sz);
}
int main() {
    float camera[3]={0,0,0},forward[3]={0,0,1};
    float r00[3]={-.5f,-.5f,1},r10[3]={.5f,-.5f,1},r01[3]={-.5f,.5f,1};
    auto view=hwdust::makeViewFrustum(r00,r10,r01);
    assert(view.valid);
    // Actual old predicate: a sphere contains the camera but its center is
    // behind it. Turning toward/away from the center changed visibility.
    float containing[3]={0,0,-9};
    const float coneCos=1/std::sqrt(1.5f);
    assert(!oldConeVisible(containing,10,camera,forward,coneCos));
    assert(hwdust::supportSphereIntersectsView(containing,10,camera,view));
    float nearPlane[3]={4,0,-.1f};
    assert(hwdust::supportSphereIntersectsView(nearPlane,4.1f,camera,view));
    float absent[3]={0,0,-100};
    assert(!hwdust::supportSphereIntersectsView(absent,1,camera,view));
    float offAxis[3]={100,0,10};
    assert(!hwdust::supportSphereIntersectsView(offAxis,1,camera,view));
    float front[3]={0,0,100};
    assert(hwdust::supportSphereIntersectsView(front,1,camera,view));
    // Tangent to the right x=.5z plane; conservative roundoff keeps it.
    float tangent[3]={10,0,10};
    assert(hwdust::supportSphereIntersectsView(tangent,float(5/std::sqrt(1.25)),camera,view));
    float beyond[3]={10.1f,0,10};
    assert(!hwdust::supportSphereIntersectsView(beyond,float(5/std::sqrt(1.25)),camera,view));
    // Reconstruction's two-pixel margin expands the whole-view cull too.
    const float marginU=2.f/640,marginV=2.f/360;
    auto padded=hwdust::makeViewFrustum(r00,r10,r01,marginU,marginV);
    float filterPoint[3]={(.5f+marginU*.75f)*100,0,100};
    assert(!hwdust::supportSphereIntersectsView(filterPoint,0,camera,view));
    assert(hwdust::supportSphereIntersectsView(filterPoint,0,camera,padded));
    float zero[3]={};
    auto invalid=hwdust::makeViewFrustum(zero,zero,zero);
    assert(!invalid.valid && hwdust::supportSphereIntersectsView(absent,1,camera,invalid));
    float invalidPoint[3]={std::numeric_limits<float>::quiet_NaN(),0,0};
    assert(hwdust::supportSphereIntersectsView(invalidPoint,1,camera,view));

    std::mt19937 random(0x3254921u);
    std::uniform_real_distribution<float> unit(-1,1),angle(-3.14159f,3.14159f),positive(0,1);
    unsigned visiblePoints=0,containingCases=0,oldFailures=0;
    for(int trial=0;trial<6000;++trial) {
        float sx[2],sy[2],sz[2];
        for(auto pair:{sx,sy,sz}){float a=angle(random);pair[0]=std::sin(a);pair[1]=std::cos(a);}
        const float tx=.15f+positive(random)*2.5f,ty=.15f+positive(random)*2.5f;
        float a00[3]={-tx,-ty,1},a10[3]={tx,-ty,1},a01[3]={-tx,ty,1};
        for(auto ray:{a00,a10,a01})rotate(ray,sx,sy,sz);
        auto rotated=hwdust::makeViewFrustum(a00,a10,a01,marginU,marginV);
        assert(rotated.valid);
        const float scale=trial%3==0?10000000.f:1000.f;
        float eye[3]={unit(random)*scale,unit(random)*scale,unit(random)*scale};
        const float radius=(.1f+positive(random))*scale*.05f;
        float local[3]={unit(random)*radius*.5f,unit(random)*radius*.5f,unit(random)*radius*.5f};
        float world[3]={local[0],local[1],local[2]};rotate(world,sx,sy,sz);
        for(int k=0;k<3;++k)world[k]+=eye[k];
        assert(hwdust::supportSphereIntersectsView(world,radius,eye,rotated));++containingCases;
        if(!oldConeVisible(local,radius,camera,forward,1/std::sqrt(1+tx*tx+ty*ty)))++oldFailures;
        // Use visible points as witnesses. A sphere containing any such point
        // must survive, across rotations, translation, scale and side tangency.
        for(int sample=0;sample<20;++sample){
            const float u=-marginU+positive(random)*(1+2*marginU);
            const float v=-marginV+positive(random)*(1+2*marginV);
            const float z=(.0001f+positive(random))*scale*.2f;
            float point[3]={(2*u-1)*tx*z,(2*v-1)*ty*z,z};
            float offset[3]={unit(random),unit(random),unit(random)};
            const float length=std::sqrt(offset[0]*offset[0]+offset[1]*offset[1]+offset[2]*offset[2]);
            const float offsetScale=radius*.99f*positive(random)/std::max(length,.000001f);
            float center[3];for(int k=0;k<3;++k)center[k]=point[k]+offset[k]*offsetScale;
            rotate(center,sx,sy,sz);for(int k=0;k<3;++k)center[k]+=eye[k];
            assert(hwdust::supportSphereIntersectsView(center,radius,eye,rotated));++visiblePoints;
        }
    }
    assert(oldFailures>0);
    std::printf("PASS: %u camera-inside rotations; %u visible sphere witnesses; old predicate failed %u containing cases.\n",
        containingCases,visiblePoints,oldFailures);
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-field-visibility-') as tmp:
            folder = Path(tmp)
            (folder / 'test.cpp').write_text(harness)
            command = (
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && '
                f'cl /nologo /std:c++17 /W4 /O2 /I"{ROOT / "src/Modern"}" '
                'test.cpp /Fe:test.exe"')
            result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            execution = subprocess.run([str(folder / 'test.exe')], cwd=folder,
                                       capture_output=True, text=True)
            self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
            print(execution.stdout.strip())


if __name__ == '__main__':
    unittest.main()
