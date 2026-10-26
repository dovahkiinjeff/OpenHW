#ifndef OPENHW_REMASTERED_FX_PLAYBACK_H
#define OPENHW_REMASTERED_FX_PLAYBACK_H
#include "RemasteredFxGraph.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmFxPlayback HwRmFxPlayback;
typedef struct HwRmFxTrailPoint {float position[3],age;} HwRmFxTrailPoint;
typedef struct HwRmFxDraw {
    const HwRmFxNode *node;
    float matrix[16],scale,age,phase,length;
    unsigned random;
    float spin[3],radiusMultiplier;
    /* Borrowed, oldest-to-newest world positions. Length on STYLE_TRAIL is
       a time window, not a straight beam's distance. Valid during draw only. */
    const HwRmFxTrailPoint *trail;
    unsigned trailCount;
} HwRmFxDraw;
typedef struct HwRmFxPlaybackOps {
    int (*anchor)(void *user,uint64_t handle,float matrix[16],float velocity[3],float *length);
    void (*releaseAnchor)(void *user,uint64_t handle);
    void (*draw)(void *user,const HwRmFxDraw *draw);
} HwRmFxPlaybackOps;
/* All allocation occurs at Create. Playback never reads files or consumes the
   gameplay RNG. Source graph and callback context must outlive the controller. */
HwRmFxPlayback *hwRmFxPlaybackCreate(const HwRmFxGraph *graph,const HwRmFxPlaybackOps *ops,void *user);
void hwRmFxPlaybackFree(HwRmFxPlayback *playback);
void hwRmFxPlaybackReset(HwRmFxPlayback *playback);
/* Matrix maps native RM emitter coordinates into world space; scale affects
   visual dimensions only, not the translation or gameplay. Anchor ownership
   transfers only on success. Zero anchor is a detached world-space effect. */
int hwRmFxPlaybackSpawn(HwRmFxPlayback *playback,const char *choices,unsigned random,
    double clock,const float matrix[16],const float velocity[3],float scale,float length,uint64_t anchor);
int hwRmFxPlaybackUpdate(HwRmFxPlayback *playback,double clock);
void hwRmFxPlaybackDraw(HwRmFxPlayback *playback);
unsigned hwRmFxPlaybackCount(const HwRmFxPlayback *playback);
unsigned hwRmFxPlaybackRejected(const HwRmFxPlayback *playback);
#ifdef __cplusplus
}
#endif
#endif
