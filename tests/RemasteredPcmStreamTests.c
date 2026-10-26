#define SDL_MAIN_HANDLED
#include "Types.h"
#include "soundlow.h"
#include "File.h"
#include "RemasteredPcmStream.h"
#include "RemasteredMusicPaths.h"
#include <stdlib.h>
#include <string.h>

bool32 soundinited=TRUE;
static float busGain=1;
sdword SNDchannel(sdword handle) { return handle<0?-1:(handle&255); }
float soundBusGain(int bus) { return bus==HW_AUDIO_MUSIC || bus==HW_AUDIO_SFX?busGain:0; }
void memFree(void *p) { free(p); }
/* In-memory PCM fixture: 4 stereo frames at the mixer rate. */
static const unsigned char wav[]={
    'R','I','F','F',52,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
    1,0,2,0,68,172,0,0,16,177,2,0,4,0,16,0,'d','a','t','a',16,0,0,0,
    16,39,240,216,16,39,240,216,16,39,240,216,16,39,240,216
};
bool32 fileExists(char *name,udword flags)
{
    FILE *f;
    if(!strcmp(name,"fixture")) return TRUE;
    f=fopen(name,"rb");if(!f)return FALSE;fclose(f);return TRUE;
}
sdword fileSizeGet(char *name,udword flags)
{
    FILE *f;long length;
    if(!strcmp(name,"fixture")) return sizeof(wav);
    f=fopen(name,"rb");if(!f)return -1;
    fseek(f,0,SEEK_END);length=ftell(f);fclose(f);
    return length>0&&length<128*1024*1024?(sdword)length:-1;
}
sdword fileLoadAlloc(char *name,void **out,udword flags)
{
    int length=fileSizeGet(name,flags);FILE *f;
    *out=NULL;if(length<=0)return -1;
    *out=malloc(length);if(!*out)return -1;
    if(!strcmp(name,"fixture")){memcpy(*out,wav,length);return length;}
    f=fopen(name,"rb");
    if(!f){free(*out);*out=NULL;return -1;}
    if(fread(*out,1,length,f)!=(size_t)length){fclose(f);free(*out);*out=NULL;return -1;}
    fclose(f);return length;
}
#define CHECK(x) do {if(!(x)){printf("failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    short output[8]={0};int track,count=0;
    CHECK(rmPcmStreamPlay(256,"fixture",1,HW_AUDIO_MUSIC));
    CHECK(rmPcmStreamQuery(256,0)==0 && rmPcmStreamQuery(256,2)==255);
    CHECK(rmPcmStreamQuery(512,0)==-1); /* same channel, different generation */
    rmPcmStreamMix(output,sizeof(output));
    CHECK(output[0]==10000 && output[1]==-10000 && rmPcmStreamQuery(256,0)==1);
    CHECK(rmPcmStreamPlay(256,"fixture",1,HW_AUDIO_MUSIC));
    busGain=0;memset(output,0,sizeof(output));rmPcmStreamMix(output,4);
    CHECK(!output[0] && !output[1] && rmPcmStreamQuery(256,0)==0);
    CHECK(rmPcmStreamVolume(256,SOUND_VOL_AUTOSTOP,0));
    CHECK(rmPcmStreamQuery(256,0)==1);
    rmPcmStreamRelease(256);CHECK(rmPcmStreamQuery(256,0)==-1);
    CHECK(!rmPcmStreamPlay(-1,"fixture",1,HW_AUDIO_MUSIC));
    CHECK(!rmPcmStreamPlay(255,"fixture",1,HW_AUDIO_MUSIC));
    CHECK(!rmPcmStreamPlay(256,"missing-test-asset",1,HW_AUDIO_MUSIC));
    {
        int clip,voice,oldVoice,voices[64],i;
        rmPcmStreamReset();busGain=1;
        clip=rmPcmEffectPrecache("fixture");CHECK(clip>0);
        CHECK(rmPcmEffectPrecache("fixture")==clip);
        CHECK(!rmPcmEffectPrecache("missing-test-asset"));
        CHECK(!rmPcmEffectPlay(0,1,0));
        voice=rmPcmEffectPlay(clip,1,-1);CHECK(voice>0);oldVoice=voice;
        CHECK(rmPcmEffectActive(voice));CHECK(!rmPcmEffectActive(0));
        memset(output,0,sizeof(output));rmPcmStreamMix(output,4);
        CHECK(output[0]==10000 && output[1]==0);
        CHECK(rmPcmEffectUpdate(voice,.5f,1));
        memset(output,0,sizeof(output));rmPcmStreamMix(output,4);
        CHECK(output[0]==0 && output[1]==-5000);
        rmPcmEffectStop(voice);CHECK(!rmPcmEffectUpdate(voice,1,0));
        CHECK(!rmPcmEffectActive(voice));
        for(i=0;i<64;++i){voices[i]=rmPcmEffectPlay(clip,1,0);CHECK(voices[i]>0);}
        CHECK(!rmPcmEffectPlay(clip,1,0));
        memset(output,0,sizeof(output));rmPcmStreamMix(output,4);
        CHECK(output[0]==32767 && output[1]==-32768);
        busGain=0;memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));
        CHECK(!output[0] && !output[1]);CHECK(!rmPcmEffectUpdate(voices[0],1,0));
        rmPcmStreamReset();CHECK(!rmPcmEffectPlay(clip,1,0));
        CHECK(!rmPcmEffectUpdate(oldVoice,1,0));
        CHECK(rmPcmEffectPrecache("fixture")!=clip);
        rmPcmStreamReset();busGain=1;
    }
    if(argc>2 && !strcmp(argv[1],"--effect")){
        int clip=rmPcmEffectPrecache(argv[2]),voice;
        CHECK(clip>0);voice=rmPcmEffectPlay(clip,1,0);CHECK(voice>0);
        memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));
        rmPcmEffectStop(voice);CHECK(!rmPcmEffectUpdate(voice,1,0));++count;
    }else if(argc>1)for(track=0;track<48;++track){
        const char *relative=rmMusicPath(track);char path[2048];
        if(!relative)continue;
        snprintf(path,sizeof(path),"%s/%s",argv[1],relative);
        CHECK(rmPcmStreamPlay(256,path,1,HW_AUDIO_MUSIC));
        CHECK(rmPcmStreamQuery(256,0)==0);++count;
    }
    rmPcmStreamReset();CHECK(rmPcmStreamQuery(256,0)==-1);
    printf("PCM stream lifecycle/conversion passed; %d real RM tracks checked\n",count);
    return 0;
}
