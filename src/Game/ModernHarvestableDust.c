/* Compatibility bridge for retired editor branches. RM resource-cloud visuals
   live in RemasteredWeaponFX, not mission volumetric dust or authored presets.
   The old source/presets are retained under deferred/harvestable-dust-pre-rm. */
#include "ModernHarvestableDust.h"
static HWModernVolumetricDustVolume retiredStyle;
HWModernVolumetricDustVolume *modernHarvestableDustStyle(void) {return &retiredStyle;}
HWModernVolumetricDustVolume *modernHarvestableDustStyleFor(udword id,bool32 edit) {(void)id;(void)edit;return &retiredStyle;}
bool32 modernHarvestableDustHasOverride(udword id) {(void)id;return FALSE;}
void modernHarvestableDustResetOverride(udword id) {(void)id;}
bool32 modernHarvestableDustSave(void) {return FALSE;}
void modernHarvestableDustReload(void) {}
bool32 modernHarvestableDustActive(void) {return FALSE;}
bool32 modernHarvestableDustActiveFor(udword id) {(void)id;return FALSE;}
void modernHarvestableDustUpdate(void) {hwModernGraphicsSetHarvestableDustVolumes(NULL,0);}
