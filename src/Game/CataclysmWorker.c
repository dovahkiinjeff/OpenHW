/* Worker repair imports: owned 005c7d60, 005c8600, 005c8740, 005c89a0,
   005c9580/9710/9770/97d0. Typed host adapters; no HW1 repair/header alias.
   Runtime IDs/counters live in the existing saved AIVar bank so old Worker
   saves retain their four-byte specifics allocation and contain no pointers. */
#include "CataclysmWorker.h"
#include "CataclysmCampaign.h"
#include "CataclysmShips.h"
#include "CataclysmSalvage.h"
#include "Mesh.h"
#include "Universe.h"
#include "SinglePlayer.h"
#include "AIVar.h"
#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "ShipSelect.h"
#include "StatScript.h"
#include "MeshAnim.h"
#include "ETG.h"
#include "UnivUpdate.h"
#include "Debug.h"
#include "FastMath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
static struct CataWorkerStatic {
    real32 repairDistance,repairCone,repairBeamWidth,jackRetractTime,salvageFlyToDistance,repairCos;
    vector nozzle;
} workerStatic;
static real32 workerRepairRate[NUM_CLASSES];
static sdword workerValueID(udword id,const char *field,bool32 write,sdword value)
{
    char key[48];AIVar *var;snprintf(key,sizeof(key),"CW%u_%s",(unsigned)id,field);var=aivarFind(key);
    if(write){if(!var)var=aivarCreate(key);aivarValueSet(var,value);}
    return var?aivarValueGet(var):0;
}
static sdword workerValue(Ship *ship,const char *field,bool32 write,sdword value)
{return workerValueID(ship->shipID.shipNumber,field,write,value);}
static real32 workerFloat(Ship *ship,const char *field,bool32 set,real32 value)
{
    sdword bits;if(set){memcpy(&bits,&value,4);workerValue(ship,field,TRUE,bits);}
    bits=workerValue(ship,field,FALSE,0);memcpy(&value,&bits,4);return value;
}
static real32 workerDeadline(Ship *ship,bool32 set,real32 value)
{return workerFloat(ship,"deadline",set,value);}
static Ship *workerFindShip(sdword saved)
{
    Node *node;if(saved<=0)return NULL;
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *ship=listGetStructOfNode(node);if(ship->shipID.shipNumber==(udword)(saved-1))return ship;
    }
    return NULL;
}
static Ship *workerTarget(Ship *ship){return workerFindShip(workerValue(ship,"target",FALSE,0));}
static void workerCounter(Ship *ship,const char *field,sdword change)
{workerValue(ship,field,TRUE,(sdword)((udword)workerValue(ship,field,FALSE,0)+(udword)change));}
static void workerDeleteBeam(Ship *ship)
{
    if(ship->rceffect){
        etgEffectDelete(ship->rceffect);univRemoveObjFromRenderList((SpaceObj*)ship->rceffect);
        listDeleteNode(&ship->rceffect->objlink);ship->rceffect=NULL;
    }
    workerValue(ship,"beam",TRUE,0);
    workerFloat(ship,"standoff",TRUE,0);
}
static void workerResetRepair(Ship *ship)
{
    Ship *target=workerTarget(ship);
    if(target)workerCounter(target,"active",-1);
    workerValue(ship,"target",TRUE,0);workerValue(ship,"repair",TRUE,0);workerDeleteBeam(ship);
}
void cataWorkerCancelSpecial(Ship *ship)
{
    if(!ship || ship->shiptype!=CataWorker)return;
    if(workerValue(ship,"role",FALSE,0)==1)cataSalvageCancel(ship);else workerResetRepair(ship);
    workerValue(ship,"role",TRUE,2);
}
void cataWorkerSetRole(Ship *ship,sdword role)
{
    if(!ship || ship->shiptype!=CataWorker)return;
    cataWorkerCancelSpecial(ship);workerValue(ship,"role",TRUE,role);
}
static Ship *workerMostDamaged(Ship *host)
{
    Ship *best=host;Node *node;real32 fraction=host->health/host->staticinfo->maxhealth;
    if(host->shipsInsideMe)for(node=host->shipsInsideMe->insideList.head;node;node=node->next){
        Ship *candidate=((InsideShip*)listGetStructOfNode(node))->ship;
        real32 health=candidate->health/candidate->staticinfo->maxhealth;
        if(health<fraction){fraction=health;best=candidate;}
    }
    return best;
}
static real32 workerRate(Ship *ship)
{
    if(cataShipIsAttachedComponent(ship) || ship->staticinfo->shipclass>=NUM_CLASSES)return 0;
    return workerRepairRate[ship->staticinfo->shipclass];
}
bool32 cataWorkerCanRepairTarget(SpaceObjRotImpTarg *target, Player *owner)
{
    return target->objtype == OBJ_ShipType && ((Ship *)target)->shiptype != CataMessagePod &&
           ((Ship *)target)->playerowner != NULL &&
           allianceIsShipAlly((Ship *)target, owner) && workerRate((Ship *)target) > 0;
}

static Ship *workerSelectRepair(Ship *ship,SpecialCommand *targets,bool32 *priority)
{
    sdword i=0,available=-1,busy=-1,minPriority=SDWORD_Max,priorityBest=-1,priorityFallback=-1;
    real64 nearest=REALlyBig,priorityDistance=REALlyBig;MaxSelection full={0};
    *priority=FALSE;
    while(i<targets->numTargets){
        Ship *candidate=(Ship*)targets->TargetPtr[i];vector delta;real64 distance;
        if(!cataWorkerCanRepairTarget(targets->TargetPtr[i],ship->playerowner)){
            targets->TargetPtr[i]=targets->TargetPtr[--targets->numTargets];continue;
        }
        if(!(candidate->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace))){
            Ship *damaged=workerMostDamaged(candidate);
            if(damaged->health>=damaged->staticinfo->maxhealth){
                if(full.numShips<COMMAND_MAX_SHIPS)full.ShipPtr[full.numShips++]=candidate;
            }else{
                vecSub(delta,candidate->posinfo.position,ship->posinfo.position);distance=vecMagnitudeSquared(delta);
                if(distance<nearest){
                    if(workerValue(candidate,"active",FALSE,0)){
                        if(available<0 && busy<0)busy=i;
                    }else{nearest=distance;available=i;}
                }
            }
        }
        ++i;
    }
    if(available>=0)return (Ship*)targets->TargetPtr[available];
    if(busy>=0)return (Ship*)targets->TargetPtr[busy];
    for(i=0;i<full.numShips;i++)minPriority=min(minPriority,workerValue(full.ShipPtr[i],"priority",FALSE,0));
    for(i=0;i<full.numShips;i++){
        vector delta;real64 distance;Ship *candidate=full.ShipPtr[i];
        vecSub(delta,candidate->posinfo.position,ship->posinfo.position);distance=vecMagnitudeSquared(delta);
        if(distance<priorityDistance){
            if(workerValue(candidate,"priority",FALSE,0)==minPriority){priorityDistance=distance;priorityBest=i;}
            else if(priorityBest<0 && priorityFallback<0)priorityFallback=i;
        }
    }
    if(priorityBest<0)priorityBest=priorityFallback;
    if(priorityBest<0)return NULL;*priority=TRUE;return full.ShipPtr[priorityBest];
}
/* Original 005c80d0/005c7f80: center plus extents, including Component
   statics attached to the composite root. The returned extents deliberately
   retain the retail full-length initialization, rather than halving it. */
static void workerBoxFromStatic(const StaticCollInfo *box,vector *center,vector *extent)
{
    extent->x=box->uplength;extent->y=box->rightlength;extent->z=box->forwardlength;
    vecScalarMultiply(*center,*extent,.5f);vecAddTo(*center,box->collrectoffset);
}
static void workerCompoundBox(Ship *target,vector *center,vector *extent)
{
    Node *node;
    workerBoxFromStatic(&target->staticinfo->staticheader.staticCollInfo,center,extent);
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *part=listGetStructOfNode(node);vector other,otherExtent,low,high;
        if(!cataShipIsAttachedComponent(part) || part->clampInfo->host!=(SpaceObjRotImpTarg*)target)continue;
        workerBoxFromStatic(&part->staticinfo->staticheader.staticCollInfo,&other,&otherExtent);
        low.x=min(center->x-extent->x,other.x-otherExtent.x);
        low.y=min(center->y-extent->y,other.y-otherExtent.y);
        low.z=min(center->z-extent->z,other.z-otherExtent.z);
        high.x=max(center->x+extent->x,other.x+otherExtent.x);
        high.y=max(center->y+extent->y,other.y+otherExtent.y);
        high.z=max(center->z+extent->z,other.z+otherExtent.z);
        vecSub(*extent,high,low);vecMultiplyByScalar(*extent,.5f);vecAdd(*center,low,*extent);
    }
}
static bool32 workerInRepairRange(Ship *ship,Ship *target)
{
    StaticCollInfo *box=&target->staticinfo->staticheader.staticCollInfo;
    vector delta,local,center,extent;real32 range=max(800.0f,min(1500.0f,box->collspheresize*2.0f));
    vecSub(delta,ship->posinfo.position,target->posinfo.position);matMultiplyVecByMat(&local,&delta,&target->rotinfo.coordsys);
    workerCompoundBox(target,&center,&extent);
    return local.x>center.x-extent.x-range && local.x<center.x+extent.x+range &&
        local.y>center.y-extent.y-range && local.y<center.y+extent.y+range &&
        local.z>center.z-extent.z-range && local.z<center.z+extent.z+range;
}
/* 005c81e0 leaves an in-box axis unchanged. Preserve that original detail. */
static real32 workerBeamAxis(real32 value,real32 center,real32 extent)
{return value<center-extent?center-extent-value:value>center+extent?center+extent-value:value;}
static real32 workerBeamLength(const vector *position,Ship *target)
{
    vector delta,local,center,extent;vecSub(delta,*position,target->posinfo.position);
    matMultiplyVecByMat(&local,&delta,&target->rotinfo.coordsys);workerCompoundBox(target,&center,&extent);
    local.x=workerBeamAxis(local.x,center.x,extent.x);local.y=workerBeamAxis(local.y,center.y,extent.y);
    local.z=workerBeamAxis(local.z,center.z,extent.z);return fsqrt(vecMagnitudeSquared(local));
}
static bool32 workerRepairApproach(Ship *ship,Ship *target)
{
    vector delta,heading;real32 distance,dot;bool32 inRange=workerInRepairRange(ship,target);
    vecSub(delta,target->posinfo.position,ship->posinfo.position);distance=fsqrt(vecMagnitudeSquared(delta));
    if(distance>0){vecMultiplyByScalar(delta,1.0f/distance);}
    matGetVectFromMatrixCol3(heading,ship->rotinfo.coordsys);dot=vecDotProduct(heading,delta);
    if(!inRange)aishipFlyToPointAvoidingObjsWithVel(ship,&target->posinfo.position,0,0,NULL);
    else{
        real32 standoff=workerFloat(ship,"standoff",FALSE,0);
        if(workerValue(ship,"beam",FALSE,0) && distance>0 && distance<standoff){
            vector destination;vecScalarMultiply(destination,delta,-(standoff-distance));
            vecAddTo(destination,ship->posinfo.position);
            aishipFlyToPointAvoidingObjsWithVel(ship,&destination,0,0,NULL);
        }else{
            aitrackForceGuidanceZeroRotation((SpaceObjRotImpTargGuidance*)ship);
            aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance*)ship,&target->posinfo.velocity);
        }
    }
    if(aitrackHeadingFunc((SpaceObjRotImpTargGuidance*)ship,&delta,.99f,.99f,0,0))aitrackZeroRotationAnywhere(ship);
    return inRange && dot>=workerStatic.repairCos;
}
static void workerBeamPosition(Ship *ship,vector *position)
{
    vector nozzle = workerStatic.nozzle;
    meshRemasteredMarkerPosition(ship, "repair_nozzle", &nozzle);
    matMultiplyMatByVec(position, &ship->rotinfo.coordsys, &nozzle);
    vecAddTo(*position, ship->posinfo.position);
}
static void workerUpdateBeam(Ship *ship, Ship *target)
{
    vector position, trajectory;
    matrix orientation;
    real32 distance, length, width;

    workerBeamPosition(ship, &position);
    vecSub(trajectory, target->posinfo.position, position);
    distance = fsqrt(vecMagnitudeSquared(trajectory));
    if (distance <= 0.0f) return;
    vecMultiplyByScalar(trajectory, 1.0f / distance);
    matCreateCoordSysFromHeading(&orientation, &trajectory);
    length = workerBeamLength(&position, target);
    width = target->staticinfo->staticheader.staticCollInfo.collspheresize * 2.0f;
    width=max(length*2.0f*tanf(DEG_TO_RAD(10.0f)),min(width,length*2.0f*tanf(DEG_TO_RAD(26.0f))));

    if (ship->rceffect)
    {
        real32 *parameters = (real32 *)ship->rceffect->variable;
        ship->rceffect->posinfo.position = position;
        ship->rceffect->rotinfo.coordsys = orientation;
        parameters[SCALESHIFT(ETG_ResourceLengthParam)] = length;
        parameters[SCALESHIFT(ETG_ResourceRadiusParam)] = width;
    }
    else
    {
        etgeffectstatic *effect = etgEffectStaticFind("Cataclysm\\ETG\\Cata_repair_corvette.ebg", FALSE);
        if (!effect) return;
        ship->rceffect = etgEffectCreate(effect, ship, &position, &ship->posinfo.velocity,
            &orientation, 1.0f, EAF_Velocity | EAF_NLips, 2,
            SCALECAST(Real32ToUdword(length)), SCALECAST(Real32ToUdword(width)));
        if (ship->rceffect)
        {
            workerValue(ship, "beam", TRUE, 1);
            workerFloat(ship, "standoff", TRUE, length);
        }
    }
}
bool32 cataWorkerExtendArm(Ship *ship)
{
    madanim *animation=ship->madBindings;
    if(!animation || !animation->header || !animation->header->nAnimations)return TRUE;
    if(animation->nCurrentAnim==0){
        workerDeadline(ship,TRUE,universe.totaltimeelapsed+workerStatic.jackRetractTime);
        if (animation->bPaused && animation->time < animation->header->anim[0].endTime)
            madAnimationPause(ship, FALSE);
        return animation->bPaused;
    }
    if(animation->nCurrentAnim==MAD_NoAnimation || animation->bPaused){
        if (animation->bPaused) madAnimationPause(ship, FALSE);
        madAnimationStart(ship,0);workerDeadline(ship,TRUE,universe.totaltimeelapsed+workerStatic.jackRetractTime);
    }
    return FALSE;
}
static bool32 workerRepair(Ship *ship,SpecialCommand *targets)
{
    sdword state=workerValue(ship,"repair",FALSE,0);Ship *target=workerTarget(ship);
    if(ship->flags&(SOF_Dead|SOF_Disabled|SOF_Hyperspace)){
        workerResetRepair(ship);return FALSE;
    }
    switch(state){
    case 0:{bool32 priority;
        if(!targets || !targets->numTargets){workerValue(ship,"repair",TRUE,5);return FALSE;}
        target=workerSelectRepair(ship,targets,&priority);if(!target)return FALSE;
        workerCounter(target,priority?"priority":"active",1);
        workerValue(ship,"target",TRUE,(sdword)target->shipID.shipNumber+1);
        if(priority){workerRepairApproach(ship,target);workerResetRepair(ship);return FALSE;}
        workerValue(ship,"repair",TRUE,1);return FALSE;}
    case 1:
        if(!target){workerResetRepair(ship);return FALSE;}
        if(workerRepairApproach(ship,target))workerValue(ship,"repair",TRUE,3);
        return FALSE;
    case 3:
        if(!target){workerResetRepair(ship);return FALSE;}workerRepairApproach(ship,target);
        if(cataWorkerExtendArm(ship)){workerUpdateBeam(ship,target);workerValue(ship,"repair",TRUE,4);}
        return FALSE;
    case 4:{Ship *damaged;real32 amount;
        if(!target){workerResetRepair(ship);return FALSE;}
        workerRepairApproach(ship,target);
        if (!cataWorkerExtendArm(ship)) return FALSE;
        workerUpdateBeam(ship,target);
        damaged=workerMostDamaged(target);amount=universe.phystimeelapsed*workerRate(target);
        if(tutorial==1)amount*=10.0f;
        damaged->health=min(damaged->staticinfo->maxhealth,damaged->health+amount);
        if(damaged->health==damaged->staticinfo->maxhealth)workerResetRepair(ship);
        return FALSE;}
    case 5:workerResetRepair(ship);return TRUE;
    default:return TRUE;
    }
}
static bool32 workerSpecial(Ship *ship,void *custom)
{
    sdword role=workerValue(ship,"role",FALSE,0);bool32 done;
    if(role==2 && custom && ((SpecialCommand*)custom)->numTargets){
        SpaceObjRotImpTarg *first=((SpecialCommand*)custom)->TargetPtr[0];
        role=cataWorkerCanRepairTarget(first,ship->playerowner)?0:1;
        workerValue(ship,"role",TRUE,role);
    }
    if(role==0){if(!cataCampaignShipHasUpgrade(ship,CATA_UPGRADE_REPAIR))return TRUE;done=workerRepair(ship,(SpecialCommand*)custom);}
    else if(role==1)done=cataSalvageSpecial(ship,(SpecialCommand*)custom,workerStatic.salvageFlyToDistance);
    else return TRUE;
    if(done){if(role==1)cataSalvageCancel(ship);workerValue(ship,"role",TRUE,2);}return done;
}
static void workerHousekeep(Ship *ship)
{
    madanim *animation=ship->madBindings;Ship *target=workerTarget(ship);
    if(!ship->command || ship->command->ordertype.order!=COMMAND_SPECIAL){
        if(workerValue(ship,"target",FALSE,0) || workerValue(ship,"role",FALSE,0)==1)cataWorkerCancelSpecial(ship);
    }
    if(workerValue(ship,"role",FALSE,0)!=1 &&
       workerDeadline(ship,FALSE,0)>0 && workerDeadline(ship,FALSE,0)<universe.totaltimeelapsed &&
       animation && animation->header && animation->header->nAnimations>1 && animation->nCurrentAnim==0 && animation->bPaused){
        madAnimationPause(ship,FALSE);
        madAnimationStart(ship,1);workerDeadline(ship,TRUE,1.0e21f);
    }
}
static void workerRemoveReference(Ship *ship,Ship *removed)
{
    cataSalvageRemoveReference(ship,removed);
    /* Removal callbacks can run after the target has left ShipList. Compare
       the saved identity rather than requiring a successful world lookup. */
    if(removed && workerValue(ship,"target",FALSE,0)==(sdword)(removed->shipID.shipNumber+1))
        workerResetRepair(ship);
}
static void workerInit(Ship *ship)
{ship->ShipSpecifics[0]=0;workerValue(ship,"role",TRUE,2);workerValue(ship,"repair",TRUE,0);workerValue(ship,"target",TRUE,0);workerDeadline(ship,TRUE,1.0e21f);cataCampaignInitializeShipUpgrades(ship);}
static scriptStructEntry workerTable[]={
#define WORKER_FIELD(name) {#name,scriptSetReal32CB,offsetof(struct CataWorkerStatic,name),0}
    WORKER_FIELD(repairDistance),
    WORKER_FIELD(repairCone),
    WORKER_FIELD(repairBeamWidth),
    WORKER_FIELD(jackRetractTime),
    WORKER_FIELD(salvageFlyToDistance),
#undef WORKER_FIELD
    {NULL,NULL,0,0}
};
static void workerStaticInit(char *directory,char *filename,ShipStaticInfo *info)
{
    sdword i;scriptEntry rates[NUM_CLASSES+1];char names[NUM_CLASSES][64];DockStaticPoint *nozzle=NULL;
    memset(&workerStatic,0,sizeof(workerStatic));workerStatic.jackRetractTime=6.0f;
    scriptSetStruct(directory,filename,workerTable,(ubyte*)&workerStatic);
    if(workerStatic.repairDistance<=0 || workerStatic.repairCone<=0 || workerStatic.repairBeamWidth<=0)
        dbgFatalf(DBG_Loc,"Owned Worker repair definitions are incomplete");
    workerStatic.repairCos=cosf(DEG_TO_RAD(workerStatic.repairCone*.5f));
    for(i=0;i<NUM_CLASSES;i++){
        snprintf(names[i],sizeof(names[i]),"twRepairRate[%s]",ShipClassToStr(i));
        rates[i].name=names[i];rates[i].setVarCB=scriptSetReal32CB;rates[i].dataPtr=&workerRepairRate[i];
    }
    memset(&rates[NUM_CLASSES],0,sizeof(rates[NUM_CLASSES]));scriptSet("Cataclysm\\","Tweak.script",rates);
    cataSalvageLoadStatics();
    if(info->dockStaticInfo)for(i=0;i<info->dockStaticInfo->numDockPoints;i++)
        if(!strcmp(info->dockStaticInfo->dockstaticpoints[i].name,"gun0"))nozzle=&info->dockStaticInfo->dockstaticpoints[i];
    if(!nozzle)dbgFatalf(DBG_Loc,"Owned Worker jackhammer attachment is missing");
    workerStatic.nozzle=nozzle->position;info->custstatinfo=&workerStatic;
    info->canSpecialBandBoxFriendlies=TRUE;info->canSingleClickSpecialActivate=TRUE;
}
void cataWorkerConfigure(ShipStaticInfo *info)
{
    CustShipHeader *header;if(info->shiptype!=CataWorker)return;header=&info->custshipheader;
    memset(header,0,sizeof(*header));header->shiptype=CataWorker;header->sizeofShipSpecifics=sizeof(udword);
    header->CustShipStaticInit=workerStaticInit;header->CustShipInit=workerInit;
    header->CustShipSpecialTarget=workerSpecial;header->CustShipHousekeep=workerHousekeep;
    header->CustShipRemoveShipReferences=workerRemoveReference;
    header->CustShipDied=cataWorkerCancelSpecial;header->CustShipClose=cataWorkerCancelSpecial;
}
