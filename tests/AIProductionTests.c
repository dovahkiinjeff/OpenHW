/* Execute the production factory scheduler and research planner without a game
   window, renderer, installation or server. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "AIPolicy.h"
#include "AIFleetMan.h"
#include "ConsMgr.h"
#include "CommandDefs.h"
#include "ResearchGUI.h"
#include "StatScript.h"
#include "Memory.h"
#include "MultiplayerGame.h"

Universe universe;
CaptainGameInfo tpGameCreated;
bool32 singlePlayerGame, rmGUIActive;
AIPlayer *aiCurrentAIPlayer;
real32 ATTMAN_BUILDPRIORITY_RATIO=.45f, DEFMAN_BUILDPRIORITY_RATIO=.1f, RESMAN_BUILDPRIORITY_RATIO=.45f;
uword RacesAllowedForGivenShip[TOTAL_NUM_SHIPS];
static Ship factories[4];
static ShipStaticInfo factoryInfo[4], ships[TOTAL_NUM_SHIPS];
static Player player;
static AIPlayer ai;
static TechStatics tech;
static Ship *builtAt[32];
static int builds;

#if MEM_USE_NAMES
void *memAllocFunction(sdword size, char *name, udword flags) { return calloc(1,size); }
#else
void *memAllocFunction(sdword size, udword flags) { return calloc(1,size); }
#endif
void memFree(void *p) { free(p); }
void rmUpdateTechList(void) { assert(!"No GUI expected"); }
ShipRace cmFactoryBuildRace(const Ship *ship) { return ship->shiprace; }
bool32 cmCanBuildShipType(Ship *ship, ShipType type, bool32 research)
{
    return type != HeavyCruiser || ship->shiptype == Mothership;
}
ShipStaticInfo *GetShipStaticInfo(ShipType type, ShipRace race) { return &ships[type]; }
void clWrapBuildShip(CommandLayer *layer, ShipType type, ShipRace race, uword index, Ship *creator)
{
    /* Deliberately defer command application, as the network packet path does. */
    assert(builds < 32); builtAt[builds++]=creator;
}

#include "../src/Game/ResearchPlanning.inc"
bool32 aiuCanBuildShipType(ShipType type, bool32 scripted) { return TRUE; }
udword aiuUnitCapCanBuildShip(AIPlayer *owner, ShipType type, sdword count) { return count; }
bool32 rmCanBuildShip(Player *owner, ShipType type) { return TRUE; }
#include "../src/Game/AIProduction.inc"

static void setup(void)
{
    int i;
    memset(&universe,0,sizeof(universe));memset(&player,0,sizeof(player));
    memset(&ai,0,sizeof(ai));memset(factories,0,sizeof(factories));
    memset(&tech,0,sizeof(tech));builds=0;singlePlayerGame=FALSE;
    player.aiPlayer=&ai;player.race=R1;player.resourceUnits=10000;
    player.PlayerMothership=&factories[0];ai.player=&player;aiCurrentAIPlayer=&ai;
    player.researchinfo.techstat=&tech;player.researchinfo.CanDoResearch=TRUE;
    for(i=0;i<4;++i) {
        factories[i].staticinfo=&factoryInfo[i];factories[i].playerowner=&player;
        factories[i].shiprace=R1;factories[i].shiptype=i?Carrier:Mothership;
        factoryInfo[i].canBuildShips=TRUE;
        listAddNode(&universe.ShipList,&factories[i].shiplink,&factories[i]);
    }
    for(i=0;i<TOTAL_NUM_SHIPS;++i) {
        ships[i].shiptype=(ShipType)i;ships[i].shiprace=R1;ships[i].buildCost=100;
        ships[i].buildTime=100;ships[i].staticheader.infoFlags=IF_InfoLoaded;
        RacesAllowedForGivenShip[i]=RaceToRaceBits(R1);
    }
    for(i=0;i<NumTechnologies;++i)tech.TimeToComplete[i]=100;
}

int main(void)
{
    RequestShips request;CommandToDo queued;char canonical[128];int i;
    setup();
    assert(aiCanonicalTweakName("AIPLAYER_UPDATE_RATE[AI_BEG]",canonical,sizeof(canonical)));
    assert(!strcmp(canonical,"AIPLAYER_UPDATE_RATE[AI_BEGINNER]"));
    assert(aiCanonicalTweakName("AIA_ATTACK_FLEET_FAST_PROB[AI_ADV]",canonical,sizeof(canonical)));
    assert(!strcmp(canonical,"AIA_ATTACK_FLEET_FAST_PROB[AI_ADVANCED]"));
    assert(!aiCanonicalTweakName("AIA_ATTACK_FLEET_FAST_PROB[AI_UNKNOWN]",canonical,sizeof(canonical)));
    assert(aiBaseDifficulty(AI_EXPERT)==AI_ADVANCED);
    assert(aiCarrierTarget(0,4000)==1 && aiCarrierTarget(12000,4000)==2);
    assert(aiCarrierTarget(120000,4000)==11); /* No single-carrier ceiling. */
    assert(aiProductionValue(&player,100)==100);
    ai.aiplayerDifficultyLevel=AI_EXPERT;
    assert(aiProductionValue(&player,100)==100 && aiProductionValue(&player,0)==0);
    assert(aiPreferredShip(&player,LightInterceptor)==LightInterceptor);
    assert(aiPreferredShip(&player,HeavyDefender)==HeavyDefender);
    assert(aiPreferredShip(&player,MinelayerCorvette)==MinelayerCorvette);
    assert(aiPreferredShip(&player,MultiGunCorvette)==MultiGunCorvette);
    assert(aiPreferredShip(&player,StandardFrigate)==StandardFrigate);
    singlePlayerGame=TRUE;assert(aiProductionValue(&player,100)==100);singlePlayerGame=FALSE;

    /* All eligible factories participate even before emitted packets execute. */
    memset(&request,0,sizeof(request));request.shiptype=AttackBomber;request.num_ships=8;
    aifBuildRequestedShips(NULL,&request,NULL,NULL);
    assert(builds==8 && request.num_ships==0 && ai.NumRUsSpentOnAttman==800);
    for(i=0;i<8;++i)assert(builtAt[i]==&factories[i%4]);
    assert(aifChooseFactory(&player,HeavyCruiser,R1)==&factories[0]);
    factories[1].flags=SOF_Dead;factories[2].flags=SOF_Hyperspace;factories[3].playerowner=NULL;
    memset(&queued,0,sizeof(queued));queued.ordertype.order=COMMAND_BUILDING_SHIP;
    queued.buildingship.creator=&factories[0];queued.buildingship.shipType=AttackBomber;
    queued.buildingship.frameAtWhichToCreate=100;
    listAddNode(&universe.mainCommandLayer.todolist,&queued.todonode,&queued);
    assert(aifChooseFactory(&player,AttackBomber,R1)==&factories[0]);
    factories[1].flags=0;assert(aifChooseFactory(&player,AttackBomber,R1)==&factories[1]);
    player.resourceUnits=0;request.num_ships=1;builds=0;
    aifBuildRequestedShips(NULL,&request,NULL,NULL);assert(!builds && request.num_ships==1);

    /* Two of six labs develop a prerequisite chain. Normal requests keep the
       other four; completed anticipation releases all capacity. */
    setup();ai.aiplayerDifficultyLevel=AI_EXPERT;
    tech.TechNeededToBuildShip[StandardDestroyer]=TechToBit(HeavyGuns);
    tech.TechNeededToResearch[HeavyGuns]=TechToBit(ConstructionTech);
    tech.TechNeededToResearch[ConstructionTech]=TechToBit(Chassis3);
    tech.TechNeededToBuildShip[IonCannonFrigate]=TechToBit(MissileWeapons);
    tech.TechNeededToResearch[MissileWeapons]=TechToBit(Chassis3);
    tech.TechNeededToBuildShip[AttackBomber]=TechToBit(PlasmaWeapons);
    for(i=0;i<NUM_RESEARCHLABS;++i)player.researchinfo.researchlabs[i].labstatus=LS_NORESEARCHITEM;
    rmUpdateExpertResearch(&player);
    for(i=0;i<2;++i)assert(player.researchinfo.researchlabs[i].topic->techresearch==Chassis3);
    assert(player.researchinfo.researchlabs[0].topic->timeleft==100.0f);
    assert(rmResearchTechForShip(&player,AttackBomber));
    for(i=2;i<6;++i)assert(player.researchinfo.researchlabs[i].topic->techresearch==PlasmaWeapons);
    rmUpdateExpertResearch(&player);assert(player.researchinfo.researchlabs[0].topic->numlabsresearching==2);
    for(i=0;i<6;++i)rmClearResearchlab(&player,i);
    player.researchinfo.HasTechnology=TechToBit(Chassis3)|TechToBit(ConstructionTech)|TechToBit(HeavyGuns)|TechToBit(MissileWeapons);
    assert(rmResearchTechForShip(&player,AttackBomber));
    for(i=0;i<6;++i)assert(player.researchinfo.researchlabs[i].topic->techresearch==PlasmaWeapons);
    for(i=0;i<6;++i)rmClearResearchlab(&player,i);
    puts("AI configuration, factory scheduling, equal costs and adaptive lab allocation passed");return 0;
}
