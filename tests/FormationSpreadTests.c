/* Exercise the actual live command implementation using game/save structures.
   Only the allocator and canonical layout generator are deterministic seams. */
#include "CommandLayer.h"
#include "CommandDefs.h"
#include "FormationSpreadPolicy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define memAlloc(size,name,flags) malloc(size)
#define memFree(pointer) free(pointer)
#include "../src/Game/FormationSpread.inc"

static int checks, generations;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static vector canonical[8];
void FormationCalculateOffsets(CommandToDo *command)
{
    int i;
    ++generations;
    for(i=0;i<command->selection->numShips;++i)
        command->selection->ShipPtr[i]->formationOffset=canonical[i];
    formationSpreadApply(command);
}

static Ship ships[4];
static ShipStaticInfo stats[4];
static CommandToDo command;
static SelectCommand *selection;
static CommandLayer layer;

static void reset(void)
{
    int i;
    memset(&command,0,sizeof(command));memset(ships,0,sizeof(ships));
    memset(stats,0,sizeof(stats));memset(canonical,0,sizeof(canonical));
    command.selection=selection;selection->numShips=3;
    command.ordertype.attributes=COMMAND_MASK_FORMATION;
    command.ordertype.order=COMMAND_NULL;
    command.formation.formationtype=WALL_FORMATION;
    command.formation.sortorder=1;
    command.move.destination.x=12345;
    for(i=0;i<4;++i)
    {
        StaticCollInfo *info=&stats[i].staticheader.staticCollInfo;
        ships[i].staticinfo=&stats[i];ships[i].formationcommand=&command;
        ships[i].posinfo.position.x=700+i;ships[i].shipidle=TRUE;
        selection->ShipPtr[i]=&ships[i];
        info->uplength=40;info->rightlength=80;info->forwardlength=200;
        info->collrectoffset.x=-20;info->collrectoffset.y=-40;info->collrectoffset.z=-100;
        info->approxcollspheresize=100;info->collspheresize=200;
    }
    canonical[1].y=400;canonical[2].y=-400;
    FormationCalculateOffsets(&command);
}

static void policy(void)
{
    unsigned tick,q,repeat;
    float previous=0;
    CHECK(formationSpreadTicks(0)==1000);CHECK(formationSpreadTicks(1)==1000);
    CHECK(formationSpreadTicks(UINT32_C(0xa0000000))==1000);
    CHECK(formationSpreadTicks(UINT32_C(0xafffff01))==1000);
    for(tick=0;tick<=2000;++tick)
    {
        uint32_t word=formationSpreadPack(1,tick,0.2173f);
        float scale=formationSpreadStoredScale(word);
        CHECK((word&FORMATION_SORT_MASK)==1);CHECK(formationSpreadTicks(word)==tick);
        CHECK(scale>=previous);previous=scale;
        CHECK(tick!=1000 || word==1);
        CHECK(tick!=2000 || scale==2);
    }
    for(q=26;q<=511;++q)
    {
        uint32_t word=formationSpreadPack(1,500,(float)q/511);
        uint32_t original=word;
        for(repeat=0;repeat<500;++repeat)
            word=formationSpreadPack(word,500,formationSpreadTight(word));
        CHECK(word==original);
    }
    CHECK(formationSpreadStoredScale(formationSpreadPack(1,0,NAN))==1);
    CHECK(formationSpreadStoredScale(formationSpreadPack(1,2000,0.2f))==2);
}

static void live(void)
{
    int i,oldgenerations;
    vector positions[3];
    CommandOrder order;
    reset();order=command.ordertype;
    for(i=0;i<3;++i)positions[i]=ships[i].posinfo.position;
    formationSetSpreadForSelection(&layer,selection,0);
    CHECK(ships[1].formationOffset.y>=81.6f && ships[1].formationOffset.y<83);
    CHECK(ships[2].formationOffset.y==-ships[1].formationOffset.y);
    CHECK(!ships[1].shipidle && !ships[2].shipidle);
    CHECK(ships[0].shipidle); /* never move/teleport the leader */
    CHECK(memcmp(&order,&command.ordertype,sizeof(order))==0);
    CHECK(command.move.destination.x==12345);
    for(i=0;i<3;++i)CHECK(memcmp(&positions[i],&ships[i].posinfo.position,sizeof(vector))==0);
    formationSetSpreadForSelection(&layer,selection,2000);
    CHECK(ships[1].formationOffset.y==800 && ships[2].formationOffset.y==-800);
    for(i=0;i<1000;++i)
    {
        formationSetSpreadForSelection(&layer,selection,(unsigned)(i%1000));
        formationSetSpreadForSelection(&layer,selection,1000);
        CHECK(memcmp(&ships[1].formationOffset,&canonical[1],sizeof(vector))==0);
        CHECK(command.formation.sortorder==1);
    }
    /* Slider immediately changes a halted group as well as an idle one. */
    command.ordertype.order=COMMAND_HALT;
    formationSetSpreadForSelection(&layer,selection,2000);
    CHECK(ships[1].formationOffset.y==800);CHECK(command.ordertype.order==COMMAND_HALT);
    /* Layout reflows are canonical, not cumulative. Simulate load/tactics. */
    for(i=0;i<100;++i)FormationCalculateOffsets(&command);
    CHECK(ships[1].formationOffset.y==800);
    { CommandToDo saved=command; command=saved; }
    CHECK(formationGetSpread(&command)==2000);
    oldgenerations=generations;
    formationSetSpreadForSelection(&layer,selection,999999);
    CHECK(generations==oldgenerations && formationGetSpread(&command)==2000);
    ships[1].formationcommand=NULL;
    formationSetSpreadForSelection(&layer,selection,0);
    CHECK(generations==oldgenerations && formationGetSpread(&command)==2000);
    ships[1].formationcommand=&command;ships[2].flags|=SOF_Dead;
    formationSetSpreadForSelection(&layer,selection,0);
    CHECK(generations==oldgenerations);
}

static void customAndMixed(void)
{
    int i;
    float tight;
    reset();command.formation.formationtype=CUSTOM_FORMATION;
    /* Offset origin may be uninitialized on an ordinary leader. */
    ships[0].formationOffset.y=NAN;
    formationSetSpreadForSelection(&layer,selection,0);
    tight=ships[1].formationOffset.y;CHECK(tight>81.5f && tight<83);
    for(i=0;i<1000;++i)
    {
        formationSetSpreadForSelection(&layer,selection,2000);
        CHECK(fabsf(ships[1].formationOffset.y-800)<0.1f);
        formationSetSpreadForSelection(&layer,selection,0);
        CHECK(fabsf(ships[1].formationOffset.y-tight)<0.02f);
    }
    formationSetSpreadForSelection(&layer,selection,1000);
    CHECK(fabsf(ships[1].formationOffset.y-400)<0.1f);
    reset();
    /* A large capital hull must remain clear of both smaller neighbors. */
    stats[1].staticheader.staticCollInfo.rightlength=400;
    stats[1].staticheader.staticCollInfo.collrectoffset.y=-200;
    formationSetSpreadForSelection(&layer,selection,0);
    CHECK(ships[1].formationOffset.y>=244.8f && ships[1].formationOffset.y<246);
    reset();
    /* Sphere spacing uses conservative orientation-independent extents. */
    command.formation.formationtype=SPHERE_FORMATION;
    formationSetSpreadForSelection(&layer,selection,0);
    CHECK(ships[1].formationOffset.y>=204 && ships[1].formationOffset.y<206);
    reset();
    command.formation.formationtype=SPHERE_FORMATION;
    command.ordertype.attributes|=COMMAND_MASK_PROTECTING;
    command.protect=(ProtectCommand *)calloc(1,sizeofProtectCommand(1));
    command.protect->numShips=1;command.protect->ShipPtr[0]=&ships[3];
    canonical[0].z=400;
    formationSetSpreadForSelection(&layer,selection,2000);
    CHECK(ships[0].formationOffset.z==800);
    free(command.protect);
}

int main(void)
{
    selection=(SelectCommand *)calloc(1,sizeofSelectCommand(4));
    policy();live();customAndMixed();free(selection);
    printf("PASS: live formation spread, stationary orders, hull clearance, saved state, custom shape and drift (%d checks)\n",checks);
    return 0;
}
