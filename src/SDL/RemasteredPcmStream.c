#include "RemasteredPcmStream.h"
#include "PcmStreamState.h"
#include "RemasteredAudioDecode.h"
#include "Types.h"
#include "soundlow.h"
#include "soundcmn.h"
#include "File.h"
#include "Memory.h"
#include <SDL.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
extern bool32 soundinited;

typedef struct RmPcmStream {
    int owned, handle, bus;
    void *allocation;
    HwPcmStreamState state;
} RmPcmStream;
static RmPcmStream pcmStreams[SOUND_MAX_STREAM_BUFFERS];
#define RM_PCM_EFFECT_CLIPS 512
#define RM_PCM_EFFECT_VOICES 64
/* Audited 313 shipped ETG samples require 162,224,128 stereo bytes. */
#define RM_PCM_EFFECT_BUDGET (256u*1024*1024)
typedef struct RmPcmClip {
    char resource[512];
    void *allocation;
    size_t frames;
    int handle;
} RmPcmClip;
typedef struct RmPcmVoice {
    HwPcmStreamState state;
    float pan;
    int handle;
} RmPcmVoice;
static RmPcmClip effectClips[RM_PCM_EFFECT_CLIPS];
static RmPcmVoice effectVoices[RM_PCM_EFFECT_VOICES];
static size_t effectBytes;
static int nextClip=1,nextVoice=1;
static int loadPcm(const char *resource,SDL_AudioCVT *output);
static RmPcmStream *lookup(int handle)
{
    int i = SNDchannel(handle);
    return handle >= 0 && i >= 0 && i < SOUND_MAX_STREAM_BUFFERS &&
        pcmStreams[i].owned && pcmStreams[i].handle == handle ? &pcmStreams[i] : NULL;
}
void rmPcmStreamRelease(int handle)
{
    void *allocation = NULL;
    RmPcmStream *s;
    SDL_LockAudio();
    s = lookup(handle);
    if (s) { allocation = s->allocation; memset(s, 0, sizeof(*s)); }
    SDL_UnlockAudio();
    SDL_free(allocation);
}
void rmPcmStreamReset(void)
{
    unsigned i;
    void *old[SOUND_MAX_STREAM_BUFFERS+RM_PCM_EFFECT_CLIPS];
    SDL_LockAudio();
    for (i=0;i<SOUND_MAX_STREAM_BUFFERS;++i) old[i]=pcmStreams[i].allocation;
    memset(pcmStreams,0,sizeof(pcmStreams));
    for(i=0;i<RM_PCM_EFFECT_CLIPS;++i)old[SOUND_MAX_STREAM_BUFFERS+i]=effectClips[i].allocation;
    memset(effectVoices,0,sizeof(effectVoices));memset(effectClips,0,sizeof(effectClips));effectBytes=0;
    SDL_UnlockAudio();
    for (i=0;i<SOUND_MAX_STREAM_BUFFERS+RM_PCM_EFFECT_CLIPS;++i) SDL_free(old[i]);
}
int rmPcmStreamPlay(int handle, const char *resource, float volume, int bus)
{
    SDL_AudioCVT cvt;
    void *old;
    int index=SNDchannel(handle);
    RmPcmStream *s;
    if (!soundinited || handle<0 || index<0 || index>=SOUND_MAX_STREAM_BUFFERS ||
        bus<0 || bus>=HW_AUDIO_BUS_COUNT || !isfinite(volume) || !loadPcm(resource,&cvt))return 0;
    SDL_LockAudio();
    s=&pcmStreams[index]; old=s->allocation;
    memset(s,0,sizeof(*s));
    s->owned=1; s->handle=handle; s->bus=bus; s->allocation=cvt.buf;
    s->state.samples=(const int16_t *)cvt.buf; s->state.frames=(size_t)cvt.len_cvt/4;
    hwPcmFade(&s->state,volume,0,0);
    SDL_UnlockAudio();
    SDL_free(old);
    return 1;
}
static int loadPcm(const char *resource,SDL_AudioCVT *output)
{
    SDL_AudioSpec spec;
    SDL_AudioCVT cvt;
    Uint8 *wave = NULL;
    Uint32 waveBytes = 0;
    void *file = NULL;
    int bytes,relic=0;
    if(!resource || !*resource)return 0;
    if (!fileExists((char *)resource, FF_ReturnNULLOnFail)) return 0;
    bytes = fileSizeGet((char *)resource, FF_ReturnNULLOnFail);
    if (bytes <= 0 || bytes > 128*1024*1024) return 0;
    if (fileLoadAlloc((char *)resource, &file, FF_ReturnNULLOnFail) != bytes || !file) {
        if (file) memFree(file);
        return 0;
    }
    if (bytes>=4 && !memcmp(file,"FORM",4)) {
        HwRmAudioInfo info; int16_t *decoded=NULL;
        if(!hwRmAudioDecode(file,(size_t)bytes,&decoded,&info)){memFree(file);return 0;}
        memset(&spec,0,sizeof(spec));spec.format=AUDIO_S16SYS;
        spec.freq=(int)info.sampleRate;spec.channels=(Uint8)info.channels;
        wave=(Uint8 *)decoded;waveBytes=(Uint32)(info.frames*info.channels*sizeof(int16_t));
        relic=1;
    } else if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(file,bytes),1,&spec,&wave,&waveBytes)) {
        memFree(file); return 0;
    }
    memFree(file);
    if (SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_S16SYS,2,HW_PCM_OUTPUT_RATE)<0 ||
        cvt.len_mult <= 0 || waveBytes > (Uint32)(INT_MAX/cvt.len_mult)) {
        if(relic)free(wave);else SDL_FreeWAV(wave); return 0;
    }
    cvt.len=(int)waveBytes;
    if ((size_t)cvt.len*cvt.len_mult > 256u*1024*1024) {
        if(relic)free(wave);else SDL_FreeWAV(wave);return 0;
    }
    cvt.buf=SDL_malloc((size_t)cvt.len*cvt.len_mult);
    if (!cvt.buf) { if(relic)free(wave);else SDL_FreeWAV(wave); return 0; }
    memcpy(cvt.buf,wave,waveBytes);
    if(relic)free(wave);else SDL_FreeWAV(wave);
    if (SDL_ConvertAudio(&cvt)<0 || cvt.len_cvt <= 0 || cvt.len_cvt%4) {
        SDL_free(cvt.buf); return 0;
    }
    *output=cvt;
    return 1;
}
int rmPcmEffectPrecache(const char *resource)
{
    unsigned i;SDL_AudioCVT cvt;RmPcmClip *clip;
    if(!soundinited || !resource || !*resource || strlen(resource)>=sizeof(effectClips[0].resource))return 0;
    /* Cache commands are main-thread only; audio reads immutable PCM, never
       names/allocations. No eviction means live voices cannot dangle. */
    for(i=0;i<RM_PCM_EFFECT_CLIPS;++i)
        if(effectClips[i].handle && !strcmp(effectClips[i].resource,resource))return effectClips[i].handle;
    for(i=0;i<RM_PCM_EFFECT_CLIPS && effectClips[i].handle;++i);
    if(i==RM_PCM_EFFECT_CLIPS || nextClip==INT_MAX || !loadPcm(resource,&cvt))return 0;
    if((size_t)cvt.len_cvt>RM_PCM_EFFECT_BUDGET-effectBytes){SDL_free(cvt.buf);return 0;}
    clip=&effectClips[i];strcpy(clip->resource,resource);clip->allocation=cvt.buf;
    clip->frames=(size_t)cvt.len_cvt/4;clip->handle=nextClip++;effectBytes+=(size_t)cvt.len_cvt;
    return clip->handle;
}
int rmPcmEffectPlay(int handle,float volume,float pan)
{
    unsigned i;RmPcmClip *clip=NULL;RmPcmVoice *voice=NULL;int result=0;
    if(!soundinited || handle<=0 || !isfinite(volume) || !isfinite(pan) || nextVoice==INT_MAX)return 0;
    for(i=0;i<RM_PCM_EFFECT_CLIPS;++i)if(effectClips[i].handle==handle){clip=&effectClips[i];break;}
    if(!clip)return 0;
    SDL_LockAudio();
    for(i=0;i<RM_PCM_EFFECT_VOICES;++i)if(!effectVoices[i].handle || effectVoices[i].state.stopped){voice=&effectVoices[i];break;}
    if(voice){
        memset(voice,0,sizeof(*voice));voice->handle=nextVoice++;voice->pan=fmaxf(-1,fminf(1,pan));
        voice->state.samples=(const int16_t *)clip->allocation;voice->state.frames=clip->frames;
        hwPcmFade(&voice->state,volume,0,0);result=voice->handle;
    }
    SDL_UnlockAudio();return result;
}
int rmPcmEffectUpdate(int handle,float volume,float pan)
{
    unsigned i;int found=0;
    if(handle<=0 || !isfinite(volume) || !isfinite(pan))return 0;
    SDL_LockAudio();
    for(i=0;i<RM_PCM_EFFECT_VOICES;++i)if(effectVoices[i].handle==handle && !effectVoices[i].state.stopped){
        hwPcmFade(&effectVoices[i].state,volume,0,0);effectVoices[i].pan=fmaxf(-1,fminf(1,pan));found=1;break;
    }
    SDL_UnlockAudio();return found;
}
void rmPcmEffectStop(int handle)
{
    unsigned i;if(handle<=0)return;
    SDL_LockAudio();
    for(i=0;i<RM_PCM_EFFECT_VOICES;++i)if(effectVoices[i].handle==handle){effectVoices[i].state.stopped=1;break;}
    SDL_UnlockAudio();
}
int rmPcmEffectActive(int handle)
{
    unsigned i;int active=0;if(handle<=0)return 0;
    SDL_LockAudio();
    for(i=0;i<RM_PCM_EFFECT_VOICES;++i)if(effectVoices[i].handle==handle){active=!effectVoices[i].state.stopped;break;}
    SDL_UnlockAudio();return active;
}
int rmPcmStreamQuery(int handle, int query)
{
    int result=-1; RmPcmStream *s;
    SDL_LockAudio();
    s=lookup(handle);
    if(s) {
        if(query==0) result=s->state.stopped;
        if(query==1) result=s->state.fadeFrames!=0;
        if(query==2) result=(int)(s->state.volume*SOUND_VOL_MAX+.5f);
    }
    SDL_UnlockAudio();
    return result;
}
int rmPcmStreamVolume(int handle,int volume,float seconds)
{
    int found=0; RmPcmStream *s;
    if(!isfinite(seconds) || seconds<0) seconds=0;
    if(seconds>3600) seconds=3600;
    SDL_LockAudio();
    s=lookup(handle);
    if(s) {
        hwPcmFade(&s->state,volume<0?0:(float)volume/SOUND_VOL_MAX,
                  (size_t)(seconds*HW_PCM_OUTPUT_RATE),volume==SOUND_VOL_AUTOSTOP);
        found=1;
    }
    SDL_UnlockAudio();
    return found;
}
void rmPcmStreamMix(void *output,int bytes)
{
    unsigned i;
    if(bytes<=0 || bytes%4) return;
    for(i=0;i<SOUND_MAX_STREAM_BUFFERS;++i) if(pcmStreams[i].owned)
        hwPcmMix(&pcmStreams[i].state,(int16_t *)output,(size_t)bytes/4,
                 soundBusGain(pcmStreams[i].bus));
    for(i=0;i<RM_PCM_EFFECT_VOICES;++i)if(effectVoices[i].handle && !effectVoices[i].state.stopped){
        /* Fixed stack chunk applies panning without an allocation or file I/O
           on the audio thread. The shared mixer owns saturation and timing. */
        size_t remaining=(size_t)bytes/4,offset=0;RmPcmVoice *voice=&effectVoices[i];
        float left=voice->pan>0?1-voice->pan:1,right=voice->pan<0?1+voice->pan:1;
        while(remaining){
            int16_t scratch[512]={0};size_t count=remaining>256?256:remaining,j;int16_t *target=(int16_t *)output+offset*2;
            hwPcmMix(&voice->state,scratch,count,soundBusGain(HW_AUDIO_SFX));
            for(j=0;j<count;++j){
                int l=target[j*2]+(int)(scratch[j*2]*left),r=target[j*2+1]+(int)(scratch[j*2+1]*right);
                target[j*2]=(int16_t)(l>32767?32767:l< -32768?-32768:l);
                target[j*2+1]=(int16_t)(r>32767?32767:r< -32768?-32768:r);
            }
            offset+=count;remaining-=count;
        }
    }
}
