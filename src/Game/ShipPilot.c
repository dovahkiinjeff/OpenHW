#include "ShipPilot.h"
#include "ShipPilotFrame.h"
#include "CommandNetwork.h"
#include "UnivUpdate.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "Universe.h"
#include "CommandLayer.h"
#include "Formation.h"
#include "Select.h"
#include "AITrack.h"
#include "Physics.h"
#include "Gun.h"
#include "Tactics.h"
#include "Globals.h"
#include "Sensors.h"
#include "NIS.h"
#include "ModernMapEditor.h"
#include "HeavyCorvette.h"
#include "CloakGenerator.h"
#include "GravWellGenerator.h"
#include "MissileDestroyer.h"
#include "CommandDefs.h"
#include "main.h"
#include "mainrgn.h"
#include "mouse.h"
#include "key.h"
#include "region.h"

typedef struct ShipPilotState {
    Ship *ship;
    Ship *target;
    uword targetID;
    bool32 tabDown;
    uword id;
    Camera saved;
    real32 throttle;
    bool32 all, wDown, sDown, uiPress;
    bool32 squad, cockpit, cockpitKeyDown;
    real32 steerX,steerY;
    real32 cockpitIdleTime;
    bool32 commandMode, modeDown;
    udword weaponsDown;
    unsigned char *weapons;
    sdword weaponCount, weaponBank;
    sdword navigation; /* 0 stop, 1 heading, 2 approach, 3 orbit, 4 range */
    Ship *navTarget;
    uword navID;
    vector navHeading;
    real32 navRange;
    Uint32 clickTime;
    sdword clickX,clickY;
    real32 freeYaw,freePitch;
    sdword x, y;
    sdword dragX, dragY;
    real32 edgeX, edgeY;
    real32 pressureX, pressureY;
    vector aim;
    Uint32 cameraTick;
    bool32 awaiting;
    sdword ownerIndex;
    uword held, pickedID;
    bool32 netFire;
    real32 netYaw,netPitch;
} ShipPilotState;
static ShipPilotState pilot;
static ShipPilotState networkPilots[MAX_MULTIPLAYER_PLAYERS];
static udword networkSequences[MAX_MULTIPLAYER_PLAYERS],localSequence;
static bool32 networkSimulation, endPending, resettingPilots;
static uword endShip;
static bool32 pilotNetworked(void) { return multiPlayerGame || playPackets || recordFakeSendPackets; }
static void pilotSendInput(ubyte action);
static void pilotUpdateSimulation(void);
static bool32 pilotSessionValid(const ShipPilotState *state);
static bool32 pilotStateControls(const ShipPilotState *state,const Ship *ship);
static uword pilotInputBit(sdword key)
{
    switch(key) {
    case QKEY:return 1;case EKEY:return 2;case RKEY:return 4;case FKEY:return 8;
    case AKEY:return 16;case DKEY:return 32;case SPACEKEY:return 64;
    default:return 0;
    }
}
static bool32 pilotFlightKey(sdword key)
{ return networkSimulation?(pilot.held&pilotInputBit(key))!=0:keyIsHit(key); }


/* Pressure tracks outward pixels for speed only, never cursor position. Reverse input clears it
   immediately; only outward travel beyond the visible boundary adds pressure. */
static real32 pilotEdgePressure(real32 pressure,int position,int delta,int extent)
{
    int end=position+delta;
    if((position<=1 && delta>0) || (position>=extent-2 && delta<0)) return 0;
    if(end<0) return min(extent*0.5f,pressure-end);
    if(end>=extent) return min(extent*0.5f,pressure+(end-extent+1));
    if(end>1 && end<extent-2) return 0;
    return pressure;
}
static real32 pilotEdgeRate(real32 pressure,int extent)
{
    real32 fraction=extent>0?min(1.0f,max(0.0f,pressure/(extent*0.5f))):0;
    /* A broad quadratic ramp gives small pushes fine control. Full speed
       requires half a viewport of additional outward mouse travel. */
    return 300.0f+2700.0f*fraction*fraction;
}
void shipPilotMouseMotion(sdword dx,sdword dy)
{
    if(!shipPilotActive()) return;
    if(pilot.cockpit)
    {
        if(mouseRightButton())
        {
            pilot.freeYaw=max(-2.6f,min(2.6f,pilot.freeYaw+dx*0.003f));
            pilot.freePitch=max(-1.3f,min(1.3f,pilot.freePitch-dy*0.003f));
            pilot.steerX=pilot.steerY=0;
        }
        else { pilot.steerX+=dx;pilot.steerY-=dy; }
        mousePositionSet(MAIN_WindowWidth/2,MAIN_WindowHeight/2);
        return;
    }
    pilot.pressureX=pilotEdgePressure(pilot.pressureX,mouseCursorX(),dx,MAIN_WindowWidth);
    pilot.pressureY=pilotEdgePressure(pilot.pressureY,mouseCursorY(),dy,MAIN_WindowHeight);
}

static void pilotToggleWeapon(sdword key)
{
    sdword slot;
    udword bit;
    if(!pilot.commandMode || key<F1KEY || key>F12KEY) return;
    bit=1u<<(key-F1KEY);
    if(pilot.weaponsDown&bit) return;
    pilot.weaponsDown|=bit;
    slot=pilot.weaponBank*12+key-F1KEY;
    if(slot<pilot.weaponCount) pilot.weapons[slot]=!pilot.weapons[slot];
}
static void pilotThrottleUpdate(void)
{
    if(!keyIsHit(MKEY)) pilot.modeDown=FALSE;
    for(sdword k=0;k<12;k++)
    {
        if(!keyIsHit(F1KEY+k)) pilot.weaponsDown&=~(1u<<k);
        else if(SDL_GetKeyboardFocus()) pilotToggleWeapon(F1KEY+k);
    }
    if(!keyIsHit(TABKEY)) pilot.tabDown=FALSE;
    if(!keyIsHit(CKEY)) pilot.cockpitKeyDown=FALSE;
    if(pilot.cockpit || pilot.commandMode) return;
    bool32 w=keyIsHit(WKEY),s=keyIsHit(SKEY);
    int step=(w && !pilot.wDown?1:0)-(s && !pilot.sDown?1:0);
    pilot.throttle=max(-0.25f,min(1.0f,pilot.throttle+step*0.25f));
    pilot.wDown=w;pilot.sDown=s;
}

static bool32 pilotOverButton(sdword x,sdword y)
{
    if(pilot.commandMode && x>=16 && x<596 && y>=MAIN_WindowHeight-224 && y<MAIN_WindowHeight-32) return TRUE;
    return (x>=MAIN_WindowWidth-80 && x<MAIN_WindowWidth-16 && y>=180 && y<244) ||
        (x>=MAIN_WindowWidth-156 && x<MAIN_WindowWidth-16 && y>=252 && y<286) ||
        (shipPilotCockpitAvailable() && x>=MAIN_WindowWidth-156 && x<MAIN_WindowWidth-16 && y>=294 && y<328);
}

static real32 pilotClamp(real32 x) { return x<0?0:(x>1?1:x); }
bool32 shipPilotActive(void) { return pilot.ship!=NULL; }
bool32 shipPilotOwns(const Ship *s)
{
    sdword i;
    if(!s)return FALSE;
    if(!pilotNetworked())return s==pilot.ship;
    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
        if(networkPilots[i].ship==s && pilotSessionValid(&networkPilots[i]))return TRUE;
    return FALSE;
}
bool32 shipPilotCockpit(void) { return pilot.ship && pilot.cockpit; }
bool32 shipPilotCommandMode(void) { return pilot.ship && pilot.commandMode; }
const char *shipPilotNavigation(void)
{ static const char *names[]={"STOP","FLY TO HEADING","APPROACH","ORBIT","KEEP AT RANGE"};return names[pilot.navigation]; }
real32 shipPilotCommandRange(void) { return pilot.navRange; }
bool32 shipPilotAutoFire(void)
{ for(sdword i=0;i<pilot.weaponCount;i++) if(pilot.weapons[i]) return TRUE;return FALSE; }
sdword shipPilotWeaponBank(void) { return pilot.weaponBank; }
static void pilotCeaseFire(void) { if(pilot.weapons) memset(pilot.weapons,0,pilot.weaponCount); }
bool32 shipPilotWeaponActive(sdword index)
{ return !pilot.commandMode || (index>=0 && index<pilot.weaponCount && pilot.weapons[index]); }
bool32 shipPilotCommandAvailable(void)
{ return shipPilotCanEnter() && selSelected.ShipPtr[0]->staticinfo->shipclass<=CLASS_Frigate; }
bool32 shipPilotCockpitAvailable(void) { return pilot.ship && pilot.ship->staticinfo->shipclass==CLASS_Fighter; }
static CommandToDo *pilotFormation(Ship *s)
{
    CommandToDo *c=s?s->formationcommand:NULL;
    sdword i;
    if(!c || c!=s->command || !(c->ordertype.attributes&COMMAND_MASK_FORMATION) ||
       c->selection->numShips<2 || c->selection->ShipPtr[0]!=s ||
       (c->formation.formationtype!=DELTA3D_FORMATION && c->formation.formationtype!=CLAW_FORMATION)) return NULL;
    for(i=0;i<c->selection->numShips;i++)
    {
        Ship *member=c->selection->ShipPtr[i];
        if(member->playerowner!=s->playerowner ||
           (member->staticinfo->shipclass!=CLASS_Fighter && member->staticinfo->shipclass!=CLASS_Corvette) ||
           (member->flags&(SOF_Dead|SOF_Hide|SOF_Disabled|SOF_Hyperspace|SOF_NISShip|SOF_Crazy|SOF_Clamped)) || member->dockingship) return NULL;
    }
    return c;
}
static bool32 pilotStateControls(const ShipPilotState *state,const Ship *s)
{
    CommandToDo *c;
    sdword i;
    if(!state->ship || !s)return FALSE;
    if(s==state->ship)return TRUE;
    c=state->squad?pilotFormation(state->ship):NULL;
    if(c && c->ordertype.order==COMMAND_HALT)
        for(i=1;i<c->selection->numShips;i++)if(c->selection->ShipPtr[i]==s)return TRUE;
    return FALSE;
}
bool32 shipPilotControls(const Ship *s)
{
    sdword i;
    if(!pilotNetworked())return pilotStateControls(&pilot,s);
    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
        if(pilotSessionValid(&networkPilots[i]) && pilotStateControls(&networkPilots[i],s))return TRUE;
    return FALSE;
}
sdword shipPilotSquadCount(void)
{
    CommandToDo *c=pilot.ship?(pilot.squad?pilotFormation(pilot.ship):NULL):
        (selSelected.numShips==1?pilotFormation(selSelected.ShipPtr[0]):NULL);
    return c?c->selection->numShips:0;
}
real32 shipPilotMaxSpeed(void)
{
    real32 speed=pilot.ship?tacticsGetShipsMaxVelocity(pilot.ship):0;
    CommandToDo *c=pilot.squad?pilotFormation(pilot.ship):NULL;
    sdword i;
    if(c) for(i=1;i<c->selection->numShips;i++) speed=min(speed,tacticsGetShipsMaxVelocity(c->selection->ShipPtr[i]));
    return speed;
}
real32 shipPilotThrottle(void) { return pilot.throttle; }
Ship *shipPilotTarget(void)
{
    Node *n;
    if(!pilot.ship || pilot.cockpit) return NULL;
    for(n=universe.ShipList.head;n;n=n->next)
    {
        Ship *s=(Ship *)listGetStructOfNode(n);
        if(s==pilot.target && s->shipID.shipNumber==pilot.targetID &&
           !(s->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace|SOF_Cloaked)) && (s->flags&SOF_Targetable)) return s;
    }
    pilot.target=NULL;return NULL;
}
bool32 shipPilotTargetScreen(const Ship *s,real32 *x,real32 *y)
{
    Camera *c=&universe.mainCameraCommand.actualcamera;
    vector forward,right,up,offset;
    real32 depth,halfHeight;
    vecSub(forward,c->lookatpoint,c->eyeposition);vecNormalize(&forward);
    vecCrossProduct(right,forward,c->upvector);vecNormalize(&right);
    vecCrossProduct(up,right,forward);vecNormalize(&up);
    vecSub(offset,s->collInfo.collPosition,c->eyeposition);
    depth=vecDotProduct(offset,forward);
    if(depth<=0.001f) return FALSE;
    halfHeight=(real32)tan(c->fieldofview*0.00872664626f);
    if(halfHeight<=0) return FALSE;
    *x=MAIN_WindowWidth*0.5f+vecDotProduct(offset,right)/depth*0.49f/halfHeight*MAIN_WindowHeight;
    *y=MAIN_WindowHeight*0.5f-vecDotProduct(offset,up)/depth*0.49f/halfHeight*MAIN_WindowHeight;
    return TRUE;
}
static void pilotLockNearest(void)
{
    Node *n;real32 best=1e30f;
    pilot.target=NULL;
    for(n=universe.ShipList.head;n;n=n->next)
    {
        Ship *s=(Ship *)listGetStructOfNode(n);real32 x,y,dist;
        if(pilotStateControls(&pilot,s) || !(s->flags&SOF_Targetable) ||
           (s->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace|SOF_Cloaked)) || !shipPilotTargetScreen(s,&x,&y)) continue;
        if(x<0 || y<0 || x>=MAIN_WindowWidth || y>=MAIN_WindowHeight) continue;
        dist=(x-mouseCursorX())*(x-mouseCursorX())+(y-mouseCursorY())*(y-mouseCursorY());
        if(dist<best) { best=dist;pilot.target=s;pilot.targetID=s->shipID.shipNumber; }
    }
}
bool32 shipPilotFireAll(void) { return pilot.all; }
void shipPilotReticle(sdword *x,sdword *y) { *x=pilot.x; *y=pilot.y; }

void shipPilotExit(void)
{
    if (!pilot.ship) return;
    if(networkSimulation) { free(pilot.weapons);memset(&pilot,0,sizeof(pilot));return; }
    if(!resettingPilots && pilotNetworked() && !playPackets) {
        endShip=pilot.id;endPending=TRUE;pilotSendInput(PILOT_FRAME_END);
    }
    universe.mainCameraCommand.actualcamera=pilot.saved;
    free(pilot.weapons);
    memset(&pilot,0,sizeof(pilot));
    camMouseX=camMouseY=wheel_up=wheel_down=0;
    mouseWheelStepsTake();
    mouseCursorShow();
    mouseClipToRect(NULL);
}

static bool32 pilotEligible(Ship *s)
{
    return s && (networkSimulation?s->playerowner->playerIndex==pilot.ownerIndex:s->playerowner==universe.curPlayerPtr) &&
        !(s->flags&(SOF_Dead|SOF_Hide|SOF_Disabled|SOF_Hyperspace|SOF_NISShip|SOF_Crazy)) &&
        !(s->flags&SOF_Clamped) && !s->dockingship;
}
Ship *shipPilotShip(void)
{
    Node *n;
    if (!pilot.ship) return NULL;
    if (!networkSimulation && (!gameIsRunning || nisIsRunning || smSensorsActive || modernMapEditorActive()))
    { shipPilotExit(); return NULL; }
    for(n=universe.ShipList.head;n;n=n->next)
    {
        Ship *s=(Ship *)listGetStructOfNode(n);
        if(s==pilot.ship && s->shipID.shipNumber==pilot.id && pilotEligible(s))
        {
            /* Mission scripts can replace our halt order. Yield to docking,
               hyperspace or other externally issued orders instead of fighting. */
            if(pilot.awaiting && !networkSimulation)return s;
            if(s->command && (pilot.squad?pilotFormation(s)!=NULL:s->command->selection->numShips==1) &&
               s->command->ordertype.order==COMMAND_HALT) return s;
            break;
        }
    }
    shipPilotExit();
    return NULL;
}
bool32 shipPilotCanEnter(void)
{
    return gameIsRunning && !playPackets && !nisIsRunning && !smSensorsActive &&
        !modernMapEditorActive() && selSelected.numShips==1 && pilotEligible(selSelected.ShipPtr[0]) &&
        (!pilotNetworked() || !selSelected.ShipPtr[0]->gunInfo ||
         selSelected.ShipPtr[0]->gunInfo->numGuns<=PILOT_WEAPON_BYTES*8);
}
static void pilotEnter(void)
{
    SelectCommand one;
    rectangle bounds={0,0,MAIN_WindowWidth,MAIN_WindowHeight};
    vector heading;
    Camera *c=&universe.mainCameraCommand.actualcamera;
    if (!shipPilotCanEnter()) return;
    memset(&pilot,0,sizeof(pilot));
    pilot.saved=*c;
    pilot.ship=selSelected.ShipPtr[0];
    pilot.weaponCount=pilot.ship->gunInfo?pilot.ship->gunInfo->numGuns:0;
    if(pilot.weaponCount>0) pilot.weapons=(unsigned char *)calloc(pilot.weaponCount,1);
    if(!pilot.weapons) pilot.weaponCount=0;
    pilot.navRange=1500;
    pilot.cameraTick=SDL_GetTicks();
    pilot.id=pilot.ship->shipID.shipNumber;
    pilot.ownerIndex=pilot.ship->playerowner->playerIndex;
    one.numShips=1; one.ShipPtr[0]=pilot.ship;
    pilot.squad=pilotFormation(pilot.ship)!=NULL;
    if(pilotNetworked())pilot.awaiting=TRUE;
    else if(pilot.squad) {
        clHaltThese(&universe.mainCommandLayer,pilot.ship->command->selection);
        pilot.ship->command->formation.formationLocked=FALSE;
    }
    else clHaltThese(&universe.mainCommandLayer,&one);
    c->distance=max(pilot.ship->staticinfo->minimumZoomDistance,
                    pilot.ship->staticinfo->staticheader.staticCollInfo.collspheresize*3.5f);
    c->lookatpoint=pilot.ship->posinfo.position;
    matGetVectFromMatrixCol3(heading,pilot.ship->rotinfo.coordsys);
    c->angle=(real32)atan2(-heading.y,-heading.x);
    c->declination=(real32)asin(max(-1.0f,min(1.0f,heading.z)))-0.15f;
    cameraSetEyePosition(c);
    pilot.x=MAIN_WindowWidth/2; pilot.y=MAIN_WindowHeight/2;
    pilot.dragX=pilot.x; pilot.dragY=pilot.y;
    mousePositionSet(pilot.x,pilot.y);
    mouseClipToRect(&bounds);
    mouseCursorHide();
    camMouseX=camMouseY=wheel_up=wheel_down=0;
    mouseWheelStepsTake();
    shipPilotCamera(c);
    if(pilotNetworked())pilotSendInput(PILOT_FRAME_BEGIN);
}
bool32 shipPilotKey(sdword key)
{
    if(!shipPilotActive()) return FALSE;
    pilotThrottleUpdate();
    if(key==PAUSEKEY || key==PKEY) return FALSE;
    if(key==MKEY && !pilot.modeDown && pilot.ship->staticinfo->shipclass<=CLASS_Frigate)
    { pilot.modeDown=TRUE;pilot.commandMode=!pilot.commandMode;pilot.navigation=0;pilot.throttle=0;pilotCeaseFire(); }
    if(pilot.commandMode)
    {
        bool32 ctrl=keyIsHit(CONTROLKEY)||keyIsHit(RCONTROLKEY);
        if(key==SPACEKEY && ctrl) { pilot.navigation=0;pilot.throttle=0; }
        if(key==PLUSKEY)
        { pilot.throttle=min(1.0f,pilot.throttle+0.1f);
          if(pilot.navigation==0) { pilot.navigation=1;matGetVectFromMatrixCol3(pilot.navHeading,pilot.ship->rotinfo.coordsys); } }
        if(key==MINUSKEY) pilot.throttle=max(0.0f,pilot.throttle-0.1f);
        pilotToggleWeapon(key);
        if(key==PAGEUPKEY) pilot.weaponBank=max(0,pilot.weaponBank-1);
        if(key==PAGEDOWNKEY) pilot.weaponBank=min(max(0,(pilot.weaponCount-1)/12),pilot.weaponBank+1);
        if((key==QKEY || key==WKEY || key==EKEY) && shipPilotTarget())
        { pilot.navTarget=shipPilotTarget();pilot.navID=pilot.targetID;
          pilot.navigation=key==QKEY?2:(key==WKEY?3:4);if(pilot.throttle==0) pilot.throttle=1; }
    }
    if(key==TABKEY && !pilot.cockpit && !pilot.tabDown) { pilot.tabDown=TRUE;pilotLockNearest(); }
    if(key==CKEY && !pilot.cockpitKeyDown && shipPilotCockpitAvailable())
    { pilot.cockpitKeyDown=TRUE;pilot.cockpit=!pilot.cockpit;pilot.steerX=pilot.steerY=pilot.freeYaw=pilot.freePitch=pilot.cockpitIdleTime=0;
      mousePositionSet(MAIN_WindowWidth/2,MAIN_WindowHeight/2); }
    if(key==ESCKEY) shipPilotExit();
    /* Prevent RTS shortcuts from issuing orders to the piloted ship. Held
       flight/fire keys are sampled each physics tick, not keyboard repeats. */
    return TRUE;
}
static Ship *pilotPickTarget(Ship *s,Camera *c,const vector *aim,vector *point)
{
    Ship *target=NULL;
    Node *n;
    vector ray,offset;
    real32 nearest;
    vecSub(ray,*aim,c->eyeposition);
    nearest=(real32)sqrt(vecMagnitudeSquared(ray));
    vecNormalize(&ray);
    for(n=universe.ShipList.head;n;n=n->next)
    {
        Ship *candidate=(Ship *)listGetStructOfNode(n);
        real32 radius=candidate->staticinfo->staticheader.staticCollInfo.collspheresize;
        real32 projection,disc,t;
        if(candidate==s || (candidate->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace))) continue;
        vecSub(offset,candidate->collInfo.collPosition,c->eyeposition);
        projection=vecDotProduct(offset,ray);
        disc=radius*radius-(vecMagnitudeSquared(offset)-projection*projection);
        if(disc<0 || projection<0) continue;
        t=projection-(real32)sqrt(disc);
        if(t>0 && t<nearest) { nearest=t;target=candidate; }
    }
    vecMultiplyByScalar(ray,nearest);vecAdd(*point,c->eyeposition,ray);
    return target;
}
bool32 shipPilotMouse(udword event,udword data)
{
    sdword x=mouseCursorX(),y=mouseCursorY();
    bool32 button=x>=MAIN_WindowWidth-80 && x<MAIN_WindowWidth-16 && y>=180 && y<244;
    if(!shipPilotActive())
    {
        if(x>=MAIN_WindowWidth-80 && x<MAIN_WindowWidth-16 && y>=108 && y<172 && shipPilotCommandAvailable())
        { if(event==RPE_ReleaseLeft) { pilotEnter();pilot.commandMode=TRUE; }return TRUE; }
        if(button && shipPilotCanEnter())
        {
            if(event==RPE_ReleaseLeft) pilotEnter();
            return TRUE;
        }
        return FALSE;
    }
    if(event==RPE_WheelUp || event==RPE_WheelDown)
    {
        /* Region.c emits data=1 for wheel steps and data=0 for the legacy
           flywheel key shadow of the same event. Count only the real steps. */
        if(data)
        {
            Camera *c=&universe.mainCameraCommand.actualcamera;
            if(pilot.commandMode && (keyIsHit(CONTROLKEY)||keyIsHit(RCONTROLKEY)))
                pilot.navRange=max(100.0f,min(50000.0f,pilot.navRange*(event==RPE_WheelUp?1.1f:1/1.1f)));
            else if(pilot.cockpit) pilot.throttle=max(-1.0f,min(1.0f,pilot.throttle+(event==RPE_WheelUp?0.1f:-0.1f)));
            else c->distance*=event==RPE_WheelUp?0.9f:1.1111111f;
        }
        return TRUE;
    }
    if(event==RPE_PressLeft) pilot.uiPress=pilotOverButton(x,y);
    if(pilot.commandMode && event==RPE_PressLeft && !pilot.uiPress)
    {
        Camera *c=&universe.mainCameraCommand.actualcamera;
        vector ray,aim,point;
        Ship *clicked;
        cameraRayCast(&ray,c,x,y,MAIN_WindowWidth,MAIN_WindowHeight);
        aim=ray;vecMultiplyByScalar(aim,1000000);vecAddTo(aim,c->eyeposition);
        clicked=pilotPickTarget(pilot.ship,c,&aim,&point);
        if(clicked && (!(clicked->flags&SOF_Targetable) || (clicked->flags&SOF_Cloaked))) clicked=NULL;
        if(keyIsHit(CONTROLKEY)||keyIsHit(RCONTROLKEY))
        { if(clicked) { pilot.target=clicked;pilot.targetID=clicked->shipID.shipNumber; }pilot.clickTime=0; }
        else if(clicked && (keyIsHit(QKEY)||keyIsHit(WKEY)||keyIsHit(EKEY)))
        { pilot.navTarget=clicked;pilot.navID=clicked->shipID.shipNumber;
          pilot.navigation=keyIsHit(QKEY)?2:(keyIsHit(WKEY)?3:4);if(pilot.throttle==0) pilot.throttle=1;pilot.clickTime=0; }
        else
        {
            Uint32 now=SDL_GetTicks();
            if(pilot.clickTime && now-pilot.clickTime<350 && abs(x-pilot.clickX)<6 && abs(y-pilot.clickY)<6)
            { pilot.navigation=clicked?2:1;pilot.navTarget=clicked;
              if(clicked) pilot.navID=clicked->shipID.shipNumber;
              pilot.navHeading=ray;vecNormalize(&pilot.navHeading);pilot.throttle=1;pilot.clickTime=0; }
            else { pilot.clickTime=now;pilot.clickX=x;pilot.clickY=y; }
        }
        return TRUE;
    }
    if(event==RPE_ReleaseLeft)
    {
        bool32 wasUI=pilot.uiPress;pilot.uiPress=FALSE;
        if(wasUI && pilot.commandMode && x>=16 && x<596 && y>=MAIN_WindowHeight-224 && y<MAIN_WindowHeight-32)
        {
            sdword row=y-(MAIN_WindowHeight-224),col=(x-16)/116;
            if(row<44)
            {
                if(col==3) { pilot.navigation=0;pilot.throttle=0; }
                else if(col==4) pilotCeaseFire();
                else if(shipPilotTarget()) { pilot.navTarget=shipPilotTarget();pilot.navID=pilot.targetID;pilot.navigation=col+2;if(pilot.throttle==0) pilot.throttle=1; }
            }
            else if(row>=48 && row<92) { pilot.throttle=pilotClamp((x-16)/576.0f);
                if(pilot.navigation==0 && pilot.throttle>0) { pilot.navigation=1;matGetVectFromMatrixCol3(pilot.navHeading,pilot.ship->rotinfo.coordsys); } }
            else if(row>=96 && row<140)
            { if(col==0) pilot.navRange=max(100.0f,pilot.navRange/1.1f);
              if(col==1) pilot.navRange=min(50000.0f,pilot.navRange*1.1f);
              if(col==4) { pilot.target=NULL;pilotCeaseFire(); } }
            return TRUE;
        }
        if(wasUI && button) { shipPilotExit(); return TRUE; }
        if(wasUI && x>=MAIN_WindowWidth-156 && x<MAIN_WindowWidth-16 && y>=294 && y<328 && shipPilotCockpitAvailable())
        { pilot.cockpit=!pilot.cockpit;pilot.steerX=pilot.steerY=pilot.freeYaw=pilot.freePitch=pilot.cockpitIdleTime=0;
          mousePositionSet(MAIN_WindowWidth/2,MAIN_WindowHeight/2);return TRUE; }
        if(wasUI && x>=MAIN_WindowWidth-156 && x<MAIN_WindowWidth-16 && y>=252 && y<286) { pilot.all=!pilot.all;return TRUE; }
    }
    if(event==RPE_PressRight)
    {
        if(pilot.commandMode) { pilot.dragX=x;pilot.dragY=y;return TRUE; }
        if(pilot.cockpit) { pilot.steerX=pilot.steerY=0;return TRUE; }
        Camera *c=&universe.mainCameraCommand.actualcamera;
        vector ray,aim,point,direction;
        Ship *target;
        /* Pick before centering, using the clicked ray rather than last tick's aim. */
        cameraRayCast(&ray,c,x,y,MAIN_WindowWidth,MAIN_WindowHeight);
        vecMultiplyByScalar(ray,max(10000.0f,c->distance*8));
        vecAdd(aim,c->eyeposition,ray);
        target=pilotPickTarget(pilot.ship,c,&aim,&point);
        if(target) point=target->collInfo.collPosition;
        vecSub(direction,point,pilot.ship->posinfo.position);
        if(!pilot.cockpit && vecMagnitudeSquared(direction)>0.0001f)
        {
            vecNormalize(&direction);
            c->angle=(real32)atan2(-direction.y,-direction.x);
            c->declination=(real32)asin(max(-1.0f,min(1.0f,direction.z)));
        }
        pilot.x=pilot.dragX=MAIN_WindowWidth/2;
        pilot.y=pilot.dragY=MAIN_WindowHeight/2;
        pilot.edgeX=pilot.edgeY=pilot.pressureX=pilot.pressureY=0;
        pilot.steerX=pilot.steerY=0;
        mousePositionSet(pilot.x,pilot.y);
        shipPilotCamera(&universe.mainCameraCommand.actualcamera);
    }
    return TRUE;
}
bool32 shipPilotCamera(Camera *c)
{
    Ship *s=shipPilotShip();
    vector ray;
    Uint32 now=SDL_GetTicks();
    real32 dt;
    if(!s) return FALSE;
    pilotThrottleUpdate();
    dt=min((Uint32)(now-pilot.cameraTick),100)*0.001f;
    pilot.cameraTick=now;
    if(pilot.cockpit)
    {
        vector forward,up,right,look;
        matGetVectFromMatrixCol3(forward,s->rotinfo.coordsys);
        matGetVectFromMatrixCol1(up,s->rotinfo.coordsys);
        matGetVectFromMatrixCol2(right,s->rotinfo.coordsys);
        if(!mouseRightButton()) pilot.freeYaw=pilot.freePitch=0;
        vecScalarMultiply(look,forward,(real32)(cos(pilot.freeYaw)*cos(pilot.freePitch)));
        vecMultiplyByScalar(right,(real32)(sin(pilot.freeYaw)*cos(pilot.freePitch)));vecAddTo(look,right);
        vecScalarMultiply(right,up,(real32)sin(pilot.freePitch));vecAddTo(look,right);
        c->eyeposition=s->posinfo.position;
        c->upvector=up;
        vecAdd(c->lookatpoint,c->eyeposition,look);
        pilot.x=MAIN_WindowWidth/2;pilot.y=MAIN_WindowHeight/2;
        cameraRayCast(&ray,c,pilot.x,pilot.y,MAIN_WindowWidth,MAIN_WindowHeight);
        vecMultiplyByScalar(ray,10000);vecAdd(pilot.aim,c->eyeposition,ray);
        pilot.dragX=pilot.x;pilot.dragY=pilot.y;
        pilot.edgeX=pilot.edgeY=pilot.pressureX=pilot.pressureY=0;
        mouseWheelStepsTake();camMouseX=camMouseY=wheel_up=wheel_down=0;
        return TRUE;
    }
    c->upvector=pilot.saved.upvector;
    if(SDL_GetKeyboardFocus())
    {
        int zoom=(keyIsHit(TWOKEY)?1:0)-(keyIsHit(ONEKEY)?1:0);
        c->distance*= (real32)exp(zoom*dt);
        c->distance=max(max(1.0f,s->staticinfo->minimumZoomDistance),min(100000.0f,c->distance));
    }
    {
        sdword edgeDX=0,edgeDY=0;
        int horizontal,vertical;
        pilot.x=mouseCursorX(); pilot.y=mouseCursorY();
        horizontal=pilot.x<=1?1:(pilot.x>=MAIN_WindowWidth-2?-1:0);
        vertical=pilot.y<=1?1:(pilot.y>=MAIN_WindowHeight-2?-1:0);
        /* Virtual mouse travel at the boundary preserves native sensitivity.
           Fractional pixels make the rate independent of rendering FPS. */
        if(!pilot.commandMode && SDL_GetKeyboardFocus() && !pilotOverButton(pilot.x,pilot.y))
        {
            if(!horizontal) pilot.pressureX=0;
            if(!vertical) pilot.pressureY=0;
            pilot.edgeX=horizontal?pilot.edgeX+horizontal*pilotEdgeRate(pilot.pressureX,MAIN_WindowWidth)*dt:0;
            pilot.edgeY=vertical?pilot.edgeY+vertical*pilotEdgeRate(pilot.pressureY,MAIN_WindowHeight)*dt:0;
            edgeDX=(sdword)pilot.edgeX; edgeDY=(sdword)pilot.edgeY;
            pilot.edgeX-=edgeDX; pilot.edgeY-=edgeDY;
        }
        else pilot.edgeX=pilot.edgeY=pilot.pressureX=pilot.pressureY=0;
        if(SDL_GetKeyboardFocus() && !pilotOverButton(pilot.x,pilot.y) && (!pilot.commandMode || mouseRightButton()))
            cameraMouseDrag(c,pilot.dragX-pilot.x+edgeDX,pilot.dragY-pilot.y+edgeDY,FALSE,FALSE);
        pilot.dragX=pilot.x; pilot.dragY=pilot.y;
    }
    camMouseX=camMouseY=0;
    {
        sdword steps=mouseWheelStepsTake();
        c->distance*= (real32)pow(0.9,max(-20,min(20,steps)));
    }
    c->distance=max(max(1.0f,s->staticinfo->minimumZoomDistance),min(100000.0f,c->distance));
    wheel_up=wheel_down=0;
    c->lookatpoint=s->posinfo.position;
    cameraSetEyePosition(c);
    cameraRayCast(&ray,c,pilot.x,pilot.y,MAIN_WindowWidth,MAIN_WindowHeight);
    vecMultiplyByScalar(ray,max(10000.0f,c->distance*8));
    vecAdd(pilot.aim,c->eyeposition,ray);
    return TRUE;
}
/* Native inertia and directional torque limits, with no direct pose writes. */
static void pilotTrackAxis(Ship *s,real32 desired,real32 current,real32 inertia,uword positive)
{
    real32 torque;
    uword direction;
    if(universe.phystimeelapsed<=0) return;
    torque=(desired-current)*inertia/universe.phystimeelapsed;
    direction=torque>=0?positive:positive+1;
    physApplyRotToObj((SpaceObjRot *)s,min((real32)fabs(torque),s->nonstatvars.rotstrength[direction]),direction);
}
/* Level the bank without changing heading/pitch or snapping the pose. Near
   vertical flight has no stable horizon, so just arrest roll there. Limit
   approach speed by available braking torque to avoid hunting past level. */
static real32 pilotLevelRoll(Ship *s,const vector *up,const vector *right)
{
    real32 angle,rate,acceleration,inertia=s->staticinfo->staticheader.momentOfInertiaZ;
    if(up->z*up->z+right->z*right->z<0.01f || inertia<=0) return 0;
    angle=(real32)atan2(right->z,up->z);
    if(fabs(angle)<0.005f) return 0;
    acceleration=min(s->nonstatvars.rotstrength[ROT_ABOUTZCCW],s->nonstatvars.rotstrength[ROT_ABOUTZCW])/inertia;
    rate=min(s->nonstatvars.turnspeed[TURN_ROLL],min((real32)fabs(angle)*3.0f,
        (real32)sqrt(max(0.0f,2.0f*acceleration*(fabs(angle)-0.005f)))));
    rate=min(rate,(real32)fabs(angle)/max(0.001f,universe.phystimeelapsed));
    return angle<0?-rate:rate;
}
/* Command navigation produces velocity requests only. Native thrust and
   attitude tracking retain ship-specific acceleration and turn limits. */
static void pilotCommandVelocity(Ship *s,vector *desired)
{
    Node *n;Ship *t=NULL;vector radial,tangent,up={0,0,1};
    real32 distance,speed=shipPilotMaxSpeed()*pilot.throttle,error,radius;
    desired->x=desired->y=desired->z=0;
    if(pilot.navigation==1) { vecScalarMultiply(*desired,pilot.navHeading,speed);return; }
    if(pilot.navigation<2) return;
    for(n=universe.ShipList.head;n;n=n->next)
    { Ship *candidate=(Ship *)listGetStructOfNode(n);
      if(candidate==pilot.navTarget && candidate->shipID.shipNumber==pilot.navID &&
         !(candidate->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace|SOF_Cloaked))) { t=candidate;break; } }
    if(!t) { pilot.navigation=0;pilot.navTarget=NULL;return; }
    vecSub(radial,t->posinfo.position,s->posinfo.position);
    distance=(real32)sqrt(vecMagnitudeSquared(radial));
    if(distance<0.01f) radial=pilot.navHeading;
    else { vecMultiplyByScalar(radial,1/distance); }
    radius=s->staticinfo->staticheader.staticCollInfo.collspheresize+t->staticinfo->staticheader.staticCollInfo.collspheresize;
    error=distance-radius-(pilot.navigation==2?100:pilot.navRange);
    /* Ease the requested closing speed, including target velocity, before
       the native acceleration-limited controller applies it. */
    vecScalarMultiply(*desired,radial,max(pilot.navigation==2?0:-speed,min(speed,error*0.5f)));
    if(pilot.navigation==3)
    {
        vecCrossProduct(tangent,radial,up);
        if(vecMagnitudeSquared(tangent)<0.001f) { up.x=1;up.z=0;vecCrossProduct(tangent,radial,up); }
        vecNormalize(&tangent);vecMultiplyByScalar(tangent,speed*max(0.0f,1-(real32)fabs(error)/max(100.0f,pilot.navRange)));
        vecAddTo(*desired,tangent);
    }
    vecAddTo(*desired,t->posinfo.velocity);
    if(vecMagnitudeSquared(*desired)>speed*speed && speed>0) { vecNormalize(desired);vecMultiplyByScalar(*desired,speed); }
    if(speed<=0) desired->x=desired->y=desired->z=0;
}
static void pilotUpdateSimulation(void)
{
    Ship *s=shipPilotShip(),*target=NULL;
    vector forward,right,up,hullForward,desired,offset;
    real32 speed;
    Camera *c=&universe.mainCameraCommand.actualcamera;
    if(!s) return;
    if(!networkSimulation && !SDL_GetKeyboardFocus()) { pilot.steerX=pilot.steerY=0;return; }
    if(!networkSimulation)pilotThrottleUpdate();
    if(pilot.cockpit && !networkSimulation)
        pilot.throttle=max(-1.0f,min(1.0f,pilot.throttle+
            ((pilotFlightKey(WKEY)?1:0)-(pilotFlightKey(SKEY)?1:0))*universe.phystimeelapsed*0.75f));
    /* Resolve real targets, never store a stack aim-point in projectiles. */
    if(networkSimulation) {
        target=(Ship *)IDToPtrTableIDToObj(&ShipIDToPtr,pilot.pickedID);
        if(target && (target->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace)))target=NULL;
        offset=pilot.aim;
    } else target=pilotPickTarget(s,c,&pilot.aim,&offset);
    if(shipPilotTarget()) { target=shipPilotTarget();offset=target->collInfo.collPosition; }
    matGetVectFromMatrixCol2(right,s->rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,s->rotinfo.coordsys);
    matGetVectFromMatrixCol3(hullForward,s->rotinfo.coordsys);
    /* Cruise is independent of camera/aim. Lateral and vertical thrusters
       also operate at STOP; the combined request cannot exceed native speed. */
    desired=hullForward;vecMultiplyByScalar(desired,pilot.throttle);
    if(pilotFlightKey(EKEY)) { vecAddTo(desired,right); }
    if(pilotFlightKey(QKEY)) { vecSubFrom(desired,right); }
    if(pilotFlightKey(RKEY)) { vecAddTo(desired,up); }
    if(pilotFlightKey(FKEY)) { vecSubFrom(desired,up); }
    if(vecMagnitudeSquared(desired)>1.0f) vecNormalize(&desired);
    speed=shipPilotMaxSpeed();
    vecMultiplyByScalar(desired,speed);
    s->autostabilizeship=FALSE;
    s->shipidle=FALSE;
    if(pilot.commandMode)
    {
        pilotCommandVelocity(s,&desired);
        if(vecMagnitudeSquared(desired)>0.01f) { forward=desired;vecNormalize(&forward);aitrackHeading(s,&forward,0.999f); }
        else aitrackZeroRotationAnywhere(s);
    }
    else if(pilot.cockpit)
    {
        real32 dt=max(0.001f,universe.phystimeelapsed);
        real32 yaw=networkSimulation?pilot.netYaw:(mouseRightButton()?0:-pilot.steerX*0.003f/dt);
        real32 pitch=networkSimulation?pilot.netPitch:(mouseRightButton()?0:pilot.steerY*0.003f/dt);
        real32 roll=((pilotFlightKey(DKEY)?1:0)-(pilotFlightKey(AKEY)?1:0))*s->nonstatvars.turnspeed[TURN_ROLL];
        bool32 steering=networkSimulation?(yaw!=0 || pitch!=0):(!mouseRightButton() && (pilot.steerX!=0 || pilot.steerY!=0));
        if(steering || pilotFlightKey(AKEY) || pilotFlightKey(DKEY)) pilot.cockpitIdleTime=0;
        else pilot.cockpitIdleTime+=universe.phystimeelapsed;
        /* Zero input always brakes yaw/pitch/roll immediately. After a short
           quiet interval, gently remove bank; firing/throttle do not inhibit
           the assist, and one quiet render frame cannot start auto-leveling. */
        if(pilot.cockpitIdleTime>=0.12f) roll=pilotLevelRoll(s,&up,&right);
        yaw=max(-s->nonstatvars.turnspeed[TURN_YAW],min(s->nonstatvars.turnspeed[TURN_YAW],yaw));
        pitch=max(-s->nonstatvars.turnspeed[TURN_PITCH],min(s->nonstatvars.turnspeed[TURN_PITCH],pitch));
        pilotTrackAxis(s,yaw,s->rotinfo.rotspeed.x,s->staticinfo->staticheader.momentOfInertiaX,ROT_ABOUTXCCW);
        pilotTrackAxis(s,pitch,s->rotinfo.rotspeed.y,s->staticinfo->staticheader.momentOfInertiaY,ROT_ABOUTYCCW);
        pilotTrackAxis(s,roll,s->rotinfo.rotspeed.z,s->staticinfo->staticheader.momentOfInertiaZ,ROT_ABOUTZCCW);
        pilot.steerX=pilot.steerY=0;
    }
    else if(!pilot.cockpit && !!pilotFlightKey(AKEY)!=!!pilotFlightKey(DKEY))
    {
        forward=right;
        if(pilotFlightKey(AKEY)) { vecMultiplyByScalar(forward,-1); }
        vecAddTo(forward,hullForward);vecNormalize(&forward);
        aitrackHeading(s,&forward,0.999f);
    }
    else aitrackZeroRotationAnywhere(s);
    aitrackVelocityVector(s,&desired);
    gunPilotAimAndFire(s,&offset,target,pilot.commandMode?
        (shipPilotTarget()!=NULL && shipPilotAutoFire()):
        (networkSimulation?pilot.netFire:(pilotFlightKey(SPACEKEY) || (mouseLeftButton() && !pilot.uiPress && !pilotOverButton(mouseCursorX(),mouseCursorY())))),pilot.all);
    if(pilot.squad && pilotFormation(s))
    {
        CommandToDo *c=pilotFormation(s);
        sdword i;
        processFormationToDo(c,FALSE,FALSE);
        for(i=1;i<c->selection->numShips;i++)
            gunPilotAimAndFire(c->selection->ShipPtr[i],&offset,target,networkSimulation?pilot.netFire:(pilotFlightKey(SPACEKEY) ||
                (mouseLeftButton() && !pilot.uiPress && !pilotOverButton(mouseCursorX(),mouseCursorY()))),pilot.all);
    }
}
#include "ShipPilotNetwork.inc"

real32 shipPilotGunReady(Ship *s,Gun *g,const char **status)
{
    real32 remaining,period;
    *status="READY";
    if(s->staticinfo->maxfuel>0 && s->fuel<=0 && s->shiptype!=MinelayerCorvette)
    { *status="NO FUEL"; return 0; }
    if((g->gunstatic->guntype==GUN_MissileLauncher || g->gunstatic->guntype==GUN_MineLauncher) && !g->numMissiles)
    { *status="NO AMMO"; return 0; }
    period=g->gunstatic->firetime;
    remaining=period-(universe.totaltimeelapsed-g->lasttimefired);
    if(g->burstState==2) /* Gun.c: GUNBURST_STATE_COOLOFF; read only. */
    { period=g->useBurstWaitTime; remaining=period-(universe.totaltimeelapsed-g->burstStartTime); }
    else if(g->gunstatic->burstFireTime>0 && g->burstState==1 &&
            universe.totaltimeelapsed-g->burstStartTime>g->useBurstFireTime)
    { *status="BURST REST"; return 0; }
    if(remaining>0) *status="RECHARGING";
    return period>0?pilotClamp(1-remaining/period):1;
}
real32 shipPilotGunSeconds(Gun *g)
{
    if(g->burstState==2)
        return max(0.0f,g->useBurstWaitTime-(universe.totaltimeelapsed-g->burstStartTime));
    if(g->gunstatic->burstFireTime>0 && g->burstState==1 &&
       universe.totaltimeelapsed-g->burstStartTime>g->useBurstFireTime)
        return g->useBurstWaitTime;
    return max(0.0f,g->gunstatic->firetime-(universe.totaltimeelapsed-g->lasttimefired));
}
real32 shipPilotSpecialReady(Ship *s)
{
    if(s->shiptype==MissileDestroyer) return missileDestroyerSpecialReadiness(s);
    if(s->shiptype==HeavyCorvette)
    {
        extern real32 burstCoolDownTime;
        HeavyCorvetteSpec *spec=(HeavyCorvetteSpec *)s->ShipSpecifics;
        return spec->cooldown && burstCoolDownTime>0?pilotClamp(1-spec->burstChargeState2/burstCoolDownTime):1;
    }
    if(s->shiptype==CloakGenerator)
    {
        CloakGeneratorStatics *st=(CloakGeneratorStatics *)s->staticinfo->custstatinfo;
        return st && st->MaxCloakingTime>0?pilotClamp(((CloakGeneratorSpec *)s->ShipSpecifics)->CloakStatus/st->MaxCloakingTime):0;
    }
    if(s->shiptype==GravWellGenerator)
    {
        GravWellGeneratorStatics *st=(GravWellGeneratorStatics *)s->staticinfo->custstatinfo;
        return st && st->OperationTime>0?pilotClamp(1-((GravWellGeneratorSpec *)s->ShipSpecifics)->TimeOn/st->OperationTime):0;
    }
    return -1; /* No invented gameplay timer for ships without a timed special. */
}
