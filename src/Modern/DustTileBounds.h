#pragma once
#include <algorithm>
#include <cmath>

namespace hwdust {
// Bound the swept line plus its maximum warped/envelope tail. Outside this
// box even 1.75x wake-amplified density is below 1e-6 (the marcher ignores
// <=1e-5). Keep the original support intersection/sample lattice unchanged.
inline float ropeSupportRadius(float density, float variation, float radius) {
    const double peak = std::max(0.0, double(density)) * 1.75 * 0.35;
    const double tail = std::sqrt(std::max(0.0, std::log(std::max(1.0, peak / 1e-6))) / 6.0);
    const double warp = 0.72 * std::max(0.0, std::min(1.0, double(variation) * 0.75));
    return float(std::max(1.0f, radius) * (1.0 + warp + tail) * 1.0001);
}

inline void ropeSupportHalfSize(const float size[3], float density, float variation, float half[3]) {
    const float x=std::max(1.0f,size[0]*0.5f), y=std::max(1.0f,size[1]*0.5f);
    const float z=std::max(1.0f,size[2]*0.5f), radius=std::min(x,y);
    const float support=ropeSupportRadius(density,variation,radius);
    half[0]=std::min(x*2.6f,support);
    half[1]=std::min(y*2.6f,support);
    half[2]=std::min(z*2.6f,std::max(0.0f,z-radius)+support);
}

struct ScreenBounds { float lo[2] = {0,0}, hi[2] = {1,1}; bool visible = true; bool behindCamera = false; };

// Inverse of the shader's world-to-local Z, Y, X rotations. Angles supplied
// here are the same precomputed negative-angle sine/cosine pairs as the GPU.
inline void localToWorld(float p[3], const float sx[2], const float sy[2], const float sz[2]) {
    float y = p[1]*sx[1] + p[2]*sx[0], z = -p[1]*sx[0] + p[2]*sx[1];
    p[1]=y; p[2]=z;
    float x = p[0]*sy[1] - p[2]*sy[0]; z=p[0]*sy[0]+p[2]*sy[1];
    p[0]=x; p[2]=z;
    x=p[0]*sz[1]+p[1]*sz[0]; y=-p[0]*sz[0]+p[1]*sz[1];
    p[0]=x; p[1]=y;
}

// Project a camera-axis AABB enclosing the rotated support box. Never cull
// camera-inside/near-plane intersections. No distance or opacity cutoff.
inline ScreenBounds projectSupport(const float position[3], const float half[3],
    const float camera[3], const float sx[2], const float sy[2], const float sz[2],
    const float bx[3], const float by[3], const float forward[3],
    float xLength, float yLength, float forwardLength) {
    ScreenBounds result;
    if (!(xLength>0 && yLength>0 && forwardLength>0)) return result;
    double center[3]={}, extent[3]={};
    const float* basis[3]={bx,by,forward};
    const float lengths[3]={xLength,yLength,forwardLength};
    for (int axis=0;axis<3;++axis) {
        for(int k=0;k<3;++k) center[axis]+=(double(position[k])-camera[k])*basis[axis][k]/lengths[axis];
        for(int local=0;local<3;++local) {
            float unit[3]={}; unit[local]=1;
            localToWorld(unit,sx,sy,sz);
            double alignment=0;
            for(int k=0;k<3;++k) alignment+=double(unit[k])*basis[axis][k]/lengths[axis];
            extent[axis]+=std::abs(alignment)*half[local];
        }
        extent[axis]+=std::max(0.01,extent[axis]*0.00001); // numerical conservatism
    }
    if(center[2]+extent[2]<0) { result.visible=false; result.behindCamera=true; return result; }
    if(center[2]-extent[2]<=0.001) return result;
    result.lo[0]=result.lo[1]=1e30f; result.hi[0]=result.hi[1]=-1e30f;
    for(int corner=0;corner<8;++corner) {
        double z=center[2]+((corner&4)?extent[2]:-extent[2]);
        for(int axis=0;axis<2;++axis) {
            double xy=center[axis]+((corner&(1<<axis))?extent[axis]:-extent[axis]);
            float uv=float(0.5+xy*forwardLength/(z*lengths[axis]));
            result.lo[axis]=std::min(result.lo[axis],uv);
            result.hi[axis]=std::max(result.hi[axis],uv);
        }
    }
    result.visible=result.hi[0]>=0 && result.lo[0]<=1 && result.hi[1]>=0 && result.lo[1]<=1;
    return result;
}
// A support box may cross the camera plane while still missing most screen
// tiles. Test its support function against each tile's four side planes;
// unlike a projected AABB this remains conservative behind the near plane.
struct TilePlane { double n[3]; };
struct SupportBox { double center[3], axis[3][3]; };
inline SupportBox makeSupportBox(const float p[3],const float half[3],const float camera[3],
    const float sx[2],const float sy[2],const float sz[2]) {
    SupportBox box{};
    for(int k=0;k<3;++k)box.center[k]=double(p[k])-camera[k];
    for(int a=0;a<3;++a){float v[3]={};v[a]=half[a];localToWorld(v,sx,sy,sz);
        for(int k=0;k<3;++k)box.axis[a][k]=v[k];}
    return box;
}
inline TilePlane tilePlane(const float tangent[3],const float ray[3],const float forward[3]) {
    TilePlane p{{double(tangent[1])*ray[2]-double(tangent[2])*ray[1],
        double(tangent[2])*ray[0]-double(tangent[0])*ray[2],
        double(tangent[0])*ray[1]-double(tangent[1])*ray[0]}};
    double facing=0;for(int k=0;k<3;++k)facing+=p.n[k]*forward[k];
    if(facing<0)for(double &n:p.n)n=-n;
    return p;
}

struct ViewFrustum {
    TilePlane side[4]{};
    TilePlane forward{};
    bool valid = false;
};

// Build once for the frame from the same unnormalized perspective corner rays
// used by the cloud shader. Expand by the reconstruction footprint before
// rejecting any field; corner-angle cone approximations can reject a sphere
// containing the camera when its center happens to lie behind the view.
inline ViewFrustum makeViewFrustum(const float rayUv00[3], const float rayUv10[3],
                                  const float rayUv01[3], float marginU = 0.0f,
                                  float marginV = 0.0f) {
    ViewFrustum result;
    if (!std::isfinite(marginU) || !std::isfinite(marginV)) return result;
    marginU = std::max(0.0f, marginU);
    marginV = std::max(0.0f, marginV);
    float bx[3], by[3], centerRay[3];
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(rayUv00[k]) || !std::isfinite(rayUv10[k]) ||
            !std::isfinite(rayUv01[k])) return result;
        bx[k] = rayUv10[k] - rayUv00[k];
        by[k] = rayUv01[k] - rayUv00[k];
        centerRay[k] = rayUv00[k] + 0.5f * (bx[k] + by[k]);
        result.forward.n[k] = centerRay[k];
    }
    for (int side = 0; side < 4; ++side) {
        const float u = side == 0 ? -marginU : side == 1 ? 1.0f + marginU : 0.5f;
        const float v = side == 2 ? -marginV : side == 3 ? 1.0f + marginV : 0.5f;
        float ray[3];
        for (int k = 0; k < 3; ++k) ray[k] = rayUv00[k] + u * bx[k] + v * by[k];
        result.side[side] = tilePlane(side < 2 ? by : bx, ray, centerRay);
    }
    for (int index = 0; index < 5; ++index) {
        TilePlane &plane = index < 4 ? result.side[index] : result.forward;
        double lengthSquared = 0.0;
        for (double value : plane.n) lengthSquared += value * value;
        if (!(lengthSquared > 1e-24) || !std::isfinite(lengthSquared)) return result;
        const double inverseLength = 1.0 / std::sqrt(lengthSquared);
        for (double &value : plane.n) value *= inverseLength;
    }
    result.valid = true;
    return result;
}

// The bounding sphere may overestimate a field's visibility, but must never
// reject a visible fragment or any camera-inside cloud. In particular, sphere
// radius cannot simply be added to a cone's axial-distance inequality.
inline bool supportSphereIntersectsView(const float position[3], float radius,
                                        const float camera[3], const ViewFrustum &view) {
    if (!view.valid || !std::isfinite(radius) || radius < 0.0f) return true;
    double center[3], distanceSquared = 0.0;
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(position[k]) || !std::isfinite(camera[k])) return true;
        center[k] = double(position[k]) - camera[k];
        distanceSquared += center[k] * center[k];
    }
    const double support = radius;
    if (distanceSquared <= support * support) return true;
    for (int index = 0; index < 5; ++index) {
        const TilePlane &plane = index < 4 ? view.side[index] : view.forward;
        double distance = 0.0;
        for (int k = 0; k < 3; ++k) distance += plane.n[k] * center[k];
        if (distance + support < -1e-7 * (1.0 + std::abs(distance) + support)) return false;
    }
    return true;
}

inline bool intersectsTile(const SupportBox &box,const TilePlane *planes) {
    for(int i=0;i<4;++i){double center=0,radius=0;
        for(int k=0;k<3;++k)center+=planes[i].n[k]*box.center[k];
        for(int a=0;a<3;++a){double dot=0;for(int k=0;k<3;++k)dot+=planes[i].n[k]*box.axis[a][k];radius+=std::abs(dot);}
        if(center+radius < -1e-5*(1+std::abs(center)+radius))return false;
    }
    return true;
}

}
