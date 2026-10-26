#ifndef HW_EFFECT_OWNER_H
#define HW_EFFECT_OWNER_H

#include "SpaceObj.h"

/* ETG's historical Ship* owner can also refer to a bullet or another object.
   Only actual ships provide Ship-specific fields. Bullet/effect staticinfo
   does not carry the model collision header used to size replacement FX. */
static inline Ship *effectOwnerShip(const Effect *effect)
{
    SpaceObj *owner = effect ? (SpaceObj *)effect->owner : NULL;
    return owner && owner->objtype == OBJ_ShipType ? (Ship *)owner : NULL;
}

static inline StaticInfo *effectOwnerModelStatic(const Effect *effect)
{
    SpaceObj *owner = effect ? (SpaceObj *)effect->owner : NULL;
    if (!owner) return NULL;
    switch (owner->objtype)
    {
        case OBJ_ShipType:
        case OBJ_AsteroidType:
        case OBJ_NebulaType:
        case OBJ_GasType:
        case OBJ_DustType:
        case OBJ_DerelictType:
        case OBJ_MissileType:
            return owner->staticinfo;
        default:
            return NULL;
    }
}

#endif
