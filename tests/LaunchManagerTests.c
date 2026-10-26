/* Production roster, command receiver and teardown. No game window or data. */
#include "Universe.h"
#include "FEFlow.h"
#include "UIControls.h"
#include "SoundEventDefs.h"
#include "LaunchHoldLayout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Universe universe;
static int checks, launches, deleted, previewClosed, overlayRestored, sounds;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
#include "../src/Game/LaunchHoldRoster.inc"
void clLaunchShip(CommandLayer *layer,SelectCommand *selection,ShipPtr host)
{
    (void)layer;
    CHECK(selection->numShips==1 && host->shiptype!=DDDFrigate);
    launches++;
}
#include "../src/Game/LaunchMultipleShips.inc"

static regionhandle lmBaseRegion,lmHoldRegion;
static listwindowhandle lmListWindow;
static ShipsInsideMe *shipsin;
static bool32 lmActive,lmIoSaveState,mrRenderMainScreen,tbDisable;
static int lmRenderEverythingCounter;
static int docked=1;
static void testDelete(regionhandle closing);
static void testSound(int event){(void)event;sounds++;}
static void svClose(void){previewClosed++;}
static void ioEnable(void){overlayRestored++;}
static bool32 mdManagerDockEnabled(void){return docked;}
#undef feScreenDelete
#define feScreenDelete testDelete
#define soundEvent(object,event) testSound(event)
#include "../src/Game/LaunchManagerLifecycle.inc"
static void testDelete(regionhandle closing)
{
    CHECK(closing!=NULL);
    CHECK(!lmActive && !lmBaseRegion && !lmHoldRegion && !lmListWindow && !shipsin);
    deleted++;
    /* A queued close/docking callback during destruction must be harmless. */
    lmDismiss();
}

static void roster(void)
{
    static Player ours,enemy;
    static Ship fleet[80];
    static ShipStaticInfo stats[80];
    static ShipsInsideMe holds[80];
    int i,count=0,page,slot;
    universe.curPlayerPtr=&ours;ours.PlayerMothership=&fleet[5];
    universe.ShipList.head=&fleet[0].shiplink;
    for(i=0;i<80;++i)
    {
        Ship *s=&fleet[i];
        s->shiptype=Carrier;s->playerowner=&ours;s->staticinfo=&stats[i];
        s->shipsInsideMe=&holds[i];stats[i].maxDockableFighters=10;
        s->shiplink.structptr=s;s->shiplink.next=i<79 ? &fleet[i+1].shiplink : NULL;
    }
    /* The exact regression: full AND empty intrinsic drone racks. Even if a
       mod gives one ordinary capacity, it never belongs in manual launches. */
    fleet[10].shiptype=DDDFrigate;holds[10].insideList.num=24;
    fleet[11].shiptype=DDDFrigate;holds[11].insideList.num=0;
    fleet[12].flags=SOF_Dead;fleet[13].flags=SOF_Hide;fleet[14].playerowner=&enemy;
    fleet[15].shipsInsideMe=NULL;
    /* An occupied captured non-carrier hold remains reachable. */
    fleet[16].shiptype=ResearchStation;stats[16].maxDockableFighters=0;
    holds[16].insideList.num=3;
    CHECK(lmHoldCount()==74);CHECK(lmHoldAt(0)==&fleet[5]);
    CHECK(lmHoldIndexOf(&fleet[10])==-1 && lmHoldIndexOf(&fleet[11])==-1);
    CHECK(lmHoldIndexOf(&fleet[16])>=0);
    for(page=0;page<lmHoldPageCount(lmHoldCount());++page)
        for(slot=0;slot<LM_HOLD_PAGE_SIZE;++slot)
        {
            int index=lmHoldSlotIndex(lmHoldCount(),page,slot);
            Ship *s;
            if(index<0)continue;
            s=lmHoldAt(index);
            CHECK(s && s->shiptype!=DDDFrigate && lmHoldIndexOf(s)==index);count++;
        }
    CHECK(count==74 && lmHoldAt(74)==NULL);
    fleet[5].shiptype=DDDFrigate;
    CHECK(lmHoldCount()==73 && lmHoldAt(0)==&fleet[0]);
    for(i=0;i<80;++i)fleet[i].flags|=SOF_Dead;
    CHECK(lmHoldCount()==0 && lmHoldAt(0)==NULL);
}

static void commandGuard(void)
{
    Ship host={0},drone={0};
    SelectCommand selection={0};
    selection.numShips=1;selection.ShipPtr[0]=&drone;
    host.shiptype=DDDFrigate;drone.shiptype=Drone;
    clLaunchMultipleShips(&universe.mainCommandLayer,&selection,&host);
    CHECK(launches==0 && drone.specialFlags==0 && drone.command==NULL);
    host.shiptype=Carrier;drone.shiptype=LightInterceptor;
    clLaunchMultipleShips(&universe.mainCommandLayer,&selection,&host);
    CHECK(launches==1);
    host.shiptype=ResearchStation;
    clLaunchMultipleShips(&universe.mainCommandLayer,&selection,&host);
    CHECK(launches==2);
    drone.attributes|=ATTRIBUTES_Defector;
    clLaunchMultipleShips(&universe.mainCommandLayer,&selection,&host);
    clLaunchMultipleShips(&universe.mainCommandLayer,&selection,NULL);
    selection.numShips=0;
    clLaunchMultipleShips(&universe.mainCommandLayer,&selection,&host);
    CHECK(launches==2);
}

static void lifecycle(void)
{
    region base={0},bank={0};
    struct uiclistwindow list={0};
    ShipsInsideMe hold={0};
    CameraCommand untouched;
    int pass;
    memset(&universe.mainCameraCommand,0x5a,sizeof(CameraCommand));
    untouched=universe.mainCameraCommand;
    for(pass=0;pass<2;++pass)
    {
        int before=deleted;
        lmBaseRegion=&base;lmHoldRegion=&bank;lmListWindow=&list;shipsin=&hold;
        lmActive=TRUE;lmIoSaveState=TRUE;lmRenderEverythingCounter=4;
        tbDisable=TRUE;mrRenderMainScreen=FALSE;docked=pass==0;
        lmDismiss();lmDismiss();
        CHECK(deleted==before+1 && previewClosed==pass+1 && overlayRestored==pass+1);
        CHECK(!lmActive && !lmBaseRegion && !lmHoldRegion && !lmListWindow && !shipsin);
        CHECK(!lmIoSaveState && !lmRenderEverythingCounter && mrRenderMainScreen);
        CHECK(docked ? tbDisable : !tbDisable);
        CHECK(memcmp(&untouched,&universe.mainCameraCommand,sizeof(CameraCommand))==0);
    }
    CHECK(sounds==3);
}

int main(void)
{
    roster();commandGuard();lifecycle();
    printf("PASS: Launch Manager roster, queued drone-command rejection, reentrant close and unchanged camera (%d checks)\n",checks);
    return 0;
}
