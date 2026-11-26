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
#include "../src/Game/DockHostPurpose.inc"
#include "../src/Game/DockCompositeCollision.h"
static int nativeHull=1;
bool32 meshRemasteredMarkerPosition(const void *ship,const char *name,vector *position)
{(void)ship;(void)position;return nativeHull && !strcmp(name,"native_hull");}
#include "../src/Game/CataclysmComponentVisual.inc"
#include "../src/Game/CataclysmInvulnerability.inc"
#include "../src/Game/CataclysmComponentIdentity.inc"
#include "../src/Game/CataclysmSoundIdentity.inc"
#include "SoundEventDefs.h"
#include "../src/Game/CataclysmUiSound.inc"
#include "SinglePlayer.h"
static MissionEnum testMission=MISSION_CATA_1_HIIGARA;
MissionEnum spGetCurrentMission(void){return testMission;}
#include "../src/Game/CataclysmNPC.c"
#include "../src/Game/CataclysmDockService.inc"
#include "CommandLayer.h"
static int reinforcementLaunchCount;
static Ship *reinforcementHost;
void clLaunchMultipleShips(CommandLayer *layer,SelectCommand *ships,ShipPtr host)
{
    sdword i;(void)layer;reinforcementLaunchCount=ships->numShips;reinforcementHost=host;
    for(i=0;i<ships->numShips;i++){
        CHECK(!(ships->ShipPtr[i]->specialFlags&SPECIAL_STAY_TILL_EXPLICITLAUNCH));
        CHECK(ships->ShipPtr[i]->specialFlags&SPECIAL_LaunchedFromKas);
        CHECK(ships->ShipPtr[i]->specialFlags&SPECIAL_KasCheckDoneLaunching);
    }
}
#include "../src/Game/CataclysmReinforcementLaunch.inc"

int main(void)
{
    {
        Ship ship={0};ShipStaticInfo info={0};
        ShipType types[]={CataScout,CataAcolyte,CataACV,LightInterceptor,CataWorker,CataRammingFrigate};
        unsigned index;
        ship.staticinfo=&info;info.maxhealth=2500;
        for(index=0;index<sizeof(types)/sizeof(types[0]);index++){
            ship.shiptype=info.shiptype=types[index];
            info.shipclass=types[index]==CataWorker?CLASS_Resource:types[index]==CataRammingFrigate?CLASS_Frigate:
                types[index]==CataACV?CLASS_Corvette:CLASS_Fighter;
            ship.health=125;info.repairOtherShipRate=0;
            cataDockServiceOnLaunch(&ship);CHECK(ship.health==2500);
        }
        ship.health=125;singlePlayerGame=FALSE;
        cataDockServiceOnLaunch(&ship);CHECK(ship.health==125);
        singlePlayerGame=TRUE;testMission=MISSION_1_KHARAK_SYSTEM;
        cataDockServiceOnLaunch(&ship);CHECK(ship.health==125);
        testMission=MISSION_CATA_1_HIIGARA;
        ship.flags=SOF_Dead;cataDockServiceOnLaunch(&ship);CHECK(ship.health==125);
        ship.flags=0;ship.health=0;cataDockServiceOnLaunch(&ship);CHECK(ship.health==0);
        ship.health=125;info.shipclass=CLASS_Mothership;
        cataDockServiceOnLaunch(&ship);CHECK(ship.health==125);
    }
    CHECK(cataUiEventRow(UI_ClickAccept)==8);
    CHECK(cataUiEventRow(UI_ClickCancel)==10);
    CHECK(cataUiEventRow(UI_BuildManager)==4);
    CHECK(cataUiEventRow(UI_SensorsManager)==1);
    CHECK(cataUiEventRow(UI_ManagerExit)==48);
    CHECK(cataUiEventRow(UI_SoundOfSpace)==0);
    {
        MaxSelection selection={0};GrowSelection newShips={0};Ship first={0},second={0},carrier={0};
        selection.numShips=2;selection.ShipPtr[0]=&first;selection.ShipPtr[1]=&second;
        newShips.selection=(SelectCommand*)&selection;
        first.specialFlags=second.specialFlags=SPECIAL_STAY_TILL_EXPLICITLAUNCH;
        cataLaunchNewReinforcements(&newShips,&carrier);
        CHECK(reinforcementLaunchCount==2&&reinforcementHost==&carrier);
    }
    {
        Ship actor={0};SpaceObj asteroid={0};
        actor.objtype=OBJ_ShipType;actor.playerowner=&universe.players[0];
        CHECK(!cataNpcControlsObject(&actor));
        actor.specialFlags2=SPECIAL_2_CataScriptControlled;
        CHECK(cataNpcControlsObject(&actor));
        actor.specialFlags2=0;actor.playerowner=&universe.players[1];
        CHECK(cataNpcControlsObject(&actor));
        testMission=MISSION_1_KHARAK_SYSTEM;CHECK(!cataNpcControlsObject(&actor));
        testMission=MISSION_CATA_1_HIIGARA;
        singlePlayerGame=FALSE;CHECK(!cataNpcControlsObject(&actor));singlePlayerGame=TRUE;
        asteroid.objtype=OBJ_AsteroidType;CHECK(!cataNpcControlsObject(&asteroid));
        CHECK(!cataNpcControlsObject(NULL));
    }
    {
        Ship soundShip={0};
        soundShip.shiprace=R1;soundShip.shiptype=CataAcolyte;
        CHECK(cataSoundShipObject(&soundShip)==1);
        soundShip.shiptype=CataWorker;CHECK(cataSoundShipObject(&soundShip)==14);
        soundShip.shiptype=CataACV;CHECK(cataSoundShipObject(&soundShip)==4);
        soundShip.shiptype=CataMothership;CHECK(cataSoundShipObject(&soundShip)==18);
        soundShip.shiptype=StandardDestroyer;CHECK(cataSoundShipObject(&soundShip)==73);
        soundShip.shiprace=R2;CHECK(cataSoundShipObject(&soundShip)==90);
        soundShip.shiptype=HeavyCruiser;CHECK(cataSoundShipObject(&soundShip)==80);
        soundShip.shiprace=R1;CHECK(cataSoundShipObject(&soundShip)==66);
        soundShip.shiptype=LightInterceptor;CHECK(cataSoundShipObject(&soundShip)==-1);
    }
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
    /* Cataclysm hangars omit HW1 pumping/repair-rate fields. Automatic Dock
       must still find them; resource-only hosts must not become hangars. */
    memset(&hostInfo,0,sizeof(hostInfo));hostInfo.shiptype=CataMothership;
    hostInfo.canReceiveShips=TRUE;
    CHECK(dockHostSupportsPurpose(&hostInfo,DOCK_FOR_REFUELREPAIR));
    CHECK(dockHostSupportsPurpose(&hostInfo,DOCK_FOR_BOTHREFUELANDREPAIR));
    CHECK(!dockHostSupportsPurpose(&hostInfo,DOCK_FOR_RETIRE));
    hostInfo.canReceiveShipsForRetire=TRUE;CHECK(dockHostSupportsPurpose(&hostInfo,DOCK_FOR_RETIRE));
    hostInfo.shiptype=Mothership;CHECK(!dockHostSupportsPurpose(&hostInfo,DOCK_FOR_REFUELREPAIR));
    hostInfo.shiptype=CataProcessor;hostInfo.canReceiveShips=FALSE;hostInfo.canReceiveResources=TRUE;
    CHECK(!dockHostSupportsPurpose(&hostInfo,DOCK_FOR_REFUELREPAIR));
    CHECK(dockHostSupportsPurpose(&hostInfo,DOCK_TO_DEPOSIT_RESOURCES));
    {
        Ship part={0},another={0};ClampInfo clamp={0};
        host.shiptype=CataMothership;visitor.dockingship=&host;
        part.clampInfo=&clamp;clamp.host=(SpaceObjRotImpTarg *)&host;
        CHECK(dockCompositeContact(&visitor,&part));
        CHECK(!dockCompositeContact(&visitor,&another));
        visitor.dockingship=NULL;CHECK(!dockCompositeContact(&visitor,&part));
        visitor.dockingship=&another;CHECK(!dockCompositeContact(&visitor,&part));
    }
    {
        Ship part={0};ClampInfo clamp={0};
        ShipType embedded[]={CataMothershipCommand,CataMothershipEngineering,
            CataMothershipEnginesLower,CataMothershipEnginesUpper};
        ShipType separate[]={CataMothershipLowerSpine,CataMothershipOreCanArmour,
            CataMothershipDockingBay,CataMothershipWeapons,CataMothershipArmour,
            CataMothershipMicro,CataMothershipSpecial};
        host.objtype=OBJ_ShipType;host.shiptype=CataMothership;
        part.clampInfo=&clamp;clamp.host=(SpaceObjRotImpTarg *)&host;
        for(i=0;i<4;i++){
            part.shiptype=embedded[i];part.flags=0; /* reveal/load cleared Hide */
            CHECK(cataShipEmbeddedInNativeHull(&part));
            nativeHull=0;CHECK(!cataShipEmbeddedInNativeHull(&part));nativeHull=1;
        }
        for(i=0;i<7;i++){
            part.shiptype=separate[i];CHECK(!cataShipEmbeddedInNativeHull(&part));
        }
        part.shiptype=CataMothershipCommand;part.clampInfo=NULL;
        CHECK(!cataShipEmbeddedInNativeHull(&part));
    }
    {
        Ship held={0};
        held.posinfo.velocity.x=10;held.rotinfo.rotspeed.z=4;
        cataSetShipInvulnerable(&held,TRUE);
        CHECK(held.specialFlags2&SPECIAL_2_CataInvulnerable);
        CHECK(!(held.flags&SOF_DontApplyPhysics));
        CHECK(held.posinfo.velocity.x==10 && held.rotinfo.rotspeed.z==4);
        held.flags|=SOF_DontApplyPhysics; /* migrate a save from the broken port */
        held.posinfo.force.x=100000;held.rotinfo.torque.z=100000;
        cataSetShipInvulnerable(&held,FALSE);
        CHECK(!(held.flags&SOF_DontApplyPhysics));
        CHECK(held.posinfo.force.x==0 && held.rotinfo.torque.z==0);
        CHECK(!(held.specialFlags2&SPECIAL_2_CataInvulnerable));
        held.posinfo.velocity.x=50;held.rotinfo.rotspeed.z=0.1f;
        cataSetShipInvulnerable(&held,FALSE);
        CHECK(held.posinfo.velocity.x==50 && held.rotinfo.rotspeed.z==0.1f);
    }
    {
        Ship module={0},worker={0};ClampInfo clamp={0};
        module.objtype=worker.objtype=host.objtype=OBJ_ShipType;
        host.shiptype=CataMothership;clamp.host=(SpaceObjRotImpTarg *)&host;
        module.clampInfo=worker.clampInfo=&clamp;worker.shiptype=CataWorker;
        for(i=CataMothershipCommand;i<=CATA_LAST_COMPONENT;i++){
            module.shiptype=i;module.flags=SOF_Disabled|SOF_DontApplyPhysics;
            CHECK(cataShipIsAttachedComponent(&module));
        }
        for(i=CATA_LAST_COMPONENT+1;i<=CATA_LAST_SHIP;i++){
            module.shiptype=i;
            CHECK(!cataShipIsAttachedComponent(&module));
        }
        module.shiptype=CataMothershipCommand;
        CHECK(!cataShipIsAttachedComponent(&worker));
        CHECK(!cataShipIsAttachedComponent(&host));
        module.clampInfo=NULL;CHECK(!cataShipIsAttachedComponent(&module));
        module.clampInfo=&clamp;host.shiptype=Carrier;
        CHECK(!cataShipIsAttachedComponent(&module));
    }
    printf("Cross-race and mission station docking: %d checks passed\n",checks);
    return 0;
}
