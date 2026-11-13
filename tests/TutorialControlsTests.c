/* Production button creation/callbacks with guarded allocator seams. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "UIControls.h"
#include "Globals.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static region root;
static regionhandle tutRootRegion=&root;
static rectangle tutTextRect={10,10,200,120};
static featom tutNextAtom,tutBackAtom;
static buttonhandle tutNextRegion,tutBackRegion;
static int tutNextVisible,tutBackVisible,tutPrevVisible,tutNextButtonState;
static char tutCurrLessonName[128]="lesson8",tutLastLessonName[128]="lesson7";
static int roots,ended,loaded,inCallback;
udword tutorial;
static unsigned long long guard=0x1122334455667788ull;
void tutHideNextButton(void); void tutHideBackButton(void);
udword tutProcessNextButton(regionhandle,smemsize,udword,udword);
udword tutProcessBackButton(regionhandle,smemsize,udword,udword);
static void tutAllocateRootRegion(void){roots++;}
static void tutDeallocateRootRegion(void){assert(roots>0);roots--;}
static void tutDrawNextButtonFunction(regionhandle r){(void)r;}
static void tutDrawBackButtonFunction(regionhandle r){(void)r;}
buttonhandle uicChildButtonAlloc(controlhandle parent,smemsize id,sdword x,sdword y,
    sdword width,sdword height,uicfunction fn,udword flags)
{
    buttonhandle b=calloc(1,sizeof(uicslider)+sizeof(guard));
    assert(parent==&root && flags==FA_Button && b);
    memcpy((char*)b+sizeof(uicslider),&guard,sizeof(guard));
    b->reg.userID=id; b->reg.rect=(rectangle){x,y,x+width,y+height};
    b->processFunction=fn;
    return b;
}
void regRegionDelete(regionhandle r)
{
    unsigned long long tail;
    memcpy(&tail,(char*)r+sizeof(uicslider),sizeof(tail));
    assert(tail==guard);free(r);
}
regiondrawfunction regDrawFunctionSet(regionhandle r,regiondrawfunction fn)
{regiondrawfunction old=r->drawFunction;r->drawFunction=fn;return old;}
udword uicButtonProcess(regionhandle r,smemsize id,udword event,udword data)
{return ((buttonhandle)r)->processFunction(r,id,event,data);}
static int fileExists(char *name,int flags){(void)flags;return !strncmp(name,"lesson",6);}
#define VERIFYSAVEFILE_OK 1
static int VerifySaveFile(char *name){(void)name;return VERIFYSAVEFILE_OK;}
static void tutEnableEverything(void){}
static void spMainScreen(void){}
static void tbForceTaskbar(int on){(void)on;}
static void gameEnd(void)
{assert(!inCallback);tutHideNextButton();tutHideBackButton();ended++;}
static void utyLoadSinglePlayerGameGivenFilename(char *name)
{assert(!strcmp(name,"lesson8") || !strcmp(name,"lesson7"));loaded++;}
#include "../src/Game/TutorialControls.inc"

int main(void)
{
    int i;
    for(i=0;i<100;i++) {
        tutShowNextButton();tutShowNextButton();
        assert(tutNextRegion->reg.userID==(smemsize)&tutNextAtom);
        assert(tutNextRegion->reg.atom==&tutNextAtom);
        assert(tutNextRegion->processFunction==tutProcessNextButton);
        tutNextRegion->processFunction(&tutNextRegion->reg,0,CM_ButtonClick,0);
        assert(tutNextButtonClicked()==1 && tutNextButtonClicked()==0);
        tutShowBackButton();
        inCallback=1;
        tutBackRegion->processFunction(&tutBackRegion->reg,0,CM_ButtonClick,0);
        assert(ended==i && loaded==i);
        inCallback=0;tutProcessDeferredRestart();
        assert(ended==i+1 && loaded==i+1 && roots==0);
        tutProcessDeferredRestart();assert(loaded==i+1);
    }
    tutShowPrevButton();
    tutBackRegion->processFunction(&tutBackRegion->reg,0,CM_ButtonClick,0);
    tutProcessDeferredRestart();assert(!strcmp(tutCurrLessonName,"lesson7"));
    puts("PASS: tutorial Next/Back memory guards, button events and deferred restart lifecycle");
    return 0;
}
