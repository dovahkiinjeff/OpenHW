/* Exercise the real docking eligibility path with campaign station hosts. */
#include "Universe.h"
#include "Dock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Universe universe;
bool32 singlePlayerGame=TRUE,opCaptureBuildAll=TRUE;
static int checks,allied;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
bool32 allianceIsShipAlly(Ship *ship,Player *host) {(void)ship;(void)host;return allied;}
sdword ThisShipIs(ShipStaticInfo *info)
{
    return info->shipclass==CLASS_Fighter?THISSHIPIS_FIGHTER:
        info->shipclass==CLASS_Corvette?THISSHIPIS_CORVETTE:THISSHIPIS_OTHERNONCAPITALSHIP;
}
bool32 shipRetireNeedsMothership(Ship *ship) {(void)ship;return FALSE;}
#ifdef HW_REPRODUCE_STATION_RACE_LOCK
#include "DockEligibility.before.inc"
#else
#include "../src/Game/DockEligibility.inc"
#endif

int main(void)
{
    static Player player,stationOwner,other;
    static Ship visitor,host;
    static ShipStaticInfo visitorInfo,hostInfo;
    ShipRace races[]={R1,R2,P1,P2,P3,Traders};ShipType stations[]={JunkYardHQ,ResearchStation};
    udword blocked[]={SOF_Dead,SOF_Disabled,SOF_Crazy,SOF_Hyperspace};
    int t,r,c,i;
    universe.curPlayerPtr=&player;visitor.playerowner=&player;host.playerowner=&stationOwner;
    visitor.staticinfo=&visitorInfo;host.staticinfo=&hostInfo;
    hostInfo.canReceiveSomething=TRUE;hostInfo.canReceiveTheseShips[THISSHIPIS_FIGHTER]=TRUE;
    hostInfo.canReceiveTheseShips[THISSHIPIS_CORVETTE]=TRUE;
    host.shiprace=Traders;host.specialFlags=SPECIAL_FriendlyStatus;
    for(t=0;t<2;++t)for(r=0;r<6;++r)for(c=0;c<2;++c)
    {
        host.shiptype=hostInfo.shiptype=stations[t];visitor.shiprace=races[r];
        visitor.shiptype=visitorInfo.shiptype=c?SalCapCorvette:LightInterceptor;
        visitorInfo.shipclass=c?CLASS_Corvette:CLASS_Fighter;
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUELREPAIR|DOCK_AT_SPECIFIC_SHIP));
        for(i=0;i<4;++i) {host.flags=blocked[i];CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));}
        host.flags=0;host.specialFlags=0;
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL)==(stations[t]==JunkYardHQ));
        allied=TRUE;CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));allied=FALSE;
        host.specialFlags=SPECIAL_FriendlyStatus;
        hostInfo.canReceiveTheseShips[ThisShipIs(&visitorInfo)]=FALSE;
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL)==(stations[t]==JunkYardHQ));
        hostInfo.canReceiveTheseShips[ThisShipIs(&visitorInfo)]=TRUE;
    }
    /* Captured ships use any allied compatible hangar, irrespective of race. */
    host.shiptype=hostInfo.shiptype=Carrier;host.playerowner=&player;
    host.shiprace=R2;visitor.shiprace=R1;
    CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    visitor.shiprace=R2;CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    visitor.shiprace=R1;opCaptureBuildAll=FALSE;
    CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    opCaptureBuildAll=TRUE;singlePlayerGame=FALSE;
    CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    singlePlayerGame=TRUE;visitor.playerowner=&other;host.playerowner=&stationOwner;
    CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    visitor.playerowner=&player;host.shiprace=Traders;host.shiptype=hostInfo.shiptype=JunkYardDawg;
    CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    host.shiptype=hostInfo.shiptype=JunkYardHQ;hostInfo.canReceiveSomething=FALSE;
    CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_FOR_REFUEL));
    hostInfo.canReceiveSomething=TRUE;host.specialFlags=0;
    CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_AT_SPECIFIC_SHIP));
    opCaptureBuildAll=FALSE;CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_AT_SPECIFIC_SHIP));
    CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_FOR_RETIRE));
    visitorInfo.shipclass=CLASS_Frigate;CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_AT_SPECIFIC_SHIP));
    visitorInfo.shipclass=CLASS_Corvette;visitor.playerowner=&other;
    CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_AT_SPECIFIC_SHIP));
    visitor.playerowner=&player;singlePlayerGame=FALSE;
    CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_AT_SPECIFIC_SHIP));
    /* Both ordinary and Quick Dock roll-call paths must accept captured
       Kadeshi fighters at either player race's carrier or mothership. */
    singlePlayerGame=TRUE;allied=TRUE;host.playerowner=&player;visitor.playerowner=&player;
    host.specialFlags=0;hostInfo.canReceiveShipsForRetire=TRUE;
    for(t=0;t<2;t++)for(r=0;r<2;r++) {
        host.shiptype=hostInfo.shiptype=t?Carrier:Mothership;host.shiprace=r?R2:R1;
        visitor.shiprace=P2;visitor.shiptype=visitorInfo.shiptype=P2Swarmer;visitorInfo.shipclass=CLASS_Fighter;
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_PERMANENTLY));
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_PERMANENTLY|DOCK_INSTANTANEOUSLY));
        visitor.shiptype=visitorInfo.shiptype=P2AdvanceSwarmer;
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_PERMANENTLY));
        visitor.shiptype=visitorInfo.shiptype=P2FuelPod;visitorInfo.shipclass=CLASS_Frigate;
        CHECK(ThisShipCanDockWith(&visitor,&host,DOCK_FOR_RETIRE));
        host.flags=SOF_Dead;CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_FOR_RETIRE));host.flags=0;
        allied=FALSE;CHECK(!ThisShipCanDockWith(&visitor,&host,DOCK_FOR_RETIRE));allied=TRUE;
    }
    printf("Cross-race and mission station docking: %d checks passed\n",checks);
    return 0;
}
