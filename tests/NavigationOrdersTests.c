/* Execute the production navigation state machine and packet validator with a
   recording command layer. No game window, graphics device or player files. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/Game/NavigationOrders.c"
#include "../src/Game/NavigationFlight.h"
#include "../src/Network/CommandValidation.c"

Universe universe;
MaxSelection selSelected;
bool32 multiPlayerGame,universePause,playPackets,recordFakeSendPackets;
udword opPauseOrders;
real32 smUniverseSizeX=100000,smUniverseSizeY=100000,smUniverseSizeZ=100000;
static Ship fleet[8];static ShipStaticInfo info[8];
static CommandToDo commands[8];static SelectCommand singles[8];
static MilitaryParadeCommand testParade;
static SelectCommand *forcedFormation;
static unsigned moves,attacks,harvests,specials,networkSends,parades;
static unsigned char saved[100000];static size_t savedSize,savedOffset;

#if MEM_USE_NAMES
void *memAllocFunction(sdword n,char *name,udword flags){void *p=calloc(1,n);assert(p);return p;}
#else
void *memAllocFunction(sdword n,udword flags){void *p=calloc(1,n);assert(p);return p;}
#endif
void memFree(void *p){free(p);}
void navPlacementCancel(void){}
void navPlacementObjectDied(TargetPtr p){}
bool32 shipPilotControls(const Ship *ship){return FALSE;}
unsigned hwMatchOption(unsigned rule,unsigned localValue){return localValue;}
sdword selShipInSelection(ShipPtr *list,sdword n,ShipPtr ship){sdword i;for(i=0;i<n;i++)if(list[i]==ship)return TRUE;return FALSE;}
bool32 cmCanBuildShipType(Ship *ship,ShipType type,bool32 research){return ship->staticinfo->canBuildShips && type==LightInterceptor;}
ShipRace cmFactoryBuildRace(const Ship *ship){return ship->shiprace;}
void MakeShipsMobile(SelectCommand *ships){}
void makeShipsControllable(SelectCommand *ships,sdword command){}
void makeShipsDockCapable(SelectCommand *ships){}
sdword MakeShipsAttackCapable(SelectCommand *to,SelectCommand *from){memcpy(to,from,sizeofSelectCommand(from->numShips));return to->numShips;}
sdword MakeShipsHarvestCapable(SelectCommand *to,SelectCommand *from){return MakeShipsAttackCapable(to,from);}
CommandToDo *getShipAndItsCommand(CommandLayer *layer,Ship *ship){return ship->command;}
CommandToDo *GetMilitaryGroupAroundShip(CommandLayer *layer,Ship *ship){return NULL;}
void clMove(CommandLayer *layer,SelectCommand *ships,vector from,vector to)
{
    sdword i;moves++;navCancelShips(ships);
    if(forcedFormation && ships->numShips==forcedFormation->numShips) {
        CommandToDo *c=&commands[1];c->ordertype.order=COMMAND_MOVE;c->ordertype.attributes=COMMAND_MASK_FORMATION;
        c->move.destination=to;c->selection=forcedFormation;
        for(i=0;i<ships->numShips;i++)ships->ShipPtr[i]->command=c;
        return;
    }
    for(i=0;i<ships->numShips;i++) {
        Ship *ship=ships->ShipPtr[i];CommandToDo *c=&commands[ship->shipID.shipNumber];
        c->ordertype.order=COMMAND_MOVE;c->move.destination=to;c->selection=&singles[ship->shipID.shipNumber];
        c->selection->numShips=1;c->selection->ShipPtr[0]=ship;ship->command=c;
    }
}
void clHalt(CommandLayer *layer,SelectCommand *ships){sdword i;navCancelShips(ships);for(i=0;i<ships->numShips;i++)ships->ShipPtr[i]->command=NULL;}
void clAttack(CommandLayer *layer,SelectCommand *ships,AttackCommand *target){attacks++;navCancelShips(ships);}
void clSpecial(CommandLayer *layer,SelectCommand *ships,SpecialCommand *target){specials++;navCancelShips(ships);}
void clCollectResource(CommandLayer *layer,SelectCommand *ships,Resource *target){harvests++;navCancelShips(ships);}
sdword clDock(CommandLayer *layer,SelectCommand *ships,DockType type,Ship *target){navCancelShips(ships);return 1;}
CommandToDo *clProtect(CommandLayer *layer,SelectCommand *ships,ProtectCommand *target){navCancelShips(ships);return NULL;}
CommandToDo *CreateMilitaryGroupAroundShipWithType(CommandLayer *layer,Ship *ship,Ship *producer,sdword type)
{
    CommandToDo *c=&commands[ship->shipID.shipNumber];parades++;
    memset(&testParade,0,sizeof(testParade));testParade.aroundShip=producer;testParade.paradeType=type;
    c->ordertype.order=COMMAND_MILITARY_PARADE;c->militaryParade=&testParade;c->selection=&singles[ship->shipID.shipNumber];
    c->selection->numShips=1;c->selection->ShipPtr[0]=ship;c->todonode.structptr=c;
    layer->todolist.head=layer->todolist.tail=&c->todonode;ship->command=c;return c;
}
void AddShipToMilitaryGroup(Ship *ship,CommandToDo *group){ship->command=group;}
void clSendNavigation(SelectCommand *ships,unsigned mode,const vector *points,unsigned count,unsigned action,TargetPtr target){networkSends++;}
Ship *ShipIDtoShip(ShipID id,bool32 inside){return id.shipNumber<8?&fleet[id.shipNumber]:NULL;}
void SetTargetID(TargetID *id,TargetPtr target){id->objtype=target->objtype;id->objNumber=((Ship *)target)->shipID.shipNumber;}
TargetPtr navTargetFromID(TargetID id){return id.objtype==OBJ_ShipType && id.objNumber<8?(TargetPtr)&fleet[id.objNumber]:NULL;}
sdword SpaceObjRegistryGetID(SpaceObj *obj){return ((Ship *)obj)->shipID.shipNumber;}
Ship *SpaceObjRegistryGetShip(sdword id){return id>=0 && id<8?&fleet[id]:NULL;}
TargetPtr SpaceObjRegistryGetTarget(sdword id){return (TargetPtr)SpaceObjRegistryGetShip(id);}
void SaveInfoNumber(sdword n){assert(savedSize+sizeof(n)<sizeof(saved));memcpy(saved+savedSize,&n,sizeof(n));savedSize+=sizeof(n);}
sdword LoadInfoNumber(void){sdword n;assert(savedOffset+sizeof(n)<=savedSize);memcpy(&n,saved+savedOffset,sizeof(n));savedOffset+=sizeof(n);return n;}
bool32 LoadInfoNumberOptional(sdword *n){if(savedOffset==savedSize)return FALSE;*n=LoadInfoNumber();return TRUE;}
void SaveStructureOfSize(void *p,sdword n){assert(savedSize+n<sizeof(saved));memcpy(saved+savedSize,p,n);savedSize+=n;}
void LoadStructureOfSizeToAddress(void *p,sdword n){assert(savedOffset+n<=savedSize);memcpy(p,saved+savedOffset,n);savedOffset+=n;}

static vector testPoint(float x,float y,float z){vector v={x,y,z};return v;}
static void setup(void)
{
    unsigned i;navReset();memset(&universe,0,sizeof(universe));memset(fleet,0,sizeof(fleet));memset(commands,0,sizeof(commands));
    universe.numPlayers=2;universe.curPlayerPtr=&universe.players[0];universe.phystimeelapsed=0.05f;
    for(i=0;i<8;i++) {
        memset(&info[i],0,sizeof(info[i]));fleet[i].staticinfo=&info[i];fleet[i].shipID.shipNumber=i;
        fleet[i].objtype=OBJ_ShipType;fleet[i].playerowner=&universe.players[0];fleet[i].shiprace=R1;
    }
    universe.players[1].playerIndex=1;info[0].canBuildShips=TRUE;fleet[0].shiptype=Mothership;
    moves=attacks=harvests=specials=parades=networkSends=0;savedSize=savedOffset=0;forcedFormation=NULL;
    multiPlayerGame=universePause=playPackets=recordFakeSendPackets=FALSE;
}
static void route_tests(void)
{
    SelectCommand ships;vector points[2]={testPoint(1000,0,0),testPoint(1000,1000,500)};udword hash;
    setup();ships.numShips=1;ships.ShipPtr[0]=&fleet[1];fleet[7].playerowner=&universe.players[1];
    navSetRoute(&universe.mainCommandLayer,&ships,points,2,NAV_ACTION_ATTACK,(TargetPtr)&fleet[7]);
    assert(routes && moves==1 && routes->index==0);hash=navStateHash();
    /* A stopped/blocked ship retries the same leg; it cannot skip the obstacle. */
    fleet[1].command=NULL;navTick(&universe.mainCommandLayer);assert(routes->index==0 && moves==2);
    fleet[1].posinfo.position=points[0];navTick(&universe.mainCommandLayer);
    assert(routes && routes->index==1 && moves==3 && navStateHash()!=hash && attacks==0);
    fleet[1].posinfo.position=points[1];navTick(&universe.mainCommandLayer);assert(!routes && attacks==1);
    navSetRoute(&universe.mainCommandLayer,&ships,points,1,NAV_ACTION_ATTACK,(TargetPtr)&fleet[7]);
    fleet[7].playerowner=&universe.players[0];fleet[1].posinfo.position=points[0];navTick(&universe.mainCommandLayer);
    assert(attacks==1 && !routes); /* target captured during the route */
    navSetRoute(&universe.mainCommandLayer,&ships,points,2,0,NULL);clHalt(&universe.mainCommandLayer,&ships);assert(!routes);
    points[0].x=NAN;navSetRoute(&universe.mainCommandLayer,&ships,points,2,0,NULL);assert(!routes);
}
static void split_and_formation_tests(void)
{
    struct {sdword numShips;Ship *ships[2];} selection={2,{&fleet[1],&fleet[2]}};
    SelectCommand one;vector points[2]={testPoint(1000,0,0),testPoint(3000,0,0)};
    setup();navSetRoute(&universe.mainCommandLayer,(SelectCommand *)&selection,points,2,0,NULL);
    one.numShips=1;one.ShipPtr[0]=&fleet[1];navCancelShips(&one);
    assert(routes->ships->numShips==1 && routes->ships->ShipPtr[0]==&fleet[2]);
    navObjectDied((TargetPtr)&fleet[2]);assert(!routes);
    navSetRoute(&universe.mainCommandLayer,(SelectCommand *)&selection,points,2,0,NULL);
    commands[1].selection=(SelectCommand *)&selection;commands[1].ordertype.attributes=COMMAND_MASK_FORMATION;
    fleet[2].command=&commands[1];fleet[1].posinfo.position=points[0];fleet[2].posinfo.position=testPoint(1000,8000,0);
    navTick(&universe.mainCommandLayer);assert(routes->index==1); /* follower uses leader arrival */
}
static void fly_through_and_loop_tests(void)
{
    SelectCommand one;vector points[2]={testPoint(1000,0,0),testPoint(1000,2000,500)};
    struct {sdword numShips;Ship *ships[2];} mixed={2,{&fleet[1],&fleet[2]}};
    unsigned lap;udword hash;real32 distances[]={4000,1000,100,10};
    setup();one.numShips=1;one.ShipPtr[0]=&fleet[1];
    navSetRoute(&universe.mainCommandLayer,&one,points,2,NAV_ACTION_NONE,NULL);
    assert(navMoveFlightFlags(&fleet[1])&AISHIP_PassThroughWaypoint);
    fleet[1].posinfo.position=points[0];navTick(&universe.mainCommandLayer);
    assert(!(navMoveFlightFlags(&fleet[1])&AISHIP_PassThroughWaypoint)); /* final stop */
    for(lap=0;lap<4;lap++) {
        vector desired=testPoint(distances[lap],0,0);
        navFlightThroughVelocity(&desired,385.0f);assert(fabsf(desired.x-385.0f)<0.001f);
    }
    assert(navFlightOrientBeforeMoving(AISHIP_FirstPointInDirectionFlying,385.0f*385.0f));
    assert(!navFlightOrientBeforeMoving(AISHIP_FirstPointInDirectionFlying|AISHIP_NavigationSteering,385.0f*385.0f));
    assert(navFlightOrientBeforeMoving(AISHIP_FirstPointInDirectionFlying|AISHIP_NavigationSteering,0));
    navSetRoute(&universe.mainCommandLayer,&one,points,2,NAV_ACTION_LOOP,NULL);
    for(lap=0;lap<5;lap++) {
        fleet[1].posinfo.position=points[0];navTick(&universe.mainCommandLayer);assert(routes && routes->index==1);
        assert(navMoveFlightFlags(&fleet[1])&AISHIP_PassThroughWaypoint);
        fleet[1].posinfo.position=points[1];navTick(&universe.mainCommandLayer);assert(routes && routes->index==0);
    }
    hash=navStateHash();navSave();navReset();navLoad();
    assert(routes && routes->action==NAV_ACTION_LOOP && navStateHash()==hash && (navMoveFlightFlags(&fleet[1])&AISHIP_PassThroughWaypoint));
    navCancelShips(&one);assert(!routes && !navMoveFlightFlags(&fleet[1]));
    setup();navSetRoute(&universe.mainCommandLayer,(SelectCommand *)&mixed,points,2,NAV_ACTION_NONE,NULL);
    fleet[1].posinfo.position=points[0];navTick(&universe.mainCommandLayer);
    assert(routeByShip[1]!=routeByShip[2] && routeByShip[1]->index==1 && routeByShip[2]->index==0);
    setup();forcedFormation=(SelectCommand *)&mixed;
    navSetRoute(&universe.mainCommandLayer,(SelectCommand *)&mixed,points,2,NAV_ACTION_NONE,NULL);
    assert(routes->ships->numShips==2 && routeByShip[1]==routeByShip[2]);
    fleet[1].posinfo.position=points[0];fleet[2].posinfo.position=testPoint(1000,5000,0);
    navTick(&universe.mainCommandLayer);assert(routes->index==1 && routes->ships->numShips==2);
    /* A previous-build save may contain one unformed multi-ship route. */
    setup();navSetRoute(&universe.mainCommandLayer,(SelectCommand *)&mixed,points,2,NAV_ACTION_NONE,NULL);
    {
        NavRoute *second=routes->next;
        memFree(routes->ships);routes->ships=memAlloc(sizeofSelectCommand(2),"legacy route fixture",0);
        memcpy(routes->ships,&mixed,sizeofSelectCommand(2));routes->next=second->next;
        memFree(second->ships);memFree(second);navSave();navReset();navLoad();
        assert(routeByShip[1]!=routeByShip[2] && routes->ships->numShips==1 && routes->next->ships->numShips==1);
    }
}
static void rally_tests(void)
{
    SelectCommand producer;vector p=testPoint(5000,2000,1000);Ship *anchor;unsigned before;
    setup();producer.numShips=1;producer.ShipPtr[0]=&fleet[0];
    navSetRally(&producer,NAV_RALLY_POINT,p,NULL);navShipBuilt(&fleet[1],&fleet[0]);
    assert(pending && !parades); /* held inside: no premature movement */
    assert(navShipLaunched(&universe.mainCommandLayer,&fleet[1],&fleet[0]));
    anchor=navParadeLeader(&testParade);
    assert(anchor!=&fleet[0] && anchor->posinfo.position.x==5000 && anchor->shiptype==Mothership);
    assert(testParade.paradeType==0 && !moves && !attacks && !harvests);
    assert(!navShipLaunched(&universe.mainCommandLayer,&fleet[1],&fleet[0])); /* repair return */
    navSetRally(&producer,NAV_RALLY_OBJECT,p,(TargetPtr)&fleet[3]);fleet[3].posinfo.position=testPoint(7000,8000,9000);
    anchor=navParadeLeader(&testParade);assert(anchor->posinfo.position.x==7000);
    navObjectDied((TargetPtr)&fleet[3]);anchor=navParadeLeader(&testParade);assert(anchor->posinfo.position.y==8000);
    navSetRally(&producer,NAV_NONE,testPoint(0,0,0),NULL);assert(navParadeLeader(&testParade)->posinfo.position.x==7000);
    navShipBuilt(&fleet[2],&fleet[0]);assert(!navShipLaunched(&universe.mainCommandLayer,&fleet[2],&fleet[0]));
    before=parades;navSetRally(&producer,NAV_RALLY_POINT,p,NULL);
    fleet[4].shiptype=ResearchShip;navShipBuilt(&fleet[4],&fleet[0]);assert(!navShipLaunched(&universe.mainCommandLayer,&fleet[4],&fleet[0]) && before==parades);
    navSetRally(&producer,NAV_RALLY_OBJECT,p,(TargetPtr)&fleet[3]);
    fleet[3].posinfo.position=testPoint(9000,9000,9000);navTick(&universe.mainCommandLayer);
    fleet[3].playerowner=&universe.players[1];fleet[3].posinfo.position=testPoint(12000,12000,12000);
    navTick(&universe.mainCommandLayer);assert(!rallies->target && navParadeLeader(&testParade)->posinfo.position.x==9000);
    /* Clearing the producer or losing it must not leave dangling tickets. */
    navShipBuilt(&fleet[2],&fleet[0]);navObjectDied((TargetPtr)&fleet[0]);assert(!rallies && !pending);
}
static void save_and_dispatch_tests(void)
{
    SelectCommand one,producer;vector points[2]={testPoint(1000,0,0),testPoint(2000,0,500)};udword hash;
    setup();one.numShips=producer.numShips=1;one.ShipPtr[0]=&fleet[1];producer.ShipPtr[0]=&fleet[0];
    navSetRoute(&universe.mainCommandLayer,&one,points,2,0,NULL);
    fleet[1].posinfo.position=points[0];navTick(&universe.mainCommandLayer);
    navSetRally(&producer,NAV_RALLY_POINT,points[1],NULL);navShipBuilt(&fleet[2],&fleet[0]);
    hash=navStateHash();navSave();navReset();navLoad();
    assert(savedOffset==savedSize && routes->index==1 && pending && rallies && navStateHash()==hash);
    navShipLaunched(&universe.mainCommandLayer,&fleet[2],&fleet[0]);
    savedSize=savedOffset=0;hash=navStateHash();navSave();navReset();navLoad();
    assert(navStateHash()==hash && rallies->parade==&testParade && navIsVirtualParade(&testParade));
    navReset();savedSize=savedOffset=0;navLoad();assert(!routes && !rallies && !pending); /* old save */
    multiPlayerGame=TRUE;navIssue(&one,NAV_WAYPOINTS,points,2,0,NULL);assert(networkSends==1 && !routes);
    universePause=TRUE;navIssue(&one,NAV_WAYPOINTS,points,2,0,NULL);assert(networkSends==1);
    opPauseOrders=TRUE;navIssue(&one,NAV_WAYPOINTS,points,2,0,NULL);assert(networkSends==2);
}
static void packet_tests(void)
{
    unsigned char bytes[4096]={0};HWPacketHeader *header=(HWPacketHeader *)bytes;
    HWCommandHeader *command=(HWCommandHeader *)(bytes+sizeof(*header));
    NetNavigationCommand *body=(NetNavigationCommand *)((unsigned char *)command+sizeof(*command));
    size_t size=sizeof(*header)+sizeof(*command)+offsetof(NetNavigationCommand,selection)+2+sizeof(ShipID);
    setup();header->type=PACKETTYPE_COMMAND;header->numberOfCommands=1;command->commandType=COMMANDTYPE_NAVIGATION;
    body->mode=NAV_WAYPOINTS;body->count=1;body->points[0]=testPoint(1000,0,0);body->target.objtype=65535;
    body->selection.numShips=1;body->selection.ShipID[0]=fleet[1].shipID;
    assert(hwValidatePlayerCommand(bytes,size,0));assert(!hwValidatePlayerCommand(bytes,size-1,0));
    body->count=65;assert(!hwValidatePlayerCommand(bytes,size,0));body->count=1;
    body->points[0].x=NAN;assert(!hwValidatePlayerCommand(bytes,size,0));body->points[0].x=1000;
    fleet[1].playerowner=&universe.players[1];assert(!hwValidatePlayerCommand(bytes,size,0));fleet[1].playerowner=&universe.players[0];
    body->mode=NAV_RALLY_POINT;assert(!hwValidatePlayerCommand(bytes,size,0)); /* cannot set a fighter's production rally */
    body->selection.ShipID[0]=fleet[0].shipID;assert(hwValidatePlayerCommand(bytes,size,0));
    body->mode=NAV_RALLY_OBJECT;body->target.objtype=OBJ_ShipType;body->target.objNumber=3;
    assert(hwValidatePlayerCommand(bytes,size,0));fleet[3].playerowner=&universe.players[1];assert(!hwValidatePlayerCommand(bytes,size,0));
    body->reserved=1;assert(!hwValidatePlayerCommand(bytes,size,0));
    body->reserved=0;body->mode=NAV_WAYPOINTS;body->target.objtype=65535;
    body->action=NAV_ACTION_LOOP;body->count=2;
    assert(hwValidatePlayerCommand(bytes,size,0));
    body->count=1;assert(!hwValidatePlayerCommand(bytes,size,0));body->count=2;
    body->target.objtype=OBJ_ShipType;body->target.objNumber=0;
    assert(!hwValidatePlayerCommand(bytes,size,0));body->target.objtype=65535;
    body->selection.numShips=2;body->selection.ShipID[1]=body->selection.ShipID[0];
    assert(!hwValidatePlayerCommand(bytes,size+sizeof(ShipID),0)); /* duplicate ship IDs */
}
int main(void)
{
    route_tests();split_and_formation_tests();fly_through_and_loop_tests();rally_tests();save_and_dispatch_tests();packet_tests();navReset();
    puts("Navigation: routes, blocked legs, splits, formations, virtual rally anchors, launch lifecycle, saves and authenticated packets passed.");return 0;
}
