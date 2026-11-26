#include "Universe.h"
#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "ETG.h"
#include "MeshAnim.h"
#include "AIVar.h"
#include "StatScript.h"
#include "CataclysmCampaign.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
Universe universe;sdword tutorial=0;
static AIVar values[256];static int valueCount,checks,flights,beams,deleted,animations;
static bool32 haveRepair=TRUE, expectSalvage;
static struct {Effect effect;real32 tail[4];} beamStorage;
#define beam beamStorage.effect
static etgeffectstatic effect;
static real32 beamLength,beamWidth;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
udword Real32ToUdword(real32 value){udword bits;memcpy(&bits,&value,sizeof(bits));return bits;}
AIVar *aivarFind(char *label){int i;for(i=0;i<valueCount;i++)if(!strcmp(values[i].label,label))return &values[i];return NULL;}
AIVar *aivarCreate(char *label){AIVar *v;CHECK(valueCount<256);v=&values[valueCount++];memset(v,0,sizeof(*v));snprintf(v->label,sizeof(v->label),"%s",label);return v;}
void aivarValueSet(AIVar *v,sdword n){v->value=n;}sdword aivarValueGet(AIVar *v){return v->value;}
bool32 cataCampaignHasResearch(const char *name){CHECK(!strcmp(name,"TECH_REPAIR"));return haveRepair;}
bool32 cataCampaignShipHasUpgrade(Ship *ship,udword ability){(void)ship;CHECK(ability==CATA_UPGRADE_REPAIR);return haveRepair;}
void cataCampaignInitializeShipUpgrades(Ship *ship){(void)ship;}
bool32 cataSalvageSpecial(Ship *ship,SpecialCommand *targets,real32 distance)
{
    (void)distance;
    CHECK(expectSalvage && targets->numTargets == 1);
    CHECK(((Ship *)targets->TargetPtr[0])->shiptype == CataMessagePod);
    CHECK(((Ship *)targets->TargetPtr[0])->playerowner == ship->playerowner);
    return FALSE;
}
void cataSalvageCancel(Ship *ship){(void)ship;CHECK(0);}
void cataSalvageRemoveReference(Ship *ship,Ship *removed){(void)ship;(void)removed;}
void cataSalvageLoadStatics(void){}
#include "../src/Game/CataclysmComponentIdentity.inc"
bool32 allianceIsShipAlly(Ship *ship,Player *owner){return ship->playerowner==owner;}
void dbgFatalf(char *file,sdword line,char *format,...){(void)file;(void)line;fprintf(stderr,"%s\n",format);exit(1);}
void listDeleteNode(Node *node){CHECK(node==&beam.objlink);++deleted;}
void univRemoveObjFromRenderList(SpaceObj *object){CHECK(object==(SpaceObj*)&beam);}
etgeffectstatic *etgEffectStaticFind(char *name,bool32 registerIt){CHECK(!registerIt&&!strcmp(name,"Cataclysm\\ETG\\Cata_repair_corvette.ebg"));return &effect;}
void etgEffectDelete(Effect *object){CHECK(object==&beam);}
Effect *etgEffectCreate(etgeffectstatic *stat,Ship *owner,vector *pos,vector *vel,matrix *orientation,real32 nlips,udword flags,sdword params,...)
{va_list args;udword length,width;(void)owner;(void)vel;(void)nlips;
 CHECK(stat==&effect&&params==2&&flags==(EAF_Velocity|EAF_NLips));va_start(args,params);
 length=(udword)va_arg(args,memsize);width=(udword)va_arg(args,memsize);va_end(args);
 memcpy(&beamLength,&length,4);memcpy(&beamWidth,&width,4);
 ++beams;memset(&beamStorage,0,sizeof(beamStorage));beam.variable=(ubyte *)beamStorage.tail;
 beam.posinfo.position=*pos;beam.rotinfo.coordsys=*orientation;return &beam;}
udword aishipFlyToPointAvoidingObjsFunc(Ship *ship,vector *position,udword flags,real32 speed,vector *velocity)
{(void)ship;(void)position;(void)velocity;CHECK(flags==0&&speed==0);++flights;return 0;}
void aitrackForceGuidanceZeroRotation(SpaceObjRotImpTargGuidance *ship){(void)ship;}
void aitrackVelocityVectorGuidance(SpaceObjRotImpTargGuidance *ship,vector *velocity){ship->posinfo.velocity=*velocity;}
bool32 aitrackHeadingFunc(SpaceObjRotImpTargGuidance *ship,vector *heading,real32 a,real32 b,real32 bank,udword flags)
{(void)ship;(void)heading;CHECK(a==.99f&&b==.99f&&bank==0&&flags==0);return TRUE;}
bool32 aitrackZeroRotationAnywhere(Ship *ship){(void)ship;return TRUE;}
void madAnimationStart(Ship *ship,sdword clip){ship->madBindings->nCurrentAnim=clip;++animations;}
void madAnimationPause(Ship *ship,bool32 pause){ship->madBindings->bPaused=pause;}
static bool32 nativeNozzle;
bool32 meshRemasteredMarkerPosition(const void *ship,const char *marker,vector *position)
{
    (void)ship;
    CHECK(!strcmp(marker,"repair_nozzle"));
    if(!nativeNozzle)return FALSE;
    position->x=7;position->y=11;position->z=13;
    return TRUE;
}
char *ShipClassToStr(ShipClass type){static char *names[]={"CLASS_Mothership","CLASS_HeavyCruiser","CLASS_Carrier","CLASS_Destroyer","CLASS_Frigate","CLASS_Corvette","CLASS_Fighter","CLASS_Resource","CLASS_NonCombat"};return names[type];}
void scriptSet(char *directory,char *filename,scriptEntry *table)
{CHECK(!strcmp(directory,"Cataclysm\\")&&!strcmp(filename,"Tweak.script"));for(;table->name;table++)*(real32*)table->dataPtr=strstr(table->name,"Mothership")||strstr(table->name,"HeavyCruiser")||strstr(table->name,"Carrier")?35:30;}
void scriptSetReal32CB(char *directory,char *field,void *out){(void)directory;*(real32*)out=(real32)atof(field);}
void scriptSetStruct(char *directory,char *filename,scriptStructEntry *entries,void *data)
{
    CHECK(!strcmp(directory,"Cataclysm\\RACE_SECT\\")&&!strcmp(filename,"sWorker.shp"));
    for(;entries->name;entries++){
        real32 value=!strcmp(entries->name,"repairDistance")?1500:!strcmp(entries->name,"repairCone")?120:
            !strcmp(entries->name,"repairBeamWidth")?100:!strcmp(entries->name,"jackRetractTime")?3:2000;
        *(real32*)((ubyte*)data+entries->offset1-entries->offset2)=value;
    }
}
#include "../src/Game/CataclysmWorker.c"
static void identity(matrix *m){memset(m,0,sizeof(*m));m->m11=m->m22=m->m33=1;}
int main(void)
{
    Ship worker={0},target={0},inside={0},enemy={0};ShipStaticInfo workerInfo={0},targetInfo={0},insideInfo={0};
    DockStaticInfo docks={0};MaxSelection selection={0};ShipsInsideMe hangar={0};InsideShip passenger={0};madanim animation={0};madheader header={0};
    worker.objtype=target.objtype=inside.objtype=enemy.objtype=OBJ_ShipType;
    worker.shiptype=CataWorker;worker.staticinfo=&workerInfo;workerInfo.shiptype=CataWorker;workerInfo.maxhealth=100;
    worker.shipID.shipNumber=1;target.shipID.shipNumber=2;inside.shipID.shipNumber=3;enemy.shipID.shipNumber=4;
    workerInfo.dockStaticInfo=&docks;docks.numDockPoints=1;strcpy(docks.dockstaticpoints[0].name,"gun0");
    cataWorkerConfigure(&workerInfo);CHECK(workerInfo.custshipheader.CustShipSpecialTarget==workerSpecial&&workerInfo.custshipheader.sizeofShipSpecifics==4);
    workerInfo.custshipheader.CustShipStaticInit("Cataclysm\\RACE_SECT\\","sWorker.shp",&workerInfo);
    CHECK(workerStatic.jackRetractTime==3&&workerRepairRate[CLASS_Destroyer]==30);
    workerInit(&worker);target.staticinfo=&targetInfo;targetInfo.maxhealth=100;targetInfo.shipclass=CLASS_Destroyer;
    inside.staticinfo=&insideInfo;insideInfo.maxhealth=100;insideInfo.shipclass=CLASS_Fighter;
    worker.playerowner=target.playerowner=inside.playerowner=&universe.players[0];enemy.playerowner=&universe.players[1];enemy.staticinfo=&targetInfo;
    identity(&worker.rotinfo.coordsys);identity(&target.rotinfo.coordsys);worker.posinfo.position.z=-500;
    worker.shiplink.structptr=&worker;target.shiplink.structptr=&target;worker.shiplink.next=&target.shiplink;universe.ShipList.head=&worker.shiplink;
    target.health=50;inside.health=10;
    selection.numShips=2;selection.ShipPtr[0]=&enemy;selection.ShipPtr[1]=&target;cataWorkerSetRole(&worker,0);
    CHECK(!workerSpecial(&worker,&selection));CHECK(selection.numShips==1&&workerTarget(&worker)==&target&&workerValue(&worker,"repair",FALSE,0)==1);
    CHECK(!workerSpecial(&worker,&selection));CHECK(workerValue(&worker,"repair",FALSE,0)==3);
    header.nAnimations=2;header.anim[0].endTime=2;animation.header=&header;animation.nCurrentAnim=MAD_NoAnimation;worker.madBindings=&animation;
    CHECK(!workerSpecial(&worker,&selection));CHECK(workerValue(&worker,"repair",FALSE,0)==3&&animations==1&&beams==0);
    animation.bPaused=TRUE;CHECK(!workerSpecial(&worker,&selection));CHECK(!animation.bPaused&&beams==0);
    animation.time=header.anim[0].endTime;animation.bPaused=TRUE;
    CHECK(!workerSpecial(&worker,&selection));CHECK(workerValue(&worker,"repair",FALSE,0)==4&&beams==1);
    CHECK(beamLength==500 && fabsf(beamWidth-1000.0f*tanf(DEG_TO_RAD(10.0f)))<.001f);
    CHECK(beamWidth>workerStatic.repairBeamWidth); /* Retail has no cap from this parsed field. */
    universe.phystimeelapsed=.1f;CHECK(!workerSpecial(&worker,&selection));CHECK(fabsf(target.health-53)<.001f);
    nativeNozzle=TRUE;CHECK(!workerSpecial(&worker,&selection));
    CHECK(beam.posinfo.position.x==7&&beam.posinfo.position.y==11&&beam.posinfo.position.z==-487);
    CHECK(beamStorage.tail[SCALESHIFT(ETG_ResourceLengthParam)]>0);
    CHECK(beam.rotinfo.coordsys.m13 < 0 && beam.rotinfo.coordsys.m23 < 0);
    target.health=53;nativeNozzle=FALSE;
    /* Retail repairs the most damaged contained ship using the OUTER class rate. */
    passenger.ship=&inside;passenger.node.structptr=&passenger;hangar.insideList.head=&passenger.node;target.shipsInsideMe=&hangar;
    CHECK(!workerSpecial(&worker,&selection));CHECK(fabsf(inside.health-13)<.001f&&target.health==53);
    tutorial=1;CHECK(!workerSpecial(&worker,&selection));CHECK(fabsf(inside.health-43)<.001f);tutorial=0;
    target.shipsInsideMe=NULL;target.health=99;CHECK(!workerSpecial(&worker,&selection));CHECK(target.health==100&&deleted==1&&workerTarget(&worker)==NULL);
    /* Saved runtime contains numeric IDs; state survives moving the bank. */
    target.health=20;cataWorkerSetRole(&worker,0);CHECK(!workerSpecial(&worker,&selection));
    {AIVar saved[256];memcpy(saved,values,sizeof(values));memset(values,0,sizeof(values));memcpy(values,saved,sizeof(values));CHECK(workerTarget(&worker)==&target);}
    cataWorkerCancelSpecial(&worker);CHECK(workerValue(&worker,"active",FALSE,0)==0&&workerValue(&worker,"role",FALSE,0)==2);
    haveRepair=FALSE;CHECK(workerSpecial(&worker,&selection));CHECK(target.health==20);
    /* Universe removal may unlink a target before delivering references. */
    haveRepair=TRUE;target.shiplink.next=NULL;worker.shiplink.next=&target.shiplink;
    cataWorkerSetRole(&worker,0);CHECK(!workerSpecial(&worker,&selection));
    CHECK(workerTarget(&worker)==&target);
    worker.shiplink.next=NULL;CHECK(workerTarget(&worker)==NULL);
    workerRemoveReference(&worker,&target);
    CHECK(workerValue(&worker,"target",FALSE,0)==0&&workerValue(&worker,"repair",FALSE,0)==0);
    {
        Ship component={0};ShipStaticInfo componentInfo={0};ClampInfo attachment={0};vector center,extent,position;
        StaticCollInfo *box=&targetInfo.staticheader.staticCollInfo;
        worker.shiplink.next=&target.shiplink;target.shiptype=CataMothership;
        box->collrectoffset.x=100;box->uplength=200;
        worker.posinfo.position.x=1150;worker.posinfo.position.z=0;CHECK(workerInRepairRange(&worker,&target));
        worker.posinfo.position.x=1200;CHECK(!workerInRepairRange(&worker,&target));
        component.objtype=OBJ_ShipType;component.shiptype=CataMothershipEngineering;component.staticinfo=&componentInfo;
        attachment.host=(SpaceObjRotImpTarg*)&target;component.clampInfo=&attachment;
        componentInfo.staticheader.staticCollInfo.collrectoffset.x=3000;
        componentInfo.staticheader.staticCollInfo.uplength=200;
        component.shiplink.structptr=&component;target.shiplink.next=&component.shiplink;
        workerCompoundBox(&target,&center,&extent);CHECK(center.x==1650&&extent.x==1650);
        worker.posinfo.position.x=4000;CHECK(workerInRepairRange(&worker,&target));
        worker.posinfo.position.x=4100;CHECK(!workerInRepairRange(&worker,&target));
        position.x=4000;position.y=position.z=0;CHECK(workerBeamLength(&position,&target)==700);
        attachment.host=(SpaceObjRotImpTarg*)&enemy;workerCompoundBox(&target,&center,&extent);
        CHECK(center.x==200&&extent.x==200); /* Other composites must not affect this target. */
        CHECK(workerBeamAxis(50,100,200)==50);
        target.shiplink.next=NULL;
    }
    cataWorkerSetRole(&worker,2);
    target.shiptype=CataMessagePod;
    expectSalvage=TRUE;
    CHECK(!workerSpecial(&worker,&selection));
    CHECK(workerValue(&worker,"role",FALSE,0)==1);
    CHECK(beams==1);printf("PASS: %d original Worker repair checks\n",checks);return 0;
}

