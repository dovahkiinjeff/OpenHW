#include "RemasteredPresentationEvents.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Reader {
    const unsigned char *p;size_t n,at=0;
    uint32_t u32(){if(n-at<4)throw std::runtime_error("truncated event field");
        uint32_t v=(uint32_t)p[at]|((uint32_t)p[at+1]<<8)|((uint32_t)p[at+2]<<16)|((uint32_t)p[at+3]<<24);at+=4;return v;}
    float number(){auto bits=u32();float v;std::memcpy(&v,&bits,4);
        if(!std::isfinite(v))throw std::runtime_error("nonfinite event value");return v;}
};
std::string folded(const char *s) {
    std::string v=s?s:"";for(char& c:v)if(c>='A'&&c<='Z')c+=32;return v;
}
void message(char *p,size_t n,const char *s){if(p&&n)std::snprintf(p,n,"%s",s);}
}
struct HwRmPresentationEvents {
    std::vector<char> strings;
    std::vector<HwRmPresentationTimeline> timelines;
    std::vector<HwRmPresentationEvent> events;
};
struct HwRmPresentationPlayer {
    const HwRmPresentationEvents *events;
    unsigned timeline=0;
    double start=0,elapsed=0;
    bool active=false;
    bool firstUpdate=true;
};
extern "C" HwRmPresentationEvents *hwRmPresentationEventsRead(const void *bytes,size_t size,char *error,size_t capacity)
{
    try {
        if(!bytes || size<20 || size>4*1024*1024 || std::memcmp(bytes,"OWEVENT2",8))
            throw std::runtime_error("invalid presentation event header");
        Reader r{static_cast<const unsigned char*>(bytes),size,8};
        auto timelineCount=r.u32(),eventCount=r.u32(),stringBytes=r.u32();
        if(timelineCount>4096 || eventCount>4096 || !stringBytes || stringBytes>1024*1024 ||
           size!=20ull+timelineCount*24ull+eventCount*56ull+stringBytes)
            throw std::runtime_error("invalid event storage bounds");
        auto out=std::make_unique<HwRmPresentationEvents>();
        const size_t stringStart=size-stringBytes;
        out->strings.assign(r.p+stringStart,r.p+size);
        auto text=[&](unsigned offset)->const char* {
            if(offset>=out->strings.size() || (offset && out->strings[offset-1]) ||
               !std::memchr(out->strings.data()+offset,0,out->strings.size()-offset))
                throw std::runtime_error("invalid event string reference");
            const char *s=out->strings.data()+offset;
            if(std::strlen(s)>4096)throw std::runtime_error("oversized event string");
            for(const char *p=s;*p;++p)if((unsigned char)*p<32 || (unsigned char)*p>126)
                throw std::runtime_error("invalid event string character");
            return s;
        };
        std::set<std::string> names;
        for(unsigned i=0;i<timelineCount;++i) {
            HwRmPresentationTimeline t{};t.name=text(r.u32());auto parent=r.u32();
            t.parent=parent==UINT32_MAX?-1:(int)parent;
            t.duration=r.number();t.minimum=r.number();t.maximum=r.number();t.loop=r.u32();
            if(!*t.name || !names.insert(folded(t.name)).second || t.duration<=0 || t.duration>3600 ||
               t.loop>1 || (parent!=UINT32_MAX && parent>=timelineCount))
                throw std::runtime_error("invalid/duplicate event timeline");
            out->timelines.push_back(t);
        }
        for(unsigned i=0;i<timelineCount;++i) {
            std::set<int> visited;int p=(int)i;
            while(p>=0) {if(!visited.insert(p).second)throw std::runtime_error("cyclic event timeline");p=out->timelines[p].parent;}
        }
        for(unsigned i=0;i<eventCount;++i) {
            HwRmPresentationEvent e{};e.timeline=r.u32();e.time=r.number();e.scale=r.number();e.flags=r.u32();
            e.markers=text(r.u32());e.fx=text(r.u32());e.sound=text(r.u32());e.gameEvent=text(r.u32());
            {
                unsigned attachment=r.u32();e.orphan=attachment&1;e.lengthMode=attachment>>1;
                e.length=r.number();e.other=r.u32();e.transform=r.u32();
                e.otherMarker=text(r.u32());e.orphanText=text(r.u32());
                if(e.lengthMode>3 || e.length<0 || e.length>100000 || e.other>2 || e.transform>5)
                    throw std::runtime_error("invalid event attachment options");
            }
            if(e.timeline>=timelineCount || (e.time<0 && e.time!=-1) || e.time>1 ||
               e.scale<0 || e.scale>100000 || (e.flags&~15u) ||
               ((e.flags&9u)==9u))throw std::runtime_error("invalid presentation event");
            out->events.push_back(e);
        }
        message(error,capacity,"");return out.release();
    }catch(const std::exception& e){message(error,capacity,e.what());return nullptr;}
}
extern "C" void hwRmPresentationEventsFree(HwRmPresentationEvents *e){delete e;}
extern "C" unsigned hwRmPresentationTimelineCount(const HwRmPresentationEvents *e){return e?(unsigned)e->timelines.size():0;}
extern "C" unsigned hwRmPresentationEventCount(const HwRmPresentationEvents *e){return e?(unsigned)e->events.size():0;}
extern "C" const HwRmPresentationTimeline *hwRmPresentationTimelineAt(const HwRmPresentationEvents *e,unsigned i)
{return e&&i<e->timelines.size()?&e->timelines[i]:nullptr;}
extern "C" const HwRmPresentationEvent *hwRmPresentationEventAt(const HwRmPresentationEvents *e,unsigned i)
{return e&&i<e->events.size()?&e->events[i]:nullptr;}
extern "C" int hwRmPresentationFindTimeline(const HwRmPresentationEvents *e,const char *name)
{
    if(e&&name)for(unsigned i=0;i<e->timelines.size();++i)if(folded(e->timelines[i].name)==folded(name))return (int)i;
    return -1;
}
extern "C" int hwRmPresentationVisitInterval(const HwRmPresentationEvents *e,unsigned timeline,
    double from,double to,HwRmPresentationVisit visit,void *user)
{
    if(!e || timeline>=e->timelines.size() || !visit || !std::isfinite(from) || !std::isfinite(to) ||
       from< -1 || to<0 || to<from)return 0;
    const auto& t=e->timelines[timeline];
    // Bound catch-up after a pause/load before emitting any partial sequence.
    double first=t.loop?std::max(0.0,std::floor(std::max(0.0,from)/t.duration)):0;
    double last=t.loop?std::floor(to/t.duration):0;
    if(!std::isfinite(first) || !std::isfinite(last) || last>4503599627370495.0 || last-first>1024)return 0;
    struct Pending {const HwRmPresentationEvent *event;double time;};std::vector<Pending> pending;
    // Allocate the entire dispatch batch before callbacks. Allocation failure
    // must not escape the C API or produce a partially dispatched interval.
    try {
    for(double cycle=first;cycle<=last;++cycle)for(const auto& event:e->events) {
        if(event.timeline!=timeline || event.time<0)continue;
        // A loop endpoint is authored at the end of the preceding cycle.
        double time=(cycle+event.time)*t.duration;
        if(time>from && time<=to) {
            if(pending.size()>=16384)return 0;
            pending.push_back({&event,time});
        }
    }
    std::stable_sort(pending.begin(),pending.end(),[](const Pending& a,const Pending& b){return a.time<b.time;});
    }catch(const std::exception&){return 0;}
    for(const auto& item:pending)visit(user,item.event,item.time);
    return 1;
}
extern "C" int hwRmPresentationVisitPhase(const HwRmPresentationEvents *e,unsigned timeline,
    double from,double to,int activation,HwRmPresentationVisit visit,void *user) {
    if(!e || timeline>=e->timelines.size() || !visit || !std::isfinite(from) ||
        !std::isfinite(to) || from< -1 || to<0 || to>1 || from>to)return 0;
    try {
        std::vector<const HwRmPresentationEvent*> pending;
        for(const auto& event:e->events)if(event.timeline==timeline &&
            ((event.time<0 && activation) || (event.time>=0 && event.time>from && event.time<=to)))
            pending.push_back(&event);
        std::stable_sort(pending.begin(),pending.end(),[](const auto* a,const auto* b){return a->time<b->time;});
        for(const auto* event:pending)visit(user,event,std::max(0.f,event->time)*e->timelines[timeline].duration);
        return 1;
    }catch(const std::exception&){return 0;}
}
extern "C" int hwRmPresentationSelectMarker(const HwRmPresentationEvent *event,unsigned sample,
    HwRmPresentationResolveMarker resolve,void *user)
{
    if(!event || !event->markers || !resolve)return -1;
    // Fixed local storage: immutable strings are bounded by the reader.
    // Two passes avoid allocation, preserve duplicate resolved entries, and
    // ensure invalid joint names do not participate in the random choice.
    auto walk=[&](unsigned wanted,bool select)->int {
        const char *p=event->markers;unsigned count=0;
        while(*p){
            const char *begin=p;while(*p && *p!=',')++p;const char *end=p;
            while(begin<end && *begin==' ')++begin;
            while(end>begin && end[-1]==' ')--end;
            if(end>begin){
                char name[4097];size_t n=(size_t)(end-begin);
                if(n>4096)return -1;
                std::memcpy(name,begin,n);name[n]=0;
                int marker=resolve(user,name);
                if(marker>=0){if(select && count==wanted)return marker;++count;}
            }
            if(*p==',')++p;
        }
        return select?-1:(int)count;
    };
    int count=walk(0,false);
    return count>0?walk(sample%(unsigned)count,true):-1;
}
extern "C" HwRmPresentationPlayer *hwRmPresentationPlayerCreate(const HwRmPresentationEvents *events)
{
    if(!events)return nullptr;
    try {auto p=new HwRmPresentationPlayer;p->events=events;return p;}
    catch(const std::exception&){return nullptr;}
}
extern "C" void hwRmPresentationPlayerFree(HwRmPresentationPlayer *p){delete p;}
extern "C" void hwRmPresentationPlayerStop(HwRmPresentationPlayer *p){if(p)p->active=false;}
extern "C" int hwRmPresentationPlayerStart(HwRmPresentationPlayer *p,const char *name,
    double clock,HwRmPresentationVisit visit,void *user)
{
    if(!p || !std::isfinite(clock) || !visit)return 0;
    int index=hwRmPresentationFindTimeline(p->events,name);
    if(index<0)return 0;
    // RM TriggerAnimEvent start (005f1af6) dispatches the negative range
    // [-100000, -0.00001], not an ordinary phase-zero update. The importer
    // accepts the authored -1 sentinel rather than arbitrary negative times.
    for(const auto& event:p->events->events)
        if(event.timeline==(unsigned)index && event.time<0)visit(user,&event,0);
    p->timeline=(unsigned)index;p->start=clock;p->elapsed=0;p->active=true;p->firstUpdate=true;
    return 1;
}
extern "C" int hwRmPresentationPlayerUpdate(HwRmPresentationPlayer *p,double clock,
    HwRmPresentationVisit visit,void *user)
{
    if(!p || !std::isfinite(clock) || !visit)return 0;
    if(!p->active)return 1;
    double elapsed=clock-p->start;
    // A load/reset must explicitly restart owner playback, never replay an
    // arbitrary interval from the pre-load owner's clock.
    if(!std::isfinite(elapsed) || elapsed<p->elapsed)return 0;
    if(elapsed==p->elapsed)return 1;
    const auto& timeline=p->events->timelines[p->timeline];
    if(!timeline.loop)elapsed=std::min(elapsed,(double)timeline.duration);
    if(!hwRmPresentationVisitInterval(p->events,p->timeline,p->firstUpdate?-1:p->elapsed,elapsed,visit,user))return 0;
    p->elapsed=elapsed;p->firstUpdate=false;
    if(!timeline.loop && elapsed>=timeline.duration)p->active=false;
    return 1;
}
