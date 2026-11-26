#include "Universe.h"
#include "CommandDefs.h"
#include "ShipSelect.h"
#include "AIVar.h"
#include "AITeam.h"
#include "Dock.h"
#include "Volume.h"
#include "Timer.h"
#include "../src/Modern/CataclysmScriptIds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
Universe universe;
uint32_t cataKasScope;
static char variableKey[32],timerKey[32];static AIVar variable;static int checks,destroyed;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
AIVar *aivarFind(char *key){snprintf(variableKey,sizeof(variableKey),"%s",key);return &variable;}
void aivarDestroy(AIVar *value){CHECK(value==&variable);++destroyed;}
void timTimerDestroy(char *key){snprintf(timerKey,sizeof(timerKey),"%s",key);}
void growSelectInit(GrowSelection *selection){memset(selection,0,sizeof(*selection));selection->selection=calloc(1,4096);}
void growSelectClose(GrowSelection *selection){free(selection->selection);}
void growSelectReset(GrowSelection *selection){selection->selection->numShips=0;}
void growSelectAddShip(GrowSelection *selection,Ship *ship){selection->selection->ShipPtr[selection->selection->numShips++]=ship;}
void growSelectAddShipNoDuplication(GrowSelection *selection,Ship *ship){int i;for(i=0;i<selection->selection->numShips;i++)if(selection->selection->ShipPtr[i]==ship)return;growSelectAddShip(selection,ship);}
bool32 AnyOfTheseShipsAreInSelection(SelectCommand *a,SelectCommand *b){int i,j;for(i=0;i<a->numShips;i++)for(j=0;j<b->numShips;j++)if(a->ShipPtr[i]==b->ShipPtr[j])return TRUE;return FALSE;}
sdword volPointInside(Volume *volume,vector *point){CHECK(volume!=NULL);return point->x<100;}
sdword cataShipOriginalID(const Ship *ship){return ship->shiptype==P2Swarmer?9:0;}
bool32 cataDisguiseIsAsteroid(const Ship *ship){return (ship->specialFlags2&SPECIAL_2_CataDisguising)!=0;}
ShipRace GetValidRaceForShipType(ShipType type)
{
    if(type==CataWorker)return R1;
    CHECK(type>=P1_FIRST_SHIP && type<=P1_LAST_SHIP);
    return P1;
}
#include "../src/Game/CataclysmMissionSupport.inc"
#include "../src/Game/CataclysmComponentIdentity.inc"

static ShipsInsideMe hangar;
static InsideShip stowedModules[2];
static unsigned stowedCount, restoredCount;

void dockEnsureShipsInside(Ship *host)
{
    CHECK(host->shipsInsideMe == &hangar);
}

void RemoveShipFromCommand(Ship *ship)
{
    CHECK(cataShipIsAttachedComponent(ship));
}

void univRemoveShipFromOutside(Ship *ship)
{
    CHECK(universe.ShipList.head == &ship->shiplink);
    universe.ShipList.head = ship->shiplink.next;
    ship->shiplink.next = NULL;
    ship->flags |= SOF_Hide;
}

void dockPutShipInside(Ship *ship, Ship *host)
{
    CHECK((ship->flags & SOF_Hide) && stowedCount < 2);
    InsideShip *entry = &stowedModules[stowedCount++];
    entry->ship = ship;
    entry->node.structptr = entry;
    host->shipsInsideMe->insideList.tail->next = &entry->node;
    host->shipsInsideMe->insideList.tail = &entry->node;
}

void dockRemoveShipFromInside(Ship *ship, Ship *host)
{
    Node *passenger = host->shipsInsideMe->insideList.head;
    Node *module = passenger->next;
    CHECK(((InsideShip *)listGetStructOfNode(module))->ship == ship);
    passenger->next = module->next;
    module->next = NULL;
}

static void PutShipOutsideAt(Ship *ship, vector *position, vector *velocity, real32 angle)
{
    const Ship *host = cataShipComponentHost(ship);
    CHECK(position == &host->posinfo.position && velocity == &host->posinfo.velocity);
    CHECK(ship == stowedModules[restoredCount++].ship && angle == 0.0f);
    ship->flags &= ~SOF_Hide;
}

#include "../src/Game/CataclysmHyperspaceModules.inc"

static void testCarriedModules(void)
{
    Ship host = {0}, lowerDecks = {0}, hangarModule = {0}, dockedWorker = {0}, destroyedModule = {0};
    ClampInfo lowerDeckMount = {0}, hangarMount = {0}, destroyedMount = {0};
    InsideShip passenger = {0};
    host.objtype = lowerDecks.objtype = hangarModule.objtype = destroyedModule.objtype = OBJ_ShipType;
    host.shiptype = CataMothership;
    lowerDecks.shiptype = CataMothershipLowerSpine;
    hangarModule.shiptype = CataMothershipDockingBay;
    destroyedModule.shiptype = CataMothershipWeapons;
    destroyedModule.flags = SOF_Dead;
    host.shipsInsideMe = &hangar;
    lowerDeckMount.host = hangarMount.host = destroyedMount.host = (SpaceObjRotImpTarg *)&host;
    lowerDecks.clampInfo = &lowerDeckMount;
    hangarModule.clampInfo = &hangarMount;
    destroyedModule.clampInfo = &destroyedMount;
    lowerDecks.shipID.shipNumber = 158;
    hangarModule.shipID.shipNumber = 165;
    hangarModule.health = 250;
    lowerDecks.shiplink.structptr = &lowerDecks;
    hangarModule.shiplink.structptr = &hangarModule;
    destroyedModule.shiplink.structptr = &destroyedModule;
    lowerDecks.shiplink.next = &hangarModule.shiplink;
    hangarModule.shiplink.next = &destroyedModule.shiplink;
    universe.ShipList.head = &lowerDecks.shiplink;
    passenger.ship = &dockedWorker;
    passenger.node.structptr = &passenger;
    hangar.insideList.head = hangar.insideList.tail = &passenger.node;

    CHECK(spCanQueueScriptedHyperspace(&host, FALSE));
    CHECK(!spCanQueueScriptedHyperspace(&lowerDecks, FALSE));
    CHECK(!spCanQueueScriptedHyperspace(&hangarModule, FALSE));
    spStowCataclysmModules(&host);
    CHECK(stowedCount == 2 && universe.ShipList.head == &destroyedModule.shiplink);
    CHECK(passenger.node.next == &stowedModules[0].node);
    CHECK(!spCanQueueScriptedHyperspace(&lowerDecks, TRUE));
    CHECK(!spCanQueueScriptedHyperspace(&hangarModule, TRUE));
    spStowCataclysmModules(&host);
    spRestoreCataclysmModules(&host);
    spRestoreCataclysmModules(&host);
    CHECK(restoredCount == 2 && passenger.node.next == NULL);
    CHECK(lowerDecks.clampInfo == &lowerDeckMount && hangarModule.clampInfo == &hangarMount);
    CHECK(lowerDecks.shipID.shipNumber == 158 && hangarModule.shipID.shipNumber == 165);
    CHECK(hangarModule.health == 250 && passenger.ship == &dockedWorker);
    /* Earlier saves can have a module in the independent hyperspace list. */
    lowerDecks.flags |= SOF_Hyperspace;
    CHECK(spCanQueueScriptedHyperspace(&lowerDecks, TRUE));
    CHECK(!spCanQueueScriptedHyperspace(&lowerDecks, FALSE));
    universe.ShipList.head = NULL;
}

int main(void){
    AITeam team={0};AITeamMove move={0};
    Ship target={0},attacker={0},other={0};CommandToDo first={0},second={0};GrowSelection targets,result,actors,attack;
    growSelectInit(&targets);growSelectInit(&result);growSelectInit(&actors);growSelectInit(&attack);
    {
        Ship player={0},scripted={0},enemy={0},swarmer={0};Player firstPlayer={0},secondPlayer={0};
        secondPlayer.playerIndex=1;player.playerowner=scripted.playerowner=swarmer.playerowner=&firstPlayer;
        enemy.playerowner=&secondPlayer;scripted.specialFlags2=SPECIAL_2_CataScriptControlled;swarmer.shiptype=P2Swarmer;
        growSelectAddShip(&actors,&player);growSelectAddShip(&actors,&scripted);growSelectAddShip(&actors,&enemy);growSelectAddShip(&actors,&swarmer);
        CHECK(cataScriptSelectPlayerCombatShips(&result,&actors)==1&&result.selection->ShipPtr[0]==&player);
        CHECK(cataScriptSelectPlayerCombatShips(&actors,&actors)==1&&actors.selection->ShipPtr[0]==&player);
        CHECK(cataScriptSelectPlayerCombatShips(&result,NULL)==0);CHECK(cataScriptSelectPlayerCombatShips(NULL,&actors)==0);
        growSelectReset(&actors);
    }
    {
        Asteroid asteroid={0};Ship ordinary={0};
        asteroid.objtype=OBJ_AsteroidType;asteroid.attributes=0x200;asteroid.objlink.structptr=&asteroid;
        ordinary.objtype=OBJ_ShipType;ordinary.attributes=0x200;ordinary.objlink.structptr=&ordinary;
        asteroid.objlink.next=&ordinary.objlink;universe.SpaceObjList.head=&asteroid.objlink;
        CHECK(cataScriptFindMarkedAsteroids(&result)==1&&result.selection->ShipPtr[0]==(Ship*)&asteroid);
        asteroid.attributes=0;CHECK(cataScriptFindMarkedAsteroids(&result)==0);
        ordinary.specialFlags2=SPECIAL_2_CataDisguising;
        CHECK(cataScriptFindMarkedAsteroids(&result)==1&&result.selection->ShipPtr[0]==&ordinary);
        universe.SpaceObjList.head=NULL;
        growSelectAddShip(&result,&ordinary);ordinary.specialFlags2=SPECIAL_2_CircularGuard;
        CHECK(!cataScriptFirstShipDocked(&result));ordinary.flags=SOF_Hide;CHECK(cataScriptFirstShipDocked(&result));
        ordinary.flags|=SOF_Hyperspace;CHECK(!cataScriptFirstShipDocked(&result));
        growSelectReset(&result);CHECK(!cataScriptFirstShipDocked(&result));CHECK(!cataScriptFirstShipDocked(NULL));
    }
    CHECK(!cataScriptTeamIdle(NULL));CHECK(cataScriptTeamIdle(&team));team.curMove=&move;
    move.type=MOVE_DONE;CHECK(cataScriptTeamIdle(&team));
    move.type=MOVE_GETSHIPS;CHECK(cataScriptTeamIdle(&team));
    move.type=MOVE_FANCYGETSHIPS;CHECK(cataScriptTeamIdle(&team));
    move.type=MOVE_SWARMATTACK;CHECK(!cataScriptTeamIdle(&team));
    move.type=MOVE_ATTACK;CHECK(!cataScriptTeamIdle(&team));
    growSelectAddShip(&targets,&target);growSelectAddShip(&actors,&attacker);growSelectAddShip(&attack,&target);
    first.ordertype.order=COMMAND_ATTACK;first.selection=actors.selection;first.attack=(AttackCommand*)attack.selection;
    second=first;second.ordertype.order=COMMAND_MOVE;second.ordertype.attributes=COMMAND_MASK_PASSIVE_ATTACKING;
    first.todonode.next=&second.todonode;first.todonode.structptr=&first;second.todonode.structptr=&second;
    universe.mainCommandLayer.todolist.head=&first.todonode;
    attacker.posinfo.position.x=1000000; // Cata must not apply HW1's 10km cutoff.
    CHECK(cataScriptFindAttackers(&targets,&result)==1);CHECK(result.selection->ShipPtr[0]==&attacker);
    CHECK(cataScriptFindAttackers(&targets,&targets)==1);CHECK(targets.selection->ShipPtr[0]==&attacker);
    CHECK(cataScriptFindAttackers(NULL,&result)==0);CHECK(cataScriptFindAttackers(&targets,NULL)==0);
    target.command=&first;other.command=&second;
    growSelectReset(&targets);growSelectAddShip(&targets,&target);growSelectAddShip(&targets,&other);
    first.ordertype.attributes=COMMAND_MASK_ATTACKING_AND_MOVING;
    CHECK(cataScriptOrderAttributes(&targets)==(COMMAND_MASK_PASSIVE_ATTACKING|COMMAND_MASK_ATTACKING_AND_MOVING));
    CHECK(cataScriptOrderAttributes(NULL)==0);
    growSelectAddShip(&targets,&attacker);
    CHECK(cataScriptSelectMembers(&result,&targets,&actors)==1);CHECK(result.selection->ShipPtr[0]==&attacker);
    CHECK(cataScriptSelectMembers(&actors,&targets,&actors)==1);CHECK(actors.selection->ShipPtr[0]==&attacker);
    first.ordertype.attributes=0;second.ordertype.attributes=COMMAND_MASK_PASSIVE_ATTACKING;
    CHECK(cataScriptOrderTargets(&actors,&result)==1);CHECK(result.selection->ShipPtr[0]==&target);
    CHECK(cataScriptOrderTargets(&actors,&actors)==1);CHECK(actors.selection->ShipPtr[0]==&target);
    CHECK(cataScriptOrderTargets(NULL,&result)==0);
    {
        ShipsInsideMe hangar={0};InsideShip firstInside={0},secondInside={0};
        target.shipsInsideMe=&hangar;firstInside.ship=&attacker;secondInside.ship=&other;
        firstInside.node.structptr=&firstInside;secondInside.node.structptr=&secondInside;
        firstInside.node.next=&secondInside.node;hangar.insideList.head=&firstInside.node;
        CHECK(cataScriptSelectInside(&actors,&actors)==2);
        CHECK(actors.selection->ShipPtr[0]==&attacker&&actors.selection->ShipPtr[1]==&other);
        CHECK(cataScriptSelectInside(&result,NULL)==0);target.shipsInsideMe=NULL;
    }
    cataKasScope=0x1234;cataScriptVariableDestroy(0x55);CHECK(!strcmp(variableKey,"C00001234_00000055"));CHECK(destroyed==1);
    cataScriptVariableDestroy(0x80000055);CHECK(!strcmp(variableKey,"C00000000_00000055"));
    cataScriptTimerDestroy(0x66);CHECK(!strcmp(timerKey,"C00001234_00000066"));
    cataScriptTimerDestroy(0x80000066);CHECK(!strcmp(timerKey,"C00000000_00000066"));
    {
        ClampInfo firstClamp={0},secondClamp={0};Volume volume={0};
        firstClamp.host=secondClamp.host=(SpaceObjRotImpTarg*)&target;
        attacker.shiptype=CataWorker;other.shiptype=SalCapCorvette;attacker.clampInfo=&firstClamp;other.clampInfo=&secondClamp;
        growSelectReset(&targets);growSelectAddShip(&targets,&target);
        growSelectReset(&actors);growSelectAddShip(&actors,&attacker);growSelectAddShip(&actors,&other);
        CHECK(cataScriptSelectSalvagers(&result,&actors,&targets)==2);
        CHECK(cataScriptSelectSalvagers(&actors,&actors,&targets)==2);
        secondClamp.host=(SpaceObjRotImpTarg*)&attacker;
        CHECK(cataScriptSelectSalvagers(&targets,&actors,&targets)==1&&targets.selection->ShipPtr[0]==&attacker);
        target.shiptype=CataSlipgateLarge;other.shiptype=CataSlipgateLarge;attacker.shiptype=CataWorker;
        target.shiplink.structptr=&target;other.shiplink.structptr=&other;attacker.shiplink.structptr=&attacker;
        target.shiplink.next=&other.shiplink;other.shiplink.next=&attacker.shiplink;universe.ShipList.head=&target.shiplink;
        target.posinfo.position.x=0;other.posinfo.position.x=200;
        CHECK(cataScriptSelectGates(&volume,&result)==1&&result.selection->ShipPtr[0]==&target);
        target.flags=SOF_Hide;CHECK(cataScriptSelectGates(&volume,&result)==0);
        CHECK(cataScriptSelectGates(NULL,&result)==0);
        CHECK(cataScriptSelectGates(&volume,NULL)==0);
    }
    /* The Mission 2 script uses the R1-backed slipgate to launch Turanic ships. */
    CHECK(cataScriptReinforcementRace(P1Fighter,"p1Fighter",R1)==P1);
    CHECK(cataScriptReinforcementRace(P1MissileCorvette,"p1MissileCorvette",R1)==P1);
    CHECK(cataScriptReinforcementRace(P1StandardCorvette,"p1StandardCorvette",R1)==P1);
    CHECK(cataScriptReinforcementRace(CataWorker,"sWorker",R1)==R1);
    CHECK(cataScriptReinforcementRace(HeavyInterceptor,"kHeavyInterceptor",R2)==R1);
    CHECK(cataScriptReinforcementRace(HeavyInterceptor,"tHeavyInterceptor",R1)==R2);
    testCarriedModules();
    growSelectClose(&targets);growSelectClose(&result);growSelectClose(&actors);growSelectClose(&attack);
    printf("Cataclysm mission support: %d checks passed.\n",checks);return 0;
}
