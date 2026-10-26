#include "RemasteredSoundBank.h"
#include "RemasteredSoundPlayback.h"
#include "RemasteredPresentationEvents.h"
#include "RemasteredWeaponPresentation.h"
#include <filesystem>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
using Bytes=std::vector<unsigned char>;
static void u(Bytes& b,unsigned v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);}
static void f(Bytes& b,float value){unsigned v;std::memcpy(&v,&value,4);u(b,v);}
static void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static Bytes fixture(bool playlist=false,bool looping=false){
    Bytes strings{0};auto text=[&](const char* value){unsigned offset=unsigned(strings.size());strings.insert(strings.end(),value,value+std::strlen(value)+1);return offset;};
    auto a=text("a"),b=text("b"),pa=text("remasteredaudio/sfx/etg/a.fda"),pb=text("remasteredaudio/sfx/etg/b.fda");
    Bytes out{'O','W','S','F','X','0','0','1'};for(unsigned n:{3u,2u,1u,2u,unsigned(strings.size())})u(out,n);
    for(unsigned v:{0u,0u,0xffffffffu,0u,0u,2u,0u})u(out,v);
    for(unsigned v:{a,pa,0u,0xffffffffu,2u,0u,1u})u(out,v);
    for(unsigned v:{b,pb,0u,0xffffffffu,2u,0u,1u})u(out,v);
    u(out,1);u(out,2);
    u(out,(1u<<2)|(1u<<9)|(1u<<19)|(playlist?((1u<<25)|(1u<<28)|(1u<<27)):0));u(out,1);u(out,0);u(out,2);u(out,0);
    for(unsigned i=0;i<30;++i)f(out,i==2?.5f:i==9?3.f:playlist&&i==25?1.f:playlist&&i==27&&looping?1.f:playlist&&i==28?2.f:0.f);for(unsigned i=0;i<28;++i)f(out,0);
    for(unsigned point=0;point<2;++point){f(out,float(point)*100);f(out,1-float(point));f(out,0);f(out,0);for(unsigned i=0;i<8;++i)f(out,1);}
    out.insert(out.end(),strings.begin(),strings.end());return out;
}
struct Backend {
    struct Voice {bool active=false;float volume=0,pan=0;};
    std::array<Voice,128> voices{};int loads=0,plays=0,stops=0,releases=0;
    bool anchorAlive=true;float anchorX=50;
};
static int precache(void* user,const char*){return ++static_cast<Backend*>(user)->loads;}
static int play(void* user,int clip,float volume,float pan){auto& b=*static_cast<Backend*>(user);if(clip<=0)return 0;int h=++b.plays;b.voices[h]={true,volume,pan};return h;}
static int update(void* user,int h,float volume,float pan){auto& v=static_cast<Backend*>(user)->voices[h];if(!v.active)return 0;v.volume=volume;v.pan=pan;return 1;}
static int active(void* user,int h){return static_cast<Backend*>(user)->voices[h].active;}
static void stop(void* user,int h){auto& b=*static_cast<Backend*>(user);b.voices[h].active=false;++b.stops;}
static int anchor(void* user,uint64_t,float* position){auto& b=*static_cast<Backend*>(user);position[0]=b.anchorX;position[1]=position[2]=0;return b.anchorAlive;}
static void releaseAnchor(void* user,uint64_t){++static_cast<Backend*>(user)->releases;}
int main(int argc,char** argv){try{
    char error[256];auto bytes=fixture();auto* bank=hwRmSoundBankRead(bytes.data(),bytes.size(),error,sizeof(error));check(bank!=nullptr,error);
    check(hwRmSoundBankCount(bank)==3&&hwRmSoundBankFind(bank,"A")==1,"case-insensitive source sound identity failed");
    check(hwRmSoundBankFind(bank,"../a")==-1&&hwRmSoundBankFind(bank,"a.fda")==-1,"invented sound alias accepted");
    HwRmSoundSelection selected{};check(hwRmSoundBankSelect(bank,"",1,&selected,error,sizeof(error))!=0,error);
    check(selected.node==2&&selected.profile.values[9]==3,"random child or inherited profile failed");
    HwRmSoundEnvelope envelope;check(hwRmSoundBankEnvelope(&selected,50,&envelope)&&envelope.volume==.25f,"distance envelope/gain failed");
    check(hwRmSoundBankSelect(bank,"a",0,&selected,error,sizeof(error))&&selected.node==1,"direct sample selection failed");
    {
        Backend backend;HwRmSoundPlaybackOps ops{precache,play,update,active,stop,anchor,releaseAnchor};
        auto* playback=hwRmSoundPlaybackCreate(bank,&ops,&backend,error,sizeof(error));check(playback!=nullptr,error);
        check(backend.loads==2,"sample precache was not completed before play");
        const float zero[3]={0,0,0},right[3]={1,0,0};check(hwRmSoundPlaybackListener(playback,zero,right)!=0,"listener failed");
        int voice=hwRmSoundPlaybackPlay(playback,"a",0,zero,1,error,sizeof(error));check(voice>0,error);
        check(backend.voices[voice].volume==.25f&&backend.voices[voice].pan==1,"attached sound gain/pan failed");
        backend.anchorX=-25;hwRmSoundPlaybackUpdate(playback);
        check(backend.voices[voice].volume==.375f&&backend.voices[voice].pan== -1,"moving sound did not update");
        check(hwRmSoundPlaybackPlay(playback,"a",0,zero,0,error,sizeof(error))>0,error);
        check(hwRmSoundPlaybackPlay(playback,"a",0,zero,0,error,sizeof(error))>0,error);
        check(!hwRmSoundPlaybackPlay(playback,"a",0,zero,0,error,sizeof(error)),"per-cue polyphony was ignored");
        check(backend.loads==2,"fire/update performed sample I/O");
        backend.anchorAlive=false;hwRmSoundPlaybackUpdate(playback);
        check(!backend.voices[voice].active&&backend.releases==1,"deleted owner retained its attached voice");
        check(hwRmSoundPlaybackPlay(playback,"a",0,zero,0,error,sizeof(error))>0,"dead voice did not free polyphony");
        check(!hwRmSoundPlaybackPlay(playback,"missing",0,zero,0,error,sizeof(error)),"missing cue played a guessed sample");
        hwRmSoundPlaybackFree(playback);check(backend.releases==1,"anchor released twice");
    }
    hwRmSoundBankFree(bank);
    {
        auto data=fixture(true);bank=hwRmSoundBankRead(data.data(),data.size(),error,sizeof(error));check(bank!=nullptr,error);
        check(hwRmSoundBankSelectStep(bank,"",99,1,&selected,error,sizeof(error))&&selected.node==2&&selected.playlistCount==2,"ordered playlist entry selection failed");
        Backend backend;HwRmSoundPlaybackOps ops{precache,play,update,active,stop,anchor,releaseAnchor};
        auto* playback=hwRmSoundPlaybackCreate(bank,&ops,&backend,error,sizeof(error));check(playback!=nullptr,error);
        float zero[3]={};int first=hwRmSoundPlaybackPlay(playback,"",0,zero,1,error,sizeof(error));check(first>0,error);
        backend.voices[first].active=false;check(hwRmSoundPlaybackAdvance(playback,1),"playlist clock failed");
        check(backend.plays==1&&backend.releases==0,"playlist released its owner during silence");
        check(hwRmSoundPlaybackAdvance(playback,2.9)&&backend.plays==1,"playlist silence was skipped");
        check(hwRmSoundPlaybackAdvance(playback,3)&&backend.plays==2,"playlist next entry did not start");
        check(!hwRmSoundPlaybackAdvance(playback,2),"backward playlist clock accepted");
        backend.voices[2].active=false;hwRmSoundPlaybackAdvance(playback,4);
        check(backend.releases==1&&backend.plays==2&&backend.loads==2,"finite playlist did not finish without I/O");
        hwRmSoundPlaybackFree(playback);check(backend.releases==1,"playlist owner released twice");hwRmSoundBankFree(bank);
    }
    {
        auto data=fixture(true,true);bank=hwRmSoundBankRead(data.data(),data.size(),error,sizeof(error));check(bank!=nullptr,error);
        Backend backend;HwRmSoundPlaybackOps ops{precache,play,update,active,stop,anchor,releaseAnchor};
        auto* playback=hwRmSoundPlaybackCreate(bank,&ops,&backend,error,sizeof(error));check(playback!=nullptr,error);
        float zero[3]={};check(hwRmSoundPlaybackPlay(playback,"",0,zero,9,error,sizeof(error))>0,error);
        for(int entry=1;entry<=3;++entry){backend.voices[entry].active=false;
            hwRmSoundPlaybackAdvance(playback,entry*3.0);hwRmSoundPlaybackAdvance(playback,entry*3.0+2);}
        check(backend.plays==4&&backend.releases==0,"looping playlist ended at its first cycle");
        backend.voices[4].active=false;hwRmSoundPlaybackAdvance(playback,12);
        backend.anchorAlive=false;hwRmSoundPlaybackAdvance(playback,13);
        check(backend.releases==1&&backend.plays==4,"owner loss during playlist silence did not cancel replay");
        hwRmSoundPlaybackFree(playback);check(backend.releases==1,"looping playlist owner released twice");hwRmSoundBankFree(bank);
    }
    for(size_t length=0;length<bytes.size();++length)check(!hwRmSoundBankRead(bytes.data(),length,error,sizeof(error)),"truncated sound bank accepted");
    auto invalid=bytes;invalid[7]='0';check(!hwRmSoundBankRead(invalid.data(),invalid.size(),error,sizeof(error)),"obsolete sound bank accepted");
    if(argc>=2){std::ifstream file(argv[1],std::ios::binary);check(bool(file),"real sound bank missing");Bytes real((std::istreambuf_iterator<char>(file)),{});
        bank=hwRmSoundBankRead(real.data(),real.size(),error,sizeof(error));check(bank!=nullptr,error);unsigned samples=0,selectable=0,unavailable=0;
        for(unsigned i=0;i<hwRmSoundBankCount(bank);++i){const auto* node=hwRmSoundBankAt(bank,i);if(*node->sample)++samples;
            if(hwRmSoundBankSelect(bank,node->name,73,&selected,error,sizeof(error)))++selectable;else ++unavailable;}
        std::printf("Real sound bank: %u samples, %u selectable nodes, %u directory/playlist nodes not selected\n",samples,selectable,unavailable);
        unsigned cues=0,missing=0;
        auto verify=[&](const char* cue,const std::filesystem::path& path){if(!cue||!*cue)return;++cues;
            if(!hwRmSoundBankSelect(bank,cue,37,&selected,error,sizeof(error))){++missing;std::printf("Unresolved sound cue %s in %s: %s\n",cue,path.string().c_str(),error);}};
        for(int arg=2;arg<argc;++arg)for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[arg])){
            auto extension=entry.path().extension();if(extension!=".owevents"&&extension!=".wf")continue;
            std::ifstream source(entry.path(),std::ios::binary);Bytes data((std::istreambuf_iterator<char>(source)),{});
            if(extension==".owevents"){
                auto* events=hwRmPresentationEventsRead(data.data(),data.size(),error,sizeof(error));check(events!=nullptr,error);
                for(unsigned i=0;i<hwRmPresentationEventCount(events);++i)verify(hwRmPresentationEventAt(events,i)->sound,entry.path());hwRmPresentationEventsFree(events);
            }else{
                auto* weapon=hwRmWeaponPresentationRead(data.data(),data.size(),error,sizeof(error));check(weapon!=nullptr,error);
                for(unsigned i=0;i<HW_RM_WEAPON_SLOT_COUNT;++i)verify(hwRmWeaponPresentationAt(weapon,i)->sound,entry.path());hwRmWeaponPresentationFree(weapon);
            }
        }
        if(argc>2){std::printf("Native event/weapon sound coverage: %u references, %u unresolved\n",cues,missing);check(!missing,"native event/weapon sound references remain unresolved");}
        hwRmSoundBankFree(bank);
    }
    std::puts("Sound bank bounds, inheritance, selection and envelopes passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
