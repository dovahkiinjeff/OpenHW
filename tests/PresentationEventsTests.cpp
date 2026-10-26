#include "RemasteredPresentationEvents.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#define CHECK(x) do {if(!(x)){std::printf("failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void u32(std::vector<unsigned char>& b,uint32_t v)
{for(unsigned i=0;i<4;++i)b.push_back((unsigned char)(v>>(i*8)));}
static void f32(std::vector<unsigned char>& b,float f){uint32_t v;std::memcpy(&v,&f,4);u32(b,v);}
static std::vector<unsigned char> fixture()
{
    const char strings[]="\0Weapon_Gun0\0Muzzle\0flash\0sound\0";
    std::vector<unsigned char>b={'O','W','E','V','E','N','T','2'};
    u32(b,1);u32(b,3);u32(b,sizeof(strings));
    u32(b,1);u32(b,UINT32_MAX);f32(b,1);f32(b,0);f32(b,0);u32(b,1);
    for(float time: {0.0f,0.0f,-1.0f}){
        u32(b,0);f32(b,time);f32(b,1);u32(b,0);
        u32(b,13);u32(b,20);u32(b,26);u32(b,0);
        u32(b,0);f32(b,1);u32(b,0);u32(b,0);u32(b,0);u32(b,0);
    }
    b.insert(b.end(),strings,strings+sizeof(strings));return b;
}
struct Visit {std::vector<double> times;};
static void record(void *user,const HwRmPresentationEvent *,double time)
{static_cast<Visit*>(user)->times.push_back(time);}
static int resolveMarker(void *,const char *name)
{return !std::strcmp(name,"Muzzle")?11:!std::strcmp(name,"Smoke")?22:-1;}
int main(int argc,char **argv)
{
    auto b=fixture();char error[512];auto *events=hwRmPresentationEventsRead(b.data(),b.size(),error,sizeof(error));
    if(!events)std::printf("%s\n",error);CHECK(events);
    CHECK(hwRmPresentationTimelineCount(events)==1 && hwRmPresentationEventCount(events)==3);
    CHECK(hwRmPresentationFindTimeline(events,"weapon_gun0")==0);
    CHECK(!std::strcmp(hwRmPresentationEventAt(events,0)->markers,"Muzzle"));
    {
        auto p=hwRmPresentationPlayerCreate(events);CHECK(p);Visit playback;
        CHECK(hwRmPresentationPlayerStart(p,"Weapon_Gun0",10,record,&playback));
        CHECK(playback.times.size()==1); // -1 activation only
        CHECK(hwRmPresentationPlayerUpdate(p,10,record,&playback));CHECK(playback.times.size()==1);
        CHECK(hwRmPresentationPlayerUpdate(p,11,record,&playback));CHECK(playback.times.size()==5);
        CHECK(!hwRmPresentationPlayerUpdate(p,10.5,record,&playback));
        CHECK(!hwRmPresentationPlayerStart(p,"missing",12,record,&playback));
        hwRmPresentationPlayerStop(p);
        CHECK(hwRmPresentationPlayerUpdate(p,20,record,&playback));CHECK(playback.times.size()==5);
        CHECK(hwRmPresentationPlayerStart(p,"weapon_gun0",20,record,&playback));CHECK(playback.times.size()==6);
        hwRmPresentationPlayerFree(p);
    }
    {
        auto e=*hwRmPresentationEventAt(events,0);e.markers="missing, Muzzle, Smoke ";
        CHECK(hwRmPresentationSelectMarker(&e,0,resolveMarker,nullptr)==11);
        CHECK(hwRmPresentationSelectMarker(&e,1,resolveMarker,nullptr)==22);
        CHECK(hwRmPresentationSelectMarker(&e,2,resolveMarker,nullptr)==11);
        e.markers="missing,, ";CHECK(hwRmPresentationSelectMarker(&e,0,resolveMarker,nullptr)==-1);
        CHECK(hwRmPresentationSelectMarker(nullptr,0,resolveMarker,nullptr)==-1);
    }
    Visit visit;CHECK(hwRmPresentationVisitInterval(events,0,-1,0,record,&visit));CHECK(visit.times.size()==2);
    CHECK(hwRmPresentationVisitInterval(events,0,0,2,record,&visit));CHECK(visit.times.size()==6);
    CHECK(visit.times[2]==1 && visit.times[4]==2);
    CHECK(!hwRmPresentationVisitInterval(events,0,2,1,record,&visit));
    CHECK(!hwRmPresentationVisitInterval(events,0,0,1e300,record,&visit));
    {
        Visit phase;
        CHECK(hwRmPresentationVisitPhase(events,0,0,0,1,record,&phase));CHECK(phase.times.size()==1);
        CHECK(hwRmPresentationVisitPhase(events,0,-1,.25,0,record,&phase));CHECK(phase.times.size()==3);
        CHECK(hwRmPresentationVisitPhase(events,0,.25,1,0,record,&phase));CHECK(phase.times.size()==3);
        CHECK(hwRmPresentationVisitPhase(events,0,1,1,0,record,&phase));CHECK(phase.times.size()==3);
        CHECK(!hwRmPresentationVisitPhase(events,0,.5,.25,0,record,&phase));
        CHECK(!hwRmPresentationVisitPhase(events,0,0,1.1,0,record,&phase));
    }
    hwRmPresentationEventsFree(events);
    {
        auto v=fixture();
        uint32_t attachment=7,other=2,transform=5,marker=13;
        std::memcpy(v.data()+76,&attachment,4);std::memcpy(v.data()+84,&other,4);
        std::memcpy(v.data()+88,&transform,4);std::memcpy(v.data()+92,&marker,4);
        events=hwRmPresentationEventsRead(v.data(),v.size(),error,sizeof(error));CHECK(events);
        auto event=hwRmPresentationEventAt(events,0);
        CHECK(event->orphan==1 && event->lengthMode==3 && event->other==2 && event->transform==5);
        CHECK(!std::strcmp(event->otherMarker,"Muzzle"));
        hwRmPresentationEventsFree(events);
        for(size_t n=0;n<v.size();++n)CHECK(!hwRmPresentationEventsRead(v.data(),n,error,sizeof(error)));
        v[76]=9; // invalid length mode 4
        CHECK(!hwRmPresentationEventsRead(v.data(),v.size(),error,sizeof(error)));
    }
    {
        auto normalized=b;float duration=2,phase=.5f;
        std::memcpy(normalized.data()+28,&duration,4);std::memcpy(normalized.data()+104,&phase,4);
        events=hwRmPresentationEventsRead(normalized.data(),normalized.size(),error,sizeof(error));CHECK(events);
        Visit elapsed;CHECK(hwRmPresentationVisitInterval(events,0,-1,.75,record,&elapsed));CHECK(elapsed.times.size()==1);
        CHECK(hwRmPresentationVisitInterval(events,0,.75,1.25,record,&elapsed));
        CHECK(elapsed.times.size()==2 && elapsed.times[1]==1);
        hwRmPresentationEventsFree(events);
    }
    for(size_t n=0;n<b.size();++n)CHECK(!hwRmPresentationEventsRead(b.data(),n,error,sizeof(error)));
    auto invalid=b;invalid[24]=0;invalid[25]=invalid[26]=invalid[27]=0; // self-parent
    CHECK(!hwRmPresentationEventsRead(invalid.data(),invalid.size(),error,sizeof(error)));
    invalid=b;invalid[7]='1'; // obsolete generated data must be regenerated
    CHECK(!hwRmPresentationEventsRead(invalid.data(),invalid.size(),error,sizeof(error)));
    unsigned count=0;
    if(argc>1)for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[1])){
        if(entry.path().extension()!=".owevents")continue;
        std::ifstream f(entry.path(),std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),{});
        events=hwRmPresentationEventsRead(data.data(),data.size(),error,sizeof(error));
        if(!events)std::printf("%s: %s\n",entry.path().string().c_str(),error);
        CHECK(events);hwRmPresentationEventsFree(events);++count;
    }
    std::printf("Presentation events: scheduling/bounds passed, %u real files checked\n",count);return 0;
}
