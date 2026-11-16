#ifndef OPENHW_NAVIGATION_ORDERS_H
#define OPENHW_NAVIGATION_ORDERS_H
#include "CommandLayer.h"
#include "FEFlow.h"

#define NAV_MAX_POINTS 64
#define NAV_MAX_SHIPS COMMAND_MAX_SHIPS
enum { NAV_NONE, NAV_WAYPOINTS, NAV_RALLY_POINT, NAV_RALLY_OBJECT };
enum { NAV_ACTION_NONE, NAV_ACTION_ATTACK, NAV_ACTION_GUARD, NAV_ACTION_SPECIAL,
       NAV_ACTION_HARVEST, NAV_ACTION_DOCK, NAV_ACTION_LOOP };

bool32 navCanProduce(const Ship *ship);
bool32 navValidPoint(vector point);
void navSetRoute(CommandLayer *layer, SelectCommand *ships, const vector *points,
                 unsigned count, unsigned action, TargetPtr target);
void navSetRally(SelectCommand *ships, unsigned mode, vector point, TargetPtr target);
void navCancelShips(SelectCommand *ships);
void navObjectDied(TargetPtr object);
void navReset(void);
void navTick(CommandLayer *layer);
udword navMoveFlightFlags(const Ship *ship);
void navShipBuilt(Ship *ship, Ship *producer);
bool32 navShipLaunched(CommandLayer *layer, Ship *ship, Ship *producer);
Ship *navParadeLeader(MilitaryParadeCommand *parade);
bool32 navIsVirtualParade(const MilitaryParadeCommand *parade);
void navParadeFreed(MilitaryParadeCommand *parade);
void navSave(void);
void navLoad(void);
udword navStateHash(void);

void clSendNavigation(SelectCommand *ships, unsigned mode, const vector *points,
                      unsigned count, unsigned action, TargetPtr target);
void navIssue(SelectCommand *ships, unsigned mode, const vector *points,
              unsigned count, unsigned action, TargetPtr target);

/* Placement is client-only; committed orders always use navIssue. */
void navBeginWaypoints(char *string, featom *atom);
void navBeginRallyPoint(char *string, featom *atom);
void navBeginRallyObject(char *string, featom *atom);
void navClearRally(char *string, featom *atom);
bool32 navPlacementActive(void);
bool32 navPlacementClick(vector point);
bool32 navPlacementTarget(TargetPtr target);
bool32 navPlacementKey(sdword key);
void navPlacementCancel(void);
void navPlacementObjectDied(TargetPtr object);
void navVisitLines(void (*visit)(vector from,vector to,unsigned number,unsigned mode));
void navDraw(void);
#endif
