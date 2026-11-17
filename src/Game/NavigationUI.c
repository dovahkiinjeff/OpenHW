#include "NavigationOrders.h"
#include "NavigationGeometry.h"
#include <SDL.h>
#include "PiePlate.h"
#include "Select.h"
#include "ShipSelect.h"
#include "Universe.h"
#include "UIControls.h"
#include "KeyBindings.h"
#include "CommandDefs.h"
#include "Memory.h"
#include "mainrgn.h"
#include "mouse.h"
#include "prim2d.h"
#include "render.h"
#include "font.h"
#include "FontReg.h"
#include "Sensors.h"
#include "SoundEvent.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static struct {
    unsigned mode,count,action;
    vector points[NAV_MAX_POINTS];
    SelectCommand *ships;
    TargetPtr target;
} draft;

bool32 navPlacementActive(void){return draft.mode!=NAV_NONE;}
void navPlacementCancel(void)
{
    bool32 active=navPlacementActive();
    if(draft.ships)memFree(draft.ships);
    memset(&draft,0,sizeof(draft));
    if(active && piePointSpecMode!=PSM_Idle)piePointModeOnOff();
}
void navPlacementObjectDied(TargetPtr object)
{
    if(draft.target==object || (draft.ships && object->objtype==OBJ_ShipType &&
       selShipInSelection(draft.ships->ShipPtr,draft.ships->numShips,(Ship *)object)))navPlacementCancel();
}
static void begin(unsigned mode)
{
    sdword i;
    if(mode==NAV_WAYPOINTS && draft.mode==mode) {
        navPlacementKey(ESCKEY);return;
    }
    navPlacementCancel();
    while(feMenuLevel>0)feMenuDisappear(NULL,NULL);
    if(!selSelected.numShips)return;
    draft.ships=memAlloc(sizeofSelectCommand(selSelected.numShips),"navigation draft selection",0);
    draft.ships->numShips=0;
    for(i=0;i<selSelected.numShips;i++) {
        Ship *ship=selSelected.ShipPtr[i];
        if(ship->playerowner!=universe.curPlayerPtr)continue;
        if(mode!=NAV_WAYPOINTS && !navCanProduce(ship))continue;
        draft.ships->ShipPtr[draft.ships->numShips++]=ship;
    }
    if(mode==NAV_WAYPOINTS)MakeShipsMobile(draft.ships);
    if(!draft.ships->numShips){navPlacementCancel();soundEvent(NULL,UI_ClickCancel);return;}
    draft.mode=mode;MP_HyperSpaceFlag=FALSE;
    if(piePointSpecMode!=PSM_Idle)piePointModeOnOff();
    piePointModeOnOff();
}
void navBeginWaypoints(char *string,featom *atom){begin(NAV_WAYPOINTS);}
void navBeginRallyPoint(char *string,featom *atom){begin(NAV_RALLY_POINT);}
void navBeginRallyObject(char *string,featom *atom){begin(NAV_RALLY_OBJECT);}
void navClearRally(char *string,featom *atom)
{
    navPlacementCancel();while(feMenuLevel>0)feMenuDisappear(NULL,NULL);
    navIssue((SelectCommand *)&selSelected,NAV_NONE,NULL,0,NAV_ACTION_NONE,NULL);
}
static bool32 draftValid(const vector *points,unsigned count,bool32 loop,ubyte *bad)
{return navGeometryCheck(points,count,loop,navGeometryRadius(draft.ships),bad);}
static void finish(void)
{
    if(draft.mode==NAV_WAYPOINTS && draft.count && !draftValid(draft.points,draft.count,draft.action==NAV_ACTION_LOOP,NULL)) {
        soundEvent(NULL,UI_ClickCancel);return;
    }
    if(draft.ships && draft.count)navIssue(draft.ships,draft.mode,draft.points,draft.count,draft.action,draft.target);
    navPlacementCancel();
}
static bool32 firstPointHit(void)
{
    real32 x,y,r;sdword dx,dy;
    if(draft.mode!=NAV_WAYPOINTS || draft.count<2)return FALSE;
    selCircleComputeGeneral(&rndCameraMatrix,&rndProjectionMatrix,&draft.points[0],1,&x,&y,&r);
    if(!isfinite(x) || !isfinite(y) || x<-1 || x>1 || y<-1 || y>1 || r<0)return FALSE;
    dx=mouseCursorX()-primGLToScreenX(x);dy=mouseCursorY()-primGLToScreenY(y);
    return dx*dx+dy*dy<=14*14;
}
bool32 navPlacementClick(vector point)
{
    if(!navPlacementActive())return FALSE;
    if(draft.mode==NAV_RALLY_OBJECT)return navPlacementTarget(NULL);
    if(firstPointHit()){draft.action=NAV_ACTION_LOOP;finish();if(navPlacementActive())draft.action=NAV_ACTION_NONE;return TRUE;}
    if(!navValidPoint(point)){soundEvent(NULL,UI_ClickCancel);return TRUE;}
    if(draft.count==NAV_MAX_POINTS){soundEvent(NULL,UI_ClickCancel);return TRUE;}
    if(draft.mode==NAV_WAYPOINTS) {
        draft.points[draft.count]=point;
        if(!draftValid(draft.points,draft.count+1,FALSE,NULL)){soundEvent(NULL,UI_ClickCancel);return TRUE;}
    }
    draft.points[draft.count++]=point;
    soundEvent(NULL,UI_Click);
    if(draft.mode==NAV_RALLY_POINT)finish();
    return TRUE;
}
bool32 navPlacementTarget(TargetPtr target)
{
    Ship *ship;
    if(!navPlacementActive())return FALSE;
    if(smSensorsActive || !target)target=(TargetPtr)selSelectionClick(
        smSensorsActive?universe.SpaceObjList.head:universe.RenderList.head,
        mrCamera,mouseCursorX(),mouseCursorY(),TRUE,TRUE);
    if(!target || (target->flags&SOF_Dead)){soundEvent(NULL,UI_ClickCancel);return TRUE;}
    if(draft.mode==NAV_RALLY_POINT)return TRUE;
    if(target->objtype==OBJ_MissileType || target->objtype==OBJ_BulletType){soundEvent(NULL,UI_ClickCancel);return TRUE;}
    if(draft.mode==NAV_RALLY_OBJECT) {
        /* A rally object is a moving parade origin, never an implicit attack or harvest order. */
        if(target->objtype==OBJ_ShipType && ((Ship *)target)->playerowner!=universe.curPlayerPtr) {
            soundEvent(NULL,UI_ClickCancel);return TRUE;
        }
        draft.points[0]=target->posinfo.position;draft.count=1;draft.target=target;finish();return TRUE;
    }
    if(!draft.count){soundEvent(NULL,UI_ClickCancel);return TRUE;}
    draft.target=target;
    if(target->objtype==OBJ_ShipType) {
        ship=(Ship *)target;
        if(ship->playerowner!=universe.curPlayerPtr)draft.action=NAV_ACTION_ATTACK;
        else if(kbCommandKeyIsHit(kbDOCK))draft.action=NAV_ACTION_DOCK;
        else if(kbCommandKeyIsHit(kbSHIP_SPECIAL))draft.action=NAV_ACTION_SPECIAL;
        else draft.action=NAV_ACTION_GUARD;
    } else if(target->objtype==OBJ_DerelictType)draft.action=NAV_ACTION_SPECIAL;
    else draft.action=NAV_ACTION_HARVEST;
    /* The final contextual command executes after the last spatial waypoint.
       Do not append the target's current position: moving targets need a live reference. */
    finish();return TRUE;
}
bool32 navPlacementKey(sdword key)
{
    bool32 control=keyIsHit(LCONTROLKEY)||keyIsHit(RCONTROLKEY);
    if(key==WKEY && !control && !keyIsHit(LALTKEY) && !keyIsHit(RALTKEY)) {begin(NAV_WAYPOINTS);return TRUE;}
    if(control && key==RKEY){begin(NAV_RALLY_POINT);return TRUE;}
    if(control && key==PKEY){begin(NAV_RALLY_OBJECT);return TRUE;}
    if(!navPlacementActive())return FALSE;
    if(key==ESCKEY || key==ENTERKEY){if(draft.mode==NAV_WAYPOINTS)finish();else navPlacementCancel();return TRUE;}
    if(key==BACKSPACEKEY) {
        if(draft.count)draft.count--;else navPlacementCancel();return TRUE;
    }
    if(key==DELETEKEY){navPlacementCancel();return TRUE;}
    /* D/Z choose the route's terminal operation while held; their ordinary
       immediate-order handlers must not run in the middle of a draft. */
    if(draft.mode==NAV_WAYPOINTS && (key==DKEY || key==ZKEY))return TRUE;
    if(key==MKEY || key==DKEY || key==ZKEY || key==HKEY2 || key==TILDEKEY ||
       key==JKEY || key==IKEY || key==KKEY || key==SKEY)navPlacementCancel();
    return FALSE;
}
static bool32 drawInvalid;
static void drawLine(vector from,vector to,unsigned number,unsigned mode)
{
    real32 x0,y0,x1,y1,r;sdword a,b,c,d;char text[48];
    color ink=mode==NAV_WAYPOINTS?colRGB(238,175,61):colRGB(100,207,219);
    if(drawInvalid)ink=(SDL_GetTicks()/180)&1?colRGB(255,60,45):colRGB(130,20,15);
    selCircleComputeGeneral(&rndCameraMatrix,&rndProjectionMatrix,&from,0,&x0,&y0,&r);
    selCircleComputeGeneral(&rndCameraMatrix,&rndProjectionMatrix,&to,0,&x1,&y1,&r);
    if(x0<-2 || x0>2 || y0<-2 || y0>2 || x1<-2 || x1>2 || y1<-2 || y1>2)return;
    a=primGLToScreenX(x0);b=primGLToScreenY(y0);c=primGLToScreenX(x1);d=primGLToScreenY(y1);
    primLine2(a,b,c,d,ink);primLine2(c-4,d,c+4,d,ink);primLine2(c,d-4,c,d+4,ink);
    if(number)sprintf(text,"%u",number);else strcpy(text,mode==NAV_WAYPOINTS?"LOOP":"RALLY / PARADE");
    fontPrint(c+6,d+4,ink,text);
}
void navDraw(void)
{
    static fonthandle font;fonthandle old;unsigned i;
    if(!universe.curPlayerPtr)return;
    if(!font)font=frFontRegister("Arial_12.hff");
    old=fontMakeCurrent(font);primModeSet2();
    navVisitLines(drawLine);
    if(navPlacementActive()) {
        vector from;char text[160];sdword j;bool32 same=TRUE;
        if(piePointSpecMode==PSM_Idle)same=FALSE;
        for(j=0;j<draft.ships->numShips;j++)
            if(!selShipInSelection(selSelected.ShipPtr,selSelected.numShips,draft.ships->ShipPtr[j]))same=FALSE;
        if(!same){navPlacementCancel();fontMakeCurrent(old);return;}
        from=draft.ships->ShipPtr[0]->posinfo.position;
        if(draft.mode==NAV_WAYPOINTS) {
            vector preview[NAV_MAX_POINTS],candidate=piePointSpecZ?pieHeightPoint:piePlanePoint;
            ubyte bad[NAV_MAX_POINTS];unsigned n=draft.count;bool32 loop=firstPointHit(),valid;
            memcpy(preview,draft.points,n*sizeof(vector));
            if(!loop && n<NAV_MAX_POINTS)preview[n++]=candidate;
            valid=draftValid(preview,n,loop,bad);
            for(i=0;i<draft.count;i++) {
                drawInvalid=i>0 && bad[i-1];drawLine(from,draft.points[i],i+1,NAV_WAYPOINTS);from=draft.points[i];
            }
            drawInvalid=draft.count && bad[draft.count-1];
            if(loop)drawLine(from,draft.points[0],0,NAV_WAYPOINTS);
            else if(draft.count<NAV_MAX_POINTS)drawLine(from,candidate,draft.count+1,NAV_WAYPOINTS);
            sprintf(text,valid?
                "WAYPOINTS %u/%u / CLICK: ADD / CLICK 1: LOOP / ENTER/ESC: FINISH / BACKSPACE: UNDO / DELETE: CANCEL":
                "WAYPOINTS %u/%u / TOO TIGHT: LENGTHEN RED LEGS OR WIDEN TURN / BACKSPACE: UNDO / DELETE: CANCEL",draft.count,NAV_MAX_POINTS);
            drawInvalid=FALSE;
        } else strcpy(text,draft.mode==NAV_RALLY_POINT?
            "RALLY POINT / VIRTUAL MILITARY PARADE / CLICK: PLACE / ESC: CANCEL":
            "RALLY OBJECT / VIRTUAL MILITARY PARADE / CLICK AN OBJECT / ESC: CANCEL");
        fontPrint(24,80,colRGB(238,175,61),text);
    }
    fontMakeCurrent(old);
}
