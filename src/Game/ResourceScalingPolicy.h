#ifndef HW_RESOURCE_SCALING_POLICY_H
#define HW_RESOURCE_SCALING_POLICY_H

#include "Attributes.h"

/* Resource population only: enemy strength and harvesting yield are separate.
   A local preference must never change multiplayer resource generation. */
static unsigned int resourceCountForFleet(unsigned int authoredCount,
    int singlePlayer, unsigned int attributes, float fleetModifier,
    unsigned int disableSPResourceScaling)
{
    if (!singlePlayer || disableSPResourceScaling ||
        !(attributes & ATTRIBUTES_ScaleResources))
        return authoredCount;
    if (attributes & ATTRIBUTES_KillerCollDamage)
        return (unsigned int)((float)authoredCount * fleetModifier);
    return (unsigned int)((float)authoredCount * (1 - fleetModifier));
}

#endif
