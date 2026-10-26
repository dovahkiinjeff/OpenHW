#ifndef OPENHW_REMASTERED_SOUND_PLAYBACK_H
#define OPENHW_REMASTERED_SOUND_PLAYBACK_H
#include "RemasteredSoundBank.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmSoundPlayback HwRmSoundPlayback;
typedef struct HwRmSoundPlaybackOps {
    int (*precache)(void *user,const char *resource);
    int (*play)(void *user,int clip,float volume,float pan);
    int (*update)(void *user,int voice,float volume,float pan);
    int (*active)(void *user,int voice);
    void (*stop)(void *user,int voice);
    int (*anchor)(void *user,uint64_t handle,float position[3]);
    void (*releaseAnchor)(void *user,uint64_t handle);
} HwRmSoundPlaybackOps;
/* Initialization precaches every sample; play/update never open/decode files.
   Bank/ops user must outlive this main-thread controller. */
HwRmSoundPlayback *hwRmSoundPlaybackCreate(const HwRmSoundBank *bank,
    const HwRmSoundPlaybackOps *ops,void *user,char *error,size_t capacity);
void hwRmSoundPlaybackFree(HwRmSoundPlayback *playback);
void hwRmSoundPlaybackStop(HwRmSoundPlayback *playback);
int hwRmSoundPlaybackListener(HwRmSoundPlayback *playback,const float position[3],const float right[3]);
/* Anchor ownership transfers only when play returns a nonzero voice. */
int hwRmSoundPlaybackPlay(HwRmSoundPlayback *playback,const char *cue,unsigned random,
    const float position[3],uint64_t anchor,char *error,size_t capacity);
void hwRmSoundPlaybackUpdate(HwRmSoundPlayback *playback);
/* Monotonic simulation clock; paused gameplay does not consume playlist gaps.
   Reset/stop must be used before a new mission resets its clock. */
int hwRmSoundPlaybackAdvance(HwRmSoundPlayback *playback,double clock);
#ifdef __cplusplus
}
#endif
#endif
