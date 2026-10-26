#ifndef OPENHW_PCM_STREAM_STATE_H
#define OPENHW_PCM_STREAM_STATE_H
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#define HW_PCM_OUTPUT_RATE 44100
typedef struct HwLegacyPcmResampler { int16_t previous[2]; } HwLegacyPcmResampler;
/* The original 22050 Hz mixer keeps its simulation clock; interpolate its
   output into the 44100 Hz device alongside native RM audio. */
static void hwLegacyPcmUpsample(HwLegacyPcmResampler *s,const int16_t *input,
                               int16_t *output,size_t frames)
{
    size_t f;unsigned c;
    for(f=0;f<frames;++f)for(c=0;c<2;++c) {
        int current=input[f*2+c];
        output[f*4+c]=(int16_t)(((int)s->previous[c]+current)/2);
        output[f*4+2+c]=(int16_t)current;s->previous[c]=(int16_t)current;
    }
}
/* Audio-thread state. The caller serializes commands against mixing. */
typedef struct HwPcmStreamState {
    const int16_t *samples;
    size_t frames, position, fadeFrames;
    float volume, target, step;
    int stopped, stopAfterFade;
} HwPcmStreamState;
static void hwPcmFade(HwPcmStreamState *s, float volume, size_t frames, int stop)
{
    if (!isfinite(volume)) volume = 0;
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    /* Repeated game-frame volume updates must not keep restarting a fade. */
    if (s->target == volume && s->stopAfterFade == stop && s->fadeFrames) return;
    s->target = volume; s->stopAfterFade = stop; s->fadeFrames = frames;
    s->step = frames ? (volume - s->volume) / (float)frames : 0;
    if (!frames) { s->volume = volume; if (stop) s->stopped = 1; }
}
static void hwPcmMix(HwPcmStreamState *s, int16_t *output, size_t frames, float userGain)
{
    size_t f;
    if (!isfinite(userGain) || userGain < 0) userGain = 0;
    if (userGain > 1) userGain = 1;
    for (f = 0; f < frames && !s->stopped; ++f) {
        unsigned c;
        if (!s->samples || s->position >= s->frames) { s->stopped = 1; break; }
        for (c = 0; c < 2; ++c) {
            float v = output[f*2+c] + s->samples[s->position*2+c] * s->volume * userGain;
            output[f*2+c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        ++s->position;
        if (s->fadeFrames) {
            s->volume += s->step;
            if (!--s->fadeFrames) {
                s->volume = s->target;
                if (s->stopAfterFade) s->stopped = 1;
            }
        }
        if (s->position == s->frames) s->stopped = 1;
    }
}
#endif
