#include "AudioBus.h"
#include <math.h>
#include <stdio.h>
int main(void)
{
    int i;
    if (hwAudioGainEncode(NAN) != 0 || hwAudioGainEncode(-1) != 0 ||
        hwAudioGainEncode(2) != 10000 || hwAudioGainDecode(-5) != 0 ||
        hwAudioGainDecode(20000) != 1) return 1;
    for (i = 0; i <= 100; ++i) {
        float user = hwAudioGainDecode(hwAudioGainEncode(i / 100.0f));
        if (fabsf(user - i / 100.0f) > .0001f) return 2;
        /* Scripted volume changes never bypass a muted user bus. */
        if (user * 0.0f != 0.0f) return 3;
    }
    if (hwAudioGainDecode(5000) * .4f != .2f) return 4;
    puts("Independent user audio gain bounds/precision passed");
    return 0;
}
