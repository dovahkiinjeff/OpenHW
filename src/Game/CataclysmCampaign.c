/* Campaign adaptations belong to OpenHW. The owned installation is read-only. */
#include "CataclysmCampaign.h"
#include "CataclysmKAS.h"
#include "CataclysmShips.h"
#include "CampaignMissionPaths.h"
#include "../Modern/CataclysmData.h"
#include "../Modern/CataclysmScriptBridge.h"
#include "Universe.h"
#include "SinglePlayer.h"
#include "TradeMgr.h"
#include "AIVar.h"
#include "Blobs.h"
#include "KASFunc.h"
#include "SoundEvent.h"
#include "Sensors.h"
#include "Camera.h"
#include "Timer.h"
#include "mainrgn.h"
#include "Attributes.h"
#include "FastMath.h"
#include "CommandWrap.h"
#include "Dock.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

extern Camera smCamera;
extern vector smCameraLookatPoint,smCameraLookVelocity;
static bool32 active(void){return singlePlayerGame && hwIsCataclysmMission(spGetCurrentMission());}
static MissionEnum checkpointMission;
static char checkpointTitle[160];
static void savedSet(const char *key,sdword value){AIVar *var=aivarFind((char *)key);if(!var)var=aivarCreate((char *)key);aivarValueSet(var,value);}
#include "CataclysmResearchIdentity.inc"
#include "CataclysmResearchStatics.inc"
#include "CataclysmUpgrades.inc"
#include "CataclysmCameraSnap.inc"

void cataCampaignInitialize(void)
{
    Player *player=&universe.players[0];sdword i;CataStartingFleet fleet;char error[256];
    soundEventSelectGameBanks();
    cataCampaignResearchStatics(player);
    player->researchinfo.HasTechnology=0;player->researchinfo.CanDoResearch=TRUE;
    for(i=0;i<NumTechnologies;i++)tmTechResearchable[i]=TM_TECH_IS_NOT_RESEARCHABLE;
    rmActivateFreeLab(player); /* The installed engineering hull provides M1 research. */
    if(!hwCataMission1StartingFleet(&fleet,error,sizeof(error)))cataScriptFault(error);
    for(i=0;i<(sdword)fleet.technologyCount;i++)cataCampaignResearchAcquire(fleet.technologies[i]);
    savedSet("CataPreserveLockout",FALSE);
    savedSet("ShipsHaveArrived",0);savedSet("RecentlyLoaded",0);
    savedSet("CataArrivalStarted",0);
    soundEventStopMusic(0.0f);
}
bool32 cataCampaignKeepHandoffVariable(const char *label)
{
    return !strcmp(label, "RenderMainScreen") ||
        !strncmp(label, "CUMShip", 7) || !strncmp(label, "CUMOwner", 8) ||
        !strncmp(label, "CUTShip", 7) ||
        !strncmp(label, "CU0_", 4) || !strcmp(label, "CataFighterDrive");
}

void cataCampaignInitializeArrival(void)
{
    soundEventSelectGameBanks();
    savedSet("CataPreserveLockout", FALSE);
    savedSet("RecentlyLoaded", FALSE);
    savedSet("ShipsHaveArrived", FALSE);
}

bool32 cataCampaignCanLoadMission(MissionEnum mission)
{
    char directory[SP_MISSION_PATH_CAPACITY], file[SP_MISSION_PATH_CAPACITY];
    char path[2 * SP_MISSION_PATH_CAPACITY];
    int size;
    if (!hwIsCataclysmMission(mission) ||
        !hwCampaignMissionPaths(mission, 0, directory, sizeof(directory), file, sizeof(file)))
        return FALSE;
    snprintf(path, sizeof(path), "%s%s", directory, file);
    if (!hwCataDataView(path, &size) || size <= 0) return FALSE;

    /* The mission file can be present even if required engine identities
       haven't been ported. Test these before replacing the active world. */
    if (mission == MISSION_CATA_2_OUTSKIRTS &&
        (StrToShipType("sMimic") == (ShipType)-1 || StrToShipType("sMCV") == (ShipType)-1))
        return FALSE;
    return TRUE;
}
void cataCampaignResearchAllow(const char *name)
{
    sdword slot=cataCampaignTechSlot(name);
    if(slot<0)cataScriptFault("Unported Cataclysm research unlock");
    if(!(universe.players[0].researchinfo.HasTechnology&TechToBit(slot)))
        tmTechResearchable[slot]=TM_TECH_IS_RESEARCHABLE;
}
void cataCampaignResearchAcquire(const char *name)
{
    sdword slot=cataCampaignTechSlot(name);
    if(slot<0)cataScriptFault("Unported Cataclysm acquired technology");
    universe.players[0].researchinfo.HasTechnology|=TechToBit(slot);
    tmTechResearchable[slot]=TM_TECH_IS_ALREADY_OWNED;
    if(slot==CATA_FIGHTER_DRIVE_TECH)savedSet("CataFighterDrive",1);
}
bool32 cataCampaignHasResearch(const char *name)
{
    sdword slot=cataCampaignTechSlot(name);
    return slot>=0 && (universe.players[0].researchinfo.HasTechnology&TechToBit(slot))!=0;
}
bool32 cataCampaignLinkAvailable(void)
{
    return !active() || (universe.curPlayerPtr &&
        (universe.curPlayerPtr->researchinfo.HasTechnology&TechToBit(CATA_LINK_TECH))!=0);
}
bool32 cataCampaignShipCanLink(Ship *ship)
{
    return !active() || cataCampaignShipHasUpgrade(ship,CATA_UPGRADE_LINK);
}
void cataCampaignApplyUpgrades(GrowSelection *ships)
{
    sdword i;if(!ships)return;
    for(i=0;i<ships->selection->numShips;i++){
        Ship *ship=ships->selection->ShipPtr[i];
        if(!ship || !ship->staticinfo)cataScriptFault("Invalid campaign upgrade selection");
        cataUpgradeApplyShip(ship);
    }
}
sdword cataCampaignShipsInSensorRange(GrowSelection *ships)
{
    sdword i,count=0;if(!ships)return 0;
    for(i=0;i<ships->selection->numShips;i++){
        Ship *ship=ships->selection->ShipPtr[i];
        Node *node;
        if(ship->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace|SOF_Cloaked))continue;
        for(node=universe.ShipList.head;node;node=node->next){
            Ship *sensor=(Ship *)listGetStructOfNode(node);vector delta;real32 radius;
            if(sensor->playerowner!=&universe.players[0] || sensor->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace))continue;
            radius=cataShipSensorRadius(sensor);vecSub(delta,ship->posinfo.position,sensor->posinfo.position);
            if(radius>0 && vecMagnitudeSquared(delta)<=radius*radius){++count;break;}
        }
    }
    return count;
}
void cataCampaignSpeech(sdword event,sdword variable){soundEventCataclysmSpeech(event,variable);}
sdword cataCampaignSpeechEnded(void){return soundEventCataclysmSpeechEnded();}
void cataCampaignRequestCheckpoint(sdword missionNumber,const char *title)
{
    MissionEnum mission = spGetCurrentMission();
    if (!active() || hwCataclysmMissionNumber(mission) != missionNumber || !title || !*title)
        return;
    snprintf(checkpointTitle, sizeof(checkpointTitle), "%s", title);
    checkpointMission = mission;
}

void cataCampaignClearCheckpoint(void)
{
    checkpointMission = MISSION_ENUM_NOT_INITIALISED;
}

/* Retail queues the checkpoint in KAS and writes it at the end of the
   update, once AI, ship deletion and arrival have settled for this tick. */
void cataCampaignWriteCheckpoint(void)
{
    char levelName[sizeof(checkpointTitle) + 8];
    MissionEnum mission = checkpointMission;
    if (!mission) return;
    cataCampaignClearCheckpoint();
    if (!active() || spGetCurrentMission() != mission) return;
    snprintf(levelName, sizeof(levelName), "%02d - %s",
             hwCataclysmMissionNumber(mission), checkpointTitle);
    kasSaveLevelCheckpoint(levelName, checkpointTitle[0] != '~');
}
void cataCampaignAfterLoad(void)
{
    AIVar *arrived;
    Node *node;
    char timerKey[32];
    if (!active()) return;
    cataShipSyncComponents();
    savedSet("RecentlyLoaded",1);
    if(spGetCurrentMission()!=MISSION_CATA_1_HIIGARA)return;
    arrived=aivarFind("ShipsHaveArrived");
    if (!arrived || !aivarValueGet(arrived)) return;

    /* Older builds hid allied ships and never registered the briefing timer.
       ShipList contains ships in space. Docked ships keep their hidden state. */
    for (node=universe.ShipList.head; node; node=node->next)
    {
        Ship *ship=(Ship *)listGetStructOfNode(node);
        if(cataShipEmbeddedInNativeHull(ship))
            bitSet(ship->flags,SOF_Hide|SOF_DontDrawTrails);
        else if (ship->specialFlags2 & SPECIAL_2_CataScriptControlled)
            bitClear(ship->flags,SOF_Hide | SOF_HideSave);
    }
    universe.collUpdateAllBlobs=TRUE;
    cataScriptScopedKey(cataScriptHash("FleetIntel"),CATA_MISSION1_BRIEFING_TIMER,timerKey,sizeof(timerKey));
    timTimerCreate(timerKey);
    cataScriptScopedKey(0,CATA_MISSION1_TARGET_REFRESH_TIMER,timerKey,sizeof(timerKey));
    timTimerCreate(timerKey);
}
void cataCampaignRestoreFleet(void)
{
    /* A campaign jump already put the existing fleet in the hyperspace
       queue. Let the level transition advance it using the new start point. */
    if (spGetCurrentMission() == MISSION_CATA_2_OUTSKIRTS) return;
    /* Our starting fleet is already instantiated before script init; reveal
       it at the original intro's arrival transition, retaining parade poses. */
    Node *node;GrowSelection arrival;Ship *host=universe.players[0].PlayerMothership;
    hvector destination;vector heading;
    growSelectInit(&arrival);
    if(host && host->shiptype==CataMothership){
        destination=host->shipSinglePlayerGameInfo->midlevelHyperspaceTo;
        /* Saves from builds before the authored arrival destination existed. */
        if(destination.x==0 && destination.y==0 && destination.z==0){
            vecMakeHVecFromVec(destination,host->posinfo.position);
            matGetVectFromMatrixCol3(heading,host->rotinfo.coordsys);
            destination.w=RAD_TO_DEG((real32)atan2(heading.y,heading.x));
        }
        growSelectAddShip(&arrival,host);
    }
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *ship=(Ship *)listGetStructOfNode(node);
        if(cataShipEmbeddedInNativeHull(ship))
            bitSet(ship->flags,SOF_Hide|SOF_DontDrawTrails);
        else if(ship->playerowner==&universe.players[0] &&
           !(ship->specialFlags2&SPECIAL_2_CataScriptControlled))bitClear(ship->flags,SOF_Hide);
        if(host && ship!=host && ship->playerowner==host->playerowner &&
           IS_CATA_SHIP(ship->shiptype) && !cataShipIsAttachedComponent(ship))
            growSelectAddShip(&arrival,ship);
    }
    if(arrival.selection->numShips && (!aivarFind("CataArrivalStarted") || !aivarValueGet(aivarFind("CataArrivalStarted")))){
        sdword i;
        /* Stage the already assembled fleet in hyperspace, then let the
           existing arrival state machine perform the window and deceleration.
           This avoids a second fleet, instant reveal, or custom fake effect. */
        for(i=0;i<arrival.selection->numShips;i++)
            bitSet(arrival.selection->ShipPtr[i]->attributes,ATTRIBUTES_StartInHS);
        spHyperspaceSelectionOut(arrival.selection);
        spHyperspaceSelectionIn(arrival.selection,&destination);
        savedSet("CataArrivalStarted",1);
    }
    growSelectClose(&arrival);
    ccFocusOnPlayersMothership(&universe.mainCameraCommand,0);
}
void cataCampaignShipArrived(Ship *ship)
{
    /* Retail RestoreFleet schedules arrival; it does not signal arrival at
       the end of the NISlet. Let the existing hyperspace engine finish first. */
    if(active() && ship==universe.players[0].PlayerMothership &&
       ship->shiptype==CataMothership)
        savedSet("ShipsHaveArrived",1);
}
void cataCampaignSensorCenter(hvector *position)
{
    if(smSensorsActive && position){smCameraLookatPoint=*(vector *)position;smCamera.lookatpoint=*(vector *)position;vecZeroVector(smCameraLookVelocity);cameraSetEyePosition(&smCamera);}
}
sdword cataCampaignDifficulty(void){return 1; /* Existing story UI has no campaign difficulty override. */}
bool32 cataCampaignLockoutPreserved(void){AIVar *var=aivarFind("CataPreserveLockout");return active() && var && aivarValueGet(var);}
void cataCampaignPreserveLockout(sdword preserve){savedSet("CataPreserveLockout",preserve);if(!preserve)spUnlockout();}
