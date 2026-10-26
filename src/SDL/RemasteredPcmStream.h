#ifndef OPENHW_REMASTERED_PCM_STREAM_H
#define OPENHW_REMASTERED_PCM_STREAM_H
/* Main-thread commands; legacy stream handles remain stable. */
int rmPcmStreamPlay(int handle, const char *resource, float volume, int bus);
void rmPcmStreamRelease(int handle);
int rmPcmStreamQuery(int handle, int query); /* -1 = not owned; 0 over, 1 fading, 2 volume */
int rmPcmStreamVolume(int handle, int volume, float seconds);
void rmPcmStreamReset(void);
/* Audio callback only: stereo signed-16 at 44100 Hz. */
void rmPcmStreamMix(void *output, int bytes);
/* Effects are decoded once during main-thread precache. Fire/update commands
   perform no file I/O or decoding. Zero is an invalid clip/voice handle. */
int rmPcmEffectPrecache(const char *resource);
int rmPcmEffectPlay(int clip,float volume,float pan);
int rmPcmEffectUpdate(int voice,float volume,float pan);
void rmPcmEffectStop(int voice);
int rmPcmEffectActive(int voice);
#endif
