#include "RemasteredFxResources.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
using Bytes=std::vector<unsigned char>;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void u(Bytes& b,unsigned v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);}
static void f(Bytes& b,float v){unsigned bits;std::memcpy(&bits,&v,4);u(b,bits);}
static Bytes fixture(){Bytes strings{0};auto s=[&](const char* value){unsigned n=unsigned(strings.size());strings.insert(strings.end(),value,value+std::strlen(value)+1);return n;};
    unsigned bitmap=s("art/fx/flare.anim"),image=s("art/fx/flare.tga"),mesh=s("art/fx/chunks.ahod"),model0=s("art/fx/chunk0.hod"),model1=s("art/fx/chunk1.hod"),flare=s("effect/lensflare/star.flare");
    Bytes b={'O','W','F','X','R','S','0','1'};for(unsigned v:{1u,2u,1u,2u,1u,1u,unsigned(strings.size())})u(b,v);
    u(b,bitmap);u(b,0);u(b,2);f(b,10);u(b,0);u(b,1);
    for(unsigned row:{0u,1u})for(unsigned v:{image,row,0u,2u,2u})u(b,v);
    for(unsigned v:{mesh,0u,2u,model0,model1,flare,0u,1u})u(b,v);
    for(float v:{.1f,.2f,.1f,.2f,1.f,.9f,.8f,1.f,0.f})f(b,v);
    u(b,image);for(float v:{1.f,4.f,1.f,1.f,.5f,.2f,1.f})f(b,v);
    b.insert(b.end(),strings.begin(),strings.end());return b;
}
int main(int argc,char** argv){try{char error[256];auto bytes=fixture();auto* bank=hwRmFxResourcesRead(bytes.data(),bytes.size(),error,sizeof(error));check(bank!=nullptr,error);
    auto* bitmap=hwRmFxBitmapFind(bank,"Data:/ART/FX/FLARE.anim");check(bitmap&&bitmap->frameCount==2,"native bitmap URI lookup failed");
    check(hwRmFxBitmapSample(bitmap,0,0,0,1)->row==0,"first source atlas cell lost");
    check(hwRmFxBitmapSample(bitmap,.1f,0,0,1)->row==1,"source animation frame order lost");
    check(hwRmFxBitmapSample(bitmap,20,0,0,1)->row==1,"finite animation loop did not hold its last cell");
    check(hwRmFxBitmapSample(bitmap,0,3,1,1)->row==1,"stable random bitmap selection failed");
    check(!hwRmFxBitmapSample(bitmap,-1,0,0,1),"negative bitmap age accepted");
    {HwRmFxBitmap held=*bitmap;held.loops=0;
        check(hwRmFxBitmapSample(&held,0,0,0,1)->row==1,"zero source loopCount was guessed to mean infinite animation");
        check(hwRmFxBitmapSample(&held,0,0,1,1)->row==0,"explicit random bitmap must bypass timed frame selection");}
    auto* mesh=hwRmFxMeshFind(bank,"data:art/fx/chunks.ahod");check(mesh&&mesh->count==2&&!std::strcmp(mesh->models[1],"art/fx/chunk1.hod"),"native mesh choices lost");
    auto* flare=hwRmFxFlareFind(bank,"effect/lensflare/star.flare");check(flare&&flare->count==1&&flare->hotspots[0].scale==4&&flare->fade[1]==.2f,"native flare parameters lost");
    hwRmFxResourcesFree(bank);
    for(size_t size=0;size<bytes.size();++size)check(!hwRmFxResourcesRead(bytes.data(),size,error,sizeof(error)),"truncated FX resource index accepted");
    auto broken=bytes;broken[7]='0';check(!hwRmFxResourcesRead(broken.data(),broken.size(),error,sizeof(error)),"unknown FX resource version accepted");
    if(argc>1){std::ifstream file(argv[1],std::ios::binary);check(bool(file),"real resource index missing");Bytes real((std::istreambuf_iterator<char>(file)),{});
        bank=hwRmFxResourcesRead(real.data(),real.size(),error,sizeof(error));check(bank!=nullptr,error);
        std::printf("Real FX resources: %u bitmaps, %u mesh groups\n",hwRmFxBitmapCount(bank),hwRmFxMeshCount(bank));hwRmFxResourcesFree(bank);}
    std::puts("Native FX resource bounds, atlas timing, mesh choices and flares passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
