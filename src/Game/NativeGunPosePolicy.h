#ifndef HW_NATIVE_GUN_POSE_POLICY_H
#define HW_NATIVE_GUN_POSE_POLICY_H
#include "SpaceObj.h"

/* Only NewGimble guns initialize and maintain these mechanical angles in HW1.
   Fixed, legacy heading-only gimble, missile and mine guns leave them unused,
   including in existing saves. Keep their authored RM mechanical rest pose;
   projectile/flash aiming still uses the live gameplay gunheading. */
static void nativeGunMechanicalAngles(const Gun *gun,float *azimuth,float *declination)
{
    *azimuth=0; *declination=0;
    if(gun && gun->gunstatic && gun->gunstatic->guntype==GUN_NewGimble) {
        *azimuth=gun->angle; *declination=gun->declination;
    }
}
static void nativeGunTrackingHeading(const Gun *gun,float heading[3])
{
    heading[0]=gun->gunheading.x;heading[1]=gun->gunheading.y;heading[2]=gun->gunheading.z;
    if(gun->gunstatic && gun->gunstatic->guntype==GUN_NewGimble) {
        heading[0]=gun->curgunorientation.m13;
        heading[1]=gun->curgunorientation.m23;
        heading[2]=gun->curgunorientation.m33;
    }
}
#endif
