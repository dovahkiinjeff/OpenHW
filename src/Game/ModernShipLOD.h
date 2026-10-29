#ifndef HW_MODERN_SHIP_LOD_H
#define HW_MODERN_SHIP_LOD_H

#include <math.h>

/* Authored distance bands are squared world-space distances.  Do not scale
   them by display resolution, field of view, frame rate, or previous LOD. */
static int hwModernShipLODSelect(float distanceSquared,
    const float *boundariesSquared, int available)
{
    int level=0;
    if (!boundariesSquared || available <= 1 || !isfinite(distanceSquared) || distanceSquared <= 0.0f) return 0;
    if (available > 5) available=5;
    while (level+1<available && boundariesSquared[level]>0.0f &&
           distanceSquared>boundariesSquared[level]) ++level;
    return level;
}

#endif
