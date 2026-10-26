#include "DustTileBounds.h"
#include <cstdio>
#include <random>
#include <cstdlib>

static void require(bool condition) { if (!condition) std::abort(); }
int main() {
    // Long ropes expand their radius/tails, not the whole line length.
    float ropeSize[3]={200,200,20200},ropeHalf[3];
    hwdust::ropeSupportHalfSize(ropeSize,1.f,1.f,ropeHalf);
    require(ropeHalf[2]<11000.f && ropeHalf[2]>10000.f);
    for(float density:{0.001f,1.f,100.f,1000000.f})
    for(float variation:{0.f,0.5f,1.f,2.f}) {
        float r=hwdust::ropeSupportRadius(density,variation,100.f);
        double outside=r/100.-1.-0.72*std::min(1.f,variation*0.75f);
        double upper=density*1.75*0.35*std::exp(-6.*outside*outside);
        require(upper<=1.00001e-6);
    }
    const float bx[3]={2,0,0}, by[3]={0,2,0}, forward[3]={0,0,1};
    const float camera[3]={0,0,0}, identity[2]={0,1};
    float p[3]={0,0,20000}, half[3]={10000,100,100};
    auto thin=hwdust::projectSupport(p,half,camera,identity,identity,identity,bx,by,forward,2,2,1);
    require(thin.visible && thin.hi[1]-thin.lo[1]<0.006f);
    p[2]=0;
    auto inside=hwdust::projectSupport(p,half,camera,identity,identity,identity,bx,by,forward,2,2,1);
    require(inside.visible && inside.lo[0]==0 && inside.hi[0]==1);
    p[2]=-20000;
    require(!hwdust::projectSupport(p,half,camera,identity,identity,identity,bx,by,forward,2,2,1).visible);
    std::mt19937 random(771);
    std::uniform_real_distribution<float> angle(-3.14f,3.14f), unit(-1,1);
    for(int i=0;i<10000;++i) {
        float sx[2],sy[2],sz[2];
        for(auto pair:{sx,sy,sz}) {float a=angle(random);pair[0]=std::sin(a);pair[1]=std::cos(a);}
        const float scale=i%3==0?10000000.f:10000.f;
        for(int a=0;a<3;++a) {p[a]=unit(random)*scale;half[a]=std::max(1.f,std::abs(unit(random))*scale*.2f);}
        auto bounds=hwdust::projectSupport(p,half,camera,sx,sy,sz,bx,by,forward,2,2,1);
        for(int sample=0;sample<20;++sample) {
            float q[3]={unit(random)*half[0],unit(random)*half[1],unit(random)*half[2]};
            hwdust::localToWorld(q,sx,sy,sz);
            for(int a=0;a<3;++a) q[a]+=p[a];
            if(q[2]<=0) continue;
            float u=.5f+q[0]/(2*q[2]),v=.5f+q[1]/(2*q[2]);
            if(u<0 ||u>1||v<0||v>1) continue;
            require(bounds.visible && u>=bounds.lo[0]-1e-5f &&u<=bounds.hi[0]+1e-5f &&v>=bounds.lo[1]-1e-5f &&v<=bounds.hi[1]+1e-5f);
            unsigned tx=std::min(63u,unsigned(u*64)),ty=std::min(35u,unsigned(v*36));
            float edges[4]={float(tx)/64,float(tx+1)/64,float(ty)/36,float(ty+1)/36};
            float insideRay[3]={((tx+.5f)/64-.5f)*2,((ty+.5f)/36-.5f)*2,1};
            hwdust::TilePlane planes[4];
            for(int side=0;side<4;++side){float ray[3]={0,0,1};ray[side<2?0:1]=(edges[side]-.5f)*2;
                planes[side]=hwdust::tilePlane(side<2?by:bx,ray,insideRay);}
            require(hwdust::intersectsTile(hwdust::makeSupportBox(p,half,camera,sx,sy,sz),planes));

        }
    }

    // A narrow rope crossing the near plane used to occupy every screen tile.
    // It must remain visible, yet side planes can safely exclude distant tiles.
    float nearRope[3]={0,0,10},nearHalf[3]={.02f,.02f,20};
    auto box=hwdust::makeSupportBox(nearRope,nearHalf,camera,identity,identity,identity);
    unsigned accepted=0;
    for(unsigned ty=0;ty<36;++ty)for(unsigned tx=0;tx<64;++tx){
        float edges[4]={float(tx)/64,float(tx+1)/64,float(ty)/36,float(ty+1)/36};
        float insideRay[3]={((tx+.5f)/64-.5f)*2,((ty+.5f)/36-.5f)*2,1};
        hwdust::TilePlane planes[4];
        for(int side=0;side<4;++side){float ray[3]={0,0,1};ray[side<2?0:1]=(edges[side]-.5f)*2;
            planes[side]=hwdust::tilePlane(side<2?by:bx,ray,insideRay);}
        accepted+=hwdust::intersectsTile(box,planes);
    }
    // Camera inside support: every ray really can intersect; never discard it.
    require(accepted==64*36);
    nearRope[0]=10;box=hwdust::makeSupportBox(nearRope,nearHalf,camera,identity,identity,identity);accepted=0;
    for(unsigned tx=0;tx<64;++tx){float ray[3]={float(tx)/32-1,0,1},inside[3]={float(tx+.5f)/32-1,0,1};
        hwdust::TilePlane planes[4]={hwdust::tilePlane(by,ray,inside)};
        ray[0]=float(tx+1)/32-1;planes[1]=hwdust::tilePlane(by,ray,inside);
        // Two zero normals impose no vertical restriction in this 1D test.
        accepted+=hwdust::intersectsTile(box,planes);}
    require(accepted>0 && accepted<32);
    std::printf("Near-plane off-axis rope: %u/64 columns retained; no camera-inside culling.\n",accepted);
    std::puts("Dust rotated-support culling: 200000 projected points PASS");
}
