#ifndef OPENHW_NAVIGATION_FLIGHT_H
#define OPENHW_NAVIGATION_FLIGHT_H
#include "AIShip.h"
#include <math.h>

/* A waypoint with another leg is a direction, rather than a request to stop.
   The ordinary distance-scaled desired velocity is retained for final stops. */
static void navFlightThroughVelocity(vector *desired,real32 speed)
{
    real32 lengthSquared=vecMagnitudeSquared(*desired);
    if(lengthSquared>0.000001f) {
        real32 scale=speed/sqrtf(lengthSquared);
        desired->x*=scale;desired->y*=scale;desired->z*=scale;
    }
}
static bool32 navFlightOrientBeforeMoving(udword flags,real32 speedSquared)
{
    return (flags&(AISHIP_FirstPointInDirectionFlying|AISHIP_CarTurn)) &&
        (!(flags&AISHIP_NavigationSteering) || speedSquared<100.0f);
}
#endif
