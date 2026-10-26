/* Exercise the production controller with deterministic input/physics seams.
   No game window, graphics device, game data or save files are touched. */
#include <SDL2/SDL.h>
static Uint32 testTicks=100;
#define SDL_GetTicks() testTicks
#define SDL_GetKeyboardFocus() ((SDL_Window *)1)
#include "../src/Game/ShipPilot.c"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "../src/Game/ShipPilotWeapons.inc"
#include <stdio.h>
#include <stdlib.h>

Universe universe;
TacticsInfo tacticsInfo;
MaxSelection selSelected;
bool32 gameIsRunning=TRUE,singlePlayerGame=TRUE,nisIsRunning=FALSE,smSensorsActive=FALSE;
sdword MAIN_WindowWidth=1280,MAIN_WindowHeight=720;
sdword camMouseX,camMouseY,wheel_up,wheel_down;
real32 burstCoolDownTime=10;
volatile keyScanType keyScanCode[KEY_TOTAL_KEYS];
sdword mouseCursorXPosition=640,mouseCursorYPosition=360;
static int wheel,hidden,shots;
static int ready=1,aligned=1,tracking;
static int headingCalls,rotationBrakeCalls;
static SpaceObjRotImpTarg *shotTarget;
static GunInfo guns;
static GunStatic weapon;
static vector velocityRequest,headingRequest;
static vector trackedPoint;
static CommandToDo command;
static SelectCommand selection;
static Ship ship;
static ShipStaticInfo stats;
static Player player;
static Node node;
static int failures;
static int formationUpdates;
static real32 axisTorque[6];
void physApplyRotToObj(SpaceObjRot *obj,real32 torque,uword dir)
{ (void)obj;axisTorque[dir]=torque; }
void processFormationToDo(CommandToDo *c,bool32 steady,bool32 passive)
{ (void)c;(void)steady;(void)passive;formationUpdates++; }
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); failures++; } } while(0)

bool32 modernMapEditorActive(void) { return FALSE; }
void mousePositionSet(sdword x,sdword y) { mouseCursorXPosition=x;mouseCursorYPosition=y; }
void mouseCursorShow(void) { hidden=0; }
void mouseCursorHide(void) { hidden=1; }
void mouseClipToRect(rectangle *r) { (void)r; }
sdword mouseWheelStepsTake(void) { int result=wheel;wheel=0;return result; }
void clHaltThese(CommandLayer *layer,SelectCommand *s)
{
    if(s->numShips>1) { s->ShipPtr[0]->command->ordertype.order=COMMAND_HALT;return; }
    (void)layer;selection=*s;command.selection=&selection;
    command.ordertype.order=COMMAND_HALT;s->ShipPtr[0]->command=&command;
}
void cameraSetEyePosition(Camera *c) { c->eyeposition=c->lookatpoint;c->eyeposition.z-=c->distance; }
void cameraMouseDrag(Camera *c,sdword x,sdword y,bool32 zoom,bool32 limit)
{ (void)zoom;(void)limit;c->angle+=x*0.01f;c->declination+=y*0.01f; }
void cameraRayCast(vector *v,Camera *c,sdword x,sdword y,sdword w,sdword h)
{ (void)c;(void)x;(void)y;(void)w;(void)h;v->x=0;v->y=0;v->z=1; }
void vecNormalize(vector *v)
{ float len=(float)sqrt(vecMagnitudeSquared(*v));if(len>0) { vecMultiplyByScalar(*v,1/len); } }
real32 tacticsGetShipsMaxVelocity(Ship *s) { return s->staticinfo->staticheader.maxvelocity*0.8f; }
bool32 aitrackHeadingFunc(SpaceObjRotImpTargGuidance *s,vector *v,real32 a,real32 b,real32 c,udword flags)
{ (void)s;(void)a;(void)b;(void)c;(void)flags;headingRequest=*v;headingCalls++;return FALSE; }
bool32 aitrackZeroRotationAnywhere(Ship *s) { (void)s;rotationBrakeCalls++;return FALSE; }
void aitrackVelocityVectorGuidance(SpaceObjRotImpTargGuidance *s,vector *v) { (void)s;velocityRequest=*v; }
void matMultiplyVecByMat(vector *out,vector *v,matrix *m)
{
    out->x=v->x*m->m11+v->y*m->m21+v->z*m->m31;
    out->y=v->x*m->m12+v->y*m->m22+v->z*m->m32;
    out->z=v->x*m->m13+v->y*m->m23+v->z*m->m33;
}
bool32 gunCanShoot(Ship *s,Gun *g) { (void)s;(void)g;return ready; }
bool32 gunOrientGimbleGun(Ship *s,Gun *g,SpaceObjRotImpTarg *t)
{ (void)s;(void)g;CHECK(t->objtype==OBJ_AsteroidType);trackedPoint=t->collInfo.collPosition;tracking++;return aligned; }
void gunShoot(Ship *s,Gun *g,SpaceObjRotImpTarg *t) { (void)s;(void)g;shots++;shotTarget=t; }
void missileShoot(Ship *s,Gun *g,SpaceObjRotImpTarg *t) { gunShoot(s,g,t); }
void madOpenGunsShip(Ship *s) { s->madGunStatus=MAD_STATUS_GUNS_OPENING; }
real32 missileDestroyerSpecialReadiness(Ship *s) { (void)s;return 1; }

int main(void)
{
    Gun gun={0};GunStatic gs={0};const char *label;
    Camera *c=&universe.mainCameraCommand.actualcamera;
    memset(&ship,0,sizeof(ship));memset(&stats,0,sizeof(stats));
    stats.staticheader.maxvelocity=100;stats.minimumZoomDistance=30;
    stats.staticheader.staticCollInfo.collspheresize=10;
    ship.staticinfo=&stats;ship.playerowner=&player;ship.shipID.shipNumber=7;
    guns.numGuns=1;guns.guns[0].gunstatic=&weapon;ship.gunInfo=&guns;
    weapon.guntype=GUN_Fixed;weapon.gunnormal.z=1;weapon.triggerHappy=0.99f;
    ship.rotinfo.coordsys.m11=ship.rotinfo.coordsys.m22=ship.rotinfo.coordsys.m33=1;
    node.structptr=&ship;universe.ShipList.head=&node;universe.curPlayerPtr=&player;
    selSelected.numShips=1;selSelected.ShipPtr[0]=&ship;
    c->angle=0.7f;c->declination=0.3f;c->distance=500;
    CHECK(shipPilotCanEnter());pilotEnter();CHECK(shipPilotActive());CHECK(hidden);
    CHECK(ship.command==&command);
    shipPilotUpdate();CHECK(vecMagnitudeSquared(velocityRequest)==0);
    keyScanCode[WKEY].keypressed=1;shipPilotUpdate();CHECK(pilot.throttle==0.25f);
    shipPilotUpdate();CHECK(pilot.throttle==0.25f);CHECK(velocityRequest.z==20);
    keyScanCode[WKEY].keypressed=0;shipPilotUpdate();CHECK(velocityRequest.z==20);
    for(int i=0;i<6;i++) {
        keyScanCode[WKEY].keypressed=1;shipPilotUpdate();
        keyScanCode[WKEY].keypressed=0;shipPilotUpdate();
    }
    CHECK(pilot.throttle==1);
    for(int i=0;i<6;i++) {
        keyScanCode[SKEY].keypressed=1;shipPilotUpdate();
        keyScanCode[SKEY].keypressed=0;shipPilotUpdate();
    }
    CHECK(pilot.throttle==-0.25f && velocityRequest.z==-20);
    /* All independent maneuver combinations obey the native speed ceiling. */
    pilot.throttle=0;
    for(int mask=0;mask<16;mask++) {
        keyScanCode[QKEY].keypressed=mask&1;
        keyScanCode[EKEY].keypressed=(mask>>1)&1;
        keyScanCode[RKEY].keypressed=(mask>>2)&1;
        keyScanCode[FKEY].keypressed=(mask>>3)&1;
        shipPilotUpdate();
        CHECK(vecMagnitudeSquared(velocityRequest)<=6400.01f);
        CHECK(velocityRequest.z==0);
    }
    memset((void *)keyScanCode,0,sizeof(keyScanCode));
    keyScanCode[EKEY].keypressed=1;shipPilotUpdate();CHECK(velocityRequest.y==80);
    keyScanCode[EKEY].keypressed=0;shipPilotUpdate();CHECK(vecMagnitudeSquared(velocityRequest)==0);
    keyScanCode[AKEY].keypressed=1;shipPilotUpdate();CHECK(headingRequest.y<0);
    keyScanCode[AKEY].keypressed=0;keyScanCode[DKEY].keypressed=1;shipPilotUpdate();CHECK(headingRequest.y>0);
    keyScanCode[DKEY].keypressed=0;
    headingCalls=0;pilot.aim.x=10000;shipPilotUpdate();CHECK(headingCalls==0);
    CHECK(ship.posinfo.velocity.x==0 && ship.posinfo.velocity.y==0 && ship.posinfo.velocity.z==0);
    {
        real32 distance=c->distance,angle=c->angle;
        shipPilotMouse(RPE_WheelUp,1);CHECK(c->distance<distance && pilot.throttle==0);
        distance=c->distance;shipPilotMouse(RPE_WheelUp,0);CHECK(c->distance==distance);
        keyScanCode[TWOKEY].keypressed=1;testTicks+=100;shipPilotCamera(c);
        CHECK(c->distance>distance);keyScanCode[TWOKEY].keypressed=0;
        mouseCursorXPosition-=20;shipPilotCamera(c);CHECK(c->angle!=angle);
        mouseCursorXPosition=0;shipPilotCamera(c);
        angle=c->angle;testTicks+=100;shipPilotCamera(c);CHECK(c->angle>angle);
        mouseCursorXPosition=400;mouseCursorYPosition=300;shipPilotCamera(c);
        angle=c->angle;
        keyScanCode[RMOUSE_BUTTON].keypressed=1;shipPilotMouse(RPE_PressRight,0);
        CHECK(pilot.x==MAIN_WindowWidth/2 && pilot.y==MAIN_WindowHeight/2);
        CHECK(pilot.pressureX==0 && pilot.pressureY==0);
        angle=c->angle;
        mouseCursorXPosition-=20;shipPilotCamera(c);
        CHECK(c->angle!=angle);
        angle=c->angle;
        weapon.guntype=GUN_NewGimble;aligned=1;ready=1;shots=0;
        keyScanCode[SPACEKEY].keypressed=1;shipPilotUpdate();CHECK(shots==1);
        keyScanCode[SPACEKEY].keypressed=0;
        keyScanCode[LMOUSE_BUTTON].keypressed=1;shipPilotMouse(RPE_PressLeft,0);
        shipPilotUpdate();CHECK(shots==2);
        keyScanCode[LMOUSE_BUTTON].keypressed=0;shipPilotMouse(RPE_ReleaseLeft,0);
        keyScanCode[RMOUSE_BUTTON].keypressed=0;shipPilotCamera(c);
        CHECK(c->angle==angle);
        CHECK(pilotEdgePressure(0,0,-10,1280)==10);
        CHECK(pilotEdgePressure(10,0,-10,1280)==20);
        CHECK(pilotEdgePressure(600,0,1,1280)==0);
        CHECK(pilotEdgePressure(600,1279,-1,1280)==0);
        CHECK(pilotEdgePressure(0,1279,10,1280)==10);
        CHECK(pilotEdgePressure(600,0,-100,1280)==640);
        CHECK(pilotEdgeRate(0,1280)==300);
        CHECK(pilotEdgeRate(64,1280)<330);
        CHECK(pilotEdgeRate(320,1280)==975);
        CHECK(pilotEdgeRate(640,1280)==3000);
        CHECK(pilotEdgeRate(1280,1280)==3000);
        CHECK(pilotEdgeRate(640,2560)==pilotEdgeRate(320,1280));
        mouseCursorXPosition=0;shipPilotMouseMotion(-20,0);
        CHECK(pilot.pressureX==20 && mouseCursorXPosition==0);
        shipPilotMouseMotion(1,0);CHECK(pilot.pressureX==0);
        mouseCursorXPosition=MAIN_WindowWidth-100;mouseCursorYPosition=265;
        keyScanCode[LMOUSE_BUTTON].keypressed=1;shipPilotMouse(RPE_PressLeft,0);
        shipPilotUpdate();CHECK(shots==2);
        keyScanCode[LMOUSE_BUTTON].keypressed=0;shipPilotMouse(RPE_ReleaseLeft,0);
        CHECK(pilot.all);pilot.all=FALSE;
        weapon.guntype=GUN_Fixed;
    }
    /* Right-click frames the picked target from behind our ship without
       changing zoom or hull orientation. Hidden targets are not picked. */
    {
        Ship target={0};ShipStaticInfo targetStats={0};Node targetNode={0};
        vector aim={0,0,1000},point;
        real32 distance=c->distance;
        target.staticinfo=&targetStats;targetStats.staticheader.staticCollInfo.collspheresize=50;
        target.collInfo.collPosition.z=500;
        targetNode.structptr=&target;node.next=&targetNode;
        CHECK(pilotPickTarget(&ship,c,&aim,&point)==&target);
        target.flags=SOF_Hide;CHECK(pilotPickTarget(&ship,c,&aim,&point)==NULL);
        target.flags=0;
        shipPilotMouse(RPE_PressRight,0);
        CHECK(fabs(c->declination-1.57079632679f)<0.0001f);
        CHECK(c->distance==distance && ship.rotinfo.coordsys.m33==1);
        CHECK(pilot.x==MAIN_WindowWidth/2 && pilot.y==MAIN_WindowHeight/2);
        node.next=NULL;
    }
    /* Reading cooldown does not mutate burst state or last-fire timestamp. */
    gun.gunstatic=&gs;gs.firetime=10;gun.lasttimefired=20;universe.totaltimeelapsed=25;
    CHECK(fabs(shipPilotGunReady(&ship,&gun,&label)-0.5f)<0.001f);
    CHECK(gun.lasttimefired==20 && gun.burstState==0);
    gun.burstState=2;gun.useBurstWaitTime=20;gun.burstStartTime=20;
    CHECK(fabs(shipPilotGunReady(&ship,&gun,&label)-0.25f)<0.001f);
    shipPilotExit();CHECK(!shipPilotActive());CHECK(!hidden);CHECK(c->angle==0.7f && c->distance==500);
    pilotEnter();ship.flags|=SOF_Dead;CHECK(shipPilotShip()==NULL);CHECK(!shipPilotActive());
    ship.flags=0;pilotEnter();command.ordertype.order=COMMAND_DOCK;CHECK(shipPilotShip()==NULL);
    pilotEnter();ship.shipID.shipNumber++;CHECK(shipPilotShip()==NULL);
    {
        vector point={100,0,0};Ship target={0};Gun *g=&guns.guns[0];
        shots=0;
        gunPilotAimAndFire(&ship,&point,&target,TRUE,FALSE);CHECK(shots==0);
        gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);CHECK(shots==1);
        CHECK(g->gunheading.z==1 && shotTarget==NULL);
        weapon.guntype=GUN_Gimble;weapon.cosmaxAngleFromNorm=0.8660254f;weapon.cosminAngleFromNorm=1;
        gunPilotAimAndFire(&ship,&point,&target,TRUE,FALSE);CHECK(shots==1);
        CHECK(fabs(g->gunheading.z-0.8660254f)<0.0001f && fabs(g->gunheading.x-0.5f)<0.0001f);
        gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);CHECK(shots==2 && shotTarget==NULL);
        point.x=0;point.z=-100;
        gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);
        CHECK(fabs(vecMagnitudeSquared(g->gunheading)-1)<0.0001f);
        CHECK(fabs(g->gunheading.z-0.8660254f)<0.0001f);
        point.z=100;
        gunPilotAimAndFire(&ship,&point,&target,TRUE,FALSE);CHECK(shotTarget==(SpaceObjRotImpTarg *)&target);
        weapon.guntype=GUN_NewGimble;aligned=0;ready=0;tracking=0;
        g->curgunorientation.m33=1;shots=0;
        gunPilotAimAndFire(&ship,&point,&target,FALSE,FALSE);CHECK(tracking==1 && shots==0);
        gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);CHECK(tracking==2 && shots==0);
        ready=1;gunPilotAimAndFire(&ship,&point,&target,TRUE,FALSE);CHECK(shots==0);
        gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);CHECK(shots==1 && shotTarget==NULL);
        weapon.guntype=GUN_MissileLauncher;g->numMissiles=0;
        gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);CHECK(shots==1);
        g->numMissiles=1;gunPilotAimAndFire(&ship,&point,&target,TRUE,TRUE);CHECK(shots==2);
    }
    shipPilotExit();
    ship.flags=0;ship.shipID.shipNumber=7;ship.command=NULL;ship.formationcommand=NULL;
    stats.shipclass=CLASS_Fighter;pilotEnter();
    CHECK(shipPilotCockpitAvailable());shipPilotKey(CKEY);CHECK(shipPilotCockpit());
    pilot.throttle=0;shipPilotMouse(RPE_WheelUp,1);CHECK(fabs(pilot.throttle-0.1f)<0.0001f);
    for(int j=0;j<30;j++) shipPilotMouse(RPE_WheelDown,1);
    CHECK(pilot.throttle==-1);
    pilot.throttle=0;
    universe.phystimeelapsed=0.05f;
    stats.staticheader.momentOfInertiaX=stats.staticheader.momentOfInertiaY=stats.staticheader.momentOfInertiaZ=1;
    for(int j=0;j<6;j++) ship.nonstatvars.rotstrength[j]=5;
    for(int j=0;j<3;j++) ship.nonstatvars.turnspeed[j]=1;
    shipPilotMouseMotion(100,-100);
    CHECK(mouseCursorXPosition==MAIN_WindowWidth/2 && mouseCursorYPosition==MAIN_WindowHeight/2);
    shipPilotCamera(c);headingCalls=0;shipPilotUpdate();
    CHECK(headingCalls==0 && axisTorque[ROT_YAWRIGHT]==5 && axisTorque[ROT_PITCHUP]==5);
    CHECK(pilot.steerX==0 && pilot.steerY==0);
    CHECK(c->eyeposition.x==ship.posinfo.position.x && c->eyeposition.z==ship.posinfo.position.z);
    keyScanCode[DKEY].keypressed=1;keyScanCode[WKEY].keypressed=1;
    shipPilotUpdate();CHECK(velocityRequest.y==0 && velocityRequest.z>0 && axisTorque[ROT_ROLLRIGHT]==5);
    CHECK(fabs(pilot.throttle-0.0375f)<0.0001f);
    keyScanCode[RMOUSE_BUTTON].keypressed=1;shipPilotMouseMotion(100,50);shipPilotCamera(c);
    CHECK(pilot.freeYaw>0 && pilot.freePitch<0 && pilot.steerX==0);
    keyScanCode[RMOUSE_BUTTON].keypressed=0;shipPilotCamera(c);
    CHECK(pilot.freeYaw==0 && pilot.freePitch==0);
    memset((void *)keyScanCode,0,sizeof(keyScanCode));
    shipPilotKey(CKEY);CHECK(!shipPilotCockpit());
    shipPilotExit();stats.shipclass=CLASS_Corvette;pilotEnter();
    CHECK(!shipPilotCockpitAvailable());shipPilotKey(CKEY);CHECK(!shipPilotCockpit());shipPilotExit();
    {
        Ship wing={0};ShipStaticInfo wingStats={0};CommandToDo squad={0};
        struct {sdword numShips;Ship *ShipPtr[2];} members;
        wing.staticinfo=&wingStats;wing.playerowner=&player;
        wingStats.shipclass=CLASS_Fighter;wingStats.staticheader.maxvelocity=40;
        stats.shipclass=CLASS_Fighter;
        members.numShips=2;members.ShipPtr[0]=&ship;members.ShipPtr[1]=&wing;
        squad.selection=(SelectCommand *)&members;squad.ordertype.attributes=COMMAND_MASK_FORMATION;
        squad.formation.formationtype=DELTA3D_FORMATION;
        ship.command=ship.formationcommand=&squad;wing.command=wing.formationcommand=&squad;
        CHECK(shipPilotSquadCount()==2);pilotEnter();
        CHECK(pilot.squad && ship.command==&squad && shipPilotControls(&wing));
        CHECK(shipPilotMaxSpeed()==32);pilot.throttle=1;formationUpdates=0;
        shipPilotUpdate();CHECK(velocityRequest.z==32 && formationUpdates==1);
        wingStats.staticheader.maxvelocity=20;CHECK(shipPilotMaxSpeed()==16);
        squad.formation.formationtype=CLAW_FORMATION;CHECK(shipPilotShip()==&ship);
        shipPilotExit();selSelected.ShipPtr[0]=&wing;CHECK(shipPilotSquadCount()==0);
        selSelected.ShipPtr[0]=&ship;pilotEnter();squad.ordertype.order=COMMAND_DOCK;
        CHECK(shipPilotShip()==NULL);
        ship.command=ship.formationcommand=NULL;
    }
    {
        Ship target={0};Node targetNode={0};Gun testGun={0};GunStatic testStatic={0};
        stats.shipclass=CLASS_Frigate;pilotEnter();
        target.flags=SOF_Targetable;target.shipID.shipNumber=42;
        target.collInfo.collPosition.z=100;targetNode.structptr=&target;node.next=&targetNode;
        c->eyeposition.x=c->eyeposition.y=c->eyeposition.z=0;
        c->lookatpoint.x=c->lookatpoint.y=0;c->lookatpoint.z=1;
        c->upvector.x=0;c->upvector.y=1;c->upvector.z=0;c->fieldofview=60;
        mousePositionSet(MAIN_WindowWidth/2,MAIN_WindowHeight/2);
        pilot.tabDown=FALSE;shipPilotKey(TABKEY);CHECK(shipPilotTarget()==&target);
        testGun.gunstatic=&testStatic;testStatic.guntype=GUN_Fixed;
        testStatic.gunnormal.z=1;testStatic.triggerHappy=.99f;testStatic.bulletrange=200;
        memset(&ship.posinfo.position,0,sizeof(vector));
        CHECK(shipPilotWeaponCanHit(&ship,&testGun,(SpaceObjRotImpTarg *)&target));
        target.collInfo.collPosition.z=201;
        CHECK(!shipPilotWeaponCanHit(&ship,&testGun,(SpaceObjRotImpTarg *)&target));
        target.collInfo.collPosition.z=100;target.collInfo.collPosition.x=100;
        CHECK(!shipPilotWeaponCanHit(&ship,&testGun,(SpaceObjRotImpTarg *)&target));
        target.flags|=SOF_Dead;CHECK(shipPilotTarget()==NULL);
        node.next=NULL;shipPilotExit();
    }
    {
        vector levelUp={0,0,1}, levelRight={1,0,0};
        real32 savedDt=universe.phystimeelapsed;
        stats.staticheader.momentOfInertiaZ=2;
        ship.nonstatvars.turnspeed[TURN_ROLL]=1;
        ship.nonstatvars.rotstrength[ROT_ABOUTZCCW]=4;
        ship.nonstatvars.rotstrength[ROT_ABOUTZCW]=4;
        CHECK(pilotLevelRoll(&ship,&levelUp,&levelRight)==0);
        for(int sign=-1;sign<=1;sign+=2) for(int hz=20;hz<=60;hz+=40) {
            real32 bank=sign*2.0f,rate=sign*1.0f;
            universe.phystimeelapsed=1.0f/hz;
            for(int tick=0;tick<hz*8;++tick) {
                levelUp.z=(real32)cos(bank);levelRight.z=-(real32)sin(bank);
                memset(axisTorque,0,sizeof(axisTorque));
                pilotTrackAxis(&ship,pilotLevelRoll(&ship,&levelUp,&levelRight),rate,2,ROT_ABOUTZCCW);
                CHECK(axisTorque[ROT_ABOUTZCCW]<=4&&axisTorque[ROT_ABOUTZCW]<=4);
                rate+=(axisTorque[ROT_ABOUTZCCW]-axisTorque[ROT_ABOUTZCW])*universe.phystimeelapsed/2;
                bank+=rate*universe.phystimeelapsed;
            }
            CHECK(fabs(bank)<.006f&&fabs(rate)<.001f);
        }
        /* Released input brakes either rotation sign without crossing zero. */
        for(int axis=0;axis<3;++axis) for(int sign=-1;sign<=1;sign+=2) {
            real32 rate=sign*.01f;
            memset(axisTorque,0,sizeof(axisTorque));
            pilotTrackAxis(&ship,0,rate,2,(uword)(axis*2));
            rate+=(axisTorque[axis*2]-axisTorque[axis*2+1])*universe.phystimeelapsed/2;
            CHECK(fabs(rate)<.0001f);
        }
        universe.phystimeelapsed=savedDt;
    }
    {
        Ship target={0};Node targetNode={0};vector requested;
        memset((void *)keyScanCode,0,sizeof(keyScanCode));
        stats.shipclass=CLASS_Frigate;target.staticinfo=&stats;
        target.playerowner=ship.playerowner; /* Deliberate friendly-fire lock. */
        target.flags=SOF_Targetable;target.shipID.shipNumber=43;
        target.posinfo.position.z=target.collInfo.collPosition.z=2000;
        targetNode.structptr=&target;node.next=&targetNode;
        CHECK(shipPilotCommandAvailable());
        mousePositionSet(MAIN_WindowWidth-40,140);shipPilotMouse(RPE_ReleaseLeft,0);
        CHECK(shipPilotCommandMode());CHECK(!shipPilotAutoFire());
        /* The fullscreen HUD need not dispatch a legacy key callback. Every
           function key is sampled on the controller path, once per press. */
        free(pilot.weapons);pilot.weaponCount=12;pilot.weapons=(unsigned char *)calloc(12,1);
        for(int k=0;k<12;k++)
        {
            keyScanCode[F1KEY+k].keypressed=1;pilotThrottleUpdate();
            CHECK(shipPilotWeaponActive(k));pilotThrottleUpdate();CHECK(shipPilotWeaponActive(k));
            keyScanCode[F1KEY+k].keypressed=0;pilotThrottleUpdate();
            keyScanCode[F1KEY+k].keypressed=1;pilotThrottleUpdate();CHECK(!shipPilotWeaponActive(k));
            keyScanCode[F1KEY+k].keypressed=0;pilotThrottleUpdate();
        }
        mousePositionSet(640,300);keyScanCode[CONTROLKEY].keypressed=1;
        shipPilotMouse(RPE_PressLeft,0);CHECK(shipPilotTarget()==&target);
        CHECK(pilot.navigation==0 && !shipPilotAutoFire());
        keyScanCode[CONTROLKEY].keypressed=0;
        shipPilotKey(QKEY);CHECK(pilot.navigation==2);
        pilotCommandVelocity(&ship,&requested);CHECK(requested.z>0 && vecMagnitudeSquared(requested)<=6400.1f);
        shipPilotKey(EKEY);pilot.navRange=1980;pilotCommandVelocity(&ship,&requested);
        CHECK(vecMagnitudeSquared(requested)<.01f);
        pilot.navRange=3000;pilotCommandVelocity(&ship,&requested);CHECK(requested.z<0);
        shipPilotKey(WKEY);pilot.navRange=1980;pilotCommandVelocity(&ship,&requested);
        CHECK(vecMagnitudeSquared(requested)>0 && vecMagnitudeSquared(requested)<=6400.1f);
        CHECK(fabs(requested.z)<.01f); /* Tangent, even when radial parallels world up. */
        keyScanCode[F1KEY].keypressed=1;shipPilotKey(F1KEY);CHECK(shipPilotWeaponActive(0));
        {
            int before=shots;
            weapon.guntype=GUN_NewGimble;weapon.bulletrange=10000;aligned=ready=1;
            gunPilotAimAndFire(&ship,&target.collInfo.collPosition,&target,TRUE,FALSE);CHECK(shots==before+1);
        }
        shipPilotKey(F1KEY);CHECK(shipPilotWeaponActive(0)); /* No repeat toggles. */
        keyScanCode[F1KEY].keypressed=0;pilotThrottleUpdate();
        keyScanCode[F1KEY].keypressed=1;shipPilotKey(F1KEY);CHECK(!shipPilotWeaponActive(0));
        {
            int before=shots;
            gunPilotAimAndFire(&ship,&target.collInfo.collPosition,&target,TRUE,FALSE);CHECK(shots==before);
        }
        keyScanCode[F1KEY].keypressed=0;
        {
            real32 angle=c->angle;
            mousePositionSet(400,300);testTicks+=20;shipPilotCamera(c);CHECK(c->angle==angle);
            keyScanCode[RMOUSE_BUTTON].keypressed=1;mousePositionSet(420,300);shipPilotCamera(c);CHECK(c->angle!=angle);
            keyScanCode[RMOUSE_BUTTON].keypressed=0;
        }
        target.flags|=SOF_Dead;pilotCommandVelocity(&ship,&requested);
        CHECK(pilot.navigation==0 && vecMagnitudeSquared(requested)==0);
        node.next=NULL;
        testTicks+=500;mousePositionSet(640,300);shipPilotMouse(RPE_PressLeft,0);
        testTicks+=100;shipPilotMouse(RPE_PressLeft,0);CHECK(pilot.navigation==1);
        keyScanCode[CONTROLKEY].keypressed=1;shipPilotKey(SPACEKEY);
        CHECK(pilot.navigation==0 && pilot.throttle==0);
        keyScanCode[CONTROLKEY].keypressed=0;shipPilotExit();
        stats.shipclass=CLASS_Fighter;CHECK(!shipPilotCommandAvailable());
    }
    fprintf(stdout,"Ship piloting checks: %s\n",failures?"FAILED":"passed");
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
