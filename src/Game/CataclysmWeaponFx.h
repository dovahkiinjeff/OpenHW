#ifndef OPENHW_CATACLYSM_WEAPON_FX_H
#define OPENHW_CATACLYSM_WEAPON_FX_H
#include "SpaceObj.h"
static bool32 cataEnergyGun(const void *owner,unsigned index)
{
    const Ship *ship=owner;const GunStatic *gun;
    if(!ship||!ship->gunInfo||index>=(unsigned)ship->gunInfo->numGuns)return FALSE;
    gun=ship->gunInfo->guns[index].gunstatic;
    return gun&&gun->cataDefinition&&(gun->cataWeaponClass==0||gun->cataWeaponClass==4||
        gun->cataWeaponClass==10||gun->cataWeaponClass==14);
}
#endif
