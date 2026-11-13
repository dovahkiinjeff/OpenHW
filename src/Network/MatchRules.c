#include "MatchRules.h"
#include "Globals.h"
#include "Universe.h"
#include "TitanInterfaceC.h"
#include "mainswitches.h"
#include "MultiplayerGame.h"
#include "utility.h"
#include "Randy.h"
#include "AIPlayer.h"
#include "Dock.h"
#include "UnivUpdate.h"
#include "MissionStartLaunchPolicy.h"
#include <string.h>
static OHLoad match;
static int active;
int hwMatchInstall(const OHLoad *load)
{
    static const unsigned minimum[OH_RULE_COUNT]={0,0,0,1,0,0,0,0,0,0,0,0,0,0};
    static const unsigned maximum[OH_RULE_COUNT]={1,1,1,4,1,2,100,1,1,1,1,1,1,0};
    unsigned i;
    if(!load || load->reserved || !load->humans || load->humans>load->players ||
       load->players>OH_MAX_PLAYERS || load->ownSlot>=load->humans ||
       load->config.reserved || load->config.startingFleet>2 || load->config.startingResources>2 ||
       load->config.bounty>3 || (load->config.gameFlags & ~0x7eeeu) ||
       load->config.injectionInterval>86400 || load->config.injectionAmount>1000000 ||
       load->config.lumpTime>86400 || load->config.lumpAmount>1000000 ||
       !memchr(load->config.map,0,sizeof(load->config.map)))return 0;
    for(i=0;i<OH_RULE_COUNT;i++)
        if(load->config.rules[i]<minimum[i] || load->config.rules[i]>maximum[i])return 0;
    for(i=0;i<OH_MAX_PLAYERS;i++) {
        const OHSlot *slot=&load->config.slots[i];
        if(i>=load->players) { if(slot->kind)return 0;continue; }
        if(slot->kind!=(i<load->humans?1:2) || slot->race>1 || slot->difficulty>2 || slot->team>8 ||
           slot->ready>1 || slot->reserved[0] || slot->reserved[1] || slot->reserved[2] ||
           !memchr(slot->name,0,sizeof(slot->name)))return 0;
    }
    match=*load;active=1;return 1;
}
void hwMatchClear(void){active=0;memset(&match,0,sizeof(match));}
int hwMatchActive(void){return active;}
unsigned hwMatchOption(unsigned rule,unsigned localValue)
{return active && rule<OH_RULE_COUNT ? match.config.rules[rule] : localValue;}
int hwMatchPlayerOption(unsigned rule,unsigned localValue,unsigned player)
{
    if(active)return player<match.players && hwMatchOption(rule,0)!=0;
    return singlePlayerGame && player==(unsigned)universe.curPlayerIndex && localValue!=0;
}
const OHLoad *hwMatchConfiguration(void){return active?&match:NULL;}
void hwMatchPrepareGame(void)
{
    unsigned i;
    if(!active)return;
    memset(&tpGameCreated,0,sizeof(tpGameCreated));
    tpGameCreated.numPlayers=match.humans;tpGameCreated.numComputers=match.players-match.humans;
    tpGameCreated.startingFleet=match.config.startingFleet;
    tpGameCreated.startingResources=match.config.startingResources;
    tpGameCreated.bountySize=match.config.bounty;
    tpGameCreated.flag=(uword)match.config.gameFlags;
    tpGameCreated.resourceInjectionInterval=match.config.injectionInterval;
    tpGameCreated.resourceInjectionsAmount=match.config.injectionAmount;
    tpGameCreated.resourceLumpSumTime=match.config.lumpTime;
    tpGameCreated.resourceLumpSumAmount=match.config.lumpAmount;
    tpGameCreated.aiplayerDifficultyLevel=50;
    strncpy(tpGameCreated.MapName,match.config.map,sizeof(tpGameCreated.MapName)-1);
    for(i=0;i<match.players;i++) {
        const OHSlot *slot=&match.config.slots[i];
        tpGameCreated.playerInfo[i].race=slot->race?R2:R1;
        tpGameCreated.playerInfo[i].colorsPicked=TRUE;
        tpGameCreated.playerInfo[i].baseColor=slot->base;
        tpGameCreated.playerInfo[i].stripeColor=slot->stripe;
        strncpy(tpGameCreated.playerInfo[i].PersonalName,slot->name,sizeof(tpGameCreated.playerInfo[i].PersonalName)-1);
        ComputerPlayerEnabled[i]=slot->kind==2;
    }
    sigsPlayerIndex=match.ownSlot;sigsNumPlayers=match.humans;numPlayers=match.players;
    whichRaceSelected=match.config.slots[match.ownSlot].race?R2:R1;
    singlePlayerGame=FALSE;multiPlayerGame=TRUE;
    determCompPlayer=TRUE;
}
void hwMatchApplyTeams(void)
{
    unsigned i,j;
    if(!active)return;
    for(i=0;i<match.players;i++) {
        universe.players[i].Allies=0;
        for(j=0;j<match.players;j++) {
            if(i!=j && match.config.slots[i].team && match.config.slots[i].team==match.config.slots[j].team)
                universe.players[i].Allies|=(uword)(1u<<j);
        }
        for(j=1;j<match.players;j++) {
            unsigned enemy=(i+j)%match.players;
            if(!(universe.players[i].Allies&(1u<<enemy))) {universe.aiplayerEnemy[i]=(sbyte)enemy;break;}
        }
    }
}

void hwMatchSeedRandom(void)
{
    unsigned stream;
    if(!active)return;
    for(stream=0;stream<RANDOM_NUMBER_STREAMS;stream++)
        ranNumberSet((sdword)stream,match.seed ^ (0x9e3779b9u*(stream+1)));
}

/* Initial holding uses ordinary instantaneous docking commands, so the same
   launch/docking state is created by every simulation, including the server. */
void hwMatchInitialDocking(void)
{
    Node *node;unsigned mode;
    if(!active || !(mode=match.config.rules[OH_MISSION_START_LAUNCH]))return;
    for(node=universe.ShipList.head;node;node=node->next) {
        Ship *ship=(Ship*)listGetStructOfNode(node),*home=ship->playerowner->PlayerMothership;
        int combat=ship->staticinfo->shipclass==CLASS_Fighter || ship->staticinfo->shipclass==CLASS_Corvette;
        int utility=ship->shiptype==SalCapCorvette || ship->shiptype==RepairCorvette;
        if(combat && home && home!=ship && home->shipsInsideMe && missionStartHoldShip(mode,combat,utility)) {
            SelectCommand selection;selection.numShips=1;selection.ShipPtr[0]=ship;
            ship->specialFlags|=SPECIAL_STAY_TILL_EXPLICITLAUNCH;
            clDock(&universe.mainCommandLayer,&selection,DOCK_INSTANTANEOUSLY|DOCK_STAY_TILL_EXPLICITLAUNCH,home);
        }
    }
}

static void signatureBytes(uint64_t *hash,const void *data,size_t size)
{
    const unsigned char *p=(const unsigned char*)data;
    while(size--){*hash^=*p++;*hash*=UINT64_C(1099511628211);}
}

/* A load barrier check, never a hash of pointers, textures, local camera or
   options. It covers the initial world and loaded ship gameplay parameters.
   The normal per-frame synchronization checks continue after this barrier. */
uint64_t hwMatchWorldSignature(void)
{
    uint64_t hash=UINT64_C(14695981039346656037);unsigned i,race,type;Node *node;
#define HASH(value) signatureBytes(&hash,&(value),sizeof(value))
    HASH(universe.numPlayers);HASH(universe.radius);
    for(i=0;i<universe.numPlayers;i++) {
        Player *p=&universe.players[i];HASH(p->race);HASH(p->resourceUnits);HASH(p->Allies);
        HASH(p->totalships);HASH(p->researchinfo.HasTechnology);
    }
    for(node=universe.ShipList.head;node;node=node->next) {
        Ship *ship=(Ship*)listGetStructOfNode(node);
        HASH(ship->shipID.shipNumber);HASH(ship->shiptype);HASH(ship->shiprace);
        HASH(ship->playerowner->playerIndex);HASH(ship->health);HASH(ship->fuel);
        HASH(ship->posinfo.position);HASH(ship->rotinfo.coordsys);
    }
    for(node=universe.ResourceList.head;node;node=node->next) {
        Resource *resource=(Resource*)listGetStructOfNode(node);
        HASH(resource->objtype);HASH(resource->resourceID.resourceNumber);
        HASH(resource->posinfo.position);HASH(resource->resourcevalue);
    }
    for(race=R1;race<=R2;race++)for(type=0;type<TOTAL_NUM_SHIPS;type++) {
        ShipStaticInfo *s=GetShipStaticInfoSafe((ShipType)type,(ShipRace)race);
        if(!s)continue;
        HASH(type);HASH(race);HASH(s->maxhealth);HASH(s->staticheader.maxvelocity);
        HASH(s->thruststrengthstat);HASH(s->rotstrengthstat);HASH(s->turnspeedstat);
        HASH(s->buildTime);HASH(s->buildCost);HASH(s->maxfuel);HASH(s->fuelburnrate);
        HASH(s->resourcesAtOneTime);HASH(s->maxresources);HASH(s->harvestRate);HASH(s->harvestAmount);
        HASH(s->staticheader.staticCollInfo.collspheresize);
        if(s->gunStaticInfo)for(i=0;i<(unsigned)s->gunStaticInfo->numGuns;i++) {
            GunStatic *g=&s->gunStaticInfo->gunstatics[i];
            HASH(g->guntype);HASH(g->position);HASH(g->gunDamageLo);HASH(g->gunDamageHi);
            HASH(g->firetime);HASH(g->bulletspeed);HASH(g->bulletrange);
        }
        if(s->dockStaticInfo)for(i=0;i<(unsigned)s->dockStaticInfo->numDockPoints;i++) {
            DockStaticPoint *point=&s->dockStaticInfo->dockstaticpoints[i];
            HASH(point->type);HASH(point->position);HASH(point->conenormal);
        }
    }
#undef HASH
    return hash;
}
