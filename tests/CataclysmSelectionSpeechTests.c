#include "Universe.h"
#include "File.h"
#include "SpeechEvent.h"
#include "SoundEvent.h"
#include "SoundEventPrivate.h"
#include "soundlow.h"
#include "Randy.h"
#include "../src/Modern/CataclysmData.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
Universe universe;
bool32 enableSpeech=TRUE,soundpaused=FALSE,bCommandsOn=TRUE,nisIsRunning=FALSE;
real32 volSpeech=1.0f;
sdword actorFlagsEnabled=SPEECH_ACTOR_PILOT|SPEECH_ACTOR_FLEET_COMMAND;
sdword streamhandle[4]={10,11,12,13};
static filehandle cataVoiceFile=7;
Ship *lastshiptospeak;
static int queued,busy[2],row,variable,checks,channel;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void cataVoiceOpen(unsigned checksum){CHECK(checksum==123);}
udword ranRandomFn(sdword stream){CHECK(stream==RANDOM_SOUND_BOTH_THREADS);return 0;}
int hwCataSpeechPhraseRandom(unsigned event,int var,CataSpeechRandom random,CataSpeechPhrase *phrase,char *error,size_t capacity)
{
    (void)random;(void)error;(void)capacity;row=(int)event;variable=var;
    if(var>=61)return 0;
    memset(phrase,0,sizeof(*phrase));phrase->checksum=123;phrase->bitrate=512;phrase->count=1;phrase->offsets[0]=456;return 1;
}
sdword soundstreamnumqueued(sdword handle){return busy[handle==10?0:1];}
sdword soundstreamover(sdword handle){return !busy[handle==10?0:1];}
sdword soundstreamqueuePatch(sdword handle,smemsize file,smemsize offset,udword flags,sword vol,sword pan,sword channels,sword bitrate,EFFECT *fx,STREAMEQ *eq,STREAMDELAY *delay,void *mix,sdword level,real32 silence,real32 fade,sdword actor,sdword event,bool32 wait)
{
    (void)flags;(void)vol;(void)pan;(void)channels;(void)fx;(void)eq;(void)delay;(void)mix;(void)level;(void)silence;(void)fade;(void)actor;(void)event;(void)wait;
    CHECK(file==7&&offset==456&&bitrate==512);++queued;channel=handle;return 0;
}
#include "../src/Game/CataclysmSelectionSpeech.inc"
int main(void)
{
    Ship ship={0};ShipStaticInfo info={0};
    universe.curPlayerPtr=&universe.players[0];universe.curPlayerIndex=0;
    ship.objtype=OBJ_ShipType;ship.shiptype=CataAcolyte;ship.shiprace=R1;ship.playerowner=universe.curPlayerPtr;ship.staticinfo=&info;info.shipclass=CLASS_Fighter;
    CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_OK);CHECK(row==0&&variable==1&&channel==10&&lastshiptospeak==&ship);
    ship.shiptype=CataWorker;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_OK);CHECK(row==0&&variable==14);
    CHECK(cataSelectionSpeech(&ship,COMM_AssGrp_Select,7,-1)==SOUND_OK);CHECK(row==77&&variable==0);
    info.shipclass=CLASS_Mothership;ship.shiptype=CataMothership;
    CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_OK);CHECK(row==0&&variable==18&&channel==10);
    busy[0]=1;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);busy[0]=0;
    universe.curPlayerPtr->PlayerMothership=&ship;
    CHECK(cataSelectionSpeech(NULL,COMM_F_MoShip_Selected,0,0)==SOUND_OK);
    CHECK(lastshiptospeak==&ship&&row==0&&variable==18&&channel==10);
    ship.flags=SOF_Dead;CHECK(cataSelectionSpeech(NULL,COMM_F_MoShip_Selected,0,0)==SOUND_ERR);ship.flags=0;
    info.shipclass=CLASS_Fighter;ship.shiptype=CataScout;
    bCommandsOn=FALSE;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);bCommandsOn=TRUE;
    actorFlagsEnabled=0;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);actorFlagsEnabled=SPEECH_ACTOR_PILOT;
    busy[0]=1;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);busy[0]=0;
    ship.playerowner=&universe.players[1];CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);ship.playerowner=universe.curPlayerPtr;
    ship.flags=SOF_Disabled;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);ship.flags=0;
    ship.shiptype=ProximitySensor;CHECK(cataSelectionSpeech(&ship,COMM_Selection,0,-1)==SOUND_ERR);
    CHECK(cataSelectionSpeech(&ship,12345,0,-1)==SOUND_ERR);CHECK(queued==5);
    printf("Cataclysm selection speech: %d checks passed.\n",checks);return 0;
}
