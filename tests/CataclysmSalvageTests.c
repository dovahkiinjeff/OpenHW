#include "Universe.h"
#include "CataclysmSalvage.h"
#include "CataclysmCampaign.h"
#include "CommandDefs.h"
#include "SinglePlayer.h"
#include "AIVar.h"
#include "AIShip.h"
#include "AITrack.h"
#include "MeshAnim.h"
#include "StatScript.h"
#include "ETG.h"
#include "Attack.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
Universe universe;bool32 singlePlayerGame=TRUE;
static AIVar bank[1024];static sdword used;static int checks,flights,halts,deleted,steps[4];
static bool32 research=TRUE,approachReady=TRUE;static ClampInfo clamp;
static vector flightDestination;
static real32 flightSpeed;
static int restoredContacts,removedCargoOrders;
static CommandToDo haltOrder;
static etgeffectstatic salvageEffect;static Effect beam;static int effectStarts,effectStops,parades;
static Ship capturedReplacement;static ShipStaticInfo replacementInfo;static int created,copiedUpgrades;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
AIVar *aivarFind(char *name){sdword i;for(i=0;i<used;i++)if(!strcmp(bank[i].label,name))return &bank[i];return NULL;}
AIVar *aivarCreate(char *name){AIVar *v;CHECK(used<1024);v=&bank[used++];memset(v,0,sizeof(*v));snprintf(v->label,sizeof(v->label),"%s",name);return v;}
sdword aivarValueGet(AIVar *var){return var->value;}void aivarValueSet(AIVar *var,sdword value){var->value=value;}
bool32 cataCampaignHasResearch(const char *name){CHECK(!strcmp(name,"TECH_SALVAGE"));return research;}
bool32 cataCampaignShipHasUpgrade(Ship *ship,udword ability){(void)ship;CHECK(ability==CATA_UPGRADE_SALVAGE);return research;}
void cataCampaignInitializeShipUpgrades(Ship *ship){(void)ship;}
void cataCampaignCopyShipUpgrades(Ship *destination,Ship *source)
{CHECK(destination==&capturedReplacement&&source->shiptype==StandardDestroyer);++copiedUpgrades;}
Ship *univAddShip(ShipType type,ShipRace race,vector *position,Player *owner,sdword built)
{
    memset(&capturedReplacement,0,sizeof(capturedReplacement));
    memset(&replacementInfo,0,sizeof(replacementInfo));replacementInfo.maxhealth=6000;replacementInfo.maxresources=100;
    capturedReplacement.staticinfo=&replacementInfo;capturedReplacement.shiptype=type;capturedReplacement.shiprace=race;
    capturedReplacement.playerowner=owner;capturedReplacement.posinfo.position=*position;
    CHECK(type==StandardDestroyer&&built==0);++created;return &capturedReplacement;
}
void unitCapCreateShip(Ship *ship,Player *owner){CHECK(ship==&capturedReplacement&&ship->playerowner==owner);}
void univUpdateObjRotInfo(SpaceObjRot *ship){CHECK(ship==(SpaceObjRot*)&capturedReplacement);}
bool32 allianceIsShipAlly(Ship *ship,Player *owner){return ship->playerowner==owner;}
MissionEnum spGetCurrentMission(void){return MISSION_CATA_2_OUTSKIRTS;}
udword aishipFlyToPointAvoidingObjsFunc(Ship *ship,vector *destination,udword flags,real32 speed,vector *velocity)
{
    (void)flags;(void)velocity;
    if(ship->shiptype==CataMessagePod)CHECK(ship->command==NULL);
    flightDestination=*destination;flightSpeed=speed;++flights;return 0;
}
bool32 MoveReachedDestinationVariable(Ship *ship,vector *destination,real32 bounds)
{return fabsf(ship->posinfo.position.x-destination->x)<bounds && fabsf(ship->posinfo.position.y-destination->y)<bounds && fabsf(ship->posinfo.position.z-destination->z)<bounds;}
void aitrackForceSteadyShip(Ship *ship){vecZeroVector(ship->posinfo.velocity);}
bool32 aitrackHeadingFunc(SpaceObjRotImpTargGuidance *ship,vector *heading,real32 accuracy,real32 up,real32 bank,udword flags)
{(void)ship;(void)heading;(void)accuracy;(void)up;(void)bank;(void)flags;return TRUE;}
void clampObjToObj(SpaceObjRotImpTargGuidance *ship,SpaceObjRotImpTargGuidance *target)
{CHECK(ship->clampInfo==NULL);clamp.host=(SpaceObjRotImpTarg*)target;ship->clampInfo=&clamp;}
void unClampObj(SpaceObjRotImpTargGuidance *ship){CHECK(ship->clampInfo==&clamp);ship->clampInfo=NULL;}
void clHalt(CommandLayer *layer,SelectCommand *selection)
{
    CHECK(layer==&universe.mainCommandLayer&&selection->numShips==1&&selection->ShipPtr[0]->shiptype!=CataWorker);
    haltOrder.ordertype.order=COMMAND_HALT;
    selection->ShipPtr[0]->command=&haltOrder;
    ++halts;
}
void RemoveShipsFromDoingStuff(CommandLayer *layer,SelectCommand *selection)
{
    CHECK(layer==&universe.mainCommandLayer&&selection->numShips==1&&selection->ShipPtr[0]->shiptype==CataMessagePod);
    CHECK(selection->ShipPtr[0]->command==&haltOrder);
    selection->ShipPtr[0]->command=NULL;
    ++removedCargoOrders;
}
bool32 cataDockSalvageStep(Ship *ship,Ship *host,sdword port,sdword stage)
{(void)ship;CHECK(host&&port>=0&&stage>=0&&stage<4);++steps[stage];return approachReady;}
bool32 cataDockSalvageIntakePoint(Ship *host,sdword port,DockStaticPoint *point)
{*point=host->staticinfo->dockStaticInfo->dockstaticpoints[port];point->position.y=100;return TRUE;}
bool32 cataDockWorkerGrapplePosition(Ship *ship,vector *grip)
{CHECK(ship->shiptype==CataWorker);vecSet(*grip,-65.75f,0,-18.1f);return TRUE;}
void cataDockRestoreSalvageContact(Ship *ship,Ship *host,sdword port)
{CHECK(ship->clampInfo&&ship->clampInfo->host==(SpaceObjRotImpTarg*)host&&port==0);++restoredContacts;}
void cataDockSalvageFinish(Ship *ship){ship->dockvars.dockship=NULL;ship->dockvars.dockstaticpoint=NULL;}
void madAnimationStart(Ship *ship,sdword clip){ship->madBindings->nCurrentAnim=clip;ship->madBindings->time=0;}
void madAnimationPause(Ship *ship,bool32 pause){ship->madBindings->bPaused=pause;}
void GroupShipIntoMilitaryParade(CommandLayer *layer,Ship *ship,Ship *host)
{CHECK(layer==&universe.mainCommandLayer && ship->shiptype==CataWorker && host->shiptype==CataMothership);++parades;}
etgeffectstatic *etgEffectStaticFind(char *name,bool32 registerIt)
{CHECK(!registerIt&&!strcmp(name,"Cataclysm\\ETG\\Cata_workersalvage.ebg"));return &salvageEffect;}
bool32 etgFrequencyExceeded(etgeffectstatic *effect){CHECK(effect==&salvageEffect);return FALSE;}
Effect *etgEffectCreate(etgeffectstatic *effect,Ship *owner,vector *position,vector *velocity,matrix *orientation,real32 scale,udword flags,sdword params,...)
{
    CHECK(effect==&salvageEffect&&owner->shiptype==CataWorker&&position&&velocity&&!orientation);
    CHECK(scale==0&&flags==(EAF_Coordsys|EAF_NLips)&&params==0);
    CHECK(vecMagnitudeSquared(*velocity)==0.0f);
    beam.posinfo.position=*position;
    beam.flags=flags;
    ++effectStarts;return &beam;
}
void etgEffectDelete(Effect *effect){CHECK(effect==&beam);++effectStops;}
void univRemoveObjFromRenderList(SpaceObj *object){CHECK(object==(SpaceObj*)&beam);}
void listDeleteNode(Node *node){CHECK(node==&beam.objlink);}
void univReallyDeleteThisShipRightNow(Ship *ship)
{
    Node **link=&universe.ShipList.head;CHECK(ship->shiptype==CataMessagePod||ship->shiptype==StandardDestroyer);
    while(*link && *link!=&ship->shiplink)link=&(*link)->next;
    CHECK(*link==&ship->shiplink);*link=ship->shiplink.next;++deleted;
}
void univReallyRemoveShipForTransformation(Ship *ship){univReallyDeleteThisShipRightNow(ship);}
char *ShipClassToStr(ShipClass type)
{static char *names[]={"CLASS_Mothership","CLASS_HeavyCruiser","CLASS_Carrier","CLASS_Destroyer","CLASS_Frigate","CLASS_Corvette","CLASS_Fighter","CLASS_Resource","CLASS_NonCombat"};return names[type];}
void scriptSetReal32CB(char *directory,char *field,void *data){(void)directory;*(real32*)data=(real32)atof(field);}
void scriptSet(char *directory,char *filename,scriptEntry *entries)
{CHECK(!strcmp(directory,"Cataclysm\\")&&!strcmp(filename,"Tweak.script"));for(;entries->name;entries++)
    *(real32*)entries->dataPtr=!strcmp(entries->name,"twSalTransSpeed")?1000.0f:!strcmp(entries->name,"twSalTransMaxTime")?20.0f:.5f;}
scriptStructEntry AttackSideStepParametersScriptTable[]={{NULL,NULL,0,0}};
void attackSideStepInit(AttackSideStep *state){(void)state;}
void scriptSetStruct(char *directory,char *filename,scriptStructEntry *entries,void *data)
{(void)directory;(void)filename;for(;entries->name;entries++)*(real32*)((ubyte*)data+entries->offset1-entries->offset2)=2000;}
void dbgFatalf(char *file,sdword line,char *format,...){(void)file;(void)line;fprintf(stderr,"%s\n",format);exit(1);}
static void fixtureP1Attack(Ship *ship,SpaceObjRotImpTarg *target,real32 range){(void)ship;(void)target;(void)range;}
#include "../src/Game/CataclysmSalvage.c"
static void initShip(Ship *ship,ShipStaticInfo *info,ShipType type,udword id,sdword owner)
{
    memset(ship,0,sizeof(*ship));memset(info,0,sizeof(*info));ship->objtype=OBJ_ShipType;
    ship->staticinfo=info;ship->shiptype=info->shiptype=type;ship->shipID.shipNumber=id;
    ship->playerowner=&universe.players[owner];ship->flags=SOF_Selectable|SOF_Targetable;
    ship->health=info->maxhealth=8000;info->shipclass=CLASS_NonCombat;
    ship->shiplink.structptr=ship;ship->rotinfo.coordsys.m11=ship->rotinfo.coordsys.m22=ship->rotinfo.coordsys.m33=1;
}
int main(void)
{
    Ship worker,pod,host;ShipStaticInfo workerInfo,podInfo,hostInfo;MaxAnySelection targets={0};sdword status;
    madanim animation={0};madheader *clips=calloc(1,madHeaderSize(4));
    DockStaticInfo *points=calloc(1,sizeof(DockStaticInfo)+2*sizeof(DockStaticPoint));
    DockInfo *ports=calloc(1,sizeof(DockInfo)+2*sizeof(DockPoint));
    DockStaticInfo hostPoints={0};DockInfo hostPorts={0};
    universe.players[0].playerIndex=0;universe.players[1].playerIndex=1;
    {
        ShipStaticInfo generic={0};generic.shiptype=SalCapCorvette;generic.shiprace=R2;
        generic.custshipheader.sizeofShipSpecifics=128;generic.custshipheader.CustShipAttack=fixtureP1Attack;
        cataSalvageConfigure(&generic);
        CHECK(generic.custshipheader.sizeofShipSpecifics==128&&!generic.custshipheader.CustShipAttack);
        CHECK(generic.custshipheader.CustShipSpecialTarget==salvageGenericSpecial);
        generic.custshipheader.CustShipStaticInit("Cataclysm\\RACE_TAIDAN\\","tSalCapCorvette.shp",&generic);
        CHECK(salvagerStatics[R2].approachDistance==2000);
        generic.shiptype=P1StandardCorvette;generic.shiprace=P1;generic.custshipheader.CustShipAttack=fixtureP1Attack;
        cataSalvageConfigure(&generic);CHECK(generic.custshipheader.CustShipAttack==fixtureP1Attack);
    }
    initShip(&worker,&workerInfo,CataWorker,10,0);initShip(&pod,&podInfo,CataMessagePod,11,1);
    initShip(&host,&hostInfo,CataMothership,12,0);
    worker.shiplink.next=&pod.shiplink;pod.shiplink.next=&host.shiplink;universe.ShipList.head=&worker.shiplink;
    podInfo.dockStaticInfo=points;pod.dockInfo=ports;points->numDockPoints=ports->numDockPoints=3;
    points->dockstaticpoints[0].type=DPT_Salvage;points->dockstaticpoints[1].type=DPT_SalvageLatch;
    points->dockstaticpoints[2].type=DPT_Salvage;
    cataSalvageLoadStatics();CHECK(salvageCapacity(&pod)==3);
    CHECK(salvageReserve(&pod)==1);CHECK(salvageReserve(&pod)==2);CHECK(salvageReserve(&pod)==0);CHECK(salvageReserve(&pod)==-1);
    salvageReleasePort(&pod,0);salvageReleasePort(&pod,1);salvageReleasePort(&pod,2);
    targets.numTargets=1;targets.TargetPtr[0]=(SpaceObjRotImpTarg*)&pod;
    CHECK(salvagePick(&worker,(SpecialCommand*)&targets,&status)==&pod&&status==0);
    pod.shiptype=StandardDestroyer;pod.health=5000;
    CHECK(salvagePick(&worker,(SpecialCommand*)&targets,&status)==&pod&&status==2);
    pod.health=4000;CHECK(salvageWeakEnough(&pod));
    pod.playerowner=worker.playerowner;
    CHECK(!salvagePick(&worker,(SpecialCommand*)&targets,&status));
    pod.shiptype=CataMessagePod;pod.health=8000;
    CHECK(salvagePick(&worker,(SpecialCommand*)&targets,&status)==&pod&&status==0);
    points->numDockPoints=ports->numDockPoints=1;points->dockstaticpoints[0].type=DPT_SalvageLatch;
    hostInfo.dockStaticInfo=&hostPoints;host.dockInfo=&hostPorts;hostPoints.numDockPoints=hostPorts.numDockPoints=1;
    hostPoints.dockstaticpoints[0].type=DPT_Med_In_Engineering;
    hostPoints.dockstaticpoints[0].conenormal.y=1.0f;
    hostPoints.dockstaticpoints[0].mindist=1200;
    hostPoints.dockstaticpoints[0].maxdist=1500;
    podInfo.staticheader.maxvelocity=300;
    worker.madBindings=&animation;animation.header=clips;clips->nAnimations=4;
    clips->anim[WORKER_DOCK_OPEN].endTime=1.0f;
    animation.nCurrentAnim=WORKER_JACK_OUT;animation.bPaused=TRUE;
    CHECK(salvageNearestHost(&worker,&pod)==&host);
    /* Each original state is exercised; an approach cannot finish on a timer. */
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==1);
    CHECK(salvageClaims(&pod,worker.playerowner,0)==1);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==2);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==3&&ports->dockpoints[0].thisDockBusy==1);
    approachReady=FALSE;CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==3&&!worker.clampInfo);
    approachReady=TRUE;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==4);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==5);
    {AIVar saved[1024];memcpy(saved,bank,sizeof(bank));memset(bank,0,sizeof(bank));memcpy(bank,saved,sizeof(bank));CHECK(salvageTarget(&worker)==&pod);}
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(animation.nCurrentAnim==WORKER_JACK_CLOSE&&!animation.bPaused&&!worker.clampInfo);
    animation.nCurrentAnim=MAD_NoAnimation;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(animation.nCurrentAnim==WORKER_DOCK_OPEN&&!worker.clampInfo&&sv(&worker,"state",FALSE,0)==5);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(!worker.clampInfo&&sv(&worker,"state",FALSE,0)==5);
    animation.time=clips->anim[WORKER_DOCK_OPEN].endTime;
    animation.bPaused=TRUE;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==7);
    CHECK(effectStarts==0&&!worker.rceffect);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==9&&worker.clampInfo&&halts==1);
    CHECK(effectStarts==1&&worker.rceffect==&beam);
    CHECK(fabsf(beam.posinfo.position.x+27.85f)<.01f&&fabsf(beam.posinfo.position.z+5.4f)<.01f);
    /* Saves retain latch state but discard effects. Refresh an old downward contact. */
    sv(&worker,"grapple",TRUE,2);worker.rceffect=NULL;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(restoredContacts==1&&worker.clampInfo&&animation.nCurrentAnim==WORKER_DOCK_OPEN);
    CHECK(sv(&worker,"state",FALSE,0)==10&&effectStarts==2&&worker.rceffect==&beam);
    vecSet(worker.posinfo.position,100,200,300);
    worker.rotinfo.coordsys.m11=worker.rotinfo.coordsys.m22=0;
    worker.rotinfo.coordsys.m12=-1;worker.rotinfo.coordsys.m21=1;
    cataSalvageUpdate();
    CHECK(fabsf(beam.posinfo.position.x-100.0f)<.01f&&fabsf(beam.posinfo.position.y-172.15f)<.01f);
    CHECK(fabsf(beam.posinfo.position.z-294.6f)<.01f);
    pod.posinfo.position.x=7000;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(flightSpeed==300&&flightDestination.y==0&&hostPorts.dockpoints[0].thisDockBusy==0);
    CHECK(halts==1&&removedCargoOrders==1&&!pod.command&&(pod.flags&SOF_Disabled));
    /* An older save can hold a Halt even after the tow has disabled the beacon. */
    pod.command=&haltOrder;
    vecSet(pod.posinfo.position,0,-2500,0);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(removedCargoOrders==2&&!pod.command);
    CHECK(flightDestination.y==1600&&pod.dockingship==NULL);
    pod.posinfo.position.y=1800;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    CHECK(flightDestination.y==1600&&pod.dockingship==&host);
    pod.posinfo.position.y=1600;
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==11&&hostPorts.dockpoints[0].thisDockBusy==1);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==12);
    CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(sv(&worker,"state",FALSE,0)==13);
    CHECK(cataSalvageProcessedCount(CataMessagePod,0)==0);
    CHECK(animation.nCurrentAnim==WORKER_DOCK_OPEN&&animation.bPaused&&effectStarts==2&&effectStops==0);
    approachReady=FALSE;cataSalvageUpdate();cataSalvageUpdate();CHECK(deleted==0&&sv(&pod,"process",FALSE,0)==1);
    CHECK(!worker.clampInfo&&effectStarts==2&&effectStops==1&&parades==1&&animation.nCurrentAnim==WORKER_DOCK_CLOSE);
    sv(&pod,"process",TRUE,3);sv(&pod,"intakeCourse",TRUE,0);
    cataSalvageUpdate();CHECK(deleted==0&&sv(&pod,"process",FALSE,0)==1&&sv(&pod,"intakeCourse",FALSE,0)==1);
    approachReady=TRUE;cataSalvageUpdate();CHECK(sv(&pod,"process",FALSE,0)==2);
    cataSalvageUpdate();CHECK(sv(&pod,"process",FALSE,0)==3);
    cataSalvageUpdate();CHECK(sv(&pod,"process",FALSE,0)==4&&deleted==0);
    cataSalvageUpdate();CHECK(deleted==1&&!worker.clampInfo&&ports->dockpoints[0].thisDockBusy==0&&hostPorts.dockpoints[0].thisDockBusy==0);
    CHECK(cataSalvageProcessedCount(CataMessagePod,0)==1&&cataSalvageProcessedCount(CataMessagePod,1)==0);
    CHECK(cataSalvageProcessedCount((ShipType)-1,0)==1&&cataSalvageProcessedCount((ShipType)-1,1)==0);
    CHECK(sv(&worker,"target",FALSE,0)==0&&sv(&host,"queue0",FALSE,0)==0);
    /* Original capture delay uses maximum hull health, capped by the owned
       transfer limit. Recovery preserves damage/cargo, never grants healing. */
    pod.shiptype=StandardDestroyer;pod.shiprace=R1;pod.shiplink.next=&host.shiplink;worker.shiplink.next=&pod.shiplink;
    sv(&pod,"queued",TRUE,0);
    podInfo.maxhealth=8000;pod.health=3000;pod.resources=150;pod.posinfo.velocity.x=17;pod.posinfo.force.y=12;
    pod.posinfo.isMoving=TRUE;podInfo.teamColor[0]=TRUE;universe.totaltimeelapsed=50;
    CHECK(salvageEnqueue(&worker,&pod,&host));CHECK(sf(&pod,"processEnd",FALSE,0)==58);
    cataSalvageUpdate();CHECK(sv(&pod,"process",FALSE,0)==1&&created==0);
    universe.totaltimeelapsed=57.99f;cataSalvageUpdate();CHECK(created==0);
    universe.totaltimeelapsed=58;cataSalvageUpdate();CHECK(created==1&&deleted==2&&copiedUpgrades==1);
    CHECK(capturedReplacement.health==3000&&capturedReplacement.resources==100);
    CHECK(capturedReplacement.posinfo.velocity.x==17&&capturedReplacement.posinfo.force.y==12&&capturedReplacement.posinfo.isMoving);
    CHECK(capturedReplacement.playerowner==host.playerowner&&capturedReplacement.colorScheme==0);
    CHECK(cataSalvageProcessedCount(StandardDestroyer,0)==1&&cataSalvageProcessedCount(CataMessagePod,0)==1);
    CHECK(sv(&host,"queue0",FALSE,0)==0);
    targets.numTargets=0;CHECK(!cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));CHECK(cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    research=FALSE;CHECK(cataSalvageSpecial(&worker,(SpecialCommand*)&targets,2000));
    free(clips);free(points);free(ports);printf("PASS: %d original salvage and pod-processing checks\n",checks);return 0;
}
