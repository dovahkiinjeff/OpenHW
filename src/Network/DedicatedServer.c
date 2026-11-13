/* Dedicated simulation entry. It deliberately runs before window, renderer,
   upscaler, movie, splash, single-instance and local-settings initialization. */
#include "DedicatedServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "utility.h"
#include "mainswitches.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "MultiplayerGame.h"
#include "TitanInterfaceC.h"
#include "ScenPick.h"
#include "Globals.h"
#include "Options.h"
#include "Randy.h"
#include "NetCheck.h"
#include "Debug.h"
#include "File.h"
#include "main.h"
#include "SoundEvent.h"
#include "OpenHWArchive.h"
#include "MatchRules.h"
#include "CommandValidation.h"
#include "CommandNetwork.h"
#include "Memory.h"
#include "AIPlayer.h"
#include "ShipPilot.h"
#include "HorseRace.h"
#include "GameChat.h"
#include "mainrgn.h"
#include <io.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI dedicatedException(EXCEPTION_POINTERS *exception)
{
    HANDLE process=GetCurrentProcess(), thread=GetCurrentThread();
    CONTEXT context=*exception->ContextRecord;
    STACKFRAME64 frame={0};
    unsigned i;
    SymInitialize(process,NULL,TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME);
    frame.AddrPC.Offset=context.Rip; frame.AddrPC.Mode=AddrModeFlat;
    frame.AddrFrame.Offset=context.Rbp; frame.AddrFrame.Mode=AddrModeFlat;
    frame.AddrStack.Offset=context.Rsp; frame.AddrStack.Mode=AddrModeFlat;
    fprintf(stderr,"SERVER exception %08lx at %llx image=%llx access=%llu address=%llx\n",exception->ExceptionRecord->ExceptionCode,
        (unsigned long long)context.Rip,(unsigned long long)(uintptr_t)GetModuleHandleW(NULL),
        (unsigned long long)exception->ExceptionRecord->ExceptionInformation[0],
        (unsigned long long)exception->ExceptionRecord->ExceptionInformation[1]);
    for(i=0;i<48 && frame.AddrPC.Offset;i++) {
        char storage[sizeof(SYMBOL_INFO)+1024];
        SYMBOL_INFO *symbol=(SYMBOL_INFO*)storage;
        IMAGEHLP_LINE64 line={0}; DWORD lineOffset=0; DWORD64 offset=0;
        memset(storage,0,sizeof(storage));symbol->SizeOfStruct=sizeof(*symbol);symbol->MaxNameLen=1023;
        line.SizeOfStruct=sizeof(line);
        if(SymFromAddr(process,frame.AddrPC.Offset,&offset,symbol)) {
            fprintf(stderr,"  %s+%llu",symbol->Name,(unsigned long long)offset);
            if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&lineOffset,&line)) fprintf(stderr," %s:%lu",line.FileName,line.LineNumber);
            fprintf(stderr,"\n");
        } else fprintf(stderr,"  address %llx\n",(unsigned long long)frame.AddrPC.Offset);
        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL))break;
    }
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int hwDedicatedMode = 0;
static FILE *protocolOutput;
static int readExact(void *data,size_t size)
{
    size_t n=0;
    while(n<size){size_t got=fread((char*)data+n,1,size-n,stdin);if(!got)return 0;n+=got;}
    return 1;
}
static int receiveMessage(OHHeader *header,unsigned char *payload)
{
    if(!readExact(header,sizeof(*header)))return 0;
    if(memcmp(header->magic,"OHMP",4)||header->version!=OH_PROTOCOL||header->length>OH_MAX_MESSAGE)return 0;
    return readExact(payload,header->length);
}
static int sendMessage(unsigned type,const void *payload,unsigned size)
{
    OHHeader header={{'O','H','M','P'},OH_PROTOCOL,(uint16_t)type,size};
    return fwrite(&header,1,sizeof(header),protocolOutput)==sizeof(header) &&
        (!size||fwrite(payload,1,size,protocolOutput)==size) && fflush(protocolOutput)==0;
}
static int workerRun(void)
{
    OHHeader request;
    unsigned char input[OH_MAX_MESSAGE],sync[OH_MAX_SYNC];
    uint32_t expected=0;
    struct { uint32_t ships,players,ai[8]; uint64_t signature; } ready={0};
    unsigned i;
    ready.ships=(uint32_t)universe.ShipList.num;ready.players=universe.numPlayers;
    ready.signature=hwMatchWorldSignature();
    for(i=0;i<universe.numPlayers;i++)ready.ai[i]=universe.players[i].aiPlayer?
        (uint32_t)universe.players[i].aiPlayer->aiplayerDifficultyLevel+1:0;
    if(!sendMessage(OH_WORKER_READY,&ready,sizeof(ready)))return 6;
    while(receiveMessage(&request,input)) {
        size_t at=6,used=sizeof(HWPacketHeader);uint32_t frame;uint16_t count,j,commands=0;
        HWPacketHeader header={0};
        if(request.type==OH_STOP && !request.length)return 0;
        if(request.type==OH_WORKER_INSPECT && !request.length) {
            /* Administrator-only pipe inspection for replay tests. This is
               deliberately not routed by the public gateway. */
            uint32_t facts[10]={0};size_t used=sizeof(facts);Node *node;
            facts[0]=(uint32_t)universe.ShipList.num;facts[1]=(uint32_t)universe.ResourceList.num;
            for(i=0;i<universe.numPlayers;i++)facts[2+i]=universe.players[i].Allies;
            memcpy(sync,facts,sizeof(facts));
            for(node=universe.ShipList.head;node;node=node->next) {
                Ship *ship=(Ship*)listGetStructOfNode(node);
                uint16_t id[4]={ship->shipID.shipNumber,(uint16_t)ship->shiptype,(uint16_t)ship->shiprace,ship->playerowner->playerIndex};
                uint16_t control[2]={0xffff,(uint16_t)shipPilotControls(ship)};
                CommandToDo *formation=ship->formationcommand;
                if(formation && formation->selection && formation->selection->numShips>0)
                    control[0]=formation->selection->ShipPtr[0]->shipID.shipNumber;
                if(used+sizeof(id)+sizeof(vector)+sizeof(control)>sizeof(sync))return 8;
                memcpy(sync+used,id,sizeof(id));used+=sizeof(id);
                memcpy(sync+used,&ship->posinfo.position,sizeof(vector));used+=sizeof(vector);
                memcpy(sync+used,control,sizeof(control));used+=sizeof(control);
            }
            if(!sendMessage(OH_WORKER_INSPECT,sync,(unsigned)used))return 6;
            continue;
        }
        if(hwDedicatedMode==2) {
            if(request.type!=OH_WORKER_SYNC || request.length<sizeof(HWPacketHeader))return 7;
            memcpy(&header,input,sizeof(header));
            if(header.type!=PACKETTYPE_SYNC || header.frame!=expected++)return 7;
            clProcessSyncPacket(&universe.mainCommandLayer,input,request.length);netCheck();
            if(univUpdate(UNIVERSE_UPDATE_PERIOD)||univUpdate(UNIVERSE_UPDATE_PERIOD))return 0;
#if SYNC_CHECK
            if(randSyncErr || univSyncErr || blobSyncErr || pktSyncErr) {
                fprintf(stderr,"SERVER replica mismatch frame=%u random=%u universe=%u stats=%u packet=%u\n",
                    header.frame,randSyncErr,univSyncErr,blobSyncErr,pktSyncErr);return 9;
            }
#endif
            if(!sendMessage(OH_WORKER_SYNC,input,request.length))return 6;
            continue;
        }
        if(request.type!=OH_WORKER_STEP || request.length<6)return 7;
        memcpy(&frame,input,4);memcpy(&count,input+4,2);
        if(frame!=expected || count>256)return 7;
        for(j=0;j<count;j++) {
            uint8_t player;uint32_t length;
            if(at+5>request.length)return 7;
            player=input[at++];memcpy(&length,input+at,4);at+=4;
            if(length>request.length-at || player>=hwMatchConfiguration()->humans)return 7;
            if(hwValidatePlayerCommand(input+at,length,player) &&
               used+length-sizeof(HWPacketHeader)<=sizeof(sync)) {
                memcpy(sync+used,input+at+sizeof(HWPacketHeader),length-sizeof(HWPacketHeader));
                used+=length-sizeof(HWPacketHeader);commands++;
            }
            at+=length;
        }
        if(at!=request.length)return 7;
        /* AI orders produced by the previous tick remain private to the worker.
           They enter the same canonical command frame as authenticated orders. */
        LockQueue(&ProcessCmdPktQ);
        while(queueNumberEntries(ProcessCmdPktQ)) {
            unsigned char *packet;udword length=HWDequeue(&ProcessCmdPktQ,&packet);
            if(length<sizeof(HWPacketHeader)+2 || used+length-sizeof(HWPacketHeader)>sizeof(sync)) {
                UnLockQueue(&ProcessCmdPktQ);return 8;
            }
            memcpy(sync+used,packet+sizeof(HWPacketHeader),length-sizeof(HWPacketHeader));
            used+=length-sizeof(HWPacketHeader);commands++;
        }
        UnLockQueue(&ProcessCmdPktQ);
        header.type=PACKETTYPE_SYNC;header.frame=expected++;header.from=0;header.numberOfCommands=commands;
#if SYNC_CHECK
        netcheckFillInChecksum(&header);
#endif
        memcpy(sync,&header,sizeof(header));
        clProcessSyncPacket(&universe.mainCommandLayer,sync,(udword)used);
        netCheck();
        if(univUpdate(UNIVERSE_UPDATE_PERIOD)||univUpdate(UNIVERSE_UPDATE_PERIOD)) {
            sendMessage(OH_STOP,"Match ended",11);return 0;
        }
        if(!sendMessage(OH_WORKER_SYNC,sync,(unsigned)used))return 6;
    }
    return 0;
}

int hwDedicatedRequested(int argc, char **argv)
{
    int i;
    for (i=1;i<argc;i++)
        if (!strcmp(argv[i],"--dedicated-probe") || !strcmp(argv[i],"--dedicated-worker") || !strcmp(argv[i],"--dedicated-replica")) return 1;
    return 0;
}

int hwDedicatedMain(int argc, char **argv)
{
    char *error;
    unsigned i, ticks=64, map=0;
    int worker=0,listMaps=0,checkLoadingGate=0,checkChatOverlay=0;
    OHLoad load;
    hwDedicatedMode=1;
    setvbuf(stderr,NULL,_IONBF,0);
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(dedicatedException);
#endif
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_TIMER)!=0) { fprintf(stderr,"SERVER SDL: %s\n",SDL_GetError());return 2; }
    for (i=1;i<(unsigned)argc;i++) {
        if (!strcmp(argv[i],"--ticks") && i+1<(unsigned)argc) ticks=(unsigned)strtoul(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--map-index") && i+1<(unsigned)argc) map=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--dedicated-worker"))worker=1;
        else if(!strcmp(argv[i],"--dedicated-replica")){worker=1;hwDedicatedMode=2;}
        else if(!strcmp(argv[i],"--list-maps"))listMaps=1;
        else if(!strcmp(argv[i],"--check-loading-gate"))checkLoadingGate=1;
        else if(!strcmp(argv[i],"--check-chat-overlay"))checkChatOverlay=1;
    }
    if(worker) {
        OHHeader init;unsigned char payload[OH_MAX_MESSAGE];int fd=_dup(_fileno(stdout));
        _setmode(fd,_O_BINARY);protocolOutput=_fdopen(fd,"wb");
        _dup2(_fileno(stderr),_fileno(stdout));_setmode(_fileno(stdin),_O_BINARY);
        if(!protocolOutput || !receiveMessage(&init,payload) || init.type!=OH_WORKER_INIT || init.length!=sizeof(load))return 7;
        memcpy(&load,payload,sizeof(load));
        if(!hwMatchInstall(&load))return 7;
    }
    MemoryHeapSize=256u*1024u*1024u;
    useDSound=FALSE;
    enableSpeech=FALSE;
    enableSFX=FALSE;
    enableAVI=FALSE;
    determCompPlayer=TRUE;
    {
        char archiveError[512]={0};
        if(!openhwArchiveMount(L"OpenHW-HW1.obig",archiveError,sizeof(archiveError))) {
            fprintf(stderr,"SERVER archive: %s\n",archiveError);return 3;
        }
    }
    fprintf(stderr,"SERVER preinit\n");fflush(stderr);
    error=utyGameSystemsPreInit();
    if(error) { fprintf(stderr,"SERVER preinit failed: %s\n",error);return 3; }
    fprintf(stderr,"SERVER CPU systems\n");fflush(stderr);
    error=utyDedicatedSystemsInit();
    if(error) { fprintf(stderr,"SERVER systems failed: %s\n",error);return 4; }
    if(listMaps) {
        for(i=0;i<(unsigned)spScenarioListLength;i++)fprintf(stdout,"MAP\t%s\t%u\t%u\n",spScenarios[i].fileSpec,
            spScenarios[i].minplayers,spScenarios[i].maxplayers);
        return 0;
    }
    if(worker) {
        for(map=0;map<(unsigned)spScenarioListLength;map++)if(!strcmp(load.config.map,spScenarios[map].fileSpec))break;
        if(map==(unsigned)spScenarioListLength || load.players<spScenarios[map].minplayers || load.players>spScenarios[map].maxplayers)return 5;
    }
    if(map >= (unsigned)spScenarioListLength) { fprintf(stderr,"SERVER invalid map index\n");return 5; }
    fprintf(stderr,"SERVER map %u: %s\n",map,spScenarios[map].fileSpec);fflush(stderr);
    memset(&tpGameCreated,0,sizeof(tpGameCreated));
    spCurrentSelected=(sdword)map;
    tpGameCreated.numPlayers=1;
    tpGameCreated.numComputers=1;
    tpGameCreated.startingFleet=MG_Small;
    tpGameCreated.startingResources=MG_Medium;
    tpGameCreated.aiplayerDifficultyLevel=50;
    tpGameCreated.flag=MG_HarvestinEnabled|MG_ResearchEnabled;
    strncpy(tpGameCreated.MapName,spScenarios[map].fileSpec,sizeof(tpGameCreated.MapName)-1);
    tpGameCreated.playerInfo[0].race=R1;
    tpGameCreated.playerInfo[1].race=R2;
    strcpy(tpGameCreated.playerInfo[0].PersonalName,"Server test");
    sigsPlayerIndex=0;sigsNumPlayers=1;numPlayers=2;
    whichRaceSelected=R1;
    GameCreator=TRUE;captainIndex=0;
    singlePlayerGame=FALSE;multiPlayerGame=TRUE;
    if(worker)hwMatchPrepareGame();
    if(checkLoadingGate) {
        if(!worker)return 7;
        /* Exercise the real frontend loading gate without opening a window.
           No legacy peer progress has arrived, as on the dedicated client. */
        hrRunning=TRUE;
        if(HorseRaceNext(0.5f) || !HorseRaceNext(1.0f))return 11;
        hrRunning=FALSE;
        fprintf(stderr,"SERVER dedicated local loading gate passed for %u humans\n",load.humans);
        return 0;
    }
    fprintf(stderr,"SERVER loading world\n");fflush(stderr);
    gameStart(NULL);
    fprintf(stderr,"SERVER world ready: %u ships, %u players\n",(unsigned)universe.ShipList.num,(unsigned)universe.numPlayers);fflush(stderr);
    if(checkChatOverlay) {
        extern regionhandle chatdrawregion;
        extern featom chatdrawatom;
        unsigned pass;
        /* Exercise the real multiplayer HUD dispatch with a high ASLR address,
           then its detach/reuse path. The callback exits before font rendering;
           no graphics window, device or rendering task is created. */
        mrRenderMainScreen=FALSE;
        for(pass=0;pass<2;pass++) {
            if(!gcRunning || !chatdrawregion || chatdrawregion->atom!=&chatdrawatom ||
               (featom*)chatdrawregion->userID!=&chatdrawatom ||
               chatdrawregion->drawFunction!=feUserRegionDraw ||
               sizeof(smemsize)<sizeof(void*) || (uintptr_t)&chatdrawatom<=UINT32_MAX) {
                fprintf(stderr,"SERVER chat overlay pointer contract failed on pass %u\n",pass);
                return 12;
            }
            chatdrawregion->drawFunction(chatdrawregion);
            gcShutdown();
            if(pass==0)gcStartup();
        }
        fprintf(stderr,"SERVER 64-bit multiplayer chat overlay draw and reuse passed\n");
        return 0;
    }
    if(worker)return workerRun();
    for(i=0;i<ticks;i++) {
        if(univUpdate(UNIVERSE_UPDATE_PERIOD)) break;
    }
    fprintf(stderr,"SERVER probe complete: %u ticks, %u ships\n",i,(unsigned)universe.ShipList.num);fflush(stderr);
    return 0;
}
