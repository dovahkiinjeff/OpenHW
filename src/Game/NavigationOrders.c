/* Shared deterministic waypoint/production-rally state. No fake universe objects
   and no layout changes to the legacy raw ship/command save structures. */
#include "NavigationOrders.h"
#include "AIShip.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "CommandNetwork.h"
#include "CommandDefs.h"
#include "ConsMgr.h"
#include "Memory.h"
#include "SaveGame.h"
#include "Select.h"
#include "ShipSelect.h"
#include "ShipPilot.h"
#include "MultiplayerGame.h"
#include "Options.h"
#include "Sensors.h"
#include "../Network/MatchRules.h"
#include <math.h>
#include <string.h>

typedef struct NavRoute {
    struct NavRoute *next;
    SelectCommand *ships;
    unsigned count, index, action, owner;
    vector points[NAV_MAX_POINTS];
    TargetPtr target;
    real32 retryAt;
} NavRoute;
typedef struct NavRally {
    struct NavRally *next;
    Ship *producer;
    unsigned owner, mode;
    TargetPtr target;
    vector point;
    Ship anchor; /* Pose adapter only: never registered, drawn or collided. */
    MilitaryParadeCommand *parade;
} NavRally;
typedef struct NavPending {
    struct NavPending *next;
    Ship *ship, *producer;
    unsigned owner;
} NavPending;
static NavRoute *routes;
static NavRoute *routeByShip[65536];
static NavRally *rallies;
static NavPending *pending;
static unsigned dispatching;

bool32 navValidPoint(vector p)
{
    return isfinite(p.x) && isfinite(p.y) && isfinite(p.z) &&
        fabsf(p.x)<=max(smUniverseSizeX,1.0f) &&
        fabsf(p.y)<=max(smUniverseSizeY,1.0f) &&
        fabsf(p.z)<=max(smUniverseSizeZ,1.0f);
}
bool32 navCanProduce(const Ship *ship)
{
    unsigned i;
    if(!ship || (ship->flags&SOF_Dead) || !ship->staticinfo->canBuildShips)return FALSE;
    for(i=0;i<TOTAL_NUM_SHIPS;i++)
        if(cmCanBuildShipType((Ship *)ship,(ShipType)i,FALSE))return TRUE;
    return FALSE;
}
static NavRally *rallyFor(Ship *producer)
{
    NavRally *r;for(r=rallies;r;r=r->next)if(r->producer==producer)return r;
    return NULL;
}
bool32 navIsVirtualParade(const MilitaryParadeCommand *parade)
{
    NavRally *r;for(r=rallies;r;r=r->next)if(parade && r->parade==parade)return TRUE;
    return FALSE;
}
Ship *navParadeLeader(MilitaryParadeCommand *parade)
{
    NavRally *r;
    for(r=rallies;r;r=r->next)if(r->parade==parade) {
        if(r->target) {
            r->anchor.posinfo.velocity=r->target->posinfo.velocity;
        } else vecZeroVector(r->anchor.posinfo.velocity);
        r->anchor.posinfo.position=r->target?r->target->posinfo.position:r->point;
        return &r->anchor;
    }
    return parade->aroundShip;
}
void navParadeFreed(MilitaryParadeCommand *parade)
{
    NavRally *r;for(r=rallies;r;r=r->next)if(r->parade==parade)r->parade=NULL;
}
void navCancelShips(SelectCommand *ships)
{
    NavRoute **link=&routes;NavPending **p=&pending;sdword i;
    if(dispatching || !ships)return;
    while(*link) {
        NavRoute *r=*link;
        for(i=r->ships->numShips-1;i>=0;i--)
            if(selShipInSelection(ships->ShipPtr,ships->numShips,r->ships->ShipPtr[i])) {
                routeByShip[r->ships->ShipPtr[i]->shipID.shipNumber]=NULL;
                memmove(&r->ships->ShipPtr[i],&r->ships->ShipPtr[i+1],
                    (r->ships->numShips-i-1)*sizeof(Ship *));r->ships->numShips--;
            }
        if(!r->ships->numShips){*link=r->next;memFree(r->ships);memFree(r);}
        else link=&r->next;
    }
    while(*p) {
        NavPending *entry=*p;
        if(selShipInSelection(ships->ShipPtr,ships->numShips,entry->ship)) {
            *p=entry->next;memFree(entry);
        } else p=&entry->next;
    }
}
udword navMoveFlightFlags(const Ship *ship)
{
    NavRoute *r=routeByShip[ship->shipID.shipNumber];
    if(!r)return 0;
    return AISHIP_NavigationSteering|((r->index+1<r->count || r->action==NAV_ACTION_LOOP)?AISHIP_PassThroughWaypoint:0);
}
static NavRoute *partitionRoute(NavRoute *source,CommandLayer *layer)
{
    SelectCommand *ships=source->ships;NavRoute *first=NULL,*last=NULL;unsigned i,j;
    unsigned char assigned[NAV_MAX_SHIPS]={0};MaxSelection cohort;
    CommandToDo *command=getShipAndItsCommand(layer,ships->ShipPtr[0]);
    bool32 together=command && (command->ordertype.attributes&COMMAND_MASK_FORMATION);
    for(i=1;i<(unsigned)ships->numShips && together;i++)
        if(getShipAndItsCommand(layer,ships->ShipPtr[i])!=command)together=FALSE;
    if(ships->numShips==1 || together) {
        for(i=0;i<(unsigned)ships->numShips;i++)routeByShip[ships->ShipPtr[i]->shipID.shipNumber]=source;
        return source;
    }
    for(i=0;i<(unsigned)ships->numShips;i++)if(!assigned[i]) {
        NavRoute *r;bool32 formation;
        command=getShipAndItsCommand(layer,ships->ShipPtr[i]);
        formation=command && (command->ordertype.attributes&COMMAND_MASK_FORMATION);cohort.numShips=0;
        for(j=i;j<(unsigned)ships->numShips;j++)if(!assigned[j] &&
            (j==i || (formation && getShipAndItsCommand(layer,ships->ShipPtr[j])==command))) {
            cohort.ShipPtr[cohort.numShips++]=ships->ShipPtr[j];assigned[j]=1;
        }
        r=memAlloc(sizeof(*r),"navigation cohort",0);*r=*source;r->next=NULL;
        r->ships=memAlloc(sizeofSelectCommand(cohort.numShips),"cohort selection",0);
        memcpy(r->ships,&cohort,sizeofSelectCommand(cohort.numShips));
        for(j=0;j<(unsigned)cohort.numShips;j++)routeByShip[cohort.ShipPtr[j]->shipID.shipNumber]=r;
        if(last)last->next=r;else first=r;last=r;
    }
    memFree(source->ships);memFree(source);return first;
}
void navSetRoute(CommandLayer *layer,SelectCommand *ships,const vector *points,
                 unsigned count,unsigned action,TargetPtr target)
{
    NavRoute *source,*r,*first,*last,*previousRoutes;unsigned i;
    if(!ships || ships->numShips<1 || ships->numShips>NAV_MAX_SHIPS ||
       !count || count>NAV_MAX_POINTS || action>NAV_ACTION_LOOP ||
       (action==NAV_ACTION_LOOP && (count<2 || target)))return;
    for(i=0;i<count;i++)if(!navValidPoint(points[i]))return;
    navCancelShips(ships);MakeShipsMobile(ships);if(!ships->numShips)return;
    dispatching++;clMove(layer,ships,ships->ShipPtr[0]->posinfo.position,points[0]);dispatching--;
    if(!ships->numShips)return;
    source=memAlloc(sizeof(*source),"navigation route",0);memset(source,0,sizeof(*source));
    source->ships=memAlloc(sizeofSelectCommand(ships->numShips),"route selection",0);
    memcpy(source->ships,ships,sizeofSelectCommand(ships->numShips));
    memcpy(source->points,points,count*sizeof(vector));source->count=count;source->action=action;
    source->target=target;source->owner=ships->ShipPtr[0]->playerowner->playerIndex;
    /* Real formations advance together; unformed ships have independent cursors. */
    first=partitionRoute(source,layer);last=first;while(last->next)last=last->next;
    previousRoutes=routes;last->next=routes;routes=first;
    if(first!=last)for(r=first;r!=previousRoutes;r=r->next) {
        dispatching++;clMove(layer,r->ships,r->ships->ShipPtr[0]->posinfo.position,points[0]);dispatching--;
    }
}
void navSetRally(SelectCommand *ships,unsigned mode,vector point,TargetPtr target)
{
    sdword i;
    if(mode!=NAV_NONE && mode!=NAV_RALLY_POINT && mode!=NAV_RALLY_OBJECT)return;
    if(mode!=NAV_NONE && !navValidPoint(point))return;
    if(mode==NAV_RALLY_OBJECT && !target)return;
    for(i=0;i<ships->numShips;i++) {
        Ship *producer=ships->ShipPtr[i];NavRally *r;
        if(!navCanProduce(producer))continue;
        r=rallyFor(producer);
        if(!r) {
            if(mode==NAV_NONE)continue;
            r=memAlloc(sizeof(*r),"producer rally",0);memset(r,0,sizeof(*r));
            r->producer=producer;r->owner=producer->playerowner->playerIndex;
            r->anchor=*producer;r->anchor.shiptype=Mothership;
            r->anchor.shiprace=cmFactoryBuildRace(producer)==R2?R2:R1;
            r->next=rallies;rallies=r;
        }
        if(mode==NAV_NONE) {
            if(r->target)r->point=r->target->posinfo.position;
            r->mode=NAV_NONE;r->target=NULL;continue;
        }
        r->owner=producer->playerowner->playerIndex;
        r->anchor.rotinfo.coordsys=producer->rotinfo.coordsys;
        r->mode=mode;r->point=point;r->target=mode==NAV_RALLY_OBJECT?target:NULL;
        vecZeroVector(r->anchor.posinfo.velocity);
    }
}
void navShipBuilt(Ship *ship,Ship *producer)
{
    NavPending *p=memAlloc(sizeof(*p),"pending rally departure",0);
    p->ship=ship;p->producer=producer;p->owner=ship->playerowner->playerIndex;
    p->next=pending;pending=p;
}
bool32 navShipLaunched(CommandLayer *layer,Ship *ship,Ship *producer)
{
    NavPending **link=&pending;NavRally *r;NavPending *p;
    while(*link && (*link)->ship!=ship)link=&(*link)->next;
    if(!*link)return FALSE;
    p=*link;*link=p->next;
    if(p->producer!=producer || p->owner!=ship->playerowner->playerIndex) {
        memFree(p);return FALSE;
    }
    memFree(p);r=rallyFor(producer);
    if(!r || r->mode==NAV_NONE || r->owner!=ship->playerowner->playerIndex ||
       ship->shiptype==ResearchShip)return FALSE;
    dispatching++;
    if(r->parade) {
        CommandToDo *group;
        /* Virtual parades are deliberately excluded from the normal lookup. */
        Node *node;group=NULL;
        for(node=layer->todolist.head;node;node=node->next) {
            CommandToDo *c=listGetStructOfNode(node);
            if(c->ordertype.order==COMMAND_MILITARY_PARADE && c->militaryParade==r->parade){group=c;break;}
        }
        if(group)AddShipToMilitaryGroup(ship,group);else r->parade=NULL;
    }
    if(!r->parade) {
        CommandToDo *group=CreateMilitaryGroupAroundShipWithType(layer,ship,producer,r->anchor.shiprace==R2?1:0);
        r->parade=group->militaryParade;
    }
    dispatching--;return TRUE;
}
static void terminalOrder(CommandLayer *layer,NavRoute *r)
{
    SelectCommand one;AttackCommand target;MaxSelection capable;
    if(!r->target || (r->target->flags&SOF_Dead))return;
    target.numTargets=1;target.TargetPtr[0]=r->target;
    switch(r->action) {
    case NAV_ACTION_ATTACK:
        if(r->target->objtype==OBJ_ShipType &&
           (((Ship *)r->target)->playerowner==&universe.players[r->owner] ||
            (universe.players[r->owner].Allies&(1u<<((Ship *)r->target)->playerowner->playerIndex))))break;
        MakeShipsAttackCapable((SelectCommand *)&capable,r->ships);
        if(capable.numShips)clAttack(layer,(SelectCommand *)&capable,&target);break;
    case NAV_ACTION_GUARD:
        if(r->target->objtype==OBJ_ShipType && ((Ship *)r->target)->playerowner==&universe.players[r->owner]) {
            one.numShips=1;one.ShipPtr[0]=(Ship *)r->target;clProtect(layer,r->ships,&one);
        }break;
    case NAV_ACTION_SPECIAL:clSpecial(layer,r->ships,&target);break;
    case NAV_ACTION_HARVEST:
        if(r->target->objtype>=OBJ_AsteroidType && r->target->objtype<=OBJ_DustType) {
            MakeShipsHarvestCapable((SelectCommand *)&capable,r->ships);
            if(capable.numShips)clCollectResource(layer,(SelectCommand *)&capable,(Resource *)r->target);
        }break;
    case NAV_ACTION_DOCK:
        if(r->target->objtype==OBJ_ShipType && ((Ship *)r->target)->playerowner==&universe.players[r->owner]) {
            makeShipsDockCapable(r->ships);if(r->ships->numShips)clDock(layer,r->ships,DOCK_AT_SPECIFIC_SHIP,(Ship *)r->target);
        }break;
    }
}
void navTick(CommandLayer *layer)
{
    NavRoute **link=&routes;NavRally *rally;
    for(rally=rallies;rally;rally=rally->next) {
        if(rally->target) {
            if((rally->target->flags&SOF_Dead) ||
               (rally->target->objtype==OBJ_ShipType && ((Ship *)rally->target)->playerowner->playerIndex!=rally->owner)) {
                rally->target=NULL;rally->mode=NAV_RALLY_POINT;
            } else rally->point=rally->target->posinfo.position;
        }
        if(rally->owner!=rally->producer->playerowner->playerIndex) {
            Node *node;
            if(rally->target)rally->point=rally->target->posinfo.position;
            rally->mode=NAV_NONE;rally->target=NULL;
            for(node=layer->todolist.head;node;node=node->next) {
                CommandToDo *c=listGetStructOfNode(node);
                if(c->ordertype.order==COMMAND_MILITARY_PARADE && c->militaryParade==rally->parade) {
                    dispatching++;clHalt(layer,c->selection);dispatching--;break;
                }
            }
            navParadeFreed(rally->parade);
        }
    }
    while(*link) {
        NavRoute *r=*link;sdword i;bool32 arrived=TRUE,retry=FALSE;
        for(i=r->ships->numShips-1;i>=0;i--) {
            Ship *ship=r->ships->ShipPtr[i];CommandToDo *command;Ship *leader;vector delta;
            real32 tolerance,capture;
            if(ship->playerowner->playerIndex!=r->owner || shipPilotControls(ship)) {
                routeByShip[ship->shipID.shipNumber]=NULL;
                memmove(&r->ships->ShipPtr[i],&r->ships->ShipPtr[i+1],(r->ships->numShips-i-1)*sizeof(Ship *));
                r->ships->numShips--;continue;
            }
            command=getShipAndItsCommand(layer,ship);leader=ship;
            if(command && (command->ordertype.attributes&COMMAND_MASK_FORMATION))leader=command->selection->ShipPtr[0];
            vecSub(delta,leader->posinfo.position,r->points[r->index]);
            capture=(leader->isDodging || universe.totaltimeelapsed<leader->DodgeTime)?250.0f:100.0f;
            tolerance=(r->index+1<r->count || r->action==NAV_ACTION_LOOP)?max(capture,sqrtf(vecMagnitudeSquared(leader->posinfo.velocity))*universe.phystimeelapsed*2):capture;
            if(vecMagnitudeSquared(delta)>tolerance*tolerance) {
                arrived=FALSE;
                if(!command || command->ordertype.order==COMMAND_NULL)retry=TRUE;
            }
        }
        if(!r->ships->numShips){*link=r->next;memFree(r->ships);memFree(r);continue;}
        if(arrived) {
            if(++r->index==r->count) {
                if(r->action==NAV_ACTION_LOOP)r->index=0;
                else {
                    *link=r->next;
                    for(i=0;i<r->ships->numShips;i++)routeByShip[r->ships->ShipPtr[i]->shipID.shipNumber]=NULL;
                    dispatching++;terminalOrder(layer,r);dispatching--;
                    memFree(r->ships);memFree(r);continue;
                }
            }
            retry=TRUE;r->retryAt=0;
        }
        if(retry && universe.totaltimeelapsed>=r->retryAt) {
            r->retryAt=universe.totaltimeelapsed+1.0f;
            dispatching++;clMove(layer,r->ships,r->ships->ShipPtr[0]->posinfo.position,r->points[r->index]);dispatching--;
        }
        link=&r->next;
    }
}
void navObjectDied(TargetPtr object)
{
    NavRoute *route;NavRally **link=&rallies;NavPending **p=&pending;
    navPlacementObjectDied(object);
    if(object->objtype==OBJ_ShipType){SelectCommand one;one.numShips=1;one.ShipPtr[0]=(Ship *)object;navCancelShips(&one);}
    for(route=routes;route;route=route->next)if(route->target==object)route->target=NULL;
    while(*link) {
        NavRally *r=*link;
        if((TargetPtr)r->producer==object){*link=r->next;memFree(r);continue;}
        if(r->target==object){r->point=object->posinfo.position;r->target=NULL;r->mode=NAV_RALLY_POINT;}
        link=&r->next;
    }
    while(*p) {
        NavPending *entry=*p;
        if((TargetPtr)entry->producer==object || (TargetPtr)entry->ship==object){*p=entry->next;memFree(entry);}
        else p=&entry->next;
    }
}
void navReset(void)
{
    navPlacementCancel();
    while(routes){NavRoute *r=routes;routes=r->next;memFree(r->ships);memFree(r);}
    while(rallies){NavRally *r=rallies;rallies=r->next;memFree(r);}
    while(pending){NavPending *p=pending;pending=p->next;memFree(p);}
    memset(routeByShip,0,sizeof(routeByShip));
    dispatching=0;
}

void navVisitLines(void (*visit)(vector,vector,unsigned,unsigned))
{
    NavRoute *route,*shown[64];NavRally *r;unsigned i,n,shownCount=0;sdword j;
    for(route=routes;route;route=route->next) {
        bool32 selected=FALSE,duplicate=FALSE;vector from;
        if(route->owner!=universe.curPlayerIndex)continue;
        for(j=0;j<route->ships->numShips;j++)
            if(selShipInSelection(selSelected.ShipPtr,selSelected.numShips,route->ships->ShipPtr[j])){selected=TRUE;break;}
        if(!selected)continue;
        visit(route->ships->ShipPtr[0]->posinfo.position,route->points[route->index],route->index+1,NAV_WAYPOINTS);
        for(n=0;n<shownCount;n++)if(shown[n]->count==route->count && shown[n]->action==route->action &&
            (route->action==NAV_ACTION_LOOP || shown[n]->index==route->index) &&
            !memcmp(shown[n]->points,route->points,route->count*sizeof(vector))){duplicate=TRUE;break;}
        if(duplicate)continue;
        if(shownCount<64)shown[shownCount++]=route;
        if(route->action==NAV_ACTION_LOOP) {
            from=route->points[route->count-1];i=0;
        } else {from=route->points[route->index];i=route->index+1;}
        for(;i<route->count;i++){visit(from,route->points[i],i+1,NAV_WAYPOINTS);from=route->points[i];}
    }
    for(r=rallies;r;r=r->next)if(r->mode && r->owner==universe.curPlayerIndex &&
        selShipInSelection(selSelected.ShipPtr,selSelected.numShips,r->producer)) {
        vector point=r->target?r->target->posinfo.position:r->point;
        visit(r->producer->posinfo.position,point,0,r->mode);
    }
}

/* Optional, versioned tail. Object references are resolved after legacy fixups. */
#define NAV_SAVE_MAGIC 0x4e415631
static void saveVector(vector v){SaveStructureOfSize(&v,sizeof(v));}
static vector loadVector(void){vector v;LoadStructureOfSizeToAddress(&v,sizeof(v));return v;}
static sdword savedObject(TargetPtr p){return p?SpaceObjRegistryGetID((SpaceObj *)p):-1;}
void navSave(void)
{
    NavRoute *r;NavRally *a;NavPending *p;sdword n=0,i;Node *node;
    SaveInfoNumber(NAV_SAVE_MAGIC);SaveInfoNumber(1);
    for(r=routes;r;r=r->next)n++;SaveInfoNumber(n);
    for(r=routes;r;r=r->next) {
        SaveInfoNumber(r->ships->numShips);
        for(i=0;i<r->ships->numShips;i++)SaveInfoNumber(savedObject((TargetPtr)r->ships->ShipPtr[i]));
        SaveInfoNumber(r->count);SaveInfoNumber(r->index);SaveInfoNumber(r->action);SaveInfoNumber(r->owner);
        SaveInfoNumber(savedObject(r->target));SaveStructureOfSize(&r->retryAt,sizeof(r->retryAt));
        for(i=0;i<(sdword)r->count;i++)saveVector(r->points[i]);
    }
    n=0;for(a=rallies;a;a=a->next)n++;SaveInfoNumber(n);
    for(a=rallies;a;a=a->next) {
        sdword member=-1;
        for(node=universe.mainCommandLayer.todolist.head;node;node=node->next) {
            CommandToDo *c=listGetStructOfNode(node);
            if(c->ordertype.order==COMMAND_MILITARY_PARADE && c->militaryParade==a->parade && c->selection->numShips)
                member=savedObject((TargetPtr)c->selection->ShipPtr[0]);
        }
        SaveInfoNumber(savedObject((TargetPtr)a->producer));SaveInfoNumber(a->owner);SaveInfoNumber(a->mode);
        SaveInfoNumber(savedObject(a->target));saveVector(a->point);
        SaveStructureOfSize(&a->anchor.rotinfo.coordsys,sizeof(matrix));SaveInfoNumber(member);
    }
    n=0;for(p=pending;p;p=p->next)n++;SaveInfoNumber(n);
    for(p=pending;p;p=p->next) {
        SaveInfoNumber(savedObject((TargetPtr)p->ship));SaveInfoNumber(savedObject((TargetPtr)p->producer));SaveInfoNumber(p->owner);
    }
}
void navLoad(void)
{
    sdword tag,n,j,k;NavRoute **tail=&routes;NavRally **rtail=&rallies;NavPending **ptail=&pending;
    navReset();
    if(!LoadInfoNumberOptional(&tag))return; /* Existing saves have no tail. */
    if(tag!=NAV_SAVE_MAGIC || LoadInfoNumber()!=1)return;
    n=LoadInfoNumber();if(n<0 || n>65536)return;
    for(j=0;j<n;j++) {
        sdword count=LoadInfoNumber();NavRoute *r;
        if(count<1 || count>NAV_MAX_SHIPS)return;
        r=memAlloc(sizeof(*r),"saved navigation route",0);memset(r,0,sizeof(*r));
        r->ships=memAlloc(sizeofSelectCommand(count),"saved route ships",0);r->ships->numShips=0;
        for(k=0;k<count;k++) {
            Ship *ship=SpaceObjRegistryGetShip(LoadInfoNumber());
            if(ship)r->ships->ShipPtr[r->ships->numShips++]=ship;
        }
        r->count=LoadInfoNumber();r->index=LoadInfoNumber();r->action=LoadInfoNumber();r->owner=LoadInfoNumber();
        tag=LoadInfoNumber();r->target=tag>=0?SpaceObjRegistryGetTarget(tag):NULL;
        LoadStructureOfSizeToAddress(&r->retryAt,sizeof(r->retryAt));
        if(!r->count || r->count>NAV_MAX_POINTS || r->index>=r->count || r->action>NAV_ACTION_LOOP || (r->action==NAV_ACTION_LOOP && (r->count<2 || r->target)) || r->owner>=universe.numPlayers) {
            memFree(r->ships);memFree(r);return;
        }
        for(k=0;k<(sdword)r->count;k++)r->points[k]=loadVector();
        if(r->ships->numShips) {
            NavRoute *first=partitionRoute(r,&universe.mainCommandLayer),*last=first,*part;
            while(last->next)last=last->next;
            if(first!=last)for(part=first;part;part=part->next) {
                dispatching++;clMove(&universe.mainCommandLayer,part->ships,part->ships->ShipPtr[0]->posinfo.position,part->points[part->index]);dispatching--;
            }
            *tail=first;tail=&last->next;
        } else {memFree(r->ships);memFree(r);}
    }
    n=LoadInfoNumber();if(n<0 || n>65536)return;
    for(j=0;j<n;j++) {
        NavRally *a=memAlloc(sizeof(*a),"saved producer rally",0);Ship *member;matrix orientation;
        memset(a,0,sizeof(*a));a->producer=SpaceObjRegistryGetShip(LoadInfoNumber());
        a->owner=LoadInfoNumber();a->mode=LoadInfoNumber();tag=LoadInfoNumber();
        a->target=tag>=0?SpaceObjRegistryGetTarget(tag):NULL;a->point=loadVector();
        LoadStructureOfSizeToAddress(&orientation,sizeof(orientation));tag=LoadInfoNumber();
        member=tag>=0?SpaceObjRegistryGetShip(tag):NULL;
        if(!a->producer || a->owner>=universe.numPlayers || (a->mode!=NAV_NONE && a->mode!=NAV_RALLY_POINT && a->mode!=NAV_RALLY_OBJECT)) {memFree(a);continue;}
        a->anchor=*a->producer;a->anchor.shiptype=Mothership;
        a->anchor.shiprace=cmFactoryBuildRace(a->producer)==R2?R2:R1;a->anchor.rotinfo.coordsys=orientation;
        if(member) {
            CommandToDo *c=getShipAndItsCommand(&universe.mainCommandLayer,member);
            if(c && c->ordertype.order==COMMAND_MILITARY_PARADE && c->militaryParade->aroundShip==a->producer)a->parade=c->militaryParade;
        }
        *rtail=a;rtail=&a->next;
    }
    n=LoadInfoNumber();if(n<0 || n>65536)return;
    for(j=0;j<n;j++) {
        NavPending *p=memAlloc(sizeof(*p),"saved rally departure",0);memset(p,0,sizeof(*p));
        p->ship=SpaceObjRegistryGetShip(LoadInfoNumber());p->producer=SpaceObjRegistryGetShip(LoadInfoNumber());p->owner=LoadInfoNumber();
        if(p->ship && p->producer && p->owner<universe.numPlayers){*ptail=p;ptail=&p->next;}else memFree(p);
    }
}
static udword hashWord(udword h,udword v){return (h^v)*16777619u;}
static udword hashVector(udword h,vector v)
{udword bits[3];memcpy(bits,&v,sizeof(bits));h=hashWord(h,bits[0]);h=hashWord(h,bits[1]);return hashWord(h,bits[2]);}
static udword hashTarget(udword h,TargetPtr p)
{TargetID id={0};if(p)SetTargetID(&id,p);else id.objtype=65535;h=hashWord(h,id.objtype);return hashWord(h,id.objNumber);}
udword navStateHash(void)
{
    udword h=2166136261u;NavRoute *r;NavRally *a;NavPending *p;sdword i;
    if(!routes && !rallies && !pending)return 0;
    for(r=routes;r;r=r->next) {
        h=hashWord(h,r->count);h=hashWord(h,r->index);h=hashWord(h,r->action);h=hashWord(h,r->owner);h=hashTarget(h,r->target);
        {udword bits;memcpy(&bits,&r->retryAt,sizeof(bits));h=hashWord(h,bits);}
        for(i=0;i<(sdword)r->count;i++)h=hashVector(h,r->points[i]);
        for(i=0;i<r->ships->numShips;i++)h=hashWord(h,r->ships->ShipPtr[i]->shipID.shipNumber);
    }
    for(a=rallies;a;a=a->next) {
        h=hashWord(h,a->producer->shipID.shipNumber);h=hashWord(h,a->mode);h=hashWord(h,a->owner);
        /* point is a fixed fallback; drawing a moving target never mutates it. */
        h=hashVector(h,a->point);h=hashTarget(h,a->target);
        {udword bits[9];memcpy(bits,&a->anchor.rotinfo.coordsys,sizeof(bits));for(i=0;i<9;i++)h=hashWord(h,bits[i]);}
        h=hashWord(h,a->parade!=NULL);
    }
    for(p=pending;p;p=p->next){h=hashWord(h,p->ship->shipID.shipNumber);h=hashWord(h,p->producer->shipID.shipNumber);h=hashWord(h,p->owner);}
    return h;
}
void navIssue(SelectCommand *ships,unsigned mode,const vector *points,unsigned count,unsigned action,TargetPtr target)
{
    SelectCommand *owned;sdword i;
    if(playPackets || (universePause && !hwMatchOption(OH_PAUSE_ORDERS,opPauseOrders)))return;
    if(!ships || !ships->numShips || ships->numShips>NAV_MAX_SHIPS)return;
    owned=memAlloc(sizeofSelectCommand(ships->numShips),"navigation owned selection",0);owned->numShips=0;
    for(i=0;i<ships->numShips;i++) {
        Ship *ship=ships->ShipPtr[i];
        if(ship->playerowner!=universe.curPlayerPtr || ship->flags&SOF_Dead)continue;
        if(mode!=NAV_WAYPOINTS && !navCanProduce(ship))continue;
        owned->ShipPtr[owned->numShips++]=ship;
    }
    if(mode==NAV_WAYPOINTS)makeShipsControllable(owned,COMMAND_MOVE);
    if(owned->numShips) {
        if(multiPlayerGame || recordFakeSendPackets)clSendNavigation(owned,mode,points,count,action,target);
        else if(mode==NAV_WAYPOINTS)navSetRoute(&universe.mainCommandLayer,owned,points,count,action,target);
        else navSetRally(owned,mode,count?points[0]:owned->ShipPtr[0]->posinfo.position,target);
    }
    memFree(owned);
}
