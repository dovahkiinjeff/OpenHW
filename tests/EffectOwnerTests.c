#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "EffectOwner.h"

int main(void)
{
    Effect effect = {0};
    Bullet bullet = {0};
    Ship ship = {0};
    Derelict derelict = {0};
    StaticInfo model = {0};
    SpaceObj reused = {0};
    StaticInfo *poison = (StaticInfo *)(uintptr_t)UINT64_C(0x404000003f800000);
    assert(!effectOwnerShip(NULL) && !effectOwnerModelStatic(NULL));
    assert(!effectOwnerShip(&effect) && !effectOwnerModelStatic(&effect));

    /* Exact invalid static-info value observed in the combat minidump.
       A live bullet must never be interpreted as a ship/model owner. */
    bullet.objtype = OBJ_BulletType;
    bullet.staticinfo = poison;
    effect.owner = (Ship *)&bullet;
    assert(!effectOwnerShip(&effect) && !effectOwnerModelStatic(&effect));

    /* Freed owner memory was reused for floats (1.0/3.0 in the dump). */
    reused.objtype = (ObjType)0x3f800000;
    reused.staticinfo = poison;
    effect.owner = (Ship *)&reused;
    assert(!effectOwnerShip(&effect) && !effectOwnerModelStatic(&effect));
    reused.objtype = OBJ_EffectType;
    assert(!effectOwnerShip(&effect) && !effectOwnerModelStatic(&effect));

    ship.objtype = OBJ_ShipType;
    ship.staticinfo = (ShipStaticInfo *)&model;
    effect.owner = &ship;
    model.staticheader.staticCollInfo.collspheresize = 123.0f;
    assert(effectOwnerShip(&effect) == &ship);
    assert(effectOwnerModelStatic(&effect) == &model);
    assert(effectOwnerModelStatic(&effect)->staticheader.staticCollInfo.collspheresize == 123.0f);
    derelict.objtype = OBJ_DerelictType;
    derelict.staticinfo = (DerelictStaticInfo *)&model;
    effect.owner = (Ship *)&derelict;
    assert(!effectOwnerShip(&effect) && effectOwnerModelStatic(&effect) == &model);
    puts("PASS: bullet/effect/reused owner memory cannot supply ship or model fields; valid ship/derelict radius preserved");
    return 0;
}
