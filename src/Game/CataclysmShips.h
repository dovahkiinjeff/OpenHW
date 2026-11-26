#ifndef OPENHW_CATACLYSM_SHIPS_H
#define OPENHW_CATACLYSM_SHIPS_H
#include "SpaceObj.h"
#include "ConsMgr.h"
bool32 cataShipIsCoreComponent(ShipType type);
sdword cataShipOriginalID(const Ship *ship);
void cataShipSetGatePartner(Ship *ship,Ship *partner);
Ship *cataShipGatePartner(Ship *ship);
void cataShipConfigureCombat(ShipStaticInfo *info);
bool32 cataShipEmbeddedInNativeHull(Ship *ship);
bool32 cataShipIsAttachedComponent(const Ship *ship);
Ship *cataShipComponentHost(const Ship *ship);
void cataShipUpdateComponentPose(Ship *part, Ship *host);
void cataShipSyncComponents(void);
bool32 cataShipSkipFleetOrders(const Ship *ship);
real32 cataShipSensorRadius(const Ship *ship);
bool32 cataShipUpgradeByDocking(const Ship *ship);
real32 cataShipUpgradeTimeModifier(const Ship *ship);
void cataShipCreateCoreComponents(Ship *host);
void cataShipHostRemoved(Ship *host);
SpaceObjRotImpTarg *cataShipDamageTarget(SpaceObjRotImpTarg *target);
bool32 cataShipAttachComponent(Ship *host,Ship *component);
void cataMission1PreloadFleet(void);
void cataMission2PreloadShipColors(void);
void cataMission1CreateFleet(hvector *startPoint);
void cataShipLoadEconomy(ShipStaticInfo *info,const char *directory,const char *stem);
real32 cataShipAvoidBy(const Ship *ship);
bool32 cataShipCanCreate(ShipType type,shipsinprogress *factory,shipavailable *available);
#endif
