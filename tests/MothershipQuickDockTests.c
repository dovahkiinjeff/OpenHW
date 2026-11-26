#include "Universe.h"
#include "Mothership.h"
#include "MeshAnim.h"
#include "MadLinkInDefs.h"
#include <stdio.h>
#include <stdlib.h>
static int checks,detached,stopped,unpaused;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void MothershipDettachObjectFromDoor(Ship *ship){(void)ship;++detached;}
void madAnimationPause(Ship *ship,bool32 pause){CHECK(!pause);ship->madBindings->bPaused=pause;++unpaused;}
void madAnimationStop(Ship *ship){(void)ship;++stopped;}
#include "../src/Ships/MothershipQuickDock.inc"
int main(void)
{
    Ship *ship=calloc(1,sizeof(Ship)+sizeof(MothershipSpec));ShipStaticInfo info={0};
    MothershipSpec *spec=(MothershipSpec *)ship->ShipSpecifics;madanim animation={0};madstatic clips={0};sdword clip=7;
    /* Exact crash: Somtaaw R1 mothership with no HW1 door animations. */
    ship->shiprace=R1;ship->shiptype=CataMothership;ship->staticinfo=&info;ship->madDoorStatus=MAD_STATUS_NOSTATUS;
    spec->doorCargo=(void*)1;
    mothershipCleanDoorForHSInstant(ship);CHECK(detached==0&&stopped==0&&unpaused==0&&spec->doorCargo==(void*)1);
    mothershipCleanDoorForHSInstant(NULL);
    ship->shiptype=Mothership;ship->shiprace=R2;mothershipCleanDoorForHSInstant(ship);CHECK(detached==0);
    ship->shiprace=R1;ship->madBindings=&animation;animation.bPaused=TRUE;info.madStatic=&clips;
    clips.numDoorOpenIndexes=1;clips.DoorOpenIndexes=&clip;
    mothershipCleanDoorForHSInstant(ship);CHECK(detached==1&&spec->doorCargo==NULL);
    CHECK(stopped==1&&unpaused==1&&ship->cuedAnimationIndex==7&&ship->madDoorStatus==MAD_STATUS_DOOR_CLOSED);
    mothershipCleanDoorForHSInstant(ship);CHECK(stopped==1&&unpaused==1);
    ship->madBindings=NULL;info.madStatic=NULL;ship->madDoorStatus=MAD_STATUS_DOOR_OPEN;
    mothershipCleanDoorForHSInstant(ship);CHECK(ship->madDoorStatus==MAD_STATUS_DOOR_CLOSED&&stopped==1);
    free(ship);printf("PASS: %d quick-dock door safety checks\n",checks);return 0;
}
