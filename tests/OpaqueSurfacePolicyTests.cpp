#include "OpaqueSurfacePolicy.h"
#undef NDEBUG
#include <cassert>
#include <limits>
int main() {
    using hwmodern::canSkipSurfaceAnyHit;
    assert(canSkipSurfaceAnyHit(1,true,true,255));
    assert(canSkipSurfaceAnyHit(1,false,false,0));
    assert(!canSkipSurfaceAnyHit(1,true,false,255));
    assert(!canSkipSurfaceAnyHit(1,true,true,254));
    assert(!canSkipSurfaceAnyHit(1,true,true,0));
    assert(!canSkipSurfaceAnyHit(.5f,true,true,255));
    assert(!canSkipSurfaceAnyHit(0,false,false,0));
    assert(!canSkipSurfaceAnyHit(std::numeric_limits<float>::quiet_NaN(),false,false,0));
}
