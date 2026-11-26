#ifndef OPENHW_CATACLYSM_SALVAGE_H
#define OPENHW_CATACLYSM_SALVAGE_H
#include "SpaceObj.h"
#include "ShipSelect.h"
#include "CommandLayer.h"
bool32 cataSalvageSpecial(Ship *ship,SpecialCommand *targets,real32 approachDistance);
void cataSalvageCancel(Ship *ship);
void cataSalvageRemoveReference(Ship *ship,Ship *removed);
void cataSalvageUpdate(void);
void cataSalvageLoadStatics(void);
void cataSalvageSetProcessingHost(Ship *host);
sdword cataSalvageProcessedCount(ShipType type,sdword owner);
void cataSalvageConfigure(ShipStaticInfo *info);
bool32 cataSalvageHandles(const Ship *ship);
bool32 cataSalvageCanTarget(SpaceObjRotImpTarg *target, struct Player *owner);
void cataSalvageSetActorMode(Ship *ship,sdword mode);
#endif
