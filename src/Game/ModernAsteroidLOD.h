#ifndef HW_MODERN_ASTEROID_LOD_H
#define HW_MODERN_ASTEROID_LOD_H
#include <math.h>
/* Fixed world-space bands, shared by visual and ray-traced shadow geometry. */
static int hwModernAsteroidLODSelect(float distanceSquared)
{
    if(!isfinite(distanceSquared) || distanceSquared<=0.0f) return 0;
    if(distanceSquared<=4000000.0f) return 0;       /* 2,000 */
    if(distanceSquared<=36000000.0f) return 1;     /* 6,000 */
    if(distanceSquared<=225000000.0f) return 2;    /* 15,000 */
    return 3;
}
#endif
