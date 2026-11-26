#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "../src/Game/NavigationUI.c"

Universe universe;MaxSelection selSelected;
volatile keyScanType keyScanCode[KEY_TOTAL_KEYS];
sdword feMenuLevel,piePointSpecMode,mouseCursorXPosition,mouseCursorYPosition;
real32 piePointSpecZ;vector piePlanePoint,pieHeightPoint;
bool32 smSensorsActive,MP_HyperSpaceFlag;
Camera *mrCamera;static Camera testCamera;
hmatrix rndCameraMatrix,rndProjectionMatrix;
sdword MAIN_WindowWidth=1920,MAIN_WindowHeight=1080,primModeEnabled;
static Ship ships[3];static ShipStaticInfo info[3];static Ship *hit;
static unsigned redLines;
static unsigned issued,issuedMode,issuedCount,issuedAction;static TargetPtr issuedTarget;
#if MEM_USE_NAMES
void *memAllocFunction(sdword n,char *name,udword flags){return calloc(1,n);}
#else
void *memAllocFunction(sdword n,udword flags){return calloc(1,n);}
#endif
void memFree(void *p){free(p);}
void feMenuDisappear(char *s,featom *a){feMenuLevel--;}
sdword soundEventPlay(void *object,sdword event,Gun *gun){return 0;}
void piePointModeOnOff(void){piePointSpecMode=piePointSpecMode==PSM_Idle?PSM_XY:PSM_Idle;}
bool32 navCanProduce(const Ship *ship){return ship->staticinfo->canBuildShips;}
bool32 navValidPoint(vector p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
bool32 kbCommandKeyIsHit(udword command){return keyIsHit(command==kbDOCK?DKEY:ZKEY);}
void MakeShipsMobile(SelectCommand *s){}
sdword selShipInSelection(Ship **list,sdword n,Ship *ship){sdword i;for(i=0;i<n;i++)if(list[i]==ship)return TRUE;return FALSE;}
Ship *selSelectionClick(Node *head,Camera *camera,sdword x,sdword y,bool32 derelicts,bool32 resources)
{assert(camera==mrCamera);return hit;}
void navIssue(SelectCommand *s,unsigned mode,const vector *p,unsigned count,unsigned action,TargetPtr target)
{assert(s->numShips);issued++;issuedMode=mode;issuedCount=count;issuedAction=action;issuedTarget=target;}
void primModeSetFunction2(void){primModeEnabled=TRUE;}
void primLine2(sdword a,sdword b,sdword c,sdword d,color ink){if(drawInvalid)redLines++;}
void navVisitLines(void (*visit)(vector,vector,unsigned,unsigned)){}
fonthandle fontMakeCurrent(fonthandle f){return 0;}
fonthandle frFontRegister(char *name){return 1;}
sdword fontPrint(sdword x,sdword y,color ink,char *text){return 0;}
void selCircleComputeGeneral(hmatrix *m,hmatrix *p,vector *v,real32 r,real32 *x,real32 *y,real32 *radius)
{*x=0;*y=0;*radius=1;}

static vector pos(float x,float y,float z){vector v={20*x,20*y,20*z};return v;}
static void setup(void)
{
    unsigned i;navPlacementCancel();memset(&universe,0,sizeof(universe));memset(ships,0,sizeof(ships));
    memset((void *)keyScanCode,0,sizeof(keyScanCode));universe.curPlayerPtr=&universe.players[0];
    for(i=0;i<3;i++){ships[i].objtype=OBJ_ShipType;ships[i].staticinfo=&info[i];ships[i].playerowner=universe.curPlayerPtr;info[i].canBuildShips=i==0;}
    selSelected.numShips=1;selSelected.ShipPtr[0]=&ships[1];mrCamera=&testCamera;
    piePointSpecMode=PSM_Idle;smSensorsActive=FALSE;issued=0;hit=NULL;
}
int main(void)
{
    setup();navBeginWaypoints(NULL,NULL);
    navPlacementClick(pos(0,0,0));navPlacementClick(pos(20,0,0));
    assert(draft.count==1 && !issued); /* undersized leg rejected, draft editable */
    piePlanePoint=pos(20,0,0);redLines=0;navDraw();assert(redLines>0);
    navPlacementClick(pos(400,0,0));assert(draft.count==2);
    mouseCursorXPosition=MAIN_WindowWidth/2;mouseCursorYPosition=MAIN_WindowHeight/2;
    navPlacementClick(pos(0,0,0));assert(navPlacementActive() && !issued); /* no two-point U-turn loop */
    mouseCursorXPosition=mouseCursorYPosition=0;
    navPlacementClick(pos(400,400,0));navPlacementKey(ENTERKEY);assert(issued==1 && !navPlacementActive());
    setup();feMenuLevel=1;navBeginWaypoints(NULL,NULL);
    assert(navPlacementActive() && draft.mode==NAV_WAYPOINTS && !feMenuLevel && piePointSpecMode==PSM_XY);
    navPlacementClick(pos(100,200,300));navPlacementClick(pos(500,600,700));assert(draft.count==2 && !issued);
    navPlacementKey(BACKSPACEKEY);assert(draft.count==1 && !issued);
    navPlacementKey(ESCKEY);assert(!navPlacementActive() && issued==1 && issuedMode==NAV_WAYPOINTS && issuedCount==1);
    navBeginWaypoints(NULL,NULL);navPlacementClick(pos(100,0,0));navPlacementClick(pos(500,0,0));navPlacementClick(pos(500,500,0));
    mouseCursorXPosition=MAIN_WindowWidth/2+8;mouseCursorYPosition=MAIN_WindowHeight/2;
    navPlacementClick(pos(100,0,0));assert(!navPlacementActive() && issuedAction==NAV_ACTION_LOOP && issuedCount==3);
    issued=1;mouseCursorXPosition=mouseCursorYPosition=0;
    navBeginWaypoints(NULL,NULL);navPlacementKey(DELETEKEY);assert(!navPlacementActive() && issued==1);
    navBeginWaypoints(NULL,NULL);navPlacementKey(ESCKEY);assert(issued==1); /* empty draft */
    navBeginWaypoints(NULL,NULL);assert(navPlacementKey(DKEY) && navPlacementActive());
    assert(navPlacementKey(ZKEY) && navPlacementActive());
    assert(!navPlacementKey(MKEY) && !navPlacementActive());
    navBeginWaypoints(NULL,NULL);navPlacementClick(pos(100,0,0));ships[2].playerowner=&universe.players[1];
    navPlacementTarget((TargetPtr)&ships[2]);assert(issuedAction==NAV_ACTION_ATTACK && issuedTarget==(TargetPtr)&ships[2]);
    setup();selSelected.ShipPtr[0]=&ships[0];
    navBeginRallyPoint(NULL,NULL);assert(draft.mode==NAV_RALLY_POINT);
    navPlacementClick(pos(1000,2000,3000));assert(issuedMode==NAV_RALLY_POINT && issuedAction==NAV_ACTION_NONE && !navPlacementActive());
    navBeginRallyObject(NULL,NULL);hit=&ships[2];smSensorsActive=TRUE;navPlacementClick(pos(0,0,0));
    assert(issuedMode==NAV_RALLY_OBJECT && issuedTarget==(TargetPtr)&ships[2] && !navPlacementActive());
    keyScanCode[CONTROLKEY].keypressed=TRUE;navPlacementKey(RKEY);assert(draft.mode==NAV_RALLY_POINT);
    navPlacementKey(ESCKEY);assert(!navPlacementActive());navPlacementKey(PKEY);assert(draft.mode==NAV_RALLY_OBJECT);
    navPlacementKey(ESCKEY);keyScanCode[CONTROLKEY].keypressed=FALSE;
    navBeginWaypoints(NULL,NULL);navPlacementObjectDied((TargetPtr)&ships[0]);assert(!navPlacementActive());
    navBeginWaypoints(NULL,NULL);selSelected.numShips=0;navDraw();assert(!navPlacementActive());
    selSelected.numShips=1;selSelected.ShipPtr[0]=&ships[0];
    navClearRally(NULL,NULL);assert(issuedMode==NAV_NONE && !issuedCount);
    puts("Navigation UI: menu callbacks, 3D drafts, finish/cancel/undo, terminal targets and Sensors object picking passed.");return 0;
}
