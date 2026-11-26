/* Typed imports of the Cataclysm salvage state machine (004faad0), target
   selection/reservation, tow approach and message-pod processing. Runtime
   fields use saved numeric identities, never retail offsets or pointers. */
#include "CataclysmSalvage.h"
#include "CataclysmCampaign.h"
#include "CataclysmLinking.h"
#include "Universe.h"
#include "AIVar.h"
#include "Alliance.h"
#include "AIShip.h"
#include "AITrack.h"
#include "CommandLayer.h"
#include "CommandDefs.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "Clamp.h"
#include "Dock.h"
#include "MeshAnim.h"
#include "UnivUpdate.h"
#include "StatScript.h"
#include "FastMath.h"
#include "Debug.h"
#include "ETG.h"
#include "Attack.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

static real32 salvageDamageLimit[NUM_CLASSES];
static real32 salvageTransferSpeed,salvageTransferMaxTime;
static struct CataSalvagerStatic {AttackSideStepParameters attack;real32 approachDistance;} salvagerStatics[NUM_RACES];
static bool32 salvageActor(const Ship *ship)
{return ship->shiptype==CataWorker || ship->shiptype==SalCapCorvette || ship->shiptype==P1StandardCorvette;}
bool32 cataSalvageHandles(const Ship *ship)
{return ship && salvageActor(ship) && singlePlayerGame && hwIsCataclysmMission(spGetCurrentMission());}
static sdword salvageValue(udword id,const char *field,bool32 write,sdword value)
{
    char name[48];AIVar *var;snprintf(name,sizeof(name),"CS%u_%s",(unsigned)id,field);
    var=aivarFind(name);if(write){if(!var)var=aivarCreate(name);aivarValueSet(var,value);}
    return var?aivarValueGet(var):0;
}
static sdword sv(Ship *ship,const char *field,bool32 write,sdword value)
{return salvageValue(ship->shipID.shipNumber,field,write,value);}
static real32 sf(Ship *ship,const char *field,bool32 write,real32 value)
{
    sdword bits;if(write){memcpy(&bits,&value,4);sv(ship,field,TRUE,bits);}
    bits=sv(ship,field,FALSE,0);memcpy(&value,&bits,4);return value;
}
static sdword salvageID(Ship *ship){return ship?(sdword)(ship->shipID.shipNumber+1):0;}
static Ship *salvageFind(sdword id)
{
    Node *node;if(id<=0)return NULL;
    for(node=universe.ShipList.head;node;node=node->next){Ship *ship=listGetStructOfNode(node);
        if(salvageID(ship)==id)return ship;}
    return NULL;
}
static Ship *salvageTarget(Ship *ship){return salvageFind(sv(ship,"target",FALSE,0));}
static DockStaticPoint *salvagePort(Ship *target,sdword index)
{
    DockStaticInfo *info=target?target->staticinfo->dockStaticInfo:NULL;
    return info && index>=0 && index<info->numDockPoints?&info->dockstaticpoints[index]:NULL;
}
static sdword salvageCapacity(Ship *target)
{
    DockStaticInfo *info=target->staticinfo->dockStaticInfo;sdword i,count=0;
    if(info)for(i=0;i<info->numDockPoints;i++)
        if(info->dockstaticpoints[i].type==DPT_Salvage || info->dockstaticpoints[i].type==DPT_SalvageLatch)++count;
    return count;
}
static sdword salvageClaims(Ship *target,Player *owner,sdword change)
{
    char key[20];sdword count;snprintf(key,sizeof(key),"claims%u",(unsigned)owner->playerIndex);
    count=sv(target,key,FALSE,0);if(change)count=sv(target,key,TRUE,max(0,count+change));return count;
}
bool32 cataSalvageCanTarget(SpaceObjRotImpTarg *target, Player *owner)
{
    Ship *ship;
    if (!target || target->objtype != OBJ_ShipType ||
        (target->flags & (SOF_Dead | SOF_Cloaked | SOF_Hide | SOF_Hyperspace)))
        return FALSE;
    ship = (Ship *)target;
    /* Mission 2 can transfer the beacon to the player before it is towed. */
    return ship->playerowner && salvageCapacity(ship) > 0 &&
        (ship->shiptype == CataMessagePod || !allianceIsShipAlly(ship, owner));
}
static bool32 salvageWeakEnough(Ship *target)
{
    ShipClass cls=target->staticinfo->shipclass;
    return target->shiptype==CataMessagePod || (cls<NUM_CLASSES &&
        target->health<=salvageDamageLimit[cls]*target->staticinfo->maxhealth);
}
static Ship *salvagePick(Ship *ship,SpecialCommand *selection,sdword *status)
{
    sdword i,best=-1,rejected=-1;real32 closest=1.0e21f;*status=0;
    if(!selection)return NULL;
    for(i=0;i<selection->numTargets;i++){
        Ship *candidate=(Ship*)selection->TargetPtr[i];sdword claims,capacity;vector delta;real32 distance;
        if(!cataSalvageCanTarget(selection->TargetPtr[i],ship->playerowner))continue;
        capacity=salvageCapacity(candidate);claims=salvageClaims(candidate,ship->playerowner,0);
        if(capacity==claims){if(!*status){rejected=i;*status=1;}continue;}
        if(!salvageWeakEnough(candidate)){if(!*status){rejected=i;*status=2;}continue;}
        if(claims>0 && claims<capacity){best=i;continue;}
        vecSub(delta,candidate->posinfo.position,ship->posinfo.position);distance=vecMagnitudeSquared(delta);
        if(distance<closest){closest=distance;best=i;}
    }
    if(best>=0){*status=0;return (Ship*)selection->TargetPtr[best];}
    return rejected>=0?(Ship*)selection->TargetPtr[rejected]:NULL;
}
static sdword salvageReserve(Ship *target)
{
    DockStaticInfo *info=target->staticinfo->dockStaticInfo;sdword i,best=-1;
    if(!info || !target->dockInfo)return -1;
    for(i=0;i<info->numDockPoints;i++){
        DockPointType type=info->dockstaticpoints[i].type;
        if(target->dockInfo->dockpoints[i].thisDockBusy)continue;
        if(type==DPT_SalvageLatch){best=i;break;}
        if(type==DPT_Salvage)best=i;
    }
    if(best>=0)target->dockInfo->dockpoints[best].thisDockBusy++;
    return best;
}
static void salvageReleasePort(Ship *host,sdword port)
{
    if(host && host->dockInfo && port>=0 && port<host->dockInfo->numDockPoints &&
       host->dockInfo->dockpoints[port].thisDockBusy)host->dockInfo->dockpoints[port].thisDockBusy--;
}
static sdword salvageAttached(Ship *target,Player *owner)
{
    Node *node;sdword count=0;
    /* The host clamp ABI has no reverse selection: walk its actual children. */
    for(node=universe.ShipList.head;node;node=node->next){Ship *ship=listGetStructOfNode(node);
        if(salvageActor(ship) && ship->playerowner==owner && ship->clampInfo &&
           ship->clampInfo->host==(SpaceObjRotImpTarg*)target)++count;}
    return count;
}
static void salvageStopEffect(Ship *ship)
{
    if(ship->rceffect){etgEffectDelete(ship->rceffect);univRemoveObjFromRenderList((SpaceObj*)ship->rceffect);
        listDeleteNode(&ship->rceffect->objlink);ship->rceffect=NULL;}
}
static bool32 salvageWorkerEffectOrigin(Ship *ship,vector *position)
{
    vector grip;
    if(!cataDockWorkerGrapplePosition(ship,&grip))return FALSE;
    /* The owned mesh's rim is at x=-35.9, centered near z=-12.7.
       Align it with the pads and leave two units of depth clearance
       on the Worker's side of the beacon surface. */
    grip.x+=37.9f;grip.z+=12.7f;
    matMultiplyMatByVec(position,&ship->rotinfo.coordsys,&grip);
    vecAddTo(*position,ship->posinfo.position);
    return TRUE;
}

static void salvageStartEffect(Ship *ship)
{
    DockStaticPoint *port;
    etgeffectstatic *effect;const char *name;real32 originalRace;
    vector origin,stationary={0};bool32 onPads;
    if(ship->rceffect)return;
    port=salvagePort(salvageTarget(ship),sv(ship,"port",FALSE,0)-1);
    if(port && port->type==DPT_SalvageLatch && ship->shiptype!=CataWorker)return;
    if(ship->shiptype==CataWorker){name="workersalvage";originalRace=0;}
    else if(ship->shiprace==P1){name="p1salvage";originalRace=4;}
    else if(ship->shiprace==R2){name="salvage";originalRace=3;}
    else{name="r1salvage";originalRace=2;}
    {char path[128];snprintf(path,sizeof(path),"Cataclysm\\ETG\\Cata_%s.ebg",name);effect=etgEffectStaticFind(path,FALSE);}
    if(!effect || etgFrequencyExceeded(effect))return;
    onPads=ship->shiptype==CataWorker && salvageWorkerEffectOrigin(ship,&origin);
    ship->rceffect=etgEffectCreate(effect,ship,onPads?&origin:NULL,onPads?&stationary:NULL,NULL,
        originalRace,EAF_Coordsys|EAF_NLips|(onPads?0:EAF_Position),0);
}
enum { WORKER_JACK_OUT=0, WORKER_JACK_CLOSE, WORKER_DOCK_CLOSE, WORKER_DOCK_OPEN };
/* Earlier saves used folded or downward pad geometry for this contact. */
enum { WORKER_OUTWARD_GRAPPLE_CONTACT=3 };

static void salvageWorkerPlay(Ship *ship,sdword clip)
{
    if(ship->madBindings->bPaused)madAnimationPause(ship,FALSE);
    madAnimationStart(ship,clip);
}

/* Retail 004638f0 selects DockOpen. Clear a harvesting/repair pose before
   switching to the grapples, including saves made with the old JackOut latch. */
static bool32 salvageWorkerOpenGrapples(Ship *ship)
{
    madanim *anim=ship->madBindings;
    if(ship->shiptype!=CataWorker || !anim || !anim->header ||
       anim->header->nAnimations<=WORKER_DOCK_OPEN)return TRUE;
    switch(anim->nCurrentAnim){
    case WORKER_DOCK_OPEN:
        if(!anim->bPaused)return FALSE;
        if(anim->time>=anim->header->anim[WORKER_DOCK_OPEN].endTime)return TRUE;
        madAnimationPause(ship,FALSE);
        return FALSE;
    case WORKER_JACK_OUT:salvageWorkerPlay(ship,WORKER_JACK_CLOSE);return FALSE;
    case WORKER_JACK_CLOSE:
    case WORKER_DOCK_CLOSE:return FALSE;
    default:salvageWorkerPlay(ship,WORKER_DOCK_OPEN);return FALSE;
    }
}

void cataSalvageCancel(Ship *ship)
{
    Ship *target=salvageTarget(ship);sdword port=sv(ship,"port",FALSE,0)-1;
    if(ship->clampInfo && (!target || ship->clampInfo->host==(SpaceObjRotImpTarg*)target))
        unClampObj((SpaceObjRotImpTargGuidance*)ship);
    if(target){
        salvageReleasePort(target,port);salvageClaims(target,ship->playerowner,-1);
        if(!salvageAttached(target,ship->playerowner) && !sv(target,"queued",FALSE,0)){
            Ship *host=salvageFind(sv(ship,"host",FALSE,0));
            salvageReleasePort(host,sv(target,"hostPort",FALSE,0)-1);
            sv(target,"hostPort",TRUE,0);
            if(target->dockingship==host)target->dockingship=NULL;
            sv(target,"owner",TRUE,0);
            if(sv(target,"disabledByTow",FALSE,0)){bitClear(target->flags,SOF_Disabled);sv(target,"disabledByTow",TRUE,0);}
            if(sv(target,"selectableBeforeTow",FALSE,0))bitSet(target->flags,SOF_Selectable);
        }
    }
    if(ship->dockingship==target)ship->dockingship=NULL;
    salvageStopEffect(ship);
    if(ship->shiptype==CataWorker && ship->madBindings && ship->madBindings->header){
        madanim *anim=ship->madBindings;
        if(anim->nCurrentAnim==WORKER_DOCK_OPEN && anim->header->nAnimations>WORKER_DOCK_CLOSE)
            salvageWorkerPlay(ship,WORKER_DOCK_CLOSE);
        else if(anim->nCurrentAnim==WORKER_JACK_OUT && anim->header->nAnimations>WORKER_JACK_CLOSE)
            salvageWorkerPlay(ship,WORKER_JACK_CLOSE);
    }
    cataDockSalvageFinish(ship);sv(ship,"target",TRUE,0);sv(ship,"port",TRUE,0);
    sv(ship,"state",TRUE,0);sv(ship,"host",TRUE,0);
    if(sv(ship,"grapple",FALSE,0))sv(ship,"grapple",TRUE,0);
}
void cataSalvageRemoveReference(Ship *ship,Ship *removed)
{
    if(!removed)return;
    if(sv(ship,"target",FALSE,0)==salvageID(removed))cataSalvageCancel(ship);
    else if(sv(ship,"host",FALSE,0)==salvageID(removed)){
        Ship *target=salvageTarget(ship);
        if(target){salvageReleasePort(removed,sv(target,"hostPort",FALSE,0)-1);sv(target,"hostPort",TRUE,0);}
        sv(ship,"host",TRUE,0);sv(ship,"state",TRUE,8);
    }
}
static sdword salvageProcessingHostID(bool32 set,sdword id)
{
    AIVar *var=aivarFind("CataSalvageHost");
    if(set){if(!var)var=aivarCreate("CataSalvageHost");aivarValueSet(var,id);}
    return var?aivarValueGet(var):0;
}
void cataSalvageSetProcessingHost(Ship *host){salvageProcessingHostID(TRUE,salvageID(host));}
static bool32 salvageCanProcess(Ship *host,Ship *target)
{
    DockStaticInfo *info;sdword i;
    if(host->flags&(SOF_Dead|SOF_Disabled|SOF_Hyperspace))return FALSE;
    if(host->shiptype!=CataMothership && host->shiptype!=CataProcessor &&
       host->shiptype!=P1Mothership && !(host->shiptype==Carrier&&host->shiprace==R2))return FALSE;
    if(target->shiptype!=CataMessagePod)return TRUE;
    info=host->staticinfo->dockStaticInfo;
    if(info)for(i=0;i<info->numDockPoints;i++)
        if(info->dockstaticpoints[i].type==DPT_Med_In || info->dockstaticpoints[i].type==DPT_Med_In_Engineering)return TRUE;
    return FALSE;
}
static Ship *salvageNearestHost(Ship *ship,Ship *target)
{
    Node *node;Ship *best=NULL;real32 nearest=REALlyBig;
    if(ship->playerowner->playerIndex==1){
        Ship *assigned=salvageFind(salvageProcessingHostID(FALSE,0));
        if(assigned && salvageCanProcess(assigned,target))return assigned;
    }
    for(node=universe.ShipList.head;node;node=node->next){Ship *host=listGetStructOfNode(node);vector delta;real32 distance;
        if(host->playerowner!=ship->playerowner || !(host->flags&SOF_Selectable) || !salvageCanProcess(host,target))continue;
        vecSub(delta,target->posinfo.position,host->posinfo.position);distance=vecMagnitudeSquared(delta);
        if(distance<nearest){nearest=distance;best=host;}
    }
    return best;
}
static void salvageDampen(Ship *target)
{vecMultiplyByScalar(target->posinfo.velocity,.3f);vecMultiplyByScalar(target->rotinfo.rotspeed,.3f);}
static bool32 salvageMaybeReassign(Ship *ship,Ship *target,Ship *host)
{
    Ship *nearest;vector oldDelta,newDelta;Node *node;real32 difference;
    if(sf(target,"reassign",FALSE,0)+10.0f>=universe.totaltimeelapsed)return FALSE;
    sf(target,"reassign",TRUE,universe.totaltimeelapsed);nearest=salvageNearestHost(ship,target);
    if(!nearest || nearest==host)return FALSE;
    vecSub(oldDelta,target->posinfo.position,host->posinfo.position);
    vecSub(newDelta,target->posinfo.position,nearest->posinfo.position);
    difference=fabsf(fsqrt(vecMagnitudeSquared(oldDelta))-fsqrt(vecMagnitudeSquared(newDelta)));
    if(difference<=1000.0f)return FALSE;
    salvageReleasePort(host,sv(target,"hostPort",FALSE,0)-1);sv(target,"hostPort",TRUE,0);
    if(target->dockingship==host)target->dockingship=NULL;
    for(node=universe.ShipList.head;node;node=node->next){Ship *actor=listGetStructOfNode(node);
        if(salvageActor(actor) && actor->clampInfo && actor->clampInfo->host==(SpaceObjRotImpTarg*)target){
            sv(actor,"host",TRUE,salvageID(nearest));sv(actor,"state",TRUE,9);}}
    return TRUE;
}
static bool32 salvageReached(Ship *ship,const vector *position,real32 distance)
{return MoveReachedDestinationVariable(ship,(vector*)position,distance);}
static void salvageHalt(Ship *target)
{SelectCommand selection={0};selection.numShips=1;selection.ShipPtr[0]=target;clHalt(&universe.mainCommandLayer,&selection);}
static bool32 salvageHasBay(Ship *host)
{
    Node *node;for(node=universe.ShipList.head;node;node=node->next){Ship *part=listGetStructOfNode(node);
        if(part->shiptype==CataMothershipDockingBay && !(part->flags&SOF_Dead) && part->clampInfo &&
           part->clampInfo->host==(SpaceObjRotImpTarg*)host)return TRUE;}
    return FALSE;
}
static bool32 salvageTow(Ship *target,Ship *host)
{
    sdword port=sv(target,"hostPort",FALSE,0)-1;DockStaticInfo *info=host->staticinfo->dockStaticInfo;
    DockStaticPoint intake;
    vector destination,delta,outward;
    if(!salvageReached(target,&host->posinfo.position,6000)){
        real32 speed=0.0f;
        if(target->shiptype==CataMessagePod){
            /* Original 004fa410 and 00514940 scale the pod's own speed by its
               experience and hull condition on the long leg of the tow. */
            const real32 health=target->health/target->staticinfo->maxhealth;
            real32 factor=1.0f+0.1f*min(target->veterancyRank,4);
            if(health<0.1f)factor-=0.6f;
            else if(health<0.25f)factor-=0.4f;
            else if(health<0.5f)factor-=0.2f;
            else if(health<0.75f)factor-=0.1f;
            speed=target->staticinfo->staticheader.maxvelocity*factor;
            if(target->dockingship==host)target->dockingship=NULL;
            delta=target->posinfo.velocity;
            if(vecMagnitudeSquared(delta)>0.0f){vecNormalize(&delta);aitrackHeading(target,&delta,0.999f);}
        }
        aishipFlyToPointAvoidingObjsWithVel(target,&host->posinfo.position,0,speed,NULL);return FALSE;
    }
    if(target->shiptype!=CataMessagePod)return TRUE;
    if(port>=0 && !salvagePort(host,port)){
        sv(target,"hostPort",TRUE,0);port=-1;
    }
    if(port<0){
        sdword i;DockPointType wanted=host->shiptype==CataMothership && !salvageHasBay(host)?DPT_Med_In_Engineering:DPT_Med_In;
        if(info && host->dockInfo)for(i=0;i<info->numDockPoints;i++)
            if(info->dockstaticpoints[i].type==wanted && !host->dockInfo->dockpoints[i].thisDockBusy){
                port=i;host->dockInfo->dockpoints[i].thisDockBusy++;sv(target,"hostPort",TRUE,port+1);break;}
    }
    if(port<0){aitrackForceSteadyShip(target);return FALSE;}
    if(!cataDockSalvageIntakePoint(host,port,&intake))return FALSE;
    vecScalarMultiply(destination,intake.conenormal,intake.maxdist);
    vecAddTo(destination,intake.position);
    matMultiplyMatByVec(&destination,&host->rotinfo.coordsys,&destination);vecAddTo(destination,host->posinfo.position);
    if(salvageReached(target,&destination,50))return TRUE;
    /* On the intake side of the ship the original tow allows the pod to
       approach the bay directly. From the far side it still flies around. */
    vecSub(outward,destination,host->posinfo.position);
    vecSub(delta,target->posinfo.position,host->posinfo.position);
    target->dockingship=vecDotProduct(outward,delta)>0.0f?host:NULL;
    aishipFlyToPointAvoidingObjsWithVel(target,&destination,2,0,&host->posinfo.velocity);return FALSE;
}
static bool32 salvageEnqueue(Ship *ship,Ship *target,Ship *host)
{
    sdword i;char key[16];Node *node;
    if(sv(target,"queued",FALSE,0)){
        if(salvageFind(sv(target,"queueHost",FALSE,0)))return TRUE;
        sv(target,"queued",TRUE,0);sv(target,"hostPort",TRUE,0);sv(target,"process",TRUE,0);
    }
    for(i=0;i<16;i++){snprintf(key,sizeof(key),"queue%d",i);if(!sv(host,key,FALSE,0))break;}
    if(i==16)return FALSE;
    sv(host,key,TRUE,salvageID(target));sv(target,"queueHost",TRUE,salvageID(host));
    sv(target,"queued",TRUE,1);sv(target,"process",TRUE,0);
    sf(target,"processStart",TRUE,universe.totaltimeelapsed);
    sf(target,"processEnd",TRUE,universe.totaltimeelapsed+
        min(target->staticinfo->maxhealth/salvageTransferSpeed,salvageTransferMaxTime));
    /* Retail removes the captured hull from every salvager's target list
       before host cleanup releases the clamps, preventing reacquisition. */
    for(node=universe.ShipList.head;node;node=node->next){Ship *actor=listGetStructOfNode(node);CommandToDo *command=actor->command;
        if(salvageActor(actor) && command && command->ordertype.order==COMMAND_SPECIAL && command->specialtargets){
            SpecialCommand *targets=command->specialtargets;sdword index=0;
            while(index<targets->numTargets){
                if(targets->TargetPtr[index]==(SpaceObjRotImpTarg*)target)targets->TargetPtr[index]=targets->TargetPtr[--targets->numTargets];
                else ++index;
            }
        }
    }
    return TRUE;
}
bool32 cataSalvageSpecial(Ship *ship,SpecialCommand *targets,real32 approachDistance)
{
    sdword state=sv(ship,"state",FALSE,0),port;Ship *target=salvageTarget(ship),*host;
    if(!targets || !cataCampaignShipHasUpgrade(ship,CATA_UPGRADE_SALVAGE))return TRUE;
    if(target && target->clampInfo && state>=2){cataSalvageCancel(ship);return FALSE;}
    if(state && state!=14 && !target){cataSalvageCancel(ship);return FALSE;}
    if(ship->shiptype==CataWorker && state>=8 && state<=13 && ship->clampInfo &&
       ship->clampInfo->host==(SpaceObjRotImpTarg*)target){
        if(sv(ship,"grapple",FALSE,0)!=WORKER_OUTWARD_GRAPPLE_CONTACT){
            cataDockRestoreSalvageContact(ship,target,sv(ship,"port",FALSE,0)-1);
            sv(ship,"grapple",TRUE,WORKER_OUTWARD_GRAPPLE_CONTACT);
        }
        if(!salvageWorkerOpenGrapples(ship))return FALSE;
        salvageStartEffect(ship); /* Effects are rebuilt on resume. */
    }
    switch(state){
    case 0:{sdword status;target=salvagePick(ship,targets,&status);
        if(!target){sv(ship,"state",TRUE,14);return FALSE;}
        if(status){
            if(salvageReached(ship,&target->posinfo.position,approachDistance*2))aitrackForceSteadyShip(ship);
            else aishipFlyToPointAvoidingObjsWithVel(ship,&target->posinfo.position,2,0,NULL);
            return FALSE;
        }
        salvageClaims(target,ship->playerowner,1);sv(ship,"target",TRUE,salvageID(target));sv(ship,"state",TRUE,1);return FALSE;}
    case 1:
        aishipFlyToPointAvoidingObjsWithVel(ship,&target->posinfo.position,2,0,NULL);
        if(salvageReached(ship,&target->posinfo.position,approachDistance)){
            sdword owner=sv(target,"owner",FALSE,0);
            if(!owner || owner==ship->playerowner->playerIndex+1){sv(target,"owner",TRUE,ship->playerowner->playerIndex+1);sv(ship,"state",TRUE,2);}
            else aitrackForceSteadyShip(ship);
        }return FALSE;
    case 2:
        port=salvageReserve(target);
        if(port<0)cataSalvageCancel(ship);else{sv(ship,"port",TRUE,port+1);sv(ship,"state",TRUE,3);}return FALSE;
    case 3:case 4:case 5:{
        bool32 grappleReady=state!=5 || salvageWorkerOpenGrapples(ship);
        port=sv(ship,"port",FALSE,0)-1;
        if(!salvagePort(target,port)){cataSalvageCancel(ship);return FALSE;}
        if(cataDockSalvageStep(ship,target,port,state-3) && grappleReady){
            if(state==3)ship->dockingship=target;
            if(state==5 && ship->shiptype==CataWorker)
                sv(ship,"grapple",TRUE,WORKER_OUTWARD_GRAPPLE_CONTACT);
            sv(ship,"state",TRUE,state==5?7:state+1);
        }
        salvageDampen(target);return FALSE;}
    case 7:
        if(ship->shiptype==CataWorker &&
           (sv(ship,"grapple",FALSE,0)!=WORKER_OUTWARD_GRAPPLE_CONTACT ||
            !salvageWorkerOpenGrapples(ship))){
            /* A pre-fix save can already be at the old contact position. */
            sv(ship,"state",TRUE,5);return FALSE;
        }
        clampObjToObj((SpaceObjRotImpTargGuidance*)ship,(SpaceObjRotImpTargGuidance*)target);
        salvageStartEffect(ship);
        salvageHalt(target);sv(ship,"state",TRUE,8); /* Original falls through. */
    case 8:
        host=salvageNearestHost(ship,target);sv(ship,"host",TRUE,salvageID(host));
        if(!host){cataSalvageCancel(ship);return FALSE;}
        sf(target,"reassign",TRUE,universe.totaltimeelapsed);sv(ship,"state",TRUE,9);return FALSE;
    case 9:
        if(salvageAttached(target,ship->playerowner)>=salvageCapacity(target)){
            sv(target,"selectableBeforeTow",TRUE,(target->flags&SOF_Selectable)!=0);
            bitClear(target->flags,SOF_Selectable);sv(ship,"state",TRUE,10);
        }return FALSE;
    case 10:
        if(salvageAttached(target,ship->playerowner)<salvageCapacity(target)){sv(ship,"state",TRUE,9);return FALSE;}
        host=salvageFind(sv(ship,"host",FALSE,0));if(!host){sv(ship,"state",TRUE,8);return FALSE;}
        if(salvageMaybeReassign(ship,target,host))return FALSE;
        if(target->command && target->command!=ship->command){
            SelectCommand cargo={0};
            cargo.numShips=1;cargo.ShipPtr[0]=target;
            /* Retail clears the cargo's orders at tow start (00444810).
               A queued Halt applies braking every tick of the tow. */
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer,&cargo);
        }
        if(!(target->flags&SOF_Disabled)){bitSet(target->flags,SOF_Disabled);sv(target,"disabledByTow",TRUE,1);}
        if(salvageTow(target,host))sv(ship,"state",TRUE,11);return FALSE;
    case 11:
        vecZeroVector(target->posinfo.velocity);vecZeroVector(target->rotinfo.rotspeed);sv(ship,"state",TRUE,12);return FALSE;
    case 12:
        host=salvageFind(sv(ship,"host",FALSE,0));if(!host){sv(ship,"state",TRUE,8);return FALSE;}
        if(salvageEnqueue(ship,target,host))sv(ship,"state",TRUE,13);return FALSE;
    case 13:return FALSE;
    case 14:cataSalvageCancel(ship);return TRUE;
    default:return TRUE;
    }
}
sdword cataSalvageProcessedCount(ShipType type,sdword owner)
{
    sdword i,total=0;if(owner<0 || owner>=MAX_MULTIPLAYER_PLAYERS)return 0;
    if(type==(ShipType)-1){char key[24];snprintf(key,sizeof(key),"processed%d",owner);
        for(i=0;i<TOTAL_NUM_SHIPS;i++)total+=salvageValue(i,key,FALSE,0);return total;}
    {char key[24];snprintf(key,sizeof(key),"processed%d",owner);return salvageValue((udword)type,key,FALSE,0);}
}
static void salvageReleaseWorkers(Ship *target,Ship *host)
{
    Node *node;for(node=universe.ShipList.head;node;node=node->next){Ship *actor=listGetStructOfNode(node);
        if(salvageActor(actor) && sv(actor,"target",FALSE,0)==salvageID(target)){
            CommandToDo *command=actor->command;bool32 remaining=command && command->ordertype.order==COMMAND_SPECIAL &&
                command->specialtargets && command->specialtargets->numTargets;
            cataSalvageCancel(actor);
            if(!remaining)GroupShipIntoMilitaryParade(&universe.mainCommandLayer,actor,host);
        }
    }
}
/* Non-Beast branch of 004fc8c0 and 00543950. Mission 2's raider capture
   keeps the original hull identity. The Beast conversion table and slot
   morph renderer are separate imports; do not reinterpret host race IDs. */
static Ship *salvageReplaceCapturedShip(Ship *source,Ship *host)
{
    Ship *replacement=univAddShip(source->shiptype,source->shiprace,
        &source->posinfo.position,host->playerowner,0);
    if(!replacement)return NULL;
    replacement->posinfo.velocity=source->posinfo.velocity;
    replacement->posinfo.force=source->posinfo.force;
    replacement->posinfo.isMoving=source->posinfo.isMoving;
    replacement->rotinfo.coordsys=source->rotinfo.coordsys;
    replacement->health=min(source->health,replacement->staticinfo->maxhealth);
    replacement->lastTimeRepaired=0;
    replacement->resources=min(source->resources,replacement->staticinfo->maxresources);
    cataCampaignCopyShipUpgrades(replacement,source);
    unitCapCreateShip(replacement,host->playerowner);
    univUpdateObjRotInfo((SpaceObjRot*)replacement);
    univReallyRemoveShipForTransformation(source);
    return replacement;
}
static void salvageCountProcessed(ShipType type,Player *owner)
{
    char key[24];snprintf(key,sizeof(key),"processed%d",owner->playerIndex);
    salvageValue((udword)type,key,TRUE,salvageValue((udword)type,key,FALSE,0)+1);
}
void cataSalvageUpdate(void)
{
    Node *node;
    if(!singlePlayerGame || !hwIsCataclysmMission(spGetCurrentMission()))return;
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *host=listGetStructOfNode(node);sdword i;char key[16];
        if(host->shiptype==CataWorker && host->clampInfo && host->rceffect &&
           !bitTest(host->rceffect->flags,EAF_Position) &&
           sv(host,"grapple",FALSE,0)==WORKER_OUTWARD_GRAPPLE_CONTACT){
            /* The worker has followed the rotating tow target this tick. */
            salvageWorkerEffectOrigin(host,&host->rceffect->posinfo.position);
        }
        if(host->shiptype!=CataMothership && host->shiptype!=CataProcessor && host->shiptype!=P1Mothership && host->shiptype!=Carrier)continue;
        for(i=0;i<16;i++){
            sdword state,port,id;Ship *target;snprintf(key,sizeof(key),"queue%d",i);id=sv(host,key,FALSE,0);if(!id)continue;target=salvageFind(id);
            if(!target){sv(host,key,TRUE,0);continue;}
            state=sv(target,"process",FALSE,0);port=sv(target,"hostPort",FALSE,0)-1;
            if(target->shiptype!=CataMessagePod){
                /* 004f9500: stage zero chooses an available owner color;
                   stage one waits for the queue's owned transfer deadline. */
                if(state==0){
                    sdword color=host->playerowner->playerIndex;
                    salvageReleaseWorkers(target,host);
                    if(color>=MAX_MULTIPLAYER_PLAYERS || !target->staticinfo->teamColor[color]){
                        for(color=0;color<MAX_MULTIPLAYER_PLAYERS && !target->staticinfo->teamColor[color];color++);
                        if(color==MAX_MULTIPLAYER_PLAYERS)color=0;
                    }
                    sv(target,"captureColor",TRUE,color);sv(target,"process",TRUE,1);
                }else if(state==1 && universe.totaltimeelapsed>=sf(target,"processEnd",FALSE,0)){
                    sdword color=sv(target,"captureColor",FALSE,0);ShipType type=target->shiptype;Ship *replacement;
                    sv(host,key,TRUE,0); /* Delete callbacks must not see a live queue reference. */
                    replacement=salvageReplaceCapturedShip(target,host);
                    if(replacement){replacement->colorScheme=(sbyte)color;salvageCountProcessed(type,host->playerowner);}
                }
                continue;
            }
            if(!salvagePort(host,port))continue;
            if(!sv(target,"intakeCourse",FALSE,0)){
                /* A saved beacon on the old course must reacquire the cone
                   before testing for crossing the relocated bay entrance. */
                if(state>=2){cataDockSalvageFinish(target);state=sv(target,"process",TRUE,1);}
                sv(target,"intakeCourse",TRUE,1);
            }
            target->shipidle=FALSE;target->autostabilizeship=FALSE;target->dockingship=host;
            if(state==0){salvageReleaseWorkers(target,host);sv(target,"process",TRUE,1);continue;}
            if(state>=1 && state<=3){
                if(cataDockSalvageStep(target,host,port,state==3?3:state-1))sv(target,"process",TRUE,state+1);
                continue;
            }
            if(state==4){
                Node *workerNode;
                salvageReleasePort(host,port);sv(target,"hostPort",TRUE,0);sv(host,key,TRUE,0);
                for(workerNode=universe.ShipList.head;workerNode;workerNode=workerNode->next){Ship *worker=listGetStructOfNode(workerNode);
                    if(salvageActor(worker) && sv(worker,"target",FALSE,0)==salvageID(target))cataSalvageCancel(worker);}
                salvageCountProcessed(target->shiptype,host->playerowner);
                cataDockSalvageFinish(target);univReallyDeleteThisShipRightNow(target);
            }
        }
    }
}
void cataSalvageLoadStatics(void)
{
    sdword i;scriptEntry entries[NUM_CLASSES+1];char names[NUM_CLASSES][64];
    scriptEntry transferEntries[]={
        {"twSalTransSpeed",scriptSetReal32CB,&salvageTransferSpeed},
        {"twSalTransMaxTime",scriptSetReal32CB,&salvageTransferMaxTime},
        {NULL,NULL,NULL}};
    memset(salvageDamageLimit,0,sizeof(salvageDamageLimit));
    for(i=0;i<NUM_CLASSES;i++){
        snprintf(names[i],sizeof(names[i]),"twSalvageDamageLimit[%s]",ShipClassToStr(i));
        entries[i].name=names[i];entries[i].setVarCB=scriptSetReal32CB;entries[i].dataPtr=&salvageDamageLimit[i];
    }
    memset(&entries[NUM_CLASSES],0,sizeof(entries[NUM_CLASSES]));scriptSet("Cataclysm\\","Tweak.script",entries);
    scriptSet("Cataclysm\\","Tweak.script",transferEntries);
    if(salvageTransferSpeed<=0 || salvageTransferMaxTime<=0)
        dbgFatalf(DBG_Loc,"Owned salvage transfer timing is missing");
}
void cataSalvageSetActorMode(Ship *ship,sdword mode){sv(ship,"mode",TRUE,mode);}
static void salvageGenericInit(Ship *ship)
{
    attackSideStepInit((AttackSideStep*)ship->ShipSpecifics);sv(ship,"state",TRUE,0);sv(ship,"target",TRUE,0);
    sv(ship,"port",TRUE,0);sv(ship,"mode",TRUE,0);cataCampaignInitializeShipUpgrades(ship);
}
static void salvageGenericStaticInit(char *directory,char *filename,ShipStaticInfo *info)
{
    struct CataSalvagerStatic *data=&salvagerStatics[info->shiprace];
    scriptStructEntry distance[]={
        {"salvageFlyToDistance",scriptSetReal32CB,offsetof(struct CataSalvagerStatic,approachDistance),0},
        {NULL,NULL,0,0}};
    memset(data,0,sizeof(*data));
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,&data->attack);
    scriptSetStruct(directory,filename,distance,data);info->custstatinfo=data;cataSalvageLoadStatics();
    if(data->approachDistance<=0)dbgFatalf(DBG_Loc,"Owned salvager approach distance is missing");
}
static bool32 salvageGenericSpecial(Ship *ship,void *targets)
{
    bool32 done;if(ship->shiptype==P1StandardCorvette && sv(ship,"mode",FALSE,0)!=2)return TRUE;
    done=cataSalvageSpecial(ship,(SpecialCommand*)targets,salvagerStatics[ship->shiprace].approachDistance);
    if(done){cataSalvageCancel(ship);sv(ship,"mode",TRUE,0);}return done;
}
static void salvageGenericHousekeep(Ship *ship)
{
    if((!ship->command || ship->command->ordertype.order!=COMMAND_SPECIAL) && sv(ship,"target",FALSE,0))cataSalvageCancel(ship);
    else if(ship->clampInfo && salvageTarget(ship) && !ship->rceffect)salvageStartEffect(ship);
}
void cataSalvageConfigure(ShipStaticInfo *info)
{
    CustShipHeader *header=&info->custshipheader;
    if(info->shiptype!=SalCapCorvette && info->shiptype!=P1StandardCorvette)return;
    /* SalCap's recovered attack/fire/passive slots are null. The P1 normal
       attack is the existing checked sidestep layout; preserve those slots. */
    if(info->shiptype==SalCapCorvette){size_t savedSize=header->sizeofShipSpecifics;memset(header,0,sizeof(*header));
        header->shiptype=SalCapCorvette;header->sizeofShipSpecifics=savedSize;}
    header->CustShipStaticInit=salvageGenericStaticInit;header->CustShipInit=salvageGenericInit;
    header->CustShipSpecialTarget=salvageGenericSpecial;header->CustShipHousekeep=salvageGenericHousekeep;
    header->CustShipRemoveShipReferences=cataSalvageRemoveReference;header->CustShipDied=cataSalvageCancel;
    header->CustShipClose=cataSalvageCancel;
}
