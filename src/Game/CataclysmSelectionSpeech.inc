/* Original Cata selection event is 0, not HW1's 78. Hotkey row is 77.
   Kuun-Lan is selection row 0, ship variable 18, not HW1's row 198:
   owned Cataclysm row 198 is the opening "war for Hiigara" narration.
   Source: 00522560 and the owned SectSentence/SectPhrase/SectSpeech banks. */
#include "CataclysmSoundIdentity.inc"
static unsigned cataSelectionRandom(void){return ranRandom(RANDOM_SOUND_BOTH_THREADS);}
static sdword cataSelectionSpeech(void *object,sdword event,sdword variable,sdword player)
{
    Ship *ship=NULL;CataSpeechPhrase phrase;char error[256];
    sdword row,actor,channel;unsigned i;
    if(event!=COMM_Selection && event!=COMM_AssGrp_Select && event!=COMM_F_MoShip_Selected)return SOUND_ERR;
    if(!enableSpeech||soundpaused||!bCommandsOn||nisIsRunning)return SOUND_ERR;
    if(object){
        if(((SpaceObj *)object)->objtype!=OBJ_ShipType)return SOUND_ERR;
        ship=(Ship *)object;
        if(!ship->playerowner || ship->playerowner!=universe.curPlayerPtr ||
           !ship->staticinfo || (ship->flags&(SOF_Dead|SOF_Disabled|SOF_Hide)))return SOUND_ERR;
    }else if(player!=SOUND_EVENT_DEFAULT && player!=universe.curPlayerIndex)return SOUND_ERR;
    if(!ship && event==COMM_F_MoShip_Selected && universe.curPlayerPtr)
        ship=universe.curPlayerPtr->PlayerMothership;
    if(!ship || ship->playerowner!=universe.curPlayerPtr || !ship->staticinfo ||
       (ship->flags&(SOF_Dead|SOF_Disabled|SOF_Hide)))return SOUND_ERR;
    actor=SPEECH_ACTOR_PILOT;channel=0;
    row=event==COMM_AssGrp_Select?77:0;
    variable=row==0?cataSoundShipObject(ship):0;
    if(variable<0)return SOUND_ERR;
    if(!(actorFlagsEnabled&actor))return SOUND_ERR;
    if(soundstreamnumqueued(streamhandle[channel])>0 || !soundstreamover(streamhandle[channel]))return SOUND_ERR;
    // A retail row without a clip is silent; never substitute another ship,
    // a HW1 sentence, or RM voice/audio assets.
    if(!hwCataSpeechPhraseRandom((unsigned)row,variable,cataSelectionRandom,&phrase,error,sizeof(error)))return SOUND_ERR;
    cataVoiceOpen(phrase.checksum);
    for(i=0;i<phrase.count;i++){
        if(soundstreamqueuePatch(streamhandle[channel],cataVoiceFile,phrase.offsets[i],SOUND_FLAGS_QUEUESTREAM,
            (sword)((real32)SOUND_VOL_MAX*volSpeech),SOUND_PAN_CENTER,SOUND_MONO,(sword)phrase.bitrate,
            NULL,NULL,NULL,NULL,0,0.0f,0.0f,0,event,i+1<phrase.count)<0)return SOUND_ERR;
    }
    lastshiptospeak=ship?ship:universe.curPlayerPtr?universe.curPlayerPtr->PlayerMothership:NULL;
    return SOUND_OK;
}
