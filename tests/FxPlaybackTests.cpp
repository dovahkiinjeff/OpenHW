#include "RemasteredFxPlayback.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <fstream>
using Bytes=std::vector<unsigned char>;
static void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static void u(Bytes& b,unsigned v){for(unsigned i=0;i<4;++i)b.push_back((v>>(8*i))&255);}
static void f(Bytes& b,float v){unsigned bits;std::memcpy(&bits,&v,4);u(b,bits);}
static Bytes fixture(bool force=false,bool trail=false){
    Bytes strings{0};auto s=[&](const char* name){unsigned offset=unsigned(strings.size());strings.insert(strings.end(),name,name+std::strlen(name)+1);return offset;};
    unsigned flash=s("flash"),spray=s("spray"),radius=s(trail?"length":"radius"),duration=s("duration"),
        emitterDuration=s("emitter_duration"),rate=s("emitter_rate"),child=s("particle_fx"),
        speed=s("particle_speed"),scale=s("particle_scale"),first=s("force_first"),scalespeed=s("emitter_scalespeed");
    Bytes b={'O','W','F','X','0','0','0','1'};for(unsigned v:{2u,9u,14u,unsigned(strings.size())})u(b,v);
    for(unsigned v:{flash,trail?5u:2u,0u,2u,spray,1u,2u,7u})u(b,v);
    for(unsigned v:{radius,4u,0u,0u,4u,duration,1u,0u,4u,1u,emitterDuration,1u,0u,5u,1u,
        rate,4u,0u,6u,4u,child,3u,flash,10u,0u,speed,1u,0u,10u,1u,scale,1u,0u,11u,1u,
        first,0u,0u,12u,1u,scalespeed,0u,0u,13u,1u})u(b,v);
    for(float v:{0.f,1.f,1.f,2.f,3.f,1.f,0.f,100.f,1.f,100.f,10.f,4.f,force?1.f:0.f,1.f})f(b,v);
    b.insert(b.end(),strings.begin(),strings.end());return b;
}
struct Host {float x=0,y=0;bool alive=true,capture=true;unsigned released=0,draws=0;std::vector<HwRmFxDraw> packets;};
static int anchor(void* context,uint64_t,float* frame,float* velocity,float*){auto& host=*(Host*)context;
    for(unsigned i=0;i<16;++i)frame[i]=i%5==0?1.f:0.f;frame[12]=host.x;frame[13]=host.y;velocity[0]=velocity[1]=velocity[2]=0;return host.alive;}
static void release(void* context,uint64_t){++((Host*)context)->released;}
static void draw(void* context,const HwRmFxDraw* packet){auto& host=*(Host*)context;++host.draws;
    check(std::isfinite(packet->age+packet->phase+packet->scale+packet->radiusMultiplier),"invalid FX dimensions/phase");
    for(float v:packet->matrix)check(std::isfinite(v),"invalid FX world transform");
    for(float v:packet->spin)check(std::isfinite(v),"invalid FX spin");
    if(host.capture)host.packets.push_back(*packet);}
int main(int argc,char** argv){try{
    char error[256];auto bytes=fixture();auto* graph=hwRmFxGraphRead(bytes.data(),bytes.size(),error,sizeof(error));check(graph!=nullptr,error);
    Host host;HwRmFxPlaybackOps ops{anchor,release,draw};auto* playback=hwRmFxPlaybackCreate(graph,&ops,&host);check(playback!=nullptr,"FX controller allocation failed");
    float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},velocity[3]={};
    check(hwRmFxPlaybackSpawn(playback,"spray",123,0,matrix,velocity,2,0,42),"native spray spawn failed");
    host.x=10;check(hwRmFxPlaybackUpdate(playback,1),"native spray update failed");
    check(hwRmFxPlaybackCount(playback)==100,"source spray count was capped or endpoint emissions were lost");
    hwRmFxPlaybackDraw(playback);check(host.draws==100,"native child effects were not rendered independently");
    for(const auto& packet:host.packets)check(packet.scale==8,"particle scale was not inherited exactly once");
    float firstX=host.packets.front().matrix[12],firstZ=host.packets.front().matrix[14];
    check(std::abs(firstX)<.001f&&std::abs(firstZ-20)<.001f,"particle size incorrectly changed travel speed/moving emitter path");
    host.packets.clear();host.draws=0;host.x=50;check(hwRmFxPlaybackUpdate(playback,1.1),"moving owner update failed");hwRmFxPlaybackDraw(playback);
    check(std::abs(host.packets.front().matrix[12]-firstX)<.001f&&std::abs(host.packets.front().matrix[14]-firstZ-2)<.001f,"detached particle followed the moving muzzle");
    host.alive=false;check(hwRmFxPlaybackUpdate(playback,1.2)&&host.released==1,"missing emitter did not release its anchor");
    check(hwRmFxPlaybackCount(playback)==100,"detached particles were erased with their emitter");
    check(!hwRmFxPlaybackUpdate(playback,1.1),"backward FX clock accepted");
    check(hwRmFxPlaybackUpdate(playback,5)&&hwRmFxPlaybackCount(playback)==0,"native particle lifetime did not end");
    hwRmFxPlaybackReset(playback);check(host.released==1,"FX reset released an anchor twice");
    host.packets.clear();host.draws=0;
    check(hwRmFxPlaybackSpawn(playback,"flash",12,20,matrix,velocity,1,0,0),"late frame spawn failed");
    hwRmFxPlaybackDraw(playback);
    check(host.packets.size()==1&&host.packets[0].age==0,"new effect rendered with a negative age before the next simulation tick");
    hwRmFxPlaybackFree(playback);hwRmFxGraphFree(graph);
    if(argc==2){std::ifstream file(argv[1],std::ios::binary);check(bool(file),"native graph fixture missing");Bytes real((std::istreambuf_iterator<char>(file)),{});
        graph=hwRmFxGraphRead(real.data(),real.size(),error,sizeof(error));check(graph!=nullptr,error);host=Host{};host.capture=false;
        playback=hwRmFxPlaybackCreate(graph,&ops,&host);check(playback!=nullptr,"corpus playback allocation failed");
        for(unsigned i=0;i<hwRmFxGraphCount(graph);++i){hwRmFxPlaybackReset(playback);
            check(hwRmFxPlaybackSpawn(playback,hwRmFxGraphAt(graph,i)->name,123,0,matrix,velocity,1,0,0),"source effect failed initial instantiation");
            for(unsigned tick=1;tick<=20;++tick){check(hwRmFxPlaybackUpdate(playback,tick*.05),"source FX playback failed");hwRmFxPlaybackDraw(playback);}}
        hwRmFxPlaybackReset(playback);
        if(hwRmFxGraphFind(graph,"muzzle_flash_cone_capital")>=0){
            check(hwRmFxPlaybackSpawn(playback,"muzzle_flash_cone_capital",1,0,matrix,velocity,1,0,0),"native birth layer spawn failed");
            check(hwRmFxPlaybackCount(playback)>=2,"native BirthSpawn_Fx layer was discarded");}
        std::printf("Native FX corpus playback: %u source nodes, %u finite draw packets\n",hwRmFxGraphCount(graph),host.draws);
        hwRmFxPlaybackFree(playback);hwRmFxGraphFree(graph);
    }
    bytes=fixture(false,true);graph=hwRmFxGraphRead(bytes.data(),bytes.size(),error,sizeof(error));check(graph!=nullptr,error);
    host=Host{};playback=hwRmFxPlaybackCreate(graph,&ops,&host);
    check(hwRmFxPlaybackSpawn(playback,"flash",1,0,matrix,velocity,1,0,7),"trail spawn failed");
    host.x=10;check(hwRmFxPlaybackUpdate(playback,.15),"trail first update failed");
    host.y=10;check(hwRmFxPlaybackUpdate(playback,.3),"trail second update failed");hwRmFxPlaybackDraw(playback);
    const auto& ribbon=host.packets.back();check(ribbon.trailCount>=3,"native trail did not retain time history");
    check(ribbon.trail[0].position[0]==0&&ribbon.trail[1].position[0]==10&&ribbon.trail[1].position[1]==0&&ribbon.trail[ribbon.trailCount-1].position[1]==10,"turning trail collapsed into a straight beam");
    unsigned history=ribbon.trailCount;hwRmFxPlaybackDraw(playback);check(host.packets.back().trailCount==history,"camera draws appended trail samples");
    hwRmFxPlaybackFree(playback);hwRmFxGraphFree(graph);
    bytes=fixture(true);graph=hwRmFxGraphRead(bytes.data(),bytes.size(),error,sizeof(error));check(graph!=nullptr,error);
    host=Host{};playback=hwRmFxPlaybackCreate(graph,&ops,&host);check(playback!=nullptr,"FX force-first controller failed");
    check(hwRmFxPlaybackSpawn(playback,"spray",1,0,matrix,velocity,1,0,7),"force-first spawn failed");
    check(hwRmFxPlaybackUpdate(playback,.5)&&hwRmFxPlaybackCount(playback)==2,"Force_First did not override the first frame's count");
    check(hwRmFxPlaybackUpdate(playback,.75)&&hwRmFxPlaybackCount(playback)==27,"normal emission did not resume after Force_First");
    hwRmFxPlaybackReset(playback);check(host.released==1,"live reset did not release FX owner");
    hwRmFxPlaybackFree(playback);hwRmFxGraphFree(graph);
    std::puts("Native FX playback: uncapped emission, moving attachments, particle scale, lifetimes and reset passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
