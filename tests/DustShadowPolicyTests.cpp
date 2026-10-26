#include "DustShadowPolicy.h"
#include <cstdio>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
    HWModernVolumetricDustVolume v={}; v.enabled=1; v.density=1; v.coverage=1; v.shape=HW_MODERN_DUST_BOX;
    v.size[0]=20; v.size[1]=40; v.size[2]=60;
    float p[3]={0,0,0}; CHECK(hwmodern::dustContainsAsteroid(v,p));
    p[0]=11; CHECK(hwmodern::dustContainsAsteroid(v,p));
    p[0]=27; CHECK(!hwmodern::dustContainsAsteroid(v,p));
    v.rotationDegrees[2]=90; p[0]=49; CHECK(hwmodern::dustContainsAsteroid(v,p));
    v.enabled=0; CHECK(!hwmodern::dustContainsAsteroid(v,p)); v.enabled=1;
    v.rotationDegrees[2]=0; v.shape=HW_MODERN_DUST_ELLIPSOID;
    p[0]=25; p[1]=19; CHECK(!hwmodern::dustContainsAsteroid(v,p));
    p[0]=0; p[1]=0; v.shape=HW_MODERN_DUST_SPHERE; p[2]=19;
    CHECK(hwmodern::dustContainsAsteroid(v,p)); p[2]=53; CHECK(!hwmodern::dustContainsAsteroid(v,p));
    v.shape=HW_MODERN_DUST_ROPE; p[2]=29; CHECK(hwmodern::dustContainsAsteroid(v,p));
    p[2]=31; CHECK(hwmodern::dustContainsAsteroid(v,p));
    p[2]=79; CHECK(!hwmodern::dustContainsAsteroid(v,p));
    // Regression: actual Mission 4 rock was inside the visible soft cloud,
    // but outside its unit primitive and therefore incorrectly cast shafts.
    v.shape=HW_MODERN_DUST_ELLIPSOID;
    v.position[0]=-301.174f; v.position[1]=-521.728f; v.position[2]=-5895.45f;
    v.size[0]=8000; v.size[1]=4700; v.size[2]=19500;
    v.rotationDegrees[0]=5; v.rotationDegrees[1]=-5;
    p[0]=-242.733f; p[1]=1951.6f; p[2]=-4640.89f;
    CHECK(hwmodern::dustContainsAsteroid(v,p));
    p[0]=-2727.9f; p[1]=1451.55f; p[2]=-2319.72f;
    CHECK(hwmodern::dustContainsAsteroid(v,p));
    p[0]=634.579f; p[1]=1602.16f; p[2]=9603.99f;
    CHECK(hwmodern::dustContainsAsteroid(v,p));
    CHECK(hwmodern::solidShadowMask(3)==7); CHECK(hwmodern::solidShadowMask(2)==6);
    CHECK(hwmodern::solidShadowMask(3|hwmodern::excludeAtmosphericShafts)==3);
    CHECK(hwmodern::solidShadowMask(2|hwmodern::excludeAtmosphericShafts)==2);
    puts("PASS: rotated cloud membership; contained asteroids retain solid shadows without shaft participation"); return 0;
}
