#ifndef OPENHW_CATACLYSM_WORKER_H
#define OPENHW_CATACLYSM_WORKER_H
#include "SpaceObj.h"
void cataWorkerConfigure(ShipStaticInfo *info);
void cataWorkerCancelSpecial(Ship *ship);
void cataWorkerSetRole(Ship *ship,sdword role);
bool32 cataWorkerCanRepairTarget(SpaceObjRotImpTarg *target, struct Player *owner);
bool32 cataWorkerExtendArm(Ship *ship);
#endif
