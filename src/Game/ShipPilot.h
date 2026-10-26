#ifndef OPENHW_SHIP_PILOT_H
#define OPENHW_SHIP_PILOT_H
#include "SpaceObj.h"
#include "Camera.h"
/* Transient local-player control; never serialized into ships or saves. */
bool32 shipPilotActive(void);
bool32 shipPilotOwns(const Ship *ship);
bool32 shipPilotControls(const Ship *ship);
Ship *shipPilotTarget(void);
#define PILOT_HITSCAN_BULLET 0x0004u
bool32 shipPilotWeaponCanHit(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target);
bool32 shipPilotTargetScreen(const Ship *ship,real32 *x,real32 *y);
bool32 shipPilotCockpit(void);
bool32 shipPilotCommandMode(void);
const char *shipPilotNavigation(void);
real32 shipPilotCommandRange(void);
bool32 shipPilotCommandAvailable(void);
bool32 shipPilotAutoFire(void);
bool32 shipPilotWeaponActive(sdword index);
sdword shipPilotWeaponBank(void);
bool32 shipPilotCockpitAvailable(void);
sdword shipPilotSquadCount(void);
real32 shipPilotMaxSpeed(void);
Ship *shipPilotShip(void);
bool32 shipPilotCanEnter(void);
void shipPilotExit(void);
bool32 shipPilotKey(sdword key);
bool32 shipPilotMouse(udword event, udword data);
void shipPilotMouseMotion(sdword dx, sdword dy);
bool32 shipPilotCamera(Camera *camera);
void shipPilotUpdate(void);
real32 shipPilotThrottle(void);
bool32 shipPilotFireAll(void);
void shipPilotReticle(sdword *x, sdword *y);
real32 shipPilotGunReady(Ship *ship, Gun *gun, const char **status);
real32 shipPilotGunSeconds(Gun *gun);
real32 shipPilotSpecialReady(Ship *ship);
void gunPilotAimAndFire(Ship *ship, const vector *point, Ship *target,
                       bool32 fire, bool32 all);
#endif
