#include "Universe.h"
#include <stdio.h>
#include <stdlib.h>
Universe universe;
static bool32 smSensorsActive,mrRenderMainScreen=TRUE,cmActive,lmActive,rmGUIActive,soundeventinited=TRUE,cata=TRUE;
static Ship *visible;
static int shipStops,derelictStops;
bool32 soundEventUsesCataclysm(void){return cata;}
bool32 univSpaceObjInRenderList(SpaceObj *object){return object==(SpaceObj*)visible;}
void soundEventShipRemove(Ship *ship){(void)ship;++shipStops;}
void soundEventDerelictRemove(Derelict *derelict){(void)derelict;++derelictStops;}
#include "../src/Game/SoundWorldLoopLifecycle.inc"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void)
{
    Ship heard={0},offscreen={0};Derelict wreck={0};
    visible=&heard;universe.ShipList.head=&heard.shiplink;
    heard.shiplink.structptr=&heard;heard.shiplink.next=&offscreen.shiplink;
    offscreen.shiplink.structptr=&offscreen;
    universe.DerelictList.head=&wreck.derelictlink;wreck.derelictlink.structptr=&wreck;
    soundEventPruneWorldLoops();CHECK(shipStops==1&&derelictStops==1);
    cmActive=TRUE;CHECK(!soundEventWorldAudioAllowed());
    soundEventPruneWorldLoops();CHECK(shipStops==3&&derelictStops==2);
    cmActive=FALSE;lmActive=TRUE;CHECK(!soundEventWorldAudioAllowed());
    lmActive=FALSE;smSensorsActive=TRUE;CHECK(!soundEventWorldAudioAllowed());
    soundEventPruneWorldLoops();CHECK(shipStops==5);
    smSensorsActive=FALSE;rmGUIActive=TRUE;CHECK(!soundEventWorldAudioAllowed());
    rmGUIActive=FALSE;mrRenderMainScreen=FALSE;CHECK(!soundEventWorldAudioAllowed());
    mrRenderMainScreen=TRUE;cata=FALSE;
    soundEventPruneWorldLoops();CHECK(shipStops==5);
    puts("PASS: offscreen and manager-hidden world loops stop; HW1 lifecycle remains unchanged.");return 0;
}
