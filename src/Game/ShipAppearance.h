#ifndef OPENHW_SHIP_APPEARANCE_H
#define OPENHW_SHIP_APPEARANCE_H
#include "SpaceObj.h"
enum { SHIP_PAINT_BASE,SHIP_PAINT_STRIPE,SHIP_PAINT_TRAIL,SHIP_PAINT_NAV,SHIP_PAINT_RESOURCE,SHIP_PAINT_HYPERSPACE,SHIP_PAINT_COUNT };
typedef struct ShipAppearance { udword mask; color colors[SHIP_PAINT_COUNT]; } ShipAppearance;
void shipAppearanceGet(const Ship *ship,ShipAppearance *appearance);
bool32 shipAppearanceApply(Ship *ship,const ShipAppearance *appearance);
color shipAppearanceColor(const Ship *ship,sdword channel,color fallback);
bool32 shipAppearanceHas(const Ship *ship,sdword channel);
void shipAppearanceRetire(const void *ship);
void shipAppearanceClear(void);
const Ship *shipAppearanceAt(unsigned number,ShipAppearance *appearance);
void shipAppearanceSave(void);
void shipAppearanceLoad(void);
#endif
