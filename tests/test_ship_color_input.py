"""Replay clicks through the real Region dispatcher and ship-colour handlers."""
from pathlib import Path
import unittest
from test_frontend_preview_contract import ROOT,function,run_native

class ShipColorInput(unittest.TestCase):
    def test_modal_controls_receive_clicks_and_block_gameplay(self):
        region=(ROOT/'src/Game/Region.c').read_text()
        header=(ROOT/'src/Game/Region.h').read_text()
        paint=(ROOT/'src/Game/ShipAppearanceUI.inc').read_text()
        modern=(ROOT/'src/Game/ModernUI.c').read_text()
        defines='\n'.join(line for line in header.splitlines() if line.startswith('#define ') and
            line.split()[1].startswith(('RSF_','RPE_','RPM_','RPR_','RF_ShiftBit','regInside','regLeftPressed','regRightPressed','regCentrePressed','regKeyPressed','regFlag','regStatus')))
        code=r'''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef int sdword,bool32;typedef uintptr_t smemsize;typedef unsigned udword,color;typedef unsigned short uword;typedef float real32;
typedef struct {int x0,y0,x1,y1;} rectangle;
typedef struct region region,*regionhandle;
typedef udword (*regionfunction)(regionhandle,smemsize,udword,udword);
typedef void (*regiondrawfunction)(regionhandle);
struct region {rectangle rect;regiondrawfunction drawFunction;regionfunction processFunction;
 region *parent,*child,*previous,*next;udword flags,status;smemsize userID;};
#define TRUE 1
#define FALSE 0
#define BIT10 1024
#define bitSet(v,b) ((v)|=(b))
#define bitClear(v,b) ((v)&=~(b))
#define bitTest(v,b) ((v)&(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define regVerify(r) ((void)0)
#define dbgAssertOrIgnore(x) do {if(!(x))abort();}while(0)
#define dbgMessage(...) ((void)0)
#define ESCKEY 41
#define FLYWHEEL_UP 1
#define FLYWHEEL_DOWN 2
#define LMOUSE_DOUBLE 3
#define RMOUSE_DOUBLE 4
#define MMOUSE_DOUBLE 5
#define OBJ_ShipType 1
#define SOF_Dead 2
#define SHIP_PAINT_COUNT 6
#define colRed(c) ((c)&255)
#define colGreen(c) (((c)>>8)&255)
#define colBlue(c) (((c)>>16)&255)
#define colUbyteToReal(v) ((v)/255.0f)
#define colRealToUbyte(v) ((unsigned)((v)*255+.5f))
#define colRGB(r,g,b) ((color)((r)|((g)<<8)|((b)<<16)))
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);abort();}}while(0)
typedef struct {unsigned mask;color colors[6];} ShipAppearance;
typedef struct {int objtype;void *playerowner;unsigned flags;struct {uword shipNumber;} shipID;} Ship;
static struct {int numShips;Ship *ShipPtr[1];} selSelected;
static struct {void *curPlayerPtr;} universe;
static region root,*ghMainRegion=&root,*regClickedLeft,*regClickedLeftLast;
static int MAIN_WindowWidth=2560,MAIN_WindowHeight=1440,mx,my,left,pressedKey,regKeysFocussed;
static float scale=1.5f;static int pickerOpened,applyCount,gameClicks,allocatedCount;
static region *allocated[128];static ShipAppearance applied;
static void (*pickerApply)(const real32 rgb[3],void *context);static void *pickerContext;
static regionhandle pickerRegion;
static struct {regionhandle leftRegion,rightRegion,iconRegion,ruleRegion,controlGroupRegion;} muiCommand;
static regionhandle muiPilotRegion,muiFormationSpreadRegion;
static struct {regionhandle region;} muiDossier;
static struct {regionhandle region,markerRegion;} muiLighting;
static int uiScaleSize(int value){return (int)(value*scale+.5f);}
static rectangle muiRect(int x,int y,int x1,int y1){rectangle r={x,y,x1,y1};return r;}
static int muiPointIn(const rectangle *r,int x,int y){return x>=r->x0&&x<r->x1&&y>=r->y0&&y<r->y1;}
static int mouseCursorX(void){return mx;}static int mouseCursorY(void){return my;}
static int mouseInRect(rectangle *r){return muiPointIn(r,mx,my);}
static int mouseLeftButton(void){return left;}static int mouseRightButton(void){return 0;}static int mouseCentreButton(void){return 0;}
static int mouseWheelStepsPeek(void){return 0;}static int mouseWheelStepsTake(void){return 0;}
static int keyIsHit(int key){return 0;}static int keyIsStuck(int key){return 0;}
static void keyPressUp(int key){}static void keyClearSticky(int key){}
static int keyBufferedKeyGet(int *shift){return 0;}
static int regKeysStuck(regionhandle r){return pressedKey && r->userID==pressedKey;}
static int regKeysPressed(regionhandle r){return 0;}static void regKeysUnstick(regionhandle r){pressedKey=0;}
static void regRecursiveSetDirty(regionhandle r){}static void regDrawFunctionAddPossibly(regionhandle r){}
static void modernUICloseEditorColorPicker(void){pickerRegion=NULL;}
static void modernUIOpenEditorColorPicker(const char *title,const float *rgb,float maximum,void (*apply)(const real32*,void*),void *context)
{++pickerOpened;pickerApply=apply;pickerContext=context;}
static void shipAppearanceGet(Ship *ship,ShipAppearance *paint){memset(paint,0,sizeof(*paint));for(int i=0;i<6;i++)paint->colors[i]=colRGB(10,20,30);}
static int shipAppearanceApply(Ship *ship,const ShipAppearance *paint){CHECK(ship==selSelected.ShipPtr[0]);applied=*paint;++applyCount;return 1;}
static void muiShipPaintDraw(regionhandle r){}
static void muiEditorColorBringToFront(void);
static udword noInput(regionhandle r,smemsize id,udword event,udword data){return 0;}
static regionhandle regChildAlloc(regionhandle parent,smemsize id,int x,int y,int w,int h,int extra,udword flags)
{regionhandle r=calloc(1,sizeof(*r));CHECK(allocatedCount<128);allocated[allocatedCount++]=r;r->rect=muiRect(x,y,x+w,y+h);r->flags=flags;r->parent=parent;r->userID=id;r->processFunction=noInput;
 r->next=parent->child;if(r->next)r->next->previous=r;parent->child=r;return r;}
static void regFunctionSet(regionhandle r,regionfunction p){r->processFunction=p;}
static void regDrawFunctionSet(regionhandle r,regiondrawfunction p){r->drawFunction=p;}
static void regRegionDelete(regionhandle r){r->status|=0x40;if(!(r->status&0x80)){if(r->previous)r->previous->next=r->next;else if(r->parent)r->parent->child=r->next;if(r->next)r->next->previous=r->previous;}}
static regionhandle regKeyChildAlloc(regionhandle parent,smemsize id,udword flags,regionfunction p,int count,int key)
{regionhandle r=regChildAlloc(parent,id,0,0,0,0,0,flags);r->processFunction=p;return r;}
'''+defines+'\n'
        code+=function(region,'regionhandle regSiblingMoveToFront(')+'\n'
        code+='static void muiEditorColorBringToFront(void){if(pickerRegion)regSiblingMoveToFront(pickerRegion);}\n'
        code+=paint[:paint.index('static Ship *muiShipPaintSelection(')]
        for signature in ('static Ship *muiShipPaintSelection(', 'static rectangle muiShipPaintPanel(',
                          'static rectangle muiShipPaintRow(', 'static void muiShipPaintClose(',
                          'static bool32 muiShipPaintValid(', 'static void muiShipPaintPickerApply(',
                          'static udword muiShipPaintProcess(', 'static void muiShipPaintOpen('):
            code+=function(paint,signature)+'\n'
        code+=function(modern,'void modernUICommandBarBringToFront(')+'\n'
        code+=function(region,'sdword regFunctionCall(')+'\n'
        code+=function(region,'udword regRegionProcess(')+'\n'
        code+=r'''
static udword gameplay(regionhandle r,smemsize id,udword event,udword data){if(event&(RPE_LeftClick|RPE_RightClick))++gameClicks;return 0;}
static void frame(void){regRegionProcess(&root,0xffffffffu);}
static void click(int x,int y){mx=x;my=y;left=0;frame();left=1;frame();left=0;frame();}
static void fresh(Ship *ship){int i;for(i=0;i<allocatedCount;++i)free(allocated[i]);allocatedCount=0;memset(&root,0,sizeof(root));root.processFunction=noInput;
 memset(&muiShipPaint,0,sizeof(muiShipPaint));memset(&muiCommand,0,sizeof(muiCommand));muiPilotRegion=muiFormationSpreadRegion=NULL;pickerRegion=NULL;
 regClickedLeft=regClickedLeftLast=NULL;pressedKey=left=0;gameClicks=pickerOpened=applyCount=0;
 regionhandle scene=regChildAlloc(&root,0,0,0,MAIN_WindowWidth,MAIN_WindowHeight,0,RPE_LeftClick|RPE_RightClick);regFunctionSet(scene,gameplay);
 selSelected.numShips=1;selSelected.ShipPtr[0]=ship;universe.curPlayerPtr=ship->playerowner;}
int main(void){Ship ship={OBJ_ShipType,(void*)1,0,{42}};int i,s;
 for(s=0;s<3;++s){scale=s==0?1:s==1?1.5f:2;fresh(&ship);muiShipPaintOpen(&ship);
  rectangle panel=muiShipPaintPanel();
  for(i=0;i<6;++i){rectangle row=muiShipPaintRow(&panel,i);click(row.x0+uiScaleSize(12),(row.y0+row.y1)/2);CHECK(muiShipPaint.pending.mask==((1u<<(i+1))-1));}
  for(i=0;i<6;++i){rectangle row=muiShipPaintRow(&panel,i);float rgb[]={.2f,.4f,.8f};click(row.x1-uiScaleSize(30),(row.y0+row.y1)/2);CHECK(pickerOpened==i+1);pickerApply(rgb,pickerContext);CHECK(muiShipPaint.pending.colors[i]==colRGB(51,102,204));}
  click(panel.x1-uiScaleSize(60),panel.y1-uiScaleSize(40));CHECK(applyCount==1&&applied.mask==63&&!muiShipPaint.region&&gameClicks==0);
  muiShipPaintOpen(&ship);muiShipPaint.pending.mask=63;click(panel.x0+uiScaleSize(60),panel.y1-uiScaleSize(40));CHECK(muiShipPaint.region&&muiShipPaint.pending.mask==0);
  click(panel.x1-uiScaleSize(194),panel.y1-uiScaleSize(40));CHECK(!muiShipPaint.region&&applyCount==1&&gameClicks==0);
  muiShipPaintOpen(&ship);pressedKey=ESCKEY;frame();CHECK(!muiShipPaint.region&&gameClicks==0);
  muiShipPaintOpen(&ship);click(1,1);CHECK(!muiShipPaint.region&&gameClicks==0);
  // Reproduce the old same-region modal construction: production dispatch
  // must demonstrate why it received no mouse events.
  muiShipPaint.target=&ship;muiShipPaint.id=42;muiShipPaint.region=regChildAlloc(&root,0,0,0,MAIN_WindowWidth,MAIN_WindowHeight,0,RPE_ModalBreak|RPE_LeftClick);
  regFunctionSet(muiShipPaint.region,muiShipPaintProcess);muiShipPaint.pending.mask=0;
  {rectangle row=muiShipPaintRow(&panel,0);click(row.x0+uiScaleSize(12),(row.y0+row.y1)/2);CHECK(muiShipPaint.pending.mask==0);}muiShipPaintClose();
  // Reasserting HUD order must keep the ship panel and its nested picker on top.
  muiCommand.leftRegion=regChildAlloc(&root,0,0,0,30,30,0,0);muiPilotRegion=regChildAlloc(&root,0,0,0,30,30,0,0);
  muiShipPaintOpen(&ship);pickerRegion=regChildAlloc(&root,0,0,0,30,30,0,RPE_ModalBreak);
  modernUICommandBarBringToFront();CHECK(root.child==pickerRegion&&pickerRegion->next==muiShipPaint.region);
 }
 fresh(&ship);for(i=0;i<allocatedCount;++i)free(allocated[i]);
 puts("PASS: real Region clicks reach all six ship colours, apply/reset/cancel/escape work, gameplay is blocked, nested picker stays above HUD");return 0;}
'''
        run_native(code)

if __name__=='__main__':unittest.main()
