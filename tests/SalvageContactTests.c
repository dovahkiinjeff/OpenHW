/* Execute the production salvage contact and delivery primitives without UI. */
#include "Universe.h"
#include "SalCapCorvette.h"
#include "Aitrack.h"
#include "Aiship.h"
#include "NativeVisualRegistry.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static int checks,nativeAvailable=1,missingMarker=-1,flights,endpointCalls,headingAligned=1;
static vector nativeGrip={0.000003f,0,61.7968f};
static vector requested;
static const vector contacts[5]={{15.6015f,-62.4934f,494.996f},{67.5083f,133.242f,272.975f},
    {17.7854f,-68.9996f,493.535f},{18.5142f,66.5503f,496.125f},{33.6979f,134.365f,264.46f}};
int meshNativeVisualParameters(const void *mesh,float *scale,unsigned *lod)
{(void)mesh;*scale=1.25315311f;*lod=0;return nativeAvailable;}
int meshNativeVisualRestMarker(const void *mesh,const char *name,float *matrix)
{
    unsigned i;(void)mesh;
    if(!strcmp(name,"salvage_grip")) {
        memset(matrix,0,16*sizeof(float));matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
        memcpy(matrix+12,&nativeGrip,sizeof(vector));return 1;
    }
    if(sscanf(name,"SalvagePoint%u",&i)!=1 || i>=5 || (int)i==missingMarker)return 0;
    memset(matrix,0,16*sizeof(float));matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
    memcpy(matrix+12,&contacts[i],sizeof(vector));return 1;
}
bool32 meshRemasteredDockEndpoint(const void *ship,const char *name,vector *point)
{(void)ship;(void)name;++endpointCalls;point->x=-851;point->y=213;point->z=0;return TRUE;}
bool32 MoveReachedDestinationVariable(Ship *ship,vector *destination,real32 tolerance)
{vector delta;vecSub(delta,ship->posinfo.position,*destination);return vecMagnitudeSquared(delta)<=tolerance*tolerance;}
udword aishipFlyToPointAvoidingObjsFunc(Ship *ship,vector *destination,udword flags,real32 limit,vector *velocity)
{(void)ship;(void)flags;(void)limit;(void)velocity;++flights;requested=*destination;return 0;}
bool32 aitrackHeadingFunc(SpaceObjRotImpTargGuidance *ship,vector *heading,real32 a,real32 b,real32 bank,udword flags)
{(void)ship;(void)heading;(void)a;(void)b;(void)bank;(void)flags;return headingAligned;}
real32 salCapGetMaxPushingVelocity(Ship *ship,Ship *target){(void)ship;(void)target;return 100;}
sdword salCapTrackHeadingAndUp(SpaceObjRotImpTargGuidanceShipDerelict *target,real32 angle)
{(void)target;(void)angle;return FALSE;}
#include "../src/Ships/SalvageContact.inc"
#include "../src/Ships/SalvageApproach.inc"
#include "../src/Ships/SalvageDelivery.inc"

static int same(vector a,vector b){return fabsf(a.x-b.x)<.001f && fabsf(a.y-b.y)<.001f && fabsf(a.z-b.z)<.001f;}
int main(void)
{
    Derelict wreck={0};DerelictStaticInfo info={0};lodinfo levels={0};SalvageStaticPoint points[5]={0};
    SalvageInfo *salvage=calloc(1,sizeofSalvageInfo(5));
    Ship *ship=calloc(1,sizeof(Ship)+sizeof(SalCapCorvetteSpec));
    SalCapCorvetteSpec *spec=(SalCapCorvetteSpec*)ship->ShipSpecifics;
    Ship host={0};ShipStaticInfo hostInfo={0},shipInfo={0};DockInfo dock={0};DockStaticPoint port={0};ClampInfo clamp={0};
    SpaceObjRotImpTargGuidanceShipDerelict *target=(SpaceObjRotImpTargGuidanceShipDerelict*)&wreck;
    vector expected,normal,position,contact,gripWorld,rebuilt;unsigned i;
    CHECK(ship && salvage);wreck.objtype=OBJ_DerelictType;wreck.staticinfo=&info;info.derelicttype=Shipwreck;
    info.staticheader.LOD=&levels;levels.nLevels=1;levels.level[0].flags=LT_Mesh;levels.level[0].pData=&levels;
    wreck.salvageInfo=salvage;salvage->numSalvagePoints=5;
    for(i=0;i<5;i++){salvage->salvagePoints[i].salvageStaticPoint=&points[i];points[i].position.x=9999;}
    missingMarker=3;CHECK(!salCapAlignKharSelimPoints(target));
    for(i=0;i<5;i++)CHECK(points[i].position.x==9999); /* Partial binding cannot corrupt shared static data. */
    missingMarker=-1;CHECK(salCapAlignKharSelimPoints(target));
    for(i=0;i<5;i++){CHECK(same(points[i].position,contacts[i]));CHECK(points[i].conenormal.z==1);}
    wreck.rotinfo.coordsys=(matrix){0,1,0,-1,0,0,0,0,1};wreck.posinfo.position=(vector){100,200,300};
    wreck.posinfo.velocity=(vector){2,3,4};ship->shiptype=SalCapCorvette;spec->target=target;spec->salvageIndex=1;
    ship->staticinfo=&shipInfo;shipInfo.staticheader.LOD=&levels;
    ship->rotinfo.coordsys=(matrix){1,0,0,0,-1,0,0,0,-1};
    matMultiplyMatByVec(&contact,&wreck.rotinfo.coordsys,&contacts[1]);vecAddTo(contact,wreck.posinfo.position);
    matMultiplyMatByVec(&gripWorld,&ship->rotinfo.coordsys,&nativeGrip);vecSub(expected,contact,gripWorld);
    points[1].position=(vector){314,342,482}; /* Stale classic data chosen before native registration. */
    ship->posinfo.position=expected;ship->posinfo.position.x+=20; /* Real arrival tolerance used to freeze a visible gap. */
    CHECK(salCapFlyToConeOrigin(ship,target));CHECK(same(ship->posinfo.position,expected));
    vecAdd(rebuilt,ship->posinfo.position,gripWorld);CHECK(same(rebuilt,contact)); /* Nose/grip, not center, meets target. */
    CHECK(same(ship->posinfo.velocity,wreck.posinfo.velocity));
    headingAligned=0;ship->posinfo.position.x+=20;
    CHECK(!salCapFlyToConeOrigin(ship,target));CHECK(!same(ship->posinfo.position,expected));
    headingAligned=1;
    ship->clampInfo=&clamp;clamp.host=(SpaceObjRotImpTarg*)&wreck;clamp.clampOffset=(vector){999,999,999};
    salCapRestoreKharSelimContact(ship);CHECK(!same(clamp.clampOffset,contacts[1]));CHECK(same(ship->posinfo.position,expected));
    matMultiplyMatByVec(&rebuilt,&wreck.rotinfo.coordsys,&clamp.clampOffset);vecAddTo(rebuilt,wreck.posinfo.position);
    CHECK(same(rebuilt,expected)); /* Saved clamp stores rotated host-local ship origin. */
    ship->shiprace=R2;nativeGrip.z=67.5605f;salCapRestoreKharSelimContact(ship);
    matMultiplyMatByVec(&gripWorld,&ship->rotinfo.coordsys,&nativeGrip);vecAdd(rebuilt,ship->posinfo.position,gripWorld);
    CHECK(same(rebuilt,contact));
    nativeAvailable=0;ship->posinfo.position=contact;ship->posinfo.position.x+=20;CHECK(salCapFlyToConeOrigin(ship,target));
    CHECK(fabsf(ship->posinfo.position.x-contact.x-20)<.001f); /* Preserve classic fallback arrival behavior. */
    nativeAvailable=1;
    info.derelicttype=Junk2_shipwreck;
    CHECK(!salCapAlignKharSelimPoints(target)); /* Do not relocate an unrelated wreck using Khar-Selim points. */
    info.derelicttype=Shipwreck;
    host.staticinfo=&hostInfo;host.shiptype=Mothership;host.shiprace=R1;host.dockInfo=&dock;
    dock.numDockPoints=1;dock.dockpoints[0].dockstaticpoint=&port;strcpy(port.name,"Frigate");
    port.position=(vector){10,20,30};port.conenormal=(vector){0,1,0};host.rotinfo.coordsys=wreck.rotinfo.coordsys;
    host.posinfo.position=(vector){1000,2000,3000};
    salCapDockWorld(&host,0,&normal,&position);CHECK(same(position,(vector){980,2010,3030}));
    CHECK(same(normal,(vector){-1,0,0}));CHECK(endpointCalls==0); /* No final RM waypoint + classic route mixing. */
    spec->dockwith=&host;wreck.posinfo.position=host.posinfo.position;wreck.posinfo.position.x+=4000;
    flights=0;CHECK(salCapFlyToWithinDockingRange(ship));CHECK(flights==0); /* Reserve before entering hull. */
    wreck.posinfo.position.x+=3000;CHECK(!salCapFlyToWithinDockingRange(ship));CHECK(flights==1);
    CHECK(same(requested,host.posinfo.position));
    ship->clampInfo=NULL;free(ship);free(salvage);
    printf("Salvage contact/delivery passed: %d checks; native latch, saved clamp, fallback and port arrival\n",checks);
    return 0;
}
