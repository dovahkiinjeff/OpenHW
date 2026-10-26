#ifndef OPENHW_REMASTERED_WEAPON_PRESENTATION_H
#define OPENHW_REMASTERED_WEAPON_PRESENTATION_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmWeaponPresentation HwRmWeaponPresentation;
enum { HW_RM_WEAPON_FIRE,HW_RM_WEAPON_BULLET,HW_RM_WEAPON_HIT,
       HW_RM_WEAPON_ANTICIPATE,HW_RM_WEAPON_MISS,HW_RM_WEAPON_NOPENETRATE,
       HW_RM_WEAPON_BLOWTHROUGH,HW_RM_WEAPON_SLOT_COUNT };
enum { HW_RM_WEAPON_HAS_NLIPS=1,HW_RM_WEAPON_HAS_CLAMP=2,HW_RM_WEAPON_HAS_SM=4 };
typedef struct HwRmWeaponPresentationSlot {
    const char *fx,*sound;
    float nlips,clampMinimum,clampMaximum;
    unsigned authoredOptions,sm;
} HwRmWeaponPresentationSlot;
/* Bounded native .wf data reader: literal assignments only, no Lua execution
   and no access to simulation properties. One source grammar, no fallbacks. */
HwRmWeaponPresentation *hwRmWeaponPresentationRead(const void *bytes,size_t size,char *error,size_t capacity);
void hwRmWeaponPresentationFree(HwRmWeaponPresentation *weapon);
const HwRmWeaponPresentationSlot *hwRmWeaponPresentationAt(const HwRmWeaponPresentation *weapon,unsigned slot);
const char *hwRmWeaponPresentationDeathType(const HwRmWeaponPresentation *weapon);
const char *hwRmWeaponPresentationScarType(const HwRmWeaponPresentation *weapon);
unsigned hwRmWeaponPresentationIgnoredCount(const HwRmWeaponPresentation *weapon);
const char *hwRmWeaponPresentationIgnoredAt(const HwRmWeaponPresentation *weapon,unsigned index);
#ifdef __cplusplus
}
#endif
#endif
