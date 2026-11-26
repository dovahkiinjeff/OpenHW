/* Somtaaw composite hull compatibility. The original component meshes share
   the mothership's origin; do not recenter them or edit the owned models. */
#include "CataclysmShips.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Clamp.h"
#include "Debug.h"
#include "Formation.h"
#include "LevelLoad.h"
#include "FastMath.h"
#include "ShipAppearance.h"
#include "Mesh.h"
#include "SinglePlayer.h"
#include "AIVar.h"
#include "../Modern/CataclysmData.h"
#include <string.h>
#include <stdio.h>

/* Kept outside the HW1 save layouts. Rebuilt from owned definitions whenever
   statics load; live/queued usage is derived from existing engine counts. */
static CataShipEconomy economies[NUM_RACES][TOTAL_NUM_SHIPS];
static CataShipNavigation navigation[NUM_RACES][TOTAL_NUM_SHIPS];
#include "CataclysmSoundIdentity.inc"
sdword cataShipOriginalID(const Ship *ship)
{
    if(!ship)return -1;
    if(ship->shiprace==P1 && ship->shiptype==P1StandardCorvette)return 97;
    if(ship->shiprace==P1 && ship->shiptype==P1Mothership)return 96;
    return cataSoundShipObject(ship);
}
void cataShipSetGatePartner(Ship *ship,Ship *partner)
{
    char key[40];AIVar *value;snprintf(key,sizeof(key),"CataGatePartner%u",(unsigned)ship->shipID.shipNumber);
    value=aivarFind(key);if(!value)value=aivarCreate(key);aivarValueSet(value,partner?(sdword)partner->shipID.shipNumber+1:0);
}
Ship *cataShipGatePartner(Ship *ship)
{
    char key[40];AIVar *value;Node *node;snprintf(key,sizeof(key),"CataGatePartner%u",(unsigned)ship->shipID.shipNumber);
    value=aivarFind(key);if(!value)return NULL;
    for(node=universe.ShipList.head;node;node=node->next){Ship *candidate=listGetStructOfNode(node);
        if(candidate->shiptype==CataSlipgateLarge && (sdword)candidate->shipID.shipNumber+1==aivarValueGet(value))return candidate;}
    return NULL;
}
real32 cataShipAvoidBy(const Ship *ship){return navigation[ship->shiprace][ship->shiptype].avoidBy;}
real32 cataShipSensorRadius(const Ship *ship){return economies[ship->shiprace][ship->shiptype].sensorRadius;}
bool32 cataShipUpgradeByDocking(const Ship *ship){return economies[ship->shiprace][ship->shiptype].upgradeByDocking;}
real32 cataShipUpgradeTimeModifier(const Ship *ship){return economies[ship->shiprace][ship->shiptype].upgradeTimeModifier;}
bool32 cataShipSkipFleetOrders(const Ship *ship)
{
    return ship->shiptype==CataSlipgateLarge || bitTest(ship->specialFlags2,SPECIAL_2_CataScriptControlled) ||
        (IS_CATA_SHIP(ship->shiptype) && ship->clampInfo && ship->clampInfo->host &&
         ship->clampInfo->host->objtype==OBJ_ShipType &&
         ((Ship *)ship->clampInfo->host)->shiptype==CataMothership);
}
void cataShipLoadEconomy(ShipStaticInfo *info,const char *directory,const char *stem)
{
    char error[512];
    if(!hwCataShipEconomy(directory,stem,&economies[info->shiprace][info->shiptype],error,sizeof(error)))
        dbgFatalf(DBG_Loc,"%s: %s",stem,error);
    if(!hwCataShipNavigation(stem,&navigation[info->shiprace][info->shiptype],error,sizeof(error)))
        dbgFatalf(DBG_Loc,"%s: %s",stem,error);
    info->staticheader.rightOfWay=(sbyte)navigation[info->shiprace][info->shiptype].priority;
}
static void cataShipSupportUsage(Ship *ship,Player *player,bool32 outside,sdword *used,sdword *provided)
{
    Node *node;
    if(bitTest(ship->flags,SOF_Dead) || bitTest(ship->specialFlags2,SPECIAL_2_CataScriptControlled))return;
    if(ship->playerowner==player){
        CataShipEconomy *economy=&economies[ship->shiprace][ship->shiptype];
        *used+=economy->supportRequired;
        if(outside)*provided+=economy->supportProvided;
    }
    if(ship->shipsInsideMe)for(node=ship->shipsInsideMe->insideList.head;node;node=node->next)
        cataShipSupportUsage(((InsideShip *)listGetStructOfNode(node))->ship,player,FALSE,used,provided);
}
static void cataExcludeScriptTotals(Ship *ship,Player *player,ShipType type,sdword *total,sdword *ofType)
{
    Node *node;
    if(ship->playerowner==player && bitTest(ship->specialFlags2,SPECIAL_2_CataScriptControlled)){
        --*total;if(ship->shiptype==type)--*ofType;
    }
    if(ship->shipsInsideMe)for(node=ship->shipsInsideMe->insideList.head;node;node=node->next)
        cataExcludeScriptTotals(((InsideShip *)listGetStructOfNode(node))->ship,player,type,total,ofType);
}
bool32 cataShipCanCreate(ShipType type,shipsinprogress *factory,shipavailable *available)
{
    Player *player=factory->ship->playerowner;shipsinprogress *other;Node *node;
    ShipRace race=cmFactoryBuildRace(factory->ship);CataShipEconomy *requested=&economies[race][type];
    sdword used=requested->supportRequired,provided=0,total=1,ofType=1,i,n;
    for(node=universe.ShipList.head;node;node=node->next)
        cataShipSupportUsage((Ship *)listGetStructOfNode(node),player,TRUE,&used,&provided);
    total+=player->totalships;ofType+=player->shiptotals[type];
    for(node=universe.ShipList.head;node;node=node->next)
        cataExcludeScriptTotals((Ship *)listGetStructOfNode(node),player,type,&total,&ofType);
    for(i=0;available[i].nJobs!=-1;i++)if(available[i].nJobs>0 && available[i].info){
        ShipStaticInfo *info=available[i].info;n=available[i].nJobs;
        used+=n*economies[info->shiprace][info->shiptype].supportRequired;
        total+=n;if(info->shiptype==type)ofType+=n;
    }
    for(other=(shipsinprogress *)factory->node.belongto->head;other;other=(shipsinprogress *)other->node.next)
        if(other->ship->playerowner==player)for(i=0;i<TOTAL_NUM_SHIPS;i++){
            ShipStaticInfo *info=other->progress[i].info;n=other->progress[i].nJobs;
            if(n<=0 || !info)continue;
            used+=n*economies[info->shiprace][info->shiptype].supportRequired;
            total+=n;if(i==type)ofType+=n;
        }
    return requested->totalLimit>0 && total<=requested->totalLimit &&
        (requested->typeLimit<0 || ofType<=requested->typeLimit) &&
        (requested->supportRequired==0 || used<=provided);
}

static const ShipType coreComponents[]={
    CataMothershipEnginesLower,CataMothershipEnginesUpper,CataMothershipCommand,
    CataMothershipLowerSpine,CataMothershipOreCanArmour,CataMothershipEngineering
};
bool32 cataShipIsCoreComponent(ShipType type)
{
    unsigned i;for(i=0;i<sizeof(coreComponents)/sizeof(coreComponents[0]);i++)
        if(coreComponents[i]==type)return TRUE;
    return FALSE;
}
#include "CataclysmComponentVisual.inc"
#include "CataclysmComponentIdentity.inc"
bool32 cataShipAttachComponent(Ship *host,Ship *component)
{
    ShipAppearance appearance;
    const char *anchor=NULL;
    if(!host || !component || host->shiptype!=CataMothership ||
       component->shiptype<CataMothershipCommand || component->shiptype>CATA_LAST_COMPONENT ||
       component->playerowner!=host->playerowner || component->clampInfo)return FALSE;
    component->posinfo.position=host->posinfo.position;
    component->rotinfo.coordsys=host->rotinfo.coordsys;
    switch(component->shiptype){
    case CataMothershipLowerSpine:anchor="Hardpoint_Lowerdecks_Position";break;
    case CataMothershipOreCanArmour:anchor="Hardpoint_OreCanister_Position";break;
    case CataMothershipDockingBay:anchor="Hardpoint_HangarModule_Position";break;
    case CataMothershipWeapons:anchor="Hardpoint_Generic1_Position";break;
    case CataMothershipArmour:anchor="Hardpoint_Generic2_Position";break;
    case CataMothershipMicro:anchor="Hardpoint_Generic3_Position";break;
    case CataMothershipSpecial:anchor="Hardpoint_Generic4_Position";break;
    default:break;
    }
    if(anchor){
        hmatrix mount;matrix rotation;vector v,local,world;
        if(meshRemasteredMarkerTransform(host,anchor,&mount)){
            hmatGetVectFromHMatrixCol4(local,mount);
            matMultiplyMatByVec(&world,&host->rotinfo.coordsys,&local);
            vecAdd(component->posinfo.position,host->posinfo.position,world);
            hmatGetVectFromHMatrixCol1(v,mount);matPutVectIntoMatrixCol1(v,rotation);
            hmatGetVectFromHMatrixCol2(v,mount);matPutVectIntoMatrixCol2(v,rotation);
            hmatGetVectFromHMatrixCol3(v,mount);matPutVectIntoMatrixCol3(v,rotation);
            matMultiplyMatByMat(&component->rotinfo.coordsys,&host->rotinfo.coordsys,&rotation);
        }
    }else{
        vector embedded;
        if(cataShipIsCoreComponent(component->shiptype) && meshRemasteredMarkerPosition(host,"native_hull",&embedded))
            /* The new hull already contains the command/engineering/engine
               geometry. Retain their original script/economy objects. */
            bitSet(component->flags,SOF_Hide|SOF_DontDrawTrails);
    }
    component->posinfo.velocity=host->posinfo.velocity;
    component->colorScheme=host->colorScheme;
    shipAppearanceGet(host,&appearance);shipAppearanceApply(component,&appearance);
    bitClear(component->flags,SOF_Selectable);
    bitSet(component->flags,SOF_NotBumpable);
    clampObjToObj((SpaceObjRotImpTargGuidance *)component,(SpaceObjRotImpTargGuidance *)host);
    return TRUE;
}
void cataShipUpdateComponentPose(Ship *part, Ship *host)
{
    vector offset;
    /* The native HOD mount is recorded in the original gameplay attachment. */
    matMultiplyMatByVec(&offset,&host->rotinfo.coordsys,&part->clampInfo->clampOffset);
    vecAdd(part->posinfo.position,host->posinfo.position,offset);
    matMultiplyMatByMat(&part->rotinfo.coordsys,&host->rotinfo.coordsys,&part->clampInfo->clampCoordsys);
    part->posinfo.velocity=host->posinfo.velocity;
    vecZeroVector(part->posinfo.force);vecZeroVector(part->rotinfo.torque);
    vecZeroVector(part->rotinfo.rotspeed);
    bitClear(part->flags,SOF_Selectable);
    univUpdateObjRotInfo((SpaceObjRot *)part);
}

void cataShipSyncComponents(void)
{
    Node *node;
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *part=(Ship *)listGetStructOfNode(node),*host;
        if(part->shiptype<CataMothershipCommand || part->shiptype>CATA_LAST_COMPONENT ||
           (part->flags&SOF_Dead))continue;
        /* Modules in space are installed hull sections, never independent
           launched ships. Recover older saves/construction paths that left
           one outside without its attachment. */
        if(!part->clampInfo && part->playerowner){
            host=part->playerowner->PlayerMothership;
            if(host && host->shiptype==CataMothership && !(host->flags&SOF_Dead)){
                RemoveShipFromCommand(part);
                cataShipAttachComponent(host,part);
            }
        }
        /* Follow the final host pose even when a module's own physics is
           paused or disabled. Do not integrate independent steering forces. */
        host=cataShipComponentHost(part);
        if(host && !(host->flags&SOF_Dead))cataShipUpdateComponentPose(part,host);
    }
}
void cataShipCreateCoreComponents(Ship *host)
{
    unsigned i;
    if(!host || host->shiptype!=CataMothership)return;
    /* Preserve original light timing/colors and repair behavior, relocating
       only their emitters/contact points to the replacement hull. */
    if(host->staticinfo->navlightStaticInfo)for(i=0;i<(unsigned)host->staticinfo->navlightStaticInfo->numNAVLights;i++){
        NAVLightStatic *light=&host->staticinfo->navlightStaticInfo->navlightstatics[i];
        char semantic[40];unsigned j;
        snprintf(semantic,sizeof(semantic),"nav_%s",light->name);
        for(j=0;semantic[j];j++)if(semantic[j]>='A'&&semantic[j]<='Z')semantic[j]+='a'-'A';
        meshRemasteredMarkerPosition(host,semantic,&light->position);
    }
    if(host->staticinfo->dockStaticInfo)for(i=0;i<(unsigned)host->staticinfo->dockStaticInfo->numDockPoints;i++){
        DockStaticPoint *point=&host->staticinfo->dockStaticInfo->dockstaticpoints[i];
        unsigned ordinal;char semantic[32];
        if(sscanf(point->name,"Repairbots%u",&ordinal)==1){
            snprintf(semantic,sizeof(semantic),"RepairPoint%u",ordinal);
            meshRemasteredMarkerPosition(host,semantic,&point->position);
        }
    }
    for(i=0;i<sizeof(coreComponents)/sizeof(coreComponents[0]);i++)
        if(!GetShipStaticInfoSafe(coreComponents[i],R1))
            dbgFatalf(DBG_Loc,"Missing owned Somtaaw hull component: %s",ShipTypeToStr(coreComponents[i]));
    for(i=0;i<sizeof(coreComponents)/sizeof(coreComponents[0]);i++){
        Ship *child=univAddShip(coreComponents[i],R1,&host->posinfo.position,host->playerowner,0);
        cataShipAttachComponent(host,child);
        gameStatsAddShip(child,0);
        unitCapCreateShip(child,child->playerowner);
    }
}

static void cataStartingFleet(CataStartingFleet *fleet)
{
    char error[512];unsigned i;
    if(!hwCataMission1StartingFleet(fleet,error,sizeof(error)))dbgFatalf(DBG_Loc,"%s",error);
    for(i=0;i<fleet->shipCount;i++){
        ShipType type=StrToShipType(fleet->ships[i].name);
        if(!IS_CATA_SHIP(type) || type==CataMothership || cataShipIsCoreComponent(type))
            dbgFatalf(DBG_Loc,"Unsupported Cataclysm starting fleet identity: %s",fleet->ships[i].name);
    }
}
enum { CATA_KUSHAN_DESTROYER_PAINT = 1u << 4 };

static void cataPreloadShipColors(ShipRace race, ShipType type, unsigned mask)
{
    bool8 schemes[MAX_MULTIPLAYER_PLAYERS];
    ShipStaticInfo *info = GetShipStaticInfo(type, race);
    unsigned slot;

    memcpy(schemes, info->teamColor, sizeof(schemes));
    for (slot = 0; slot < MAX_MULTIPLAYER_PLAYERS; ++slot)
        if (mask & (1u << slot))
            schemes[slot] = TRUE;
    SetTeamColorForShipAndRelatedStaticInfo(type, race, schemes);
}

void cataMission2PreloadShipColors(void)
{
    /* Bushan-Re's script selects the same paint as Mission 1's Kushan
       destroyers. Reserve it before registering the shared hull textures. */
    cataPreloadShipColors(R1, StandardDestroyer, CATA_KUSHAN_DESTROYER_PAINT);
}

void cataMission1PreloadFleet(void)
{
    static const struct {
        ShipRace race;
        ShipType type;
        unsigned mask;
    } sceneColors[] = {
        {R1, Carrier,           1u << 6},
        {R1, HeavyCorvette,     1u << 7},
        {R1, HeavyInterceptor, (1u << 4) | (1u << 7)},
        {R1, IonCannonFrigate,  1u << 6},
        {R1, ProximitySensor,   1u << 6},
        {R1, StandardDestroyer, CATA_KUSHAN_DESTROYER_PAINT},
        {R2, AttackBomber,      1u << 2},
        {R2, Carrier,           1u << 2},
        {R2, HeavyInterceptor, 1u << 2},
        {R2, IonCannonFrigate,  1u << 2},
        {R2, MissileDestroyer,  1u << 2},
        {R2, StandardDestroyer, 1u << 2},
        {R2, StandardFrigate,   1u << 2},
    };
    CataStartingFleet fleet;
    unsigned i;
    cataStartingFleet(&fleet);
    SetInfoNeededForShipAndRelatedStaticInfo(CataMothership,R1,TRUE);
    for(i=0;i<fleet.shipCount;i++)
        SetInfoNeededForShipAndRelatedStaticInfo(StrToShipType(fleet.ships[i].name),R1,TRUE);

    /* The intro and sensor-array script change actor colors after creation.
       Reserve those texture slots before statics load, alongside live colors. */
    for (i=0; i<sizeof(sceneColors)/sizeof(sceneColors[0]); ++i)
        cataPreloadShipColors(sceneColors[i].race, sceneColors[i].type, sceneColors[i].mask);
}
void cataMission1CreateFleet(hvector *startPoint)
{
    CataStartingFleet fleet;unsigned i,n;Ship *host,*ship;Player *player=&universe.players[0];
    cataStartingFleet(&fleet);
    host=univAddShip(CataMothership,R1,(vector *)startPoint,player,0);
    univRotateObjYaw((SpaceObjRot *)host,DEG_TO_RAD(startPoint->w));
    host->shipSinglePlayerGameInfo->midlevelHyperspaceTo=*startPoint;
    gameStatsAddShip(host,0);unitCapCreateShip(host,player);player->PlayerMothership=host;
    player->resourceUnits=fleet.resources;
    for(i=0;i<fleet.shipCount;i++)for(n=0;n<fleet.ships[i].count;n++){
        ship=univAddShip(StrToShipType(fleet.ships[i].name),R1,&host->posinfo.position,player,0);
        gameStatsAddShip(ship,0);unitCapCreateShip(ship,player);
        if(!cataShipAttachComponent(host,ship))
            GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,host);
    }
    clPresetShipsToPosition(&universe.mainCommandLayer);
}
SpaceObjRotImpTarg *cataShipDamageTarget(SpaceObjRotImpTarg *target)
{
    if(target && target->objtype==OBJ_ShipType){
        Ship *child=(Ship *)target;
        if(cataShipIsCoreComponent(child->shiptype) && child->clampInfo && child->clampInfo->host &&
            child->clampInfo->host->objtype==OBJ_ShipType &&
            ((Ship *)child->clampInfo->host)->shiptype==CataMothership)
            return child->clampInfo->host;
    }
    return target;
}
void cataShipHostRemoved(Ship *host)
{
    Node *node,*next;
    if(!host || host->shiptype!=CataMothership)return;
    for(node=universe.ShipList.head;node;node=next){
        Ship *child=(Ship *)listGetStructOfNode(node);next=node->next;
        if(child!=host && IS_CATA_SHIP(child->shiptype) && child->clampInfo &&
            child->clampInfo->host==(SpaceObjRotImpTarg *)host){
            unClampObj((SpaceObjRotImpTargGuidance *)child);
            if(!bitTest(child->flags,SOF_Dead))univReallyDeleteThisShipRightNow(child);
        }
    }
}
