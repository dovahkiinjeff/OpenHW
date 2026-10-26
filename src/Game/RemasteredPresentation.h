#ifndef OPENHW_REMASTERED_PRESENTATION_H
#define OPENHW_REMASTERED_PRESENTATION_H
#include "Types.h"
struct Ship;
struct Gun;
struct Camera;
void rmPresentationStartup(void);
void rmPresentationShutdown(void);
void rmPresentationReset(void);
void rmPresentationUpdate(void);
void rmPresentationDraw(struct Camera *camera);
void rmPresentationHullContact(const float position[3]);
/* 0: charging, 1: ordinary shot, 2: ion charge completed (do not replay FX). */
int rmPresentationPrepareIonShot(const void *ship,unsigned gun,const void *target);
/* Read-only HUD query. Zero means no active anticipation phase. */
float rmPresentationIonChargeRemaining(const void *ship,unsigned gun);
bool32 rmPresentationOwnsGunSound(const void *ship,unsigned gun);
bool32 rmPresentationOwnsGunFx(const void *ship,unsigned gun);
bool32 rmPresentationOwnsHarvestFx(const void *ship);
bool32 rmPresentationOwnsHarvestSound(const void *ship);
/* Presentation-only side table: no serialized projectile layout changes and
   no retained pointers into a ship/model that may die before its projectile. */
void rmPresentationCaptureProjectile(const void *projectile,const void *ship,unsigned gun);
void rmPresentationTrackProjectile(const void *projectile);
bool32 rmPresentationOwnsProjectileFx(const void *projectile);
bool32 rmPresentationShipDeath(const void *ship);
bool32 rmPresentationProjectileImpact(const void *projectile,const float position[3],const float heading[3],const float velocity[3],float damage);
void rmPresentationReleaseProjectile(const void *projectile,const float position[3],const float heading[3],const float velocity[3]);
#endif
