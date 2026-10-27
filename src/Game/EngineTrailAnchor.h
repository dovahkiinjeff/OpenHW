#ifndef HW_ENGINE_TRAIL_ANCHOR_H
#define HW_ENGINE_TRAIL_ANCHOR_H
#include <math.h>
static float engineGlowRadius(float radius, float authoredRadius, float velocityRatio,
                              float pulse, float radialAdjust, float baseScale)
{
    if (authoredRadius != -1.0f) radius = authoredRadius;
    return (1.0f-baseScale)*(radialAdjust*velocityRatio*radius*pulse)+baseScale*radius;
}
static float engineGlowOriginOffset(float spriteOffset, float xAdjust, float radius)
{
    return .6f*xAdjust + spriteOffset + fmaxf(.35f,radius*.38f);
}
#endif
