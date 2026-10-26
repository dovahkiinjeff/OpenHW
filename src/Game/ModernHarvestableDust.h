#ifndef MODERN_HARVESTABLE_DUST_H
#define MODERN_HARVESTABLE_DUST_H
#include "Types.h"
#include "ModernGraphics.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Game-wide default/preset library with individual resource overrides. */
HWModernVolumetricDustVolume *modernHarvestableDustStyle(void);
HWModernVolumetricDustVolume *modernHarvestableDustStyleFor(udword resourceId, bool32 edit);
bool32 modernHarvestableDustHasOverride(udword resourceId);
void modernHarvestableDustResetOverride(udword resourceId);
bool32 modernHarvestableDustSave(void);
void modernHarvestableDustReload(void);
bool32 modernHarvestableDustActive(void);
bool32 modernHarvestableDustActiveFor(udword resourceId);
void modernHarvestableDustUpdate(void);
#ifdef __cplusplus
}
#endif
#endif
