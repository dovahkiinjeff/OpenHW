#include "Universe.h"
#include "FEFlow.h"
#include "Dock.h"
#include <stdio.h>
#include <stdlib.h>

Universe universe;
MaxSelection selSelected;
bool32 singlePlayerGame=TRUE,playPackets,universePause,mrDisabled,mrMenuDontDisappear;
udword opPauseOrders;
static int checks,orders;
static Ship *sentTarget;
static DockType sentType;
static void *mouseCursorObjPtr;
static int tutorial;
#define TUTORIAL_ONLY 1
static struct {int bDock;} tutEnable={1};
fescreen *feTempMenuScreen;
sdword feMenuLevel;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
#define soundEvent(object,event) ((void)0)
void makeShipsDockCapable(SelectCommand *s) {(void)s;}
DockType dockGetAppropriateTypeOfDocking(SelectCommand *s) {(void)s;return DOCK_FOR_REFUELREPAIR;}
void clWrapDock(CommandLayer *layer,SelectCommand *s,DockType type,Ship *target)
{CHECK(layer==&universe.mainCommandLayer && s->numShips==1);++orders;sentTarget=target;sentType=type;}
#include "../src/SDL/DockOrderTarget.inc"
#ifdef HW_REPRODUCE_CONTEXT_DOCK_TARGET_LOSS
#include "DockOrders.before.inc"
#else
#include "../src/SDL/DockOrders.inc"
#endif

int main(void)
{
    static Ship station,mother,visitor;
    static featom menuItem;
    static fescreen menu;
    station.shiptype=JunkYardHQ;station.shiprace=Traders;station.shipID.shipNumber=23;
    mother.shiptype=Mothership;mother.shiprace=R1;mother.shipID.shipNumber=24;
    station.shiplink.structptr=&station;station.shiplink.next=&mother.shiplink;
    mother.shiplink.structptr=&mother;universe.ShipList.head=&station.shiplink;
    selSelected.numShips=1;selSelected.ShipPtr[0]=&visitor;
    /* Open over the station; moving onto Dock replaces the cursor hit. */
    mouseCursorObjPtr=&station;mrCaptureDockMenuTarget();
    feTempMenuScreen=mrDockMenuScreen=&menu;feMenuLevel=1;mouseCursorObjPtr=NULL;
    mrDockingOrders("CSM_Dock",&menuItem);
    CHECK(orders==1 && sentTarget==&station && sentType==DOCK_AT_SPECIFIC_SHIP);
    /* Expired/stale targets must never fall back to the mothership. */
    station.flags=SOF_Dead;mrDockingOrders("CSM_Dock",&menuItem);CHECK(orders==1);
    station.flags=0;station.shipID.shipNumber=99;mrDockingOrders("CSM_Dock",&menuItem);CHECK(orders==1);
    station.shipID.shipNumber=23;universe.ShipList.head=&mother.shiplink;
    mrDockingOrders("CSM_Dock",&menuItem);CHECK(orders==1);
    universe.ShipList.head=&station.shiplink;
    /* A normal HUD/keyboard Dock without a station stays nearest-host. */
    feMenuLevel=0;feTempMenuScreen=NULL;mouseCursorObjPtr=NULL;
    mrDockingOrders(NULL,NULL);CHECK(orders==2 && sentTarget==NULL && sentType==DOCK_FOR_REFUELREPAIR);
    /* Hovering the relay and pressing Dock retains that explicit target. */
    mouseCursorObjPtr=&station;mrDockingOrders(NULL,NULL);
    CHECK(orders==3 && sentTarget==&station && sentType==DOCK_AT_SPECIFIC_SHIP);
    /* Opening over another object clears old station context. */
    mouseCursorObjPtr=&mother;mrCaptureDockMenuTarget();CHECK(!mrDockMenuTarget);
    mrDockingOrders(NULL,NULL);CHECK(orders==4 && sentTarget==NULL);
    /* No unsafe dereference of arbitrary, dead or retired pointer hits. */
    mouseCursorObjPtr=(void *)1;mrCaptureDockMenuTarget();CHECK(!mrDockMenuTarget);
    mouseCursorObjPtr=&station;station.flags=SOF_Disabled;mrCaptureDockMenuTarget();CHECK(!mrDockMenuTarget);
    station.flags=0;singlePlayerGame=FALSE;mrCaptureDockMenuTarget();CHECK(!mrDockMenuTarget);
    printf("Context station docking passed: %d checks; explicit target and ordinary Dock semantics preserved\n",checks);
    return 0;
}
