/* Exercise the production animation start/stop/update during Acolyte linking. */
#include "MeshAnim.h"
#include "Universe.h"
#include "File.h"
#include "Memory.h"
#include "NIS.h"
#include "NativeVisualRegistry.h"
#include "CommandLayer.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "CRC32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static int checks,samples;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
void dbgFatalf(char *file,sdword line,char *format,...)
{(void)file;(void)line;fprintf(stderr,"%s\n",format);abort();}
int meshNativeVisualSampleShipEvent(const void *ship,const char *event,double phase)
{(void)ship;(void)event;CHECK(phase>=0&&phase<=1);++samples;return 1;}
void meshRemasteredNotifyAnimation(const void *ship,const char *event,double phase)
{(void)ship;(void)event;(void)phase;}
/* Unused file/geometry entry points are traps: this test runs real playback,
   never a file loader, hierarchy walker or renderer. */
crc16 crc16Compute(uint8_t *packet,uint32_t length){(void)packet;(void)length;abort();}
sdword fileLoadAlloc(char *name,void **address,udword flags){(void)name;(void)address;(void)flags;abort();}
#if MEM_USE_NAMES
void *memAllocFunction(sdword length,char *name,udword flags){(void)length;(void)name;(void)flags;abort();}
#else
void *memAllocFunction(sdword length,udword flags){(void)length;(void)flags;abort();}
#endif
void memFree(void *p){(void)p;abort();}
void hmatMakeHMatFromMatAndVec(hmatrix *result,matrix *mat,vector *pos){(void)result;(void)mat;(void)pos;abort();}
void matGetMatFromHMat(matrix *result,hmatrix *mat){(void)result;(void)mat;abort();}
void bsCurveStartPrealloced(splinecurve *curve,sdword count,real32 *values,real32 *times,tcb *params)
{(void)curve;(void)count;(void)values;(void)times;(void)params;abort();}
real32 bsCurveUpdate(splinecurve *curve,real32 elapsed){(void)curve;(void)elapsed;abort();}
void meshHierarchyWalk(meshdata *mesh,meshcallback pre,meshcallback post){(void)mesh;(void)pre;(void)post;abort();}
int meshNativeVisualParameters(const void *mesh,float *scale,unsigned *lod){(void)mesh;(void)scale;(void)lod;abort();}
void nisObjectEulerToMatrix(matrix *result,vector *rotation){(void)result;(void)rotation;abort();}
Universe universe;
static int halted,closing;
void clHalt(CommandLayer *layer,SelectCommand *ships)
{CHECK(layer==&universe.mainCommandLayer);CHECK(ships->numShips==2);halted=1;}
void madLinkInCloseGunsShip(Ship *ship)
{CHECK(halted);++closing;if(ship->madGunStatus!=MAD_STATUS_GUNS_CLOSED)ship->madGunStatus=MAD_STATUS_GUNS_CLOSING;}
#include "../src/Game/CataclysmLinkPreparation.inc"
static void preparation(void)
{
    Ship a={0},b={0};ShipStaticInfo info={0};madstatic settings={0};madanim anim={0};
    info.madStatic=&settings;settings.numGunCloseIndexes=1;
    a.staticinfo=b.staticinfo=&info;a.madBindings=b.madBindings=&anim;
    a.madGunStatus=MAD_STATUS_GUNS_OPEN;b.madGunStatus=MAD_STATUS_GUNS_OPENING;
    a.nextAnim=b.nextAnim=MAD_ANIMATION_GUN_OPENING;
    mrStopAcolytePairAttack(&a,&b);
    CHECK(halted&&closing==2);
    CHECK(a.nextAnim==MAD_ANIMATION_NOTHING&&b.nextAnim==MAD_ANIMATION_NOTHING);
    CHECK(!mrAcolyteGunsRetracted(&a)&&!mrAcolyteGunsRetracted(&b));
    a.madGunStatus=MAD_STATUS_GUNS_CLOSED;
    CHECK(mrAcolyteGunsRetracted(&a)&&!mrAcolyteGunsRetracted(&b));
    b.madGunStatus=MAD_STATUS_GUNS_CLOSED;
    CHECK(mrAcolyteGunsRetracted(&a)&&mrAcolyteGunsRetracted(&b));
}
static void exercise(shipbindings *original)
{
    Ship ship={0};madanim anim={0};
    madheader *header=calloc(1,madHeaderSize(3));
    CHECK(header!=NULL);
    header->nAnimations=3;
    header->anim[0].name="CombatIn";header->anim[0].endTime=1;
    header->anim[1].name="Link";header->anim[1].endTime=2;
    header->anim[2].name="Unlink";header->anim[2].endTime=1;
    anim.header=header;anim.nCurrentAnim=MAD_NoAnimation;
    ship.madBindings=&anim;ship.bindings=original;
    madAnimationStart(&ship,0);
    CHECK(anim.saveBindings==original&&ship.bindings==&anim.bindings);
    madAnimationStart(&ship,1);
    CHECK(anim.saveBindings==original&&ship.bindings==&anim.bindings);
    /* Repeated replacement, including a clip paused by an animation owner. */
    anim.bPaused=TRUE;madAnimationStart(&ship,2);
    CHECK(anim.saveBindings==original);
    madAnimationStop(&ship);
    CHECK(ship.bindings==original&&anim.nCurrentAnim==MAD_NoAnimation);
    anim.bPaused=FALSE;
    madAnimationStart(&ship,0);madAnimationStart(&ship,1);
    CHECK(!madAnimationUpdate(&ship,.5f));
    CHECK(madAnimationUpdate(&ship,3));
    CHECK(ship.bindings==original&&anim.nCurrentAnim==MAD_NoAnimation);
    /* This is also the allocation cleanup must see while Link is active. */
    madAnimationStart(&ship,0);madAnimationStart(&ship,1);
    CHECK(anim.saveBindings==original&&anim.saveBindings!=&anim.bindings);
    madAnimationStop(&ship);
    CHECK(ship.bindings==original);
    free(header);
}
int main(void)
{
    shipbindings *owned=calloc(1,sizeof(*owned));
    CHECK(owned!=NULL);exercise(owned);free(owned);
    exercise(NULL); /* Native visuals may have no classic gun binding block. */
    CHECK(samples>0);preparation();
    printf("PASS: %d animation handoff checks; original binding ownership survives CombatIn/Link/Unlink.\n",checks);
    return 0;
}
