#ifndef HW_MODERN_SHIP_LOD_H
#define HW_MODERN_SHIP_LOD_H

#include <math.h>

/* Output-pixel thresholds, not the internal FSR/DLSS resolution. Hysteresis
   prevents a ship hovering near a boundary from alternating meshes each frame.
   Never select the legacy subpixel/null entries: distant ships remain meshes. */
static int hwModernShipLODSelect(float distanceSquared, float radius,
    float fovDegrees, int outputHeight, int current, int available)
{
    static const float boundary[4] = { 320.0f, 160.0f, 80.0f, 40.0f };
    float diameterPixels;
    int last = available - 1;
    if (last > 4) last = 4;
    if (last < 0) return 0;
    if (current < 0 || current > last) current = 0;
    if (distanceSquared <= 0.0f || radius <= 0.0f) return 0;
    if (fovDegrees < 5.0f || fovDegrees > 170.0f) fovDegrees = 45.0f;
    if (outputHeight <= 0) outputHeight = 1080;
    diameterPixels = radius * (float)outputHeight /
        (sqrtf(distanceSquared) * tanf(fovDegrees * 0.00872664626f));
    while (current < last && diameterPixels < boundary[current] * 0.9f)
        current++;
    while (current > 0 && diameterPixels > boundary[current - 1] * 1.1f)
        current--;
    return current;
}

#endif
