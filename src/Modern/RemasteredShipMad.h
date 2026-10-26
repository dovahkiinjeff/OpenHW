#ifndef HW_REMASTERED_SHIP_MAD_H
#define HW_REMASTERED_SHIP_MAD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Native RM animation data. Curve indices, not channel names, identify curves:
   different clips can reference distinct curves with identical names. All names
   and arrays remain owned by HwRmMad until hwRmMadFree. No source Lua executes. */
typedef struct HwRmMad HwRmMad;
typedef struct HwRmMadKey {
    double time, value;
    float inTangent[2], outTangent[2];
} HwRmMadKey;
typedef struct HwRmMadCurve {
    const char *name;
    unsigned int keyCount;
    const HwRmMadKey *keys;
    int preInfinity, postInfinity;
} HwRmMadCurve;
typedef struct HwRmMadTrack {
    const char *joint;
    unsigned int channelCount;
    const unsigned int *curveIndices;
} HwRmMadTrack;
typedef struct HwRmMadClip {
    const char *name;
    double start, end, loopStart, loopEnd;
    unsigned int trackCount;
    const HwRmMadTrack *tracks;
    /* Authored empty placeholders are retained, but cannot be played. */
    int playable;
} HwRmMadClip;

int hwRmMadRead(const void *data, size_t size, HwRmMad **result,
                char *error, size_t errorSize);
void hwRmMadFree(HwRmMad *mad);
unsigned int hwRmMadCurveCount(const HwRmMad *mad);
unsigned int hwRmMadClipCount(const HwRmMad *mad);
unsigned int hwRmMadFrameRate(const HwRmMad *mad);
const HwRmMadCurve *hwRmMadCurve(const HwRmMad *mad, unsigned int index);
const HwRmMadClip *hwRmMadClip(const HwRmMad *mad, unsigned int index);
/* Exact, case-sensitive clip lookup; no implicit SPIN/launch/docking states. */
const HwRmMadClip *hwRmMadFindClip(const HwRmMad *mad, const char *name);
/* Sample authored absolute local channels: translation XYZ, Euler rotation XYZ
   (radians), scale XYZ. Time is SOURCE seconds, clamped to the selected clip.
   Missing channels retain defaults. Output is unchanged on failure. This does
   not start animations implicitly or guess HW1 state/clip name aliases. */
int hwRmMadSampleTrack(const HwRmMad *mad, unsigned int clip, unsigned int track,
    double sourceTime, const double *defaults, double *channels,
    char *error, size_t errorSize);
/* Validate all referenced joints against the replacement HOD before activation.
   Rename aliases belong in a per-ship binding table, not this source reader. */
int hwRmMadValidateJoints(const HwRmMad *mad, const char *const *joints,
                          unsigned int count, char *error, size_t errorSize);
#ifdef __cplusplus
}
#endif
#endif
