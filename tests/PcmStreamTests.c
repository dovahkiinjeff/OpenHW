#include "PcmStreamState.h"
#include "RemasteredMusicPaths.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("failed line %d\n",__LINE__); return 1; } } while(0)
int main(void)
{
    {
        HwLegacyPcmResampler converter={0};
        int16_t input[]={10000,-10000,20000,-20000},whole[8],split[8];
        hwLegacyPcmUpsample(&converter,input,whole,2);
        CHECK(whole[0]==5000 && whole[1]==-5000 && whole[2]==10000 && whole[4]==15000);
        memset(&converter,0,sizeof(converter));
        hwLegacyPcmUpsample(&converter,input,split,1);
        hwLegacyPcmUpsample(&converter,input+2,split+4,1);
        CHECK(!memcmp(whole,split,sizeof(whole)));
    }
    int16_t samples[]={10000,-10000,10000,-10000,10000,-10000,10000,-10000};
    int16_t output[8]={0};
    HwPcmStreamState s={0};
    s.samples=samples;s.frames=4;
    hwPcmFade(&s,1,0,0);
    hwPcmMix(&s,output,2,.5f);
    CHECK(output[0]==5000 && output[1]==-5000 && s.position==2 && !s.stopped);
    hwPcmMix(&s,output+4,2,0);
    CHECK(!output[4] && !output[5] && s.position==4 && s.stopped);
    s.position=0;s.stopped=0; memset(output,0,sizeof(output));
    hwPcmFade(&s,0,4,1);
    hwPcmMix(&s,output,2,1);
    CHECK(output[0]==10000 && output[2]==7500 && s.fadeFrames==2);
    hwPcmFade(&s,0,4,1); /* repeated per-frame command must not reset fade */
    CHECK(s.fadeFrames==2);
    hwPcmMix(&s,output+4,2,1);
    CHECK(output[4]==5000 && output[6]==2500 && s.stopped && s.volume==0);
    s.position=0;s.stopped=0;hwPcmFade(&s,1,0,0);
    output[0]=30000;output[1]=-30000;hwPcmMix(&s,output,1,1);
    CHECK(output[0]==32767 && output[1]==-32768);
    hwPcmFade(&s,0,0,1);CHECK(s.stopped);
    CHECK(!rmMusicPath(-1) && !rmMusicPath(48) && !rmMusicPath(28));
    CHECK(!strcmp(rmMusicPath(17),"nis/n01_r1opening.wav"));
    CHECK(!strcmp(rmMusicPath(31),"animatic/anim_00_opening.wav"));
    CHECK(!strcmp(rmMusicPath(47),"animatic/anim_ending.wav"));
    puts("PCM gain, mute, fade, stop, clipping and HW1 music mapping passed");
    return 0;
}
