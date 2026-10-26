#ifndef OPENHW_REMASTERED_SOUND_BANK_H
#define OPENHW_REMASTERED_SOUND_BANK_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmSoundBank HwRmSoundBank;
typedef struct HwRmSoundEnvelope {float distance,volume,reverb,duration,equalizer[8];} HwRmSoundEnvelope;
typedef struct HwRmSoundProfile {
    unsigned mask,effectMask;
    int randomSamples; /* -1 = not authored */
    float values[30],effects[28];
    unsigned envelopeCount;
    const HwRmSoundEnvelope *envelope;
} HwRmSoundProfile;
typedef struct HwRmSoundNode {
    const char *name,*sample;
    int parent;
    unsigned firstChild,childCount;
    const HwRmSoundProfile *profile;
} HwRmSoundNode;
typedef struct HwRmSoundSelection {
    unsigned node,playlistCount;
    int playlistNode;
    HwRmSoundProfile profile;
} HwRmSoundSelection;
/* One generated import index, immutable for its complete playback lifetime.
   Source profiles retain presence, rather than pre-flattening defaults. */
HwRmSoundBank *hwRmSoundBankRead(const void *bytes,size_t size,char *error,size_t capacity);
void hwRmSoundBankFree(HwRmSoundBank *bank);
unsigned hwRmSoundBankCount(const HwRmSoundBank *bank);
const HwRmSoundNode *hwRmSoundBankAt(const HwRmSoundBank *bank,unsigned node);
int hwRmSoundBankFind(const HwRmSoundBank *bank,const char *name);
/* Caller-supplied presentation randomness never touches the HW1 gameplay RNG.
   Playlist selections describe one entry; the playback controller owns their
   subsequent entries, silence intervals and owner lifetime. */
int hwRmSoundBankSelect(const HwRmSoundBank *bank,const char *name,unsigned random,
    HwRmSoundSelection *selection,char *error,size_t capacity);
int hwRmSoundBankSelectStep(const HwRmSoundBank *bank,const char *name,unsigned random,unsigned step,
    HwRmSoundSelection *selection,char *error,size_t capacity);
int hwRmSoundBankEnvelope(const HwRmSoundSelection *selection,float distance,HwRmSoundEnvelope *output);
#ifdef __cplusplus
}
#endif
#endif
