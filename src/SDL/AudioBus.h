#ifndef OPENHW_AUDIO_BUS_H
#define OPENHW_AUDIO_BUS_H
/* User mixer gains are independent of scripted fades and individual voices.
   Values cross the UI/audio-thread boundary as fixed-point atomic integers. */
enum { HW_AUDIO_SFX, HW_AUDIO_SPEECH, HW_AUDIO_MUSIC, HW_AUDIO_BUS_COUNT };
static int hwAudioGainEncode(float gain)
{
    if (!(gain > 0.0f)) return 0; /* Includes NaN. */
    if (gain >= 1.0f) return 10000;
    return (int)(gain * 10000.0f + 0.5f);
}
static float hwAudioGainDecode(int gain)
{
    return gain <= 0 ? 0.0f : gain >= 10000 ? 1.0f : gain / 10000.0f;
}
void soundBusVolume(int bus, float gain);
float soundBusGain(int bus);
void soundStreamBus(int handle, int bus);
#endif
