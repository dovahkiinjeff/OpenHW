/* Native half of the recovered-script boundary. Original IDs select this
   finite switch; all objects/callbacks are typed OpenHW references. */
#include "../Modern/CataclysmScriptBridge.h"
#include "../Modern/CataclysmData.h"
#include "CataclysmKAS.h"
#include "CataclysmCampaign.h"
#include "CataclysmSalvage.h"
#include "CataclysmWorker.h"
#include "CataclysmShips.h"
#include "CataclysmDisguise.h"
#include "AIPlayer.h"
#include "AIMoves.h"
#include "AITeam.h"
#include "AIUtilities.h"
#include "KASFunc.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Volume.h"
#include "Select.h"
#include "CommandLayer.h"
#include "CommandDefs.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "ResearchAPI.h"
#include "CameraCommand.h"
#include "Camera.h"
#include "mainrgn.h"
#include "Sensors.h"
#include "Tutor.h"
#include "AIVar.h"
#include "Randy.h"
#include "StringSupport.h"
#include "Debug.h"
#include "FastMath.h"
#include "SoundEvent.h"
#include "Dock.h"
#include "ProximitySensor.h"
#include "PiePlate.h"
#include "Timer.h"
#include "Tactics.h"
#include "ConsMgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern AITeam *CurrentTeamP;
extern AITeam *kasUnpausedTeam;
extern bool32 ccCameraTimeoutOverride;

void cataScriptFault(const char *message){dbgFatalf(DBG_Loc,"Cataclysm mission: %s",message);}
void *cataScriptCurrentTeam(void){return CurrentTeamP;}
void *cataScriptCurrentShips(void){return CurrentTeamP?&CurrentTeamP->shipList:NULL;}
int cataScriptMessageCount(void){return cataKasMessageCount();}
void cataScriptScope(int missionScope){cataKasScope=missionScope?0:CurrentTeamP?cataScriptHash(CurrentTeamP->kasLabel):0;}
const char *cataScriptText(uint32_t address){const char *text=hwCataOriginalText(address);if(!text)cataScriptFault("Original campaign text is missing or incompatible");return text;}
const char *cataScriptLocalizedText(uint32_t address){const char *text=hwCataLocalizedText(address,strCurLanguage);if(!text)cataScriptFault("Original localized campaign text is missing or incompatible");return text;}
static CataScriptArg number(intptr_t n){CataScriptArg value;value.kind=CATA_ARG_NUMBER;value.value.number=n;return value;}
static CataScriptArg object(void *p){CataScriptArg value;value.kind=CATA_ARG_OBJECT;value.value.object=p;return value;}
static sdword integer(const CataScriptArg *args,unsigned i){if(args[i].kind!=CATA_ARG_NUMBER)cataScriptFault("Expected numeric API argument");return (sdword)args[i].value.number;}
static void *pointer(const CataScriptArg *args,unsigned i){if(args[i].kind==CATA_ARG_NUMBER && !args[i].value.number)return NULL;if(args[i].kind!=CATA_ARG_OBJECT)cataScriptFault("Expected engine object API argument");return args[i].value.object;}
static char *text(const CataScriptArg *args,unsigned i){if(args[i].kind!=CATA_ARG_TEXT)cataScriptFault("Expected text API argument");return (char *)args[i].value.text;}
static CataScriptCallback callback(const CataScriptArg *args,unsigned i){if(args[i].kind!=CATA_ARG_CALLBACK)cataScriptFault("Expected callback API argument");return args[i].value.callback;}
#define N(i) integer(args,i)
#define P(i,T) ((T *)pointer(args,i))
#define S(i) P(i,GrowSelection)
#define T(i) P(i,AITeam)
#define V(i) P(i,hvector)
#define X(i) text(args,i)
#define B(i) callback(args,i)
#include "CataclysmMissionSupport.inc"
#include "CataclysmMissionVectors.inc"

static hvector *selectionCenter(GrowSelection *ships,unsigned slot)
{
    static hvector centers[4];vector center;real32 size;
    vecSet(center,0,0,0);
    if(ships && ships->selection->numShips)center=selCentrePointComputeGeneral((MaxSelection *)ships->selection,&size);
    vecMakeHVecFromVec(centers[slot],center);return &centers[slot];
}
static hvector *volumeCenter(Volume *volume){static hvector center;vector value;if(!volume)return NULL;value=volFindCenter(volume);vecMakeHVecFromVec(center,value);return &center;}
static sdword selectOwner(GrowSelection *result,GrowSelection *source,bool32 human)
{
    sdword i=0;Ship *ship;
    if(!result || !source)return 0;
    if(result!=source)growSelectReset(result);
    while(i<source->selection->numShips){
        ship=source->selection->ShipPtr[i];
        /* Script control prevents player selection, not combat targeting.
           The allied Kushan battle fleet is player 0 as in the retail script. */
        if((ship->playerowner->playerIndex==0)==human){
            if(result!=source)growSelectAddShip(result,ship);i++;}
        else if(result==source)clRemoveShipFromSelection(source->selection,ship);else i++;
    }
    return result->selection->numShips;
}
static sdword selectRace(GrowSelection *result,GrowSelection *source,sdword race)
{
    sdword i=0;Ship *ship;bool32 matches;
    if(!result || !source)return 0;if(result!=source)growSelectReset(result);
    while(i<source->selection->numShips){
        ship=source->selection->ShipPtr[i];
        matches=race==0?(ship->shiprace==R1 && IS_CATA_SHIP(ship->shiptype)):
            race==2?(ship->shiprace==R1 && !IS_CATA_SHIP(ship->shiptype)):
            race==3?ship->shiprace==R2:race==4?ship->shiprace==P1:FALSE;
        if(matches){if(result!=source)growSelectAddShip(result,ship);i++;}
        else if(result==source)clRemoveShipFromSelection(source->selection,ship);else i++;
    }
    return result->selection->numShips;
}
static void selectMothership(GrowSelection *selection){if(selection){growSelectReset(selection);if(universe.players[0].PlayerMothership)growSelectAddShip(selection,universe.players[0].PlayerMothership);}}
/* 00499330 filters actors through 005138f0 (has Salvage upgrade), issues
   the real special command, then selects the Worker's salvage role. */
static void cataScriptForceSalvage(GrowSelection *targets)
{
    MaxSelection actors={0};MaxAnySelection selected={0};sdword i;
    if(!CurrentTeamP || !targets)return;
    for(i=0;i<CurrentTeamP->shipList.selection->numShips && actors.numShips<COMMAND_MAX_SHIPS;i++){
        Ship *ship=CurrentTeamP->shipList.selection->ShipPtr[i];
        if(ship->objtype==OBJ_ShipType && cataCampaignShipHasUpgrade(ship,CATA_UPGRADE_SALVAGE))
            actors.ShipPtr[actors.numShips++]=ship;
    }
    for(i=0;i<targets->selection->numShips && selected.numTargets<COMMAND_MAX_SHIPS;i++)
        selected.TargetPtr[selected.numTargets++]=(SpaceObjRotImpTarg*)targets->selection->ShipPtr[i];
    if(!actors.numShips)return;
    clSpecial(&universe.mainCommandLayer,(SelectCommand*)&actors,(SpecialCommand*)&selected);
    for(i=0;i<actors.numShips;i++){
        if(actors.ShipPtr[i]->shiptype==CataWorker)cataWorkerSetRole(actors.ShipPtr[i],1);
        else if(actors.ShipPtr[i]->shiptype==P1StandardCorvette)cataSalvageSetActorMode(actors.ShipPtr[i],2);
    }
}
#include "CataclysmInvulnerability.inc"
static void setInvulnerable(GrowSelection *selection,bool32 enabled){sdword i;if(selection)for(i=0;i<selection->selection->numShips;i++)cataSetShipInvulnerable(selection->selection->ShipPtr[i],enabled);}

static void cataScriptSetTeamParameters(udword bits)
{
    sdword i;
    if (!CurrentTeamP) return;
    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; ++i)
    {
        Ship *ship = CurrentTeamP->shipList.selection->ShipPtr[i];
        /* Owned records pack the signed 16-bit parameter and its extension
           together. These bits are distinct from the ordinary ship flags. */
        uword upper;
        memcpy(&upper, ship->attributesPad, sizeof(upper));
        upper |= (uword)(bits >> 16);
        memcpy(ship->attributesPad, &upper, sizeof(upper));
        ship->attributesParam = (sword)((uword)ship->attributesParam | (uword)bits);
    }
}

static void cataScriptDockTeam(AITeam *hostTeam)
{
    Ship *host = NULL;
    sdword flags = dockmoveFlags_Stay;
    if (!CurrentTeamP) return;
    if (hostTeam && hostTeam->shipList.selection->numShips)
    {
        Ship *candidate = hostTeam->shipList.selection->ShipPtr[0];
        if (!(candidate->flags & (SOF_NotBumpable | SOF_Disabled | SOF_Dead | SOF_Hide)) &&
            !aiuShipIsntSelectable(candidate))
        {
            host = candidate;
            flags |= dockmoveFlags_AtShip;
        }
    }
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateDock(CurrentTeamP, flags, host, FALSE, TRUE);
}

static void cataScriptScheduleScuttle(GrowSelection *selection)
{
    sdword i;
    if (!selection) return;
    for (i = 0; i < selection->selection->numShips; ++i)
    {
        Ship *ship = selection->selection->ShipPtr[i];
        if (!(ship->flags & SOF_Dead))
            ship->deathtime = universe.totaltimeelapsed + frandom(1.0f);
    }
}

static void cataScriptHoldDisabled(GrowSelection *selection, bool32 enabled)
{
    const udword disabledFlags = SOF_Disabled | SOF_NotBumpable;
    sdword i;
    if (!selection) return;
    for (i = 0; i < selection->selection->numShips; ++i)
    {
        Ship *ship = selection->selection->ShipPtr[i];
        char label[36];
        AIVar *saved;
        snprintf(label, sizeof(label), "CataDisabled%u", (unsigned)ship->shipID.shipNumber);
        saved = aivarFind(label);
        if (enabled)
        {
            if (saved) continue;
            saved = aivarCreate(label);
            aivarValueSet(saved, (ship->flags & disabledFlags) | (ship->autostabilizeship ? 1 : 0));
            bitSet(ship->flags, disabledFlags);
            ship->autostabilizeship = FALSE;
            ApplyCareenRotationDirectly(ship);
        }
        else if (saved)
        {
            udword restore = (udword)aivarValueGet(saved);
            ship->flags = (ship->flags & ~disabledFlags) | (restore & disabledFlags);
            ship->autostabilizeship = (restore & 1) || vecMagnitudeSquared(ship->posinfo.velocity) > 0;
            aivarDestroy(saved);
        }
    }
}
static void switchOwner(GrowSelection *selection,bool32 fullTransfer)
{
    sdword i;Ship *ship;Player *previous;
    if(!selection)return;
    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,selection->selection);
    for(i=0;i<selection->selection->numShips;i++){
        ship=selection->selection->ShipPtr[i];previous=ship->playerowner;
        RemoveShipFromBeingTargeted(&universe.mainCommandLayer,ship,REMOVE_PROTECT);
        unitCapDeleteShip(ship,previous);
        if(previous==&universe.players[0]){
            clRemoveShipFromSelection((SelectCommand *)&selSelected,ship);univRemoveShipFromHotkeyGroup(ship,FALSE);
            ship->playerowner=&universe.players[1];bitSet(ship->attributes,ATTRIBUTES_Defector);
            bitClear(ship->flags,SOF_Selectable);bitClear(ship->specialFlags2,SPECIAL_2_CataScriptControlled);
        }else{
            ship->playerowner=&universe.players[0];bitClear(ship->attributes,ATTRIBUTES_Defector);
            bitClear(ship->flags,SOF_Disabled);
            bitSetTo(ship->flags,SOF_Selectable,fullTransfer);
            bitSetTo(ship->specialFlags2,SPECIAL_2_CataScriptControlled,!fullTransfer);
        }
        unitCapCreateShip(ship,ship->playerowner);
        cataCampaignInitializeShipUpgrades(ship);
    }
    universe.collUpdateAllBlobs=TRUE;
}
static sdword playerShipsNear(GrowSelection *selection,sdword radius)
{
    sdword i;Node *node;Ship *anchor,*ship;vector position,delta;
    if(!selection || !selection->selection->numShips)return 0;
    anchor=selection->selection->ShipPtr[0];
    for(i=0;i<selection->selection->numShips;i++){
        anchor=selection->selection->ShipPtr[i];
        if(anchor->playerowner==&universe.players[0])break;
    }
    position=anchor->posinfo.position;growSelectReset(selection);
    for(node=universe.ShipList.head;node;node=node->next){
        ship=(Ship *)listGetStructOfNode(node);
        /* Allied script-controlled ships are still player-owned in retail. */
        if(ship->playerowner!=&universe.players[0] || ship->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace|SOF_Cloaked))continue;
        vecSub(delta,ship->posinfo.position,position);
        if(vecMagnitudeSquared(delta)<=(real32)radius*radius)growSelectAddShip(selection,ship);
    }
    return selection->selection->numShips;
}
static sdword anyIdle(GrowSelection *selection){sdword i;if(selection)for(i=0;i<selection->selection->numShips;i++){CommandToDo *command=getShipAndItsCommand(&universe.mainCommandLayer,selection->selection->ShipPtr[i]);if(!command || command->ordertype.order==COMMAND_NULL)return 1;}return 0;}
static void scriptCloak(GrowSelection *selection,bool32 cloak){sdword i;if(selection)for(i=0;i<selection->selection->numShips;i++){Ship *ship=selection->selection->ShipPtr[i];char key[32];AIVar *saved;snprintf(key,sizeof(key),"CloakSave%u",ship->shipID.shipNumber);saved=aivarFind(key);if(cloak){if(!saved)saved=aivarCreate(key);aivarValueSet(saved,bitTest(ship->flags,SOF_Cloaked)!=0);bitSet(ship->flags,SOF_Cloaked);}else if(!saved || !aivarValueGet(saved))bitClear(ship->flags,SOF_Cloaked);}}
static void sortDistance(GrowSelection *selection,hvector *point){sdword i,j;vector a,b;if(!selection||!point)return;for(i=1;i<selection->selection->numShips;i++)for(j=0;j<i;j++){Ship *left=selection->selection->ShipPtr[j],*right=selection->selection->ShipPtr[i];vecSub(a,left->posinfo.position,*(vector *)point);vecSub(b,right->posinfo.position,*(vector *)point);if(vecMagnitudeSquared(b)<vecMagnitudeSquared(a)){selection->selection->ShipPtr[j]=right;selection->selection->ShipPtr[i]=left;}}}
#include "CataclysmReinforcementLaunch.inc"
static void createInside(GrowSelection *result,char *name,sdword count,GrowSelection *hosts,sdword owner)
{
    sdword i;ShipType type=StrToShipType(name);ShipRace race;Ship *ship,*host;vector zero={0,0,0};
    if(!result||!hosts||!hosts->selection->numShips)return;
    if(type<0 || type>=TOTAL_NUM_SHIPS || owner<0 || owner>=universe.numPlayers || count<0 || count>1000)cataScriptFault("Invalid scripted reinforcement request");
    host=hosts->selection->ShipPtr[0];
    race=cataScriptReinforcementRace(type,name,host->shiprace);
    if(!GetShipStaticInfoSafe(type,race))
        dbgFatalf(DBG_Loc,"Cataclysm mission: scripted reinforcement %s statics were not loaded for %s",
                  name,ShipRaceToStr(race));
    /* Older saves can contain a slipgate without its retail launch queue. */
    if(host->shiptype==CataSlipgateLarge)
        dockEnsureShipsInside(host);
    growSelectReset(result);
    for(i=0;i<count;i++){
        ship=univCreateShip(type,race,&zero,&universe.players[owner],0);
        if(bitTest(host->specialFlags2,SPECIAL_2_CataScriptControlled)){
            bitSet(ship->specialFlags2,SPECIAL_2_CataScriptControlled);
            bitClear(ship->flags,SOF_Selectable);
        }
        gameStatsAddShip(ship,owner);unitCapCreateShip(ship,ship->playerowner);
        dockPutShipInside(ship,host);growSelectAddShip(result,ship);
    }
    /* Original 00499c70 ends with aiuWrapDock(selection, mode 2, host).
       Cataclysm mode 2 is LAUNCH_AT_SPECIFIC_SHIP, not HW1's refuel bit.
       Rebuilt squads wait for launch completion; no later Launch call exists. */
    cataLaunchNewReinforcements(result,host);
}

CataScriptArg cataScriptDispatch(unsigned api,const CataScriptArg *args,unsigned count)
{
    sdword i;GrowSelection *ships;
    (void)count;
    switch(api){
    case 0x00417e10:cataKasSendFrom(T(0),T(1),(uint32_t)N(2));break;
    case 0x00417ec0:return number(cataKasMessagePop(N(1)));
    case 0x00490640:cataKasJump((uint32_t)N(0),B(1),B(2));break;
    case 0x00490660:cataKasFSMCreate((uint32_t)N(0),B(1),B(2),T(3));break;
    case 0x004907a0:return object(cataKasTeam((uint32_t)N(0)));
    case 0x004907e0:return object(cataKasTeamShips((uint32_t)N(0)));
    case 0x00490820:return object(selectionCenter(cataKasSelection((uint32_t)N(0)),0));
    case 0x004908c0:return object(selectionCenter(cataKasTeamShips((uint32_t)N(0)),1));
    case 0x00490960:return object(volumeCenter(cataKasVolume((uint32_t)N(0))));
    case 0x004909f0:return object(selectionCenter(CurrentTeamP?&CurrentTeamP->shipList:NULL,2));
    case 0x00490aa0:return object(cataKasPath((uint32_t)N(0)));
    case 0x00490af0:return object(cataKasVolume((uint32_t)N(0)));
    case 0x00490b30:return object(cataKasVector((uint32_t)N(0)));
    case 0x00490bb0:return object(cataKasSelection((uint32_t)N(0)));
    case 0x00491d30:cataKasVarCreateSet((uint32_t)N(0),N(1));break;
    case 0x00491d70:cataKasVarSet((uint32_t)N(0),N(1));break;
    case 0x00491db0:cataKasVarIncrement((uint32_t)N(0));break;
    case 0x00491df0:cataKasVarSet((uint32_t)N(0),cataKasVarGet((uint32_t)N(0))-1);break;
    case 0x00491e70:cataScriptVariableDestroy((uint32_t)N(0));break;
    case 0x00491e30:return number(cataKasVarGet((uint32_t)N(0)));
    case 0x00491ec0:cataKasTimerCreate((uint32_t)N(0));break;
    case 0x00491f00:cataKasTimerSet((uint32_t)N(0),N(1));break;
    case 0x00491f50:cataKasTimerStart((uint32_t)N(0));break;
    case 0x00491f90:cataKasTimerCreateSetStart((uint32_t)N(0),N(1));break;
    case 0x00492060:return number(cataKasTimerExpired((uint32_t)N(0)));
    case 0x004920a0:return number(cataKasTimerExpiredDestroy((uint32_t)N(0)));
    case 0x004920e0:cataScriptTimerDestroy((uint32_t)N(0));break;
    case 0x00454530:cmCloseIfOpen();break;
    case 0x004927b0:kasfFormationBroad();break;
    case 0x004927e0:kasfFormationDelta3D();break;
    case 0x00492840:kasfFormationWall();break;
    case 0x00492870:kasfFormationSphere();break;
    case 0x00492ae0:return number(cataScriptOrderAttributes(S(0)));
    /* These wrappers match the recovered call sequences, including the
       2-second intercept move and filtered team hyperspace-out operation. */
    case 0x004924e0:kasfIntercept(S(0));break;
    case 0x00493800:kasfTeamHyperspaceOut();break;
    case 0x004967b0:return number(kasfBuildManagerShipTypeInBuildQueue(X(0)));
    case 0x00497850:if(S(0))kasfDeleteShips(S(0));break;
    case 0x00498250:kasfHoldHyperspaceWindow(N(0)==1);break;
    case 0x004938f0:kasfShipsSetMaxVelocity(S(0),N(1));break;
    case 0x00493940:ships=S(0);if(ships)for(i=0;i<ships->selection->numShips;i++)
        bitClear(ships->selection->ShipPtr[i]->specialFlags,SPECIAL_SinglePlayerLimitSpeed);break;
    case 0x00493f40:kasfDisableAllAIFeatures();break;
    case 0x00494450:return number(cataScriptFindAttackers(T(0)?&T(0)->shipList:NULL,S(1)));
    case 0x00494510:return number(cataScriptFindAttackers(CurrentTeamP?&CurrentTeamP->shipList:NULL,S(0)));
    case 0x00498630:return number(cataScriptFindAttackers(S(0),S(1)));
    case 0x00493600:kasfTeamHyperspaceIn(V(0));break;
    case 0x00493630:{hvector destination;if(CurrentTeamP){
        cataScriptVectorRandom(&destination,V(0),N(1),0.5f);
        spHyperspaceSelectionIn(CurrentTeamP->shipList.selection,&destination);spHyperspaceDelay=0.0f;
        }break;}
    case 0x00495640:return number(kasfShipsSelectClass(S(0),S(1),X(2)));
    case 0x00496e00:if(N(0)<0||N(0)>=universe.numPlayers)cataScriptFault("Invalid RU owner");return number(universe.players[N(0)].resourceUnits);
    case 0x00492120:return number(cataKasReceived((uint32_t)N(0)));
    case 0x00492190:
        ships=S(0);
        if (kasfShipsCount(ships) && kasfTeamCount())
        {
            /* Retail issues the explicit command immediately. HW1's advanced
               AI attack rejects the entire list if even one target is hidden
               or cloaked, leaving the visible battle fleets idle. */
            GrowSelection targets, attackers;
            growSelectInit(&targets);growSelectInit(&attackers);
            for(i=0;i<CurrentTeamP->shipList.selection->numShips;i++){
                Ship *ship=CurrentTeamP->shipList.selection->ShipPtr[i];
                if(!aiuShipIsntSelectable(ship) && !(ship->flags&SOF_Dead))growSelectAddShip(&attackers,ship);
            }
            if(attackers.selection->numShips)for(i=0;i<ships->selection->numShips;i++){
                Ship *target=ships->selection->ShipPtr[i];
                Player *owner=attackers.selection->ShipPtr[0]->playerowner;
                if(target->playerowner!=owner && !(target->flags&SOF_Dead) &&
                   !aiuShipIsntTargetable(target,owner) &&
                   !cataDisguiseConcealsRaider(target,owner))growSelectAddShip(&targets,target);
            }
            if(targets.selection->numShips && attackers.selection->numShips){
                aitDeleteAllTeamMoves(CurrentTeamP);
                clAttack(&universe.mainCommandLayer,attackers.selection,(AttackCommand *)targets.selection);
                for(i=0;i<attackers.selection->numShips;i++){
                    Ship *actor=attackers.selection->ShipPtr[i];
                    if(actor->shiptype==StandardDestroyer || actor->shiptype==MissileDestroyer || actor->shiptype==HeavyCruiser){
                        sdword g;real32 fired=-1.0f;vector delta;
                        SpaceObjRotImpTarg *target=actor->attackvars.attacktarget;
                        if(actor->gunInfo)for(g=0;g<actor->gunInfo->numGuns;g++)
                            fired=max(fired,actor->gunInfo->guns[g].lasttimefired);
                        vecZeroVector(delta);
                        if(target)vecSub(delta,target->posinfo.position,actor->posinfo.position);
                        HW_DIAG_FPRINTF(stderr,"[CataclysmCombat] team=%s ship=%s id=%u flags=%08X guns=%d lastShot=%.2f target=%s range=%.1f speed=%.1f maxSpeed=%.1f spin=%.3f maxSpin=%.3f time=%.2f\n",
                            CurrentTeamP->kasLabel,ShipTypeToStr(actor->shiptype),(unsigned)actor->shipID.shipNumber,
                            (unsigned)actor->flags,actor->gunInfo?actor->gunInfo->numGuns:0,fired,
                            target&&target->objtype==OBJ_ShipType?ShipTypeToStr(((Ship *)target)->shiptype):"none",
                            fsqrt(vecMagnitudeSquared(delta)),fsqrt(vecMagnitudeSquared(actor->posinfo.velocity)),
                            tacticsGetShipsMaxVelocity(actor),fsqrt(vecMagnitudeSquared(actor->rotinfo.rotspeed)),
                            actor->staticinfo->staticheader.maxrot,universe.totaltimeelapsed);
                    }
                }
                HW_DIAG_FPRINTF(stderr,"[CataclysmMission1] %s issued attack: %d ships, %d visible targets; first=%s owner=%d at %.2f seconds.\n",
                    CurrentTeamP->kasLabel,attackers.selection->numShips,targets.selection->numShips,
                    ShipTypeToStr(targets.selection->ShipPtr[0]->shiptype),
                    targets.selection->ShipPtr[0]->playerowner->playerIndex,universe.totaltimeelapsed);
            }
            growSelectClose(&attackers);growSelectClose(&targets);
        }
        break;
    case 0x00492780:kasfFormationDelta();break;
    case 0x00492810:kasfFormationClaw();break;
    case 0x00492910:return number(kasfRandom(N(0),N(1)));
    case 0x00492960:kasfGuardShips(S(0));break;
    case 0x004929d0:kasfPatrolPath(P(0,Path));break;
    case 0x00492b20:return number(kasfTeamHealthAverage());
    case 0x00492b90:{sdword lowest=110;
        if(!CurrentTeamP || !CurrentTeamP->shipList.selection->numShips)return number(0);
        for(i=0;i<CurrentTeamP->shipList.selection->numShips;i++){
            Ship *ship=CurrentTeamP->shipList.selection->ShipPtr[i];
            sdword health=(sdword)nearbyint(100.0*ship->health/ship->staticinfo->maxhealth);
            if(health<lowest)lowest=health;
        }return number(lowest);}
    case 0x00492ce0:return number(kasfTeamCount());
    case 0x00492d10:kasfGrowSelectionClear(S(0));break;
    case 0x00492d60:return number(cataScriptFindMarkedAsteroids(S(0)));
    case 0x00493400:return number(cataScriptFirstShipDocked(S(0)));
    case 0x00493490:cataScriptSetTeamParameters((udword)N(0));break;
    case 0x00492d20:return number(kasfShipsCount(S(0)));
    case 0x00492db0:return number(kasfFindShipsInside(P(0,Volume),S(1)));
    case 0x00492e90:return number(playerShipsNear(S(0),N(1)));
    case 0x004931a0:return number(kasfFindShipsNearPoint(S(0),V(1),N(2)));
    case 0x00493980:kasfShipsSetDamageFactor(S(0),N(1));break;
    case 0x004939d0:kasfShipsClearDamageFactor(S(0));break;
    case 0x00493a10:ships=S(0);if(ships)for(i=0;i<ships->selection->numShips;i++){
        Ship *ship=ships->selection->ShipPtr[i];
        ship->singlePlayerSpeedLimiter=ship->staticinfo->staticheader.maxvelocity*(real32)N(1)*0.01f;
        bitSet(ship->specialFlags,SPECIAL_SinglePlayerLimitSpeed);
        }break;
    case 0x00493aa0:kasfTeamSetPercentDamaged(N(0));break;
    case 0x00493b90:return number(cataCampaignDifficulty());
    case 0x00493cf0:kasfReinforceTeamWithShips(T(0),S(1));break;
    case 0x00493de0:return number(kasfThisTeamIs(T(0)));
    case 0x00493e00:return number(kasfTeamCountOriginal());
    case 0x00494160:switchOwner(CurrentTeamP?&CurrentTeamP->shipList:NULL,N(0));break;
    case 0x004941a0:kasfMoveTo(V(0));break;
    case 0x00494630:cataScriptDockTeam(T(0));break;
    case 0x00494750:kasfDockStay(T(0));break;
    case 0x004947c0:kasfLaunch();break;
    /* Retail 4948a0 tests every ship's launch-completion flag and its
       in-space state; 494940 tests docked/soft-docked readiness. A docking
       test here prevented the battle teams from refreshing their targets. */
    case 0x004948a0:return number(kasfTeamFinishedLaunching());
    case 0x00494940:return number(kasfTeamDockedReadyForLaunch());
    case 0x004949b0:for(i=0;i<aiCurrentAIPlayer->teamsUsed;i++)cataKasSend(aiCurrentAIPlayer->teams[i],(uint32_t)N(0));break;
    case 0x00494a00:cataKasSend(T(0),(uint32_t)N(1));break;
    case 0x00494e70:kasfObjectiveCreate(X(0),X(1),X(2));break;
    case 0x00494f60:kasfObjectiveSet(X(0),TRUE);break;
    case 0x00495180:return number(cataScriptSelectSalvagers(S(0),S(1),S(2)));
    case 0x004952b0:return number(kasfShipsSelectNotDocked(S(0),S(1)));
    case 0x00495410:return number(cataScriptSelectMembers(S(0),S(1),S(2)));
    case 0x004954d0:return number(cataScriptSelectPlayerCombatShips(S(0),S(1)));
    case 0x00495560:return number(selectOwner(S(0),S(1),TRUE));
    case 0x00495620:return number(selectOwner(S(0),S(1),FALSE));
    case 0x00495690:return number(kasfShipsSelectType(S(0),S(1),X(2)));
    case 0x00495a50:return number(kasfShipsSelectNearby(S(0),S(1),V(2),N(3)));
    case 0x00495bd0:return number(kasfShipsAdd(S(0),S(1)));
    case 0x00495c40:return number(kasfShipsRemove(S(0),S(1)));
    case 0x00495e50:if(kasfShipsCount(S(0)))kasfPingAddShips(S(0),X(1));break;
    case 0x00495e80:kasfPingAddPoint(V(0),X(1));break;
    case 0x00495ea0:kasfPingRemove(X(0));break;
    case 0x00495eb0:kasfStop();break;
    case 0x00495f00:kasfHarvest();break;
    case 0x00496190:return number(kasfCameraGetAngleDeg());
    case 0x004961b0:ccChangeAngleDeclination(&universe.mainCameraCommand,DEG_TO_RAD((real32)N(0)),currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.declination);cameraSetEyePosition(&universe.mainCameraCommand.actualcamera);break;
    case 0x004961f0:return number(kasfCameraGetDeclinationDeg());
    case 0x00496210:ccChangeAngleDeclination(&universe.mainCameraCommand,currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.angle,DEG_TO_RAD((real32)N(0)));cameraSetEyePosition(&universe.mainCameraCommand.actualcamera);break;
    case 0x00496250:return number(kasfCameraGetDistance());
    case 0x00496260:universe.mainCameraCommand.actualcamera.distance=(real32)N(0);currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.distance=(real32)N(0);cameraSetEyePosition(&universe.mainCameraCommand.actualcamera);break;
    case 0x004963a0:if(kasfShipsCount(S(1)))kasfTutSetPointerTargetShip(X(0),S(1));break;
    case 0x004963e0:if(kasfShipsCount(S(1)))kasfTutSetPointerTargetShipSelection(X(0),S(1));break;
    case 0x00496490:kasfTutSetPointerTargetAIVolume(X(0),P(1,Volume));break;
    case 0x004964b0:kasfTutSetPointerTargetAIVolume(X(0),P(1,Volume));break;
    case 0x004964e0:kasfTutRemovePointer(X(0));break;
    case 0x00496530:kasfTutRemoveAllPointers();break;
    case 0x00496720:kasfTutDisableFlags(X(0));break;
    case 0x00496870:kasfTutCameraFocusFar(S(0));break;
    case 0x00496930:
        universe.mainCameraCommand.actualcamera.distance=(real32)N(0);
        currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.distance=(real32)N(0);
        cataCampaignCameraRequestSnap((real32)N(0));break;
    case 0x00496950:{
        GrowSelection *ships=S(0);MaxSelection selected=selSelected;
        if(ships && ships->selection->numShips){
            ccCameraTimeoutOverride=TRUE;
            if(piePointSpecMode!=PSM_Idle)piePointModeOnOff();
            spMainScreen();ccFocusClose(&universe.mainCameraCommand,(FocusCommand*)ships->selection);
        }else ccFocusClose(&universe.mainCameraCommand,(FocusCommand*)&selected);
        cataCampaignCameraRequestSnap(0);break;}
    case 0x00496ae0:kasfTutCameraFocusCancel();break;
    case 0x00496bc0:kasfOpenSensors(N(0));break;
    case 0x00496c30:
        kasfCloseSensors(0);
        smFleetIntel=FALSE;
        break;
    case 0x00496c80:return number(kasfSensorsIsOpen(N(0)));
    case 0x00496cf0:cataCampaignResearchAllow(X(0));break;
    case 0x00496d10:cataCampaignResearchAcquire(X(0));break;
    case 0x00496d40:return number(cataCampaignHasResearch(X(0)));
    case 0x00496e20:if(N(0)<0||N(0)>=universe.numPlayers)cataScriptFault("Invalid RU owner");universe.players[N(0)].resourceUnits=N(1);break;
    case 0x00496e40:return number(kasfGetWorldResources());
    case 0x00496eb0:cataCampaignSpeech(N(0),N(1));break;
    case 0x00497000:soundEventPlayMusic(N(0));break;
    case 0x00497290:cataCampaignRequestCheckpoint(N(0),X(1));break;
    /* Owned 00497420 and 00497580 are identical entry implementations. */
    case 0x00497420:
    case 0x00497580:kasfWideScreenIn(N(0));break;
    case 0x00497630:kasfWideScreenOut(N(0));break;
    case 0x00497710:kasfSubtitleSimulate(3,N(0),X(1));break;
    case 0x004977a0:scriptCloak(S(0),TRUE);break;
    case 0x00497800:scriptCloak(S(0),FALSE);break;
    case 0x00497a90:cataCampaignPreserveLockout(TRUE);break;
    case 0x00497aa0:cataCampaignPreserveLockout(FALSE);break;
    case 0x00497ad0:{char scope[32];kasUnpausedTeam=CurrentTeamP;
        snprintf(scope,sizeof(scope),"C%08X_",(unsigned)cataKasScope);
        timTimerPauseAllNotScoped(scope);break;}
    case 0x00497af0:kasfOtherKASUnpause();break;
    case 0x00497b00:return number(cataCampaignSpeechEnded());
    case 0x00497b80:kasfIntelEventNotEnded();break;
    case 0x00497ed0:
        /* Keep allied ships under script orders while their hulls remain
           available to the renderer, combat and sensors. */
        ships=S(0);
        if (ships) for (i=0; i<ships->selection->numShips; ++i)
        {
            Ship *ally=ships->selection->ShipPtr[i];
            bitSet(ally->specialFlags2,SPECIAL_2_CataScriptControlled);
            bitClear(ally->flags,SOF_Selectable);
        }
        break;
    case 0x00497f10:cataDisguiseRegister(N(0),X(1));break;
    case 0x00497f70:cataDisguiseSetGroupLimit(N(0),N(1));break;
    case 0x00497fd0:cataDisguiseApply(S(0),X(1),N(2));break;
    case 0x00498080:cataDisguiseRemove(S(0));break;
    case 0x004980c0:rmEnableShip(StrToShipRace(X(1)),StrToShipType(X(0)),N(2));break;
    case 0x00498230:cataCampaignRestoreFleet();break;
    case 0x00498270:cataScriptScheduleScuttle(S(0));break;
    case 0x004984c0:setInvulnerable(S(0),N(1));break;
    case 0x004985f0:return number(selectRace(S(0),S(1),N(2)));
    case 0x00498790:selectMothership(S(0));break;
    case 0x00498c10:cataCampaignUpgradeActivate(X(0));break;
    case 0x00498c70:cataCampaignApplyUpgrades(S(0));break;
    case 0x00498cb0:return number(cataSalvageProcessedCount(StrToShipType(X(0)),N(1)));
    case 0x00498d90:kasfGrowSelectionClear(S(0));if(kasfShipsCount(S(1)))growSelectAddShip(S(0),S(1)->selection->ShipPtr[0]);break;
    case 0x00498dd0:cataKasVectorSet((uint32_t)N(0),(real32)N(1),(real32)N(2),(real32)N(3));break;
    case 0x00498f30:cataScriptVectorBlend(V(0),V(1),V(2),N(3),N(4));break;
    case 0x004991d0:cataScriptVectorRandom(V(0),V(1),N(2),1.0f);break;
    case 0x00499330:cataScriptForceSalvage(S(0));break;
    case 0x00499570:return number(cataCampaignShipsInSensorRange(S(0)));
    case 0x004995e0:return number(cataScriptSelectGates(P(0,Volume),S(1)));
    case 0x00499870:return number(anyIdle(S(0)));
    case 0x004998c0:return number(cataScriptTeamIdle(CurrentTeamP));
    case 0x00499c70:createInside(S(0),X(1),N(2),S(3),N(4));break;
    case 0x0049a100:sortDistance(S(0),V(1));break;
    case 0x0049a790:return number(cataScriptOrderTargets(S(0),S(1)));
    case 0x0049ad80:return number(cataScriptSelectInside(S(0),S(1)));
    case 0x0049a2d0:cataScriptHoldDisabled(S(0),N(1)!=0);break;
    case 0x0049a2a0:cataCampaignSensorCenter(V(0));break;
    case 0x0049a460:ships=S(0);cataSalvageSetProcessingHost(ships && ships->selection->numShips?ships->selection->ShipPtr[0]:NULL);break;
    case 0x0049aae0:ships=S(0);if(ships)for(i=0;i<ships->selection->numShips;i++)ships->selection->ShipPtr[i]->colorScheme=(ubyte)N(1);break;
    case 0x004d6480:kasfObjectiveDestroyAll();break;
    case 0x0051be70:
        HW_DIAG_FPRINTF(stderr,"[CataclysmMission1] Scripted defeat at %.2f seconds.\n",universe.totaltimeelapsed);
        singlePlayerMissionFailedCB();break;
    default:{char error[96];snprintf(error,sizeof(error),"Unimplemented recovered API %08X",api);cataScriptFault(error);}
    }
    return number(0);
}
