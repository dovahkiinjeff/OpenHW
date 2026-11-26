/* Run the actual imported state machine with deterministic movement boundaries. */
#include "Universe.h"
#include "Dock.h"
#include "CommandLayer.h"
#include "AIShip.h"
#include "AITrack.h"
#include "Mesh.h"
#include "MeshAnim.h"
#include "Clamp.h"
#include "SinglePlayer.h"
#include "FastMath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define PERMANENTLY_DOCKED 3
#define FINISHED_LAUNCHING_FROM_DOCKING 4
Universe universe;
SinglePlayerGameInfo singlePlayerGameInfo;
bool32 singlePlayerGame=TRUE;
static int checks,allocated,removed,inside,outside,flies,clamped;
static bool32 aligned=TRUE,native=TRUE;
static udword flyFlags, launchHeading, launchUp;
static real32 flySpeed,accuracy;
static vector flyDestination,flyVelocity;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
void dbgFatalf(char *file,sdword line,char *format,...){(void)file;(void)line;fprintf(stderr,"%s\n",format);exit(1);}
/* Allocation layout matches SaveStructureWithSize, without invoking the game heap. */
#undef memAlloc
#define memAlloc(size,name,flags) testAlloc(size)
#define memFree(ptr) testFree(ptr)
static void *testAlloc(size_t size){++allocated;return calloc(1,size);}
static void testFree(void *ptr){--allocated;free(ptr);}
bool32 meshRemasteredDockEndpoint(const void *host,const char *name,vector *v)
{(void)host;(void)name;v->x=20;v->y=0;v->z=0;return native;}
sdword meshRemasteredDockPoint(const void *host,const char *name,unsigned index,vector *v)
{
    (void)host;
    if (!strncmp(name,"path",4) && (!strcmp(name,"path8") || !strcmp(name,"path10") || !strcmp(name,"path2"))) {
        if (!native || index>=4) return 0;
        vecSet(*v,index==0?0.0f:20.0f,index==0?-1000.0f:0.0f,
               !strcmp(name,"path8")?(index==3?30.0f:25.0f):!strcmp(name,"path10")?10.0f:2.0f);
        return 4;
    }
    if (!strcmp(name,"path9")) {
        if (!native || index >= 2) return 0;
        vecSet(*v, index ? -998.0f : -736.0f, 0.0f, 1025.0f);
        return 2;
    }
    vecSet(*v,30.0f,0.0f,0.0f);
    return native;
}
bool32 meshRemasteredMarkerPosition(const void *ship,const char *semantic,vector *position)
{
    (void)ship;
    if (!native || strcmp(semantic,"worker_grapple_mount")) return FALSE;
    vecSet(*position,-21.56f,0,0);
    return TRUE;
}
void GetDirectionVectorOfShip(vector *v,udword direction,Ship *host)
{
    switch(direction&3){case 0:matGetVectFromMatrixCol1(*v,host->rotinfo.coordsys);break;
    case 1:matGetVectFromMatrixCol2(*v,host->rotinfo.coordsys);break;
    default:matGetVectFromMatrixCol3(*v,host->rotinfo.coordsys);break;}
    if(direction&4){vecMultiplyByScalar(*v,-1);}
}
static void dockReserveDockPoint(Ship *ship,Ship *host,sdword index)
{CHECK(index>=0&&index<host->dockInfo->numDockPoints);CHECK(!host->dockInfo->dockpoints[index].thisDockBusy);host->dockInfo->dockpoints[index].thisDockBusy++;ship->dockvars.reserveddocking=(sbyte)index;}
static void dockFreeDockPoint(Ship *ship,Ship *host)
{CHECK(ship->dockvars.reserveddocking>=0);host->dockInfo->dockpoints[ship->dockvars.reserveddocking].thisDockBusy--;ship->dockvars.reserveddocking=-1;}
bool32 MoveReachedDestinationVariable(Ship *ship,vector *v,real32 bounds)
{vector d;vecSub(d,ship->posinfo.position,*v);return fabs(d.x)<bounds&&fabs(d.y)<bounds&&fabs(d.z)<bounds;}
udword aishipFlyToPointAvoidingObjsFunc(Ship *ship,vector *destination,udword flags,real32 speed,vector *velocity)
{(void)ship;++flies;flyFlags=flags;flySpeed=speed;flyDestination=*destination;flyVelocity=*velocity;return 0;}
void aitrackForceSteadyShip(Ship *ship){(void)ship;}
bool32 aitrackSteadyShip(Ship *ship){(void)ship;return TRUE;}
bool32 aitrackHeadingAndUp(Ship *ship,vector *heading,vector *up,real32 value)
{(void)ship;(void)heading;(void)up;accuracy=value;return aligned;}
void aitrackVelocityVectorGuidance(SpaceObjRotImpTargGuidance *ship,vector *velocity){ship->posinfo.velocity=*velocity;}
void madAnimationStart(Ship *ship,sdword clip){ship->madBindings->nCurrentAnim=clip;}
void madAnimationPause(Ship *ship,bool32 pause){ship->madBindings->bPaused=pause;}
void clampObjToObj(SpaceObjRotImpTargGuidance *ship,SpaceObjRotImpTargGuidance *host)
{(void)ship;(void)host;clamped=1;}
void unClampObj(SpaceObjRotImpTargGuidance *ship){(void)ship;clamped=0;}
void univRemoveShipFromOutside(Ship *ship){(void)ship;++removed;}
void dockPutShipInside(Ship *ship,Ship *host){(void)ship;(void)host;++inside;}
void dockRemoveShipFromInside(Ship *ship,Ship *host){(void)ship;(void)host;--inside;}
void dockPutShipOutside(Ship *ship,Ship *host,vector *v,udword heading,udword up)
{launchHeading=heading;launchUp=up;++outside;ship->posinfo.position=*v;ship->posinfo.velocity=host->posinfo.velocity;}
void dockInitShipForLaunch(Ship *ship){ship->dockvars.dockstate3=0;}
#include "../src/Game/CataclysmDock.inc"
static void identity(matrix *m){memset(m,0,sizeof(*m));m->m11=m->m22=m->m33=1;}
int main(void)
{
    Ship ship={0},host={0},bay={0};ShipStaticInfo info={0},hostInfo={0};ClampInfo bayClamp={0};
    CommandToDo command={0};DockStaticPoint point,mapped;DockStaticInfo *ports;DockInfo *busy;
    const char *names[]={"dbsmallin1","dbsmallout1","dbmedin1","dbmedout1","rpsmallin1","rpsmallout1","rpmedin1","rpmedout1","Res0","ResLeech0"};int i;
    ports=calloc(1,sizeof(*ports)+9*sizeof(DockStaticPoint));busy=calloc(1,sizeof(*busy)+9*sizeof(DockPoint));
    ports->numDockPoints=busy->numDockPoints=10;
    ship.staticinfo=&info;host.staticinfo=&hostInfo;hostInfo.dockStaticInfo=ports;host.dockInfo=busy;
    ship.playerowner=host.playerowner=&universe.players[0];host.playerowner->autoLaunch=BIT0;
    identity(&ship.rotinfo.coordsys);identity(&host.rotinfo.coordsys);host.posinfo.velocity.x=17;
    info.shipclass=CLASS_Fighter;info.maxhealth=100;ship.health=100;ship.dockvars.reserveddocking=-1;
    for(i=0;i<10;i++){strcpy(ports->dockstaticpoints[i].name,names[i]);ports->dockstaticpoints[i].dockindex=i;
        ports->dockstaticpoints[i].conenormal.z=1;ports->dockstaticpoints[i].mindist=100;
        ports->dockstaticpoints[i].maxdist=500;ports->dockstaticpoints[i].flyawaydist=200;
        ports->dockstaticpoints[i].coneangle=.8f;ports->dockstaticpoints[i].headingdirection=2;}
    bay.shiptype=CataMothershipDockingBay;bay.clampInfo=&bayClamp;bayClamp.host=(SpaceObjRotImpTarg*)&host;
    bay.shiplink.structptr=&bay;universe.ShipList.head=&bay.shiplink;
    CHECK(cataDockFindPort(&ship,&host,FALSE,FALSE)==0);
    info.shipclass=CLASS_Corvette;CHECK(cataDockFindPort(&ship,&host,FALSE,FALSE)==0);
    CHECK(!strcmp(cataNativeDockPath(&ship,FALSE,FALSE),"path8"));
    busy->dockpoints[0].thisDockBusy=1;CHECK(cataDockFindPort(&ship,&host,FALSE,FALSE)==4);
    busy->dockpoints[4].thisDockBusy=1;CHECK(cataDockFindPort(&ship,&host,FALSE,FALSE)==-1);
    busy->dockpoints[0].thisDockBusy=busy->dockpoints[4].thisDockBusy=0;
    info.shipclass=CLASS_Resource;CHECK(cataDockFindPort(&ship,&host,FALSE,FALSE)==2);
    CHECK(cataDockFindPort(&ship,&host,FALSE,TRUE)==8);
    universe.ShipList.head=NULL;CHECK(cataDockFindPort(&ship,&host,FALSE,FALSE)==6);
    universe.ShipList.head=&bay.shiplink;info.shipclass=CLASS_Fighter;
    ship.dockvars.dockstaticpoint=&ports->dockstaticpoints[0];point=*ship.dockvars.dockstaticpoint;
    mapped=cataDockPoint(&ship,&host,FALSE,FALSE);CHECK(mapped.position.x==20&&mapped.mindist==100&&mapped.conenormal.z==1);
    CHECK(ship.dockvars.dockstaticpoint==&ports->dockstaticpoints[0]&&ports->dockstaticpoints[0].position.x==0);
    ship.posinfo.position.x=2000;ship.posinfo.position.z=0;
    CHECK(!cataDockApproach(&ship,&host,&point));CHECK(flyFlags==7&&flySpeed==-80&&flyVelocity.x==17);
    ship.posinfo.position.x=200;CHECK(!cataDockApproach(&ship,&host,&point));CHECK(flyFlags==3&&flySpeed==-10);
    ship.posinfo.position.x=0;ship.posinfo.position.z=200;CHECK(cataDockApproach(&ship,&host,&point));
    ship.dockvars.dockship=&host;CHECK(cataDockRuntime(&ship)->size==sizeof(CataDockRuntime));
    CHECK(!cataDockFinal(&ship,&host,&point));CHECK(accuracy==.995f&&flyFlags==0&&flySpeed==-120);
    ship.posinfo.position.z=0;CHECK(cataDockFinal(&ship,&host,&point));cataDockRuntimeFree(&ship);
    ship.dockvars.dockstate2=0;ship.posinfo.position.z=200;ship.posinfo.position.x=20;
    CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(ship.dockvars.dockstate2==1&&removed==0);
    CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(ship.dockvars.dockstate2==2&&removed==0);
    ship.posinfo.position.z=0;CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(removed==1&&inside==1&&ship.dockvars.reserveddocking==-1);
    universe.totaltimeelapsed=1;ship.specialFlags|=SPECIAL_STAY_TILL_EXPLICITLAUNCH;
    CHECK(ShipDocksAtSomtaaw(&command,&ship,&host)==PERMANENTLY_DOCKED);
    ship.specialFlags=0;host.playerowner->autoLaunch=0;CHECK(ShipDocksAtSomtaaw(&command,&ship,&host)==PERMANENTLY_DOCKED);
    host.playerowner->autoLaunch=BIT0;CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(ship.dockvars.dockstate2==6);
    CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(outside==1&&inside==0&&ship.posinfo.position.x==30);
    CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(flyFlags==0&&flySpeed==0&&flyDestination.z==800);
    ship.posinfo.position.z=201;CHECK(ShipDocksAtSomtaaw(&command,&ship,&host)==FINISHED_LAUNCHING_FROM_DOCKING);
    CHECK(allocated==0&&ship.dockvars.reserveddocking==-1&&ship.dockingship==NULL);
    {
        Ship processor={0};ShipStaticInfo processorInfo={0};
        processor.shiptype=CataProcessor;processor.staticinfo=&processorInfo;
        processorInfo.shipclass=CLASS_Resource;
        host.shiptype=CataMothership;
        inside=1;
        CHECK(!LaunchShipFromSomtaaw(&processor,&host));
        CHECK(!inside&&processor.dockvars.reserveddocking==3);
        CHECK(processor.posinfo.position.x==-736&&processor.posinfo.position.z==1025);
        CHECK(launchHeading==4&&launchUp==2);
        CHECK(!LaunchShipFromSomtaaw(&processor,&host));
        CHECK(flyDestination.x==-1536&&flyDestination.z==1025&&flyVelocity.x==17);
        processor.posinfo.position.x=-937;
        CHECK(LaunchShipFromSomtaaw(&processor,&host));
        CHECK(processor.dockvars.reserveddocking==-1);
    }
    /* Retirement cannot complete at cone acquisition; entry owns completion. */
    ship.dockvars.dockstate2=0;ship.posinfo.position.z=200;command.dock.dockType=DOCK_FOR_RETIRE;
    CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));
    ship.posinfo.position.z=0;ship.posinfo.position.x=20;CHECK(ShipDocksAtSomtaaw(&command,&ship,&host)==TRUE);CHECK(removed==2);
    cataDockRuntimeFree(&ship);
    /* Native endpoint plus the original Worker's own Center attachment. */
    {
        DockStaticInfo center={0};center.numDockPoints=1;strcpy(center.dockstaticpoints[0].name,"center");
        center.dockstaticpoints[0].conenormal.x=-1;center.dockstaticpoints[0].position.x=5;info.dockStaticInfo=&center;
        point.position.x=20;point.conenormal.z=1;point.headingdirection=2;point.updirection=0;
        ship.posinfo.position.x=15;ship.posinfo.position.z=0;identity(&ship.rotinfo.coordsys);
        CHECK(cataDockOffset(&ship,&host,&point,FALSE));CHECK(ship.posinfo.position.x==15&&ship.posinfo.velocity.x==17);
        /* Drive real pose acquisition through all four service combinations. */
        for(i=0;i<4;i++){
        ship.dockvars.dockstate2=7;ship.dockvars.dockstaticpoint=&ports->dockstaticpoints[8];
        ship.dockvars.dockship=&host;ship.dockvars.reserveddocking=-1;dockReserveDockPoint(&ship,&host,8);
        command.dock.dockType=DOCK_TO_DEPOSIT_RESOURCES;
        ship.posinfo.position.x=20;ship.posinfo.position.z=100;
        CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(ship.dockvars.dockstate2==8);
        ship.posinfo.position.x=15;ship.posinfo.position.z=0;identity(&ship.rotinfo.coordsys);
        ship.resources=i&1?50:0;ship.health=i&2?10:100;universe.totaltimeelapsed=10;
        CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(ship.dockvars.dockstate2==3&&clamped==1);
        CHECK(cataDockRuntime(&ship)->deadline==10+(i&1?4:0)+(i&2?6:0));
        universe.totaltimeelapsed=cataDockRuntime(&ship)->deadline;
        clamped=1;command.dock.dockType=DOCK_TO_DEPOSIT_RESOURCES;
        CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));CHECK(ship.dockvars.dockstate2==3&&clamped==1);
        universe.totaltimeelapsed+=.1f;CHECK(!ShipDocksAtSomtaaw(&command,&ship,&host));
        CHECK(ship.resources==0&&ship.dockvars.dockstate2==5&&clamped==0);
        ship.posinfo.position.z=201;CHECK(ShipDocksAtSomtaaw(&command,&ship,&host)==TRUE);
        CHECK(ship.dockvars.reserveddocking==-1);cataDockRuntimeFree(&ship);
        }
        {
            Ship pod={0};ShipStaticInfo podInfo={0};DockStaticInfo latch={0};ClampInfo oldClamp={0};
            pod.shiptype=CataMessagePod;pod.staticinfo=&podInfo;podInfo.dockStaticInfo=&latch;
            latch.numDockPoints=1;latch.dockstaticpoints[0]=point;
            identity(&pod.rotinfo.coordsys);identity(&ship.rotinfo.coordsys);
            ship.shiptype=CataWorker;
            ship.clampInfo=&oldClamp;oldClamp.host=(SpaceObjRotImpTarg*)&pod;
            cataDockRestoreSalvageContact(&ship,&pod,0);
            CHECK(fabsf(ship.posinfo.position.x-45.97f)<.01f&&fabsf(oldClamp.clampOffset.x-45.97f)<.01f);
            CHECK(fabsf(ship.posinfo.position.z-7.16f)<.01f&&fabsf(oldClamp.clampOffset.z-7.16f)<.01f);
            CHECK(cataDockSalvageStep(&ship,&pod,0,2));
            ship.clampInfo=NULL;
        }
        info.dockStaticInfo=NULL;CHECK(!cataDockOffset(&ship,&host,&point,FALSE));
    }
    {
        DockStaticPoint *port=&ports->dockstaticpoints[0];
        strcpy(port->name,"rpmedin");port->type=DPT_Med_In_Engineering;
        CHECK(cataDockSalvageIntakePoint(&host,0,&mapped));
        CHECK(mapped.position.z==25.0f&&mapped.conenormal.y<-.99f);
        strcpy(port->name,"dbmedinsb");port->type=DPT_Med_In;
        CHECK(cataDockSalvageIntakePoint(&host,0,&mapped)&&mapped.position.z==10.0f);
        strcpy(port->name,"dbmedinp");
        CHECK(cataDockSalvageIntakePoint(&host,0,&mapped)&&mapped.position.z==2.0f);
    }
    CHECK(universe.players[0].resourceUnits==100&&universe.gameStats.totalResourceUnitsCollected==100);
    /* Quick Dock must not wait for a free port or run the movement FSM. */
    {
        ClampInfo contact={0};int oldInside=inside,oldRemoved=removed,oldFlies=flies;
        for(i=0;i<10;i++)busy->dockpoints[i].thisDockBusy=1;
        ship.dockvars.dockstate2=1;ship.dockvars.reserveddocking=0;
        ship.dockvars.dockship=&host;ship.dockvars.dockstaticpoint=&ports->dockstaticpoints[0];
        cataDockRuntime(&ship);contact.host=(SpaceObjRotImpTarg*)&host;ship.clampInfo=&contact;clamped=1;
        command.dock.dockType=DOCK_PERMANENTLY|DOCK_INSTANTANEOUSLY;
        CHECK(ShipDocksAtSomtaaw(&command,&ship,&host)==PERMANENTLY_DOCKED);
        CHECK(inside==oldInside+1&&removed==oldRemoved+1&&flies==oldFlies);
        CHECK(ship.dockvars.reserveddocking==-1&&busy->dockpoints[0].thisDockBusy==0);
        CHECK(allocated==0&&clamped==0);ship.clampInfo=NULL;
    }
    free(ports);free(busy);CHECK(allocated==0);printf("PASS: %d original Cataclysm docking/launch checks\n",checks);return 0;
}

