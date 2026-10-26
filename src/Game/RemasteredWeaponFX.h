#ifndef HW_REMASTERED_WEAPON_FX_H
#define HW_REMASTERED_WEAPON_FX_H
#include "SpaceObj.h"
#include "Camera.h"
/* Runtime-only visuals: never change a save layout or projectile simulation. */
extern bool32 rmWeaponFxDiagnosticsEnabled;
void rmWeaponFxDiagnosticShot(Ship *ship,Gun *gun,bool32 visible);
void rmWeaponFxDiagnosticBinding(const Ship *ship,unsigned int gun,const char *reason);
void rmWeaponFxDiagnosticFireCheck(Ship *ship,Gun *gun,const char *reason);
void rmWeaponFxDiagnosticLight(Ship *ship,Gun *gun,const vector *position,const vector *direction,
                              float radius,float intensity);
bool32 rmWeaponFxProjectileAvailable(const Ship *ship,Gun *gun);
bool32 rmWeaponFxMuzzle(Ship *ship,Gun *gun,const vector *position,const vector *direction);
bool32 rmWeaponFxImpact(Bullet *bullet,const vector *position,const vector *velocity);
bool32 rmWeaponFxMissileImpact(Missile *missile,const vector *position,const vector *velocity);
bool32 rmWeaponFxDrawBullet(Bullet *bullet,Camera *camera);
void rmWeaponFxDrawTransient(Camera *camera);
bool32 rmFxDrawResourceDust(DustCloud *dust,Camera *camera);
/* RM sprite-effect controls, per mission and independent of mission volumes. */
typedef struct RmResourceDustStyle {
    float opacity, brightness, size, phase, tint[3];
} RmResourceDustStyle;
RmResourceDustStyle *rmFxResourceDustStyle(void);
void rmFxResourceDustReload(void);
bool32 rmFxResourceDustSave(void);
void rmFxResourceDustReset(void);
bool32 rmFxDrawEngine(Ship *ship,unsigned int engine,const vector *segments,unsigned int count,Camera *camera);
bool32 rmFxShipDeath(Ship *ship);
bool32 rmFxDrawUtilityEffect(Effect *effect,Camera *camera);
bool32 rmFxDrawLegacyEffect(Effect *effect,unsigned int kind,float radius,Camera *camera);
bool32 rmFxDrawMissileTrail(Missile *missile,const vector *segments,unsigned int count,Camera *camera);
void rmWeaponFxReset(void);
void rmWeaponFxShutdown(void);
#endif
