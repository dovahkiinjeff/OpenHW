#include "OnlineClient.h"
#include "MatchRules.h"
#include "Globals.h"
#include "Universe.h"
#include "CommandNetwork.h"
#include "ScenPick.h"
#include "MultiplayerGame.h"
#include "HorseRace.h"
#include "utility.h"
#include "mouse.h"
#include "NetCheck.h"
#include "Debug.h"
#include <stdio.h>
#include <string.h>
static OHLobby lobby;
static int hasLobby,loading,wasConnected;
static char message[513];
int hwOnlineLobbySnapshot(OHLobby *snapshot){if(!hasLobby)return 0;*snapshot=lobby;return 1;}
void hwOnlineLobbyStatus(char *text,size_t capacity)
{if(message[0])snprintf(text,capacity,"%s",message);else hwOnlineStatus(text,capacity);}
void hwOnlineGameLoaded(void)
{
    if(hwMatchActive() && loading){
        uint64_t signature=hwMatchWorldSignature();loading=0;
        dbgMessagef("[Online] Local loading finished; notifying dedicated server (signature %016llx)",(unsigned long long)signature);
        hwOnlineSend(OH_LOADED,&signature,sizeof(signature));
    }
}
void hwOnlineGameEnded(void)
{
    if(!hwMatchActive())return;
    hwOnlineSend(OH_LEAVE,NULL,0);hwMatchClear();hasLobby=loading=0;
}
static void stopMatch(const char *reason)
{
    snprintf(message,sizeof(message),"%s",reason);
    if(hwMatchActive()) {
        hwOnlineGameEnded();
        if(gameIsRunning || startingGame)utyGameQuitToMain(NULL,NULL);
    }
    hasLobby=0;
}
void hwOnlineGamePump(void)
{
    unsigned type;unsigned char bytes[OH_MAX_MESSAGE];size_t length;
    int connected=hwOnlineConnected();
#if SYNC_CHECK
    if(hwMatchActive() && gameIsRunning && (randSyncErr || univSyncErr || blobSyncErr || pktSyncErr)) {
        stopMatch("Simulation mismatch detected; match stopped to protect game state");return;
    }
#endif
    if(wasConnected && !connected && hwMatchActive())stopMatch("Connection to gameplay server lost");
    wasConnected=connected;
    while((length=sizeof(bytes)),hwOnlineReceive(&type,bytes,&length)) {
        if(type==OH_WELCOME){message[0]=0;hasLobby=0;}
        else if(type==OH_PAUSED && hwMatchActive() && length==1 && bytes[0]<=1)universePause=bytes[0];
        else if(type==OH_ERROR || type==OH_STOP) {
            size_t size=length<sizeof(message)-1?length:sizeof(message)-1;
            memcpy(message,bytes,size);message[size]=0;
            if(type==OH_STOP) { char reason[513];memcpy(reason,message,sizeof(reason));stopMatch(reason); }
        }
        else if(type==OH_LOBBY && length==sizeof(OHLobby)) {
            OHLobby next;memcpy(&next,bytes,sizeof(next));
            if(next.ownSlot>=8 || next.host>1 || next.state>3 || !memchr(next.code,0,8))continue;
            lobby=next;hasLobby=1;
            if(next.state==0)message[0]=0;
        }
        else if(type==OH_LOAD && length==sizeof(OHLoad) && !gameIsRunning && !hwMatchActive()) {
            OHLoad load;unsigned i;memcpy(&load,bytes,sizeof(load));
            for(i=0;i<(unsigned)spScenarioListLength;i++)if(!strcmp(load.config.map,spScenarios[i].fileSpec))break;
            if(i==(unsigned)spScenarioListLength || !hwMatchInstall(&load)) {
                hwOnlineSend(OH_LEAVE,NULL,0);snprintf(message,sizeof(message),"Server map or match setup is incompatible");continue;
            }
            hwOnlineCloseMenu();mgScreensDisappear();
            hwMatchPrepareGame();spCurrentSelected=(sdword)i;
            universeSwitchToPlayer((uword)load.ownSlot);
            GameCreator=FALSE;captainIndex=0;receiveSyncPacketsFrom=0;
            sigsPressedStartGame=TRUE;startingGame=TRUE;startingGameState=PROCESS_TEXTURES;
            HaveSentNonCaptainReadyPacket=TRUE;multiPlayerGameUnderWay=FALSE;
            ResetQueue(&ProcessCmdPktQ);ResetQueue(&ProcessSyncPktQ);CommandNetworkReset();
            horseRaceInit();loading=1;mouseCursorShow();message[0]=0;
        }
        else if(type==OH_SYNC && hwMatchActive() && length>=sizeof(HWPacketHeader) && length<=OH_MAX_SYNC) {
            HWPacketHeader header;memcpy(&header,bytes,sizeof(header));
            if(header.type!=PACKETTYPE_SYNC || header.from!=0) {stopMatch("Invalid server simulation frame");continue;}
            ReceivedPacketCB(bytes,(udword)length);
        }
    }
}
