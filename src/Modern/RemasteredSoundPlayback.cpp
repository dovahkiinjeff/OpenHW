#include "RemasteredSoundPlayback.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
struct Voice {int handle=0,cue=-1;uint64_t anchor=0;unsigned random=0,step=0;double next=0;
    bool waiting=false;std::array<float,3> position{};HwRmSoundSelection selection{};};
void message(char* error,size_t capacity,const char* text){if(error&&capacity)std::snprintf(error,capacity,"%s",text);}
bool finite(const float* value){return value&&std::isfinite(value[0])&&std::isfinite(value[1])&&std::isfinite(value[2]);}
}
struct HwRmSoundPlayback {
    const HwRmSoundBank* bank;HwRmSoundPlaybackOps ops;void* user;
    std::vector<int> clips;std::array<Voice,64> voices;
    std::array<float,3> listener{},right{1,0,0};
    double clock=0;
};
namespace {
void release(HwRmSoundPlayback& p,Voice& voice){if(voice.anchor)p.ops.releaseAnchor(p.user,voice.anchor);voice=Voice{};}
bool spatial(HwRmSoundPlayback& p,Voice& voice,float& volume,float& pan){
    if(voice.anchor&&!p.ops.anchor(p.user,voice.anchor,voice.position.data()))return false;
    float delta[3],distance=0,side=0;for(unsigned i=0;i<3;++i){delta[i]=voice.position[i]-p.listener[i];distance+=delta[i]*delta[i];side+=delta[i]*p.right[i];}
    distance=std::sqrt(distance);if(!std::isfinite(distance))return false;pan=distance>.0001f?std::clamp(side/distance,-1.f,1.f):0;
    HwRmSoundEnvelope envelope{};if(!hwRmSoundBankEnvelope(&voice.selection,distance,&envelope))return false;
    volume=std::clamp(envelope.volume,0.f,1.f);return true;
}
}
extern "C" HwRmSoundPlayback* hwRmSoundPlaybackCreate(const HwRmSoundBank* bank,const HwRmSoundPlaybackOps* ops,void* user,char* error,size_t capacity){
    try{
        if(!bank||!ops||!ops->precache||!ops->play||!ops->update||!ops->active||!ops->stop||!ops->anchor||!ops->releaseAnchor)throw std::runtime_error("invalid sound playback backend");
        auto result=std::make_unique<HwRmSoundPlayback>();result->bank=bank;result->ops=*ops;result->user=user;
        result->clips.resize(hwRmSoundBankCount(bank));
        for(unsigned i=0;i<result->clips.size();++i){const auto* node=hwRmSoundBankAt(bank,i);if(!*node->sample)continue;
            result->clips[i]=ops->precache(user,node->sample);if(!result->clips[i])throw std::runtime_error("native sound sample precache failed");}
        message(error,capacity,"");return result.release();
    }catch(const std::exception& e){message(error,capacity,e.what());return nullptr;}
}
extern "C" void hwRmSoundPlaybackStop(HwRmSoundPlayback* p){if(p){for(auto& voice:p->voices)if(voice.cue>=0){if(voice.handle)p->ops.stop(p->user,voice.handle);release(*p,voice);}p->clock=0;}}
extern "C" void hwRmSoundPlaybackFree(HwRmSoundPlayback* p){hwRmSoundPlaybackStop(p);delete p;}
extern "C" int hwRmSoundPlaybackListener(HwRmSoundPlayback* p,const float* position,const float* right){
    if(!p||!finite(position)||!finite(right))return 0;float norm=std::sqrt(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);if(!std::isfinite(norm)||norm<.0001f)return 0;
    for(unsigned i=0;i<3;++i){p->listener[i]=position[i];p->right[i]=right[i]/norm;}return 1;
}
extern "C" void hwRmSoundPlaybackUpdate(HwRmSoundPlayback* p){
    if(!p)return;for(auto& voice:p->voices)if(voice.cue>=0){float volume,pan;
        if(!spatial(*p,voice,volume,pan)){if(voice.handle)p->ops.stop(p->user,voice.handle);release(*p,voice);continue;}
        if(voice.handle&&!p->ops.active(p->user,voice.handle)){
            voice.handle=0;
            if(voice.selection.playlistNode<0 ||
                (!voice.selection.profile.values[27] && voice.step+1>=voice.selection.playlistCount)){
                release(*p,voice);continue;
            }
            voice.random=voice.random*1664525u+1013904223u;++voice.step;
            voice.next=p->clock+std::max(0.f,voice.selection.profile.values[28])+
                (double(voice.random)/4294967296.0)*std::max(0.f,voice.selection.profile.values[29]);
            voice.waiting=true;
        }
        if(voice.waiting && p->clock>=voice.next){
            char error[128];const auto* cue=hwRmSoundBankAt(p->bank,unsigned(voice.cue));
            if(!hwRmSoundBankSelectStep(p->bank,cue->name,voice.random,voice.step,&voice.selection,error,sizeof(error)) ||
                !spatial(*p,voice,volume,pan) ||
                !(voice.handle=p->ops.play(p->user,p->clips[voice.selection.node],volume,pan))){release(*p,voice);continue;}
            voice.waiting=false;
        }
        if(voice.handle&&!p->ops.update(p->user,voice.handle,volume,pan)){p->ops.stop(p->user,voice.handle);release(*p,voice);}
    }
}
extern "C" int hwRmSoundPlaybackAdvance(HwRmSoundPlayback* p,double clock){
    if(!p||!std::isfinite(clock)||clock<p->clock)return 0;p->clock=clock;hwRmSoundPlaybackUpdate(p);return 1;
}
extern "C" int hwRmSoundPlaybackPlay(HwRmSoundPlayback* p,const char* cue,unsigned random,const float* position,uint64_t anchor,char* error,size_t capacity){
    if(!p||!finite(position)){message(error,capacity,"invalid native sound playback request");return 0;}
    hwRmSoundPlaybackUpdate(p);Voice pending;pending.cue=hwRmSoundBankFind(p->bank,cue);
    if(!hwRmSoundBankSelect(p->bank,cue,random,&pending.selection,error,capacity))return 0;
    unsigned active=0;Voice* slot=nullptr;for(auto& voice:p->voices){if(voice.cue==pending.cue)++active;if(voice.cue<0&&!slot)slot=&voice;}
    if(!slot||float(active)>=pending.selection.profile.values[9]){message(error,capacity,"native sound polyphony budget exhausted");return 0;}
    pending.anchor=anchor;pending.random=random;for(unsigned i=0;i<3;++i)pending.position[i]=position[i];float volume,pan;
    if(!spatial(*p,pending,volume,pan)){message(error,capacity,"native sound attachment unavailable");return 0;}
    pending.handle=p->ops.play(p->user,p->clips[pending.selection.node],volume,pan);
    if(!pending.handle){message(error,capacity,"native PCM voice unavailable");return 0;}
    *slot=pending;message(error,capacity,"");return pending.handle;
}
