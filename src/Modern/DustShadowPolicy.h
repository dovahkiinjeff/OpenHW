#pragma once
#include "ModernGraphics.h"
#include <algorithm>
#include <cmath>
namespace hwmodern {
// Submission metadata only: strip this bit before creating the DXR instance.
constexpr unsigned int excludeAtmosphericShafts = 0x80u;
inline unsigned int solidShadowMask(unsigned int mask) {
    if(!mask) mask=3u;
    if((mask&2u) && !(mask&excludeAtmosphericShafts)) mask|=4u;
    return mask & 0x7fu;
}
inline bool dustContainsAsteroid(const HWModernVolumetricDustVolume &v,const float p[3]) {
    if(!v.enabled || v.density<=0.00001f || v.coverage<=0.00001f) return false;
    float q[3]={p[0]-v.position[0],p[1]-v.position[1],p[2]-v.position[2]};
    const float radians=-3.14159265358979323846f/180.0f;
    float s=std::sin(v.rotationDegrees[2]*radians),c=std::cos(v.rotationDegrees[2]*radians);
    float x=q[0]*c-q[1]*s; q[1]=q[0]*s+q[1]*c; q[0]=x;
    s=std::sin(v.rotationDegrees[1]*radians); c=std::cos(v.rotationDegrees[1]*radians);
    x=q[0]*c+q[2]*s; q[2]=-q[0]*s+q[2]*c; q[0]=x;
    s=std::sin(v.rotationDegrees[0]*radians); c=std::cos(v.rotationDegrees[0]*radians);
    float y=q[1]*c-q[2]*s; q[2]=q[1]*s+q[2]*c; q[1]=y;
    float half[3]; for(unsigned i=0;i<3;++i) half[i]=std::max(1.0f,v.size[i]*0.5f);
    // Match DustRayWorldVolumeInterval's complete 2.6 support envelope.
    // The unit primitive excludes soft tails and displaced lobes that still
    // visibly contain asteroids. Never classify using camera-filtered noise.
    constexpr float supportRadius=2.6f;
    if(v.shape==HW_MODERN_DUST_SPHERE) {
        float r=(half[0]+half[1]+half[2])/3.0f;
        for(float &component:q) component/=r;
    } else for(unsigned i=0;i<3;++i) q[i]/=half[i];
    if(v.shape==HW_MODERN_DUST_BOX || v.shape==HW_MODERN_DUST_ROPE)
        return std::max(std::fabs(q[0]),std::max(std::fabs(q[1]),std::fabs(q[2])))<=supportRadius;
    return q[0]*q[0]+q[1]*q[1]+q[2]*q[2]<=supportRadius*supportRadius;
}
}
