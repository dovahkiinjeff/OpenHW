#ifndef OPENHW_REMASTERED_AUDIO_DECODE_H
#define OPENHW_REMASTERED_AUDIO_DECODE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmAudioInfo {
    unsigned channels, sampleRate, codecRate, bitrate, blocks, concealedBlocks;
    size_t frames, dataOffset, dataBytes;
} HwRmAudioInfo;
int hwRmAudioInspect(const void *bytes,size_t size,HwRmAudioInfo *info);
/* Interleaved signed-16 PCM at 44100 Hz. Caller frees with free().
   Malformed coefficient blocks are silenced and counted in concealedBlocks;
   container errors fail. Duration and following valid audio are preserved. */
int hwRmAudioDecode(const void *bytes,size_t size,int16_t **pcm,HwRmAudioInfo *info);
#ifdef __cplusplus
}
#endif
#endif
