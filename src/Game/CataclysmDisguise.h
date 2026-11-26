#ifndef CATACLYSM_DISGUISE_H
#define CATACLYSM_DISGUISE_H

#include "ShipSelect.h"

struct Player;

extern CustShipHeader CataMimicHeader;
extern CustShipHeader CataMCVHeader;

void cataDisguiseRegister(sdword group, const char *name);
void cataDisguiseSetGroupLimit(sdword group, sdword limit);
void cataDisguiseApply(GrowSelection *ships, const char *name, sdword color);
void cataDisguiseRemove(GrowSelection *ships);
void cataDisguiseForget(Ship *ship);
lodinfo *cataDisguiseVisual(const Ship *ship, sdword *color, real32 *morph, real32 *radius);
bool32 cataDisguiseIsAsteroid(const Ship *ship);
bool32 cataDisguiseConcealsRaider(const Ship *ship, const struct Player *observer);

#endif
