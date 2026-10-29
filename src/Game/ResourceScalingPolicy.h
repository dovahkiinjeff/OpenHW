#ifndef HW_RESOURCE_SCALING_POLICY_H
#define HW_RESOURCE_SCALING_POLICY_H

#include "Attributes.h"

/* Resource population only: enemy strength and harvesting yield are separate.
   A local preference must never change multiplayer resource generation. */
/* 101 means the new slider has not been saved yet: preserve the old toggle. */
static unsigned int resourceScalingPercent(unsigned int slider,unsigned int legacyDisable)
{
    return slider<=100 ? slider : (legacyDisable ? 100u : 0u);
}
static unsigned int resourceCountForFleet(unsigned int authoredCount,
    int singlePlayer, unsigned int attributes, float fleetModifier,
    unsigned int disableSPResourceScaling, unsigned int slider)
{
    unsigned int vanilla, percent;
    if (!singlePlayer || !(attributes & ATTRIBUTES_ScaleResources)) return authoredCount;
    if (attributes & ATTRIBUTES_KillerCollDamage)
        vanilla=(unsigned int)((float)authoredCount * fleetModifier);
    else vanilla=(unsigned int)((float)authoredCount * (1 - fleetModifier));
    percent=resourceScalingPercent(slider,disableSPResourceScaling);
    if(percent==100) return authoredCount;
    return vanilla+(unsigned int)((double)(authoredCount-vanilla)*percent/100.0);
}

#endif
