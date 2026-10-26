#include "NativeVisualEvents.h"
#include <string.h>

typedef struct nativeEventMap {
    const char *type, *event;
    unsigned count;
    const char *clips[4];
} nativeEventMap;
static const nativeEventMap nativeEvents[] = {
    {"Taiidan_HeavyCorvette","Wings_Open",1,{"Wings_Open"}},
    {"Taiidan_HeavyCorvette","Wings_Close",1,{"Wings_Close"}},
    {"Taiidan_HeavyCorvette","Wings_Open_DMG",1,{"Wings_Open_DMG"}},
    {"Taiidan_HeavyCorvette","Wings_Close_DMG",1,{"Wings_Close_DMG"}},
    {"Taiidan_HeavyInterceptor","Wings_Open",1,{"Wings_Open"}},
    {"Taiidan_HeavyInterceptor","Wings_Close",1,{"Wings_Close"}},
    {"Taiidan_LightCorvette","Wings_OPEN",1,{"Wings_OPEN"}},
    {"Taiidan_LightCorvette","Wings_CLOSE",1,{"Wings_CLOSE"}},
    {"Taiidan_LightCorvette","Wings_DMG_Open",1,{"Wings_DMG_Open"}},
    {"Taiidan_LightCorvette","Wings_DMG_Close",1,{"Wings_DMG_Close"}},
    {"Taiidan_SalCapCorvette","SwingCockpit_Out",1,{"SwingCockpit_OUT"}},
    {"Taiidan_SalCapCorvette","SwingCockpit_IN",1,{"SwingCockpit_IN"}},
    {"Taiidan_SensorArray","open",1,{"open"}},
    {"Taiidan_SensorArray","close",1,{"close"}},
    {"Taiidan_TargetDrone","open",1,{"open"}},
    {"Taiidan_TargetDrone","close",1,{"close"}},
    {"P1IonArrayFrigate","Array_Open",1,{"Array_Open"}},
    {"P1IonArrayFrigate","Array_Close",1,{"Array_Close"}},
    {"P1IonArrayFrigate","Array_Open_DMG",1,{"Array_Open_DMG"}},
    {"P1IonArrayFrigate","Array_Close_DMG",1,{"Array_Close_DMG"}},
    {"HeavyCorvette","Make_Ambassador",1,{"Make_Ambassador"}},
    {"Mothership","HangarDoor_OPEN",1,{"Open"}},
    {"Mothership","HangarDoor_CLOSE",1,{"Close"}},
    {"AdvanceSupportFrigate","open",4,{"open1","open2","open3","open4"}},
    {"AdvanceSupportFrigate","close",4,{"close1","close2","close3","close4"}},
    /* RM damageclose is an empty placeholder. Close all arms for the existing
       damaged-close event; do not sample its invalid FLT_MAX timeline. */
    {"AdvanceSupportFrigate","damageclose",4,{"close1","close2","close3","close4"}},
    {"ResourceController","open",1,{"open"}},
    {"ResourceController","close",1,{"close"}},
    {"ResourceController","damageclose",1,{"damageclose"}},
    {"HeavyDefender","Arms_OPEN",1,{"Arms_OPEN"}},
    {"HeavyDefender","Arms_CLOSE",1,{"Arms_CLOSE"}},
    {"HeavyDefender","Arms_Damaged_OPEN",1,{"Arms_Damaged_OPEN"}},
    {"HeavyDefender","Arms_Damaged_CLOSE",1,{"Arms_Damaged_CLOSE"}},
    {"LightDefender","Arms_OPEN",1,{"Arms_OPEN"}},
    {"LightDefender","Arms_CLOSE",1,{"Arms_CLOSE"}},
    {"GravWellGenerator","arms_open",1,{"arms_open"}},
    {"GravWellGenerator","arms_close",1,{"arms_close"}},
    {"GravWellGenerator","damage_open",1,{"damage_open"}},
    {"GravWellGenerator","damage_close",1,{"damage_close"}},
    {"Probe","opening",1,{"opening"}},
    {"Probe","closing",1,{"closing"}},
    {"Probe","damage_open",1,{"damage_open"}},
    {"Probe","damage_close",1,{"damage_close"}},
    {"ProximitySensor","open",1,{"open"}},
    {"ProximitySensor","close",1,{"close"}},
    {"ProximitySensor","damage_open",1,{"damage_open"}},
    {"ProximitySensor","damage_close",1,{"damage_close"}},
    {"SensorArray","Ears_Open",1,{"Ears_Open"}},
    {"SensorArray","Ears_Close",1,{"Ears_Close"}}
};
int meshNativeVisualBindClassicEvents(HwRmVisualAsset *asset,const char *type,char *error,size_t size)
{
    unsigned i;
    if (!asset || !type) return 0;
    for (i=0;i<sizeof(nativeEvents)/sizeof(nativeEvents[0]);++i)
    {
        const nativeEventMap *event=&nativeEvents[i];
        if (!strcmp(type,event->type) &&
            !hwRmVisualAssetBindAnimation(asset,event->event,event->clips,event->count,error,size))
            return 0; /* Loader must discard this asset, never enable partial configuration. */
    }
    if (error && size) error[0]=0;
    return 1;
}

typedef struct nativeGunMap {
    const char *type;
    unsigned gun;
    const char *slot, *azimuth, *declination, *recoil, *muzzle;
} nativeGunMap;
/* These source meshes are rigid children of Position. Latitude is only a
   marker: rotating it alone would aim the muzzle but leave the barrel behind.
   SPLIT_GUN is reserved for geometry actually parented beneath Latitude. */
#define ATTACHED_GUN(type,index,slot,prefix) \
    {type,index,slot,prefix "_Position",prefix "_Position",prefix "_Position",prefix "_Muzzle"}
#define SPLIT_GUN(type,index,slot,prefix) \
    {type,index,slot,prefix "_Position",prefix "_Latitude",prefix "_Latitude",prefix "_Muzzle"}
#define FIXED_GUN(type,index,prefix) \
    {type,index,"Hull",NULL,NULL,NULL,prefix "_Muzzle"}
#define HINGE_GUN(type,index,prefix) \
    {type,index,"Hull",prefix "_Position",NULL,prefix "_Position",prefix "_Muzzle"}
static const nativeGunMap nativeGuns[] = {
    /* R2 identities are qualified: identical HW1 type names do NOT imply
       the same RM component layout or even the same number of barrels. */
    FIXED_GUN("Taiidan_AttackBomber",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_AttackBomber",1,"Weapon_Gun1"),
    FIXED_GUN("Taiidan_HeavyInterceptor",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_HeavyInterceptor",1,"Weapon_Gun1"),
    FIXED_GUN("Taiidan_LightInterceptor",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_LightInterceptor",1,"Weapon_Gun1"),
    FIXED_GUN("Taiidan_DefenseFighter",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_IonCannonFrigate",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_MinelayerCorvette",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_Carrier",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_Carrier",1,"Weapon_Gun1"),
    FIXED_GUN("Taiidan_Carrier",2,"Weapon_Gun2"),
    FIXED_GUN("Taiidan_Carrier",3,"Weapon_Gun3"),
    FIXED_GUN("Taiidan_MissileDestroyer",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_MissileDestroyer",1,"Weapon_Gun1"),
    FIXED_GUN("Taiidan_MissileDestroyer",2,"Weapon_Gun2"),
    FIXED_GUN("Taiidan_MissileDestroyer",3,"Weapon_Gun3"),
    FIXED_GUN("Taiidan_Mothership",0,"Weapon_HullDefense1"),
    /* RM renumbered the forward/underside defense emitters: retain the
       classic combat arc by matching location, not the source suffix. */
    FIXED_GUN("Taiidan_Mothership",1,"Weapon_HullDefense4"),
    FIXED_GUN("Taiidan_Mothership",2,"Weapon_HullDefense3"),
    FIXED_GUN("Taiidan_Mothership",3,"Weapon_HullDefense2"),
    FIXED_GUN("Taiidan_Mothership",4,"Weapon_HullDefense5"),
    FIXED_GUN("Taiidan_RepairCorvette",0,"Weapon_Gun0"),
    SPLIT_GUN("Taiidan_AdvanceSupportFrigate",0,"Weapon_Gun1","Weapon_Gun1"),
    SPLIT_GUN("Taiidan_AdvanceSupportFrigate",1,"Weapon_Gun2","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_LightCorvette",0,"Hull","Weapon_Gun0"),
    ATTACHED_GUN("Taiidan_LightCorvette",1,"Hull","Weapon_Gun1"),
    SPLIT_GUN("Taiidan_HeavyCorvette",0,"Weapon_Gun0","Weapon_Top"),
    SPLIT_GUN("Taiidan_HeavyCorvette",1,"Weapon_Gun0","Weapon_TopSlave"),
    SPLIT_GUN("Taiidan_HeavyCorvette",2,"Weapon_Gun1","Weapon_Top"),
    SPLIT_GUN("Taiidan_HeavyCorvette",3,"Weapon_Gun1","Weapon_TopSlave"),
    ATTACHED_GUN("Taiidan_HeavyDefender",0,"Hull","Weapon_Gun0"),
    ATTACHED_GUN("Taiidan_HeavyDefender",1,"Hull","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_HeavyDefender",2,"Hull","Weapon_Gun2"),
    ATTACHED_GUN("Taiidan_MultiGunCorvette",0,"Hull","Weapon_Gun0"),
    ATTACHED_GUN("Taiidan_MultiGunCorvette",1,"Hull","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_MultiGunCorvette",2,"Hull","Weapon_Gun2"),
    ATTACHED_GUN("Taiidan_MultiGunCorvette",3,"Hull","Weapon_Gun3"),
    ATTACHED_GUN("Taiidan_MultiGunCorvette",4,"Hull","Weapon_Gun4"),
    ATTACHED_GUN("Taiidan_MultiGunCorvette",5,"Hull","Weapon_Gun5"),
    SPLIT_GUN("Taiidan_StandardFrigate",1,"Weapon_Gun1","Weapon_Gun1"),
    SPLIT_GUN("Taiidan_StandardFrigate",2,"Weapon_Gun2","Weapon_Gun1"),
    SPLIT_GUN("Taiidan_StandardFrigate",3,"Weapon_Gun3","Weapon_Gun1"),
    SPLIT_GUN("Taiidan_StandardFrigate",4,"Weapon_Gun4","Weapon_Gun1"),
    FIXED_GUN("Taiidan_StandardFrigate",0,"Weapon_Gun0"),
    FIXED_GUN("Taiidan_StandardFrigate",5,"Weapon_Gun5"),
    SPLIT_GUN("Taiidan_StandardDestroyer",0,"Weapon_Gun0","Weapon_Gun0"),
    SPLIT_GUN("Taiidan_StandardDestroyer",1,"Weapon_Gun0","Weapon_Gun0Slave"),
    SPLIT_GUN("Taiidan_StandardDestroyer",2,"Weapon_Gun2","Weapon_Gun0"),
    SPLIT_GUN("Taiidan_StandardDestroyer",3,"Weapon_Gun2","Weapon_Gun0Slave"),
    FIXED_GUN("Taiidan_StandardDestroyer",4,"Weapon_Gun4"),
    FIXED_GUN("Taiidan_StandardDestroyer",5,"Weapon_Gun5"),
    ATTACHED_GUN("Taiidan_HeavyCruiser",0,"Weapon_Gun0","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_HeavyCruiser",1,"Weapon_Gun1","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_HeavyCruiser",2,"Weapon_Gun2","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_HeavyCruiser",3,"Weapon_Gun3","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_HeavyCruiser",4,"Weapon_Gun4","Weapon_Gun1"),
    ATTACHED_GUN("Taiidan_HeavyCruiser",5,"Weapon_Gun5","Weapon_Gun1"),
    /* Original ion slaves share their master's RM emitter; do not invent
       extra RM turret geometry or a second attachment. */
    {"Taiidan_HeavyCruiser",6,"Weapon_Ion1",NULL,NULL,NULL,"Weapon_Ion1_Muzzle"},
    ATTACHED_GUN("Taiidan_HeavyCruiser",7,"Weapon_Ion1","Weapon_Ion1"),
    {"Taiidan_HeavyCruiser",8,"Weapon_Ion2",NULL,NULL,NULL,"Weapon_Ion1_Muzzle"},
    ATTACHED_GUN("Taiidan_HeavyCruiser",9,"Weapon_Ion2","Weapon_Ion1"),
    FIXED_GUN("P1Fighter",0,"Weapon_Gun0"),
    FIXED_GUN("P2Swarmer",0,"Weapon_Gun0"),
    FIXED_GUN("P2Swarmer",1,"Weapon_Gun1"),
    FIXED_GUN("P2AdvanceSwarmer",0,"Weapon_Gun0"),
    FIXED_GUN("P2AdvanceSwarmer",1,"Weapon_Gun1"),
    FIXED_GUN("P2AdvanceSwarmer",2,"Weapon_Gun2"),
    FIXED_GUN("P2AdvanceSwarmer",3,"Weapon_Gun3"),
    FIXED_GUN("P2FuelPod",0,"Weapon_Gun0"),
    ATTACHED_GUN("P2MultiBeamFrigate",0,"Hull","Weapon_Gun0"),
    ATTACHED_GUN("P2MultiBeamFrigate",1,"Hull","Weapon_Gun1"),
    ATTACHED_GUN("P2MultiBeamFrigate",2,"Hull","Weapon_Gun2"),
    ATTACHED_GUN("P2MultiBeamFrigate",3,"Hull","Weapon_Gun3"),
    FIXED_GUN("P2Mothership",0,"Weapon_Gun0"),
    FIXED_GUN("P2Mothership",1,"Weapon_Gun1"),
    FIXED_GUN("P2Mothership",2,"Weapon_Gun2"),
    FIXED_GUN("P2Mothership",3,"Weapon_Gun3"),
    FIXED_GUN("P2Mothership",4,"Weapon_Gun4"),
    FIXED_GUN("P2Mothership",5,"Weapon_Gun5"),
    FIXED_GUN("P2Mothership",6,"Weapon_Gun6"),
    FIXED_GUN("P2Mothership",7,"Weapon_Gun7"),
    FIXED_GUN("P2Mothership",8,"Weapon_Gun8"),
    FIXED_GUN("P2Mothership",9,"Weapon_Gun9"),
    FIXED_GUN("P2Mothership",10,"Weapon_Gun10"),
    FIXED_GUN("P2Mothership",11,"Weapon_Gun11"),
    FIXED_GUN("P2Mothership",12,"Weapon_Gun12"),
    FIXED_GUN("P2Mothership",13,"Weapon_Gun13"),
    FIXED_GUN("P1IonArrayFrigate",0,"Weapon_Gun0"),
    FIXED_GUN("P1MissileCorvette",0,"Weapon_Gun0"),
    FIXED_GUN("P1MissileCorvette",1,"Weapon_Gun1"),
    // Source Direction is local RM +Y: the outward hinge normal. RM +Y
    // becomes HW +X, not the classic Declination channel's local +Y.
    // Rotate the Position mesh around that ONE axis; Latitude is a marker.
    HINGE_GUN("P1StandardCorvette",0,"Weapon_Gun0"),
    HINGE_GUN("P1StandardCorvette",1,"Weapon_Gun1"),
    FIXED_GUN("P1Mothership",0,"Weapon_Gun0"),
    FIXED_GUN("P1Mothership",1,"Weapon_Gun1"),
    FIXED_GUN("P1Mothership",2,"Weapon_Gun2"),
    FIXED_GUN("P1Mothership",3,"Weapon_Gun3"),
    FIXED_GUN("P1Mothership",4,"Weapon_Gun4"),
    FIXED_GUN("P1Mothership",5,"Weapon_Gun5"),
    FIXED_GUN("P1Mothership",6,"Weapon_Gun6"),
    FIXED_GUN("P1Mothership",7,"Weapon_Gun7"),
    FIXED_GUN("P1Mothership",8,"Weapon_Gun8"),
    FIXED_GUN("P1Mothership",9,"Weapon_Gun9"),
    FIXED_GUN("P1Mothership",10,"Weapon_Gun10"),
    FIXED_GUN("P1Mothership",11,"Weapon_Gun11"),
    /* These barrels are baked into the hull. Native muzzle markers provide
       their origins, but aiming must never rotate an empty marker or the hull. */
    FIXED_GUN("AttackBomber",0,"Weapon_Gun0"),
    FIXED_GUN("AttackBomber",1,"Weapon_Gun1"),
    FIXED_GUN("HeavyInterceptor",0,"Weapon_Gun0"),
    FIXED_GUN("HeavyInterceptor",1,"Weapon_Gun1"),
    FIXED_GUN("LightInterceptor",0,"Weapon_Gun0"),
    FIXED_GUN("LightInterceptor",1,"Weapon_Gun1"),
    FIXED_GUN("CloakedFighter",0,"Weapon_Gun0"),
    FIXED_GUN("CloakedFighter",1,"Weapon_Gun1"),
    FIXED_GUN("IonCannonFrigate",0,"Weapon_Gun0"),
    FIXED_GUN("MinelayerCorvette",0,"Weapon_Gun0"),
    FIXED_GUN("MinelayerCorvette",1,"Weapon_Gun1"),
    FIXED_GUN("Carrier",0,"Weapon_Gun0"),
    FIXED_GUN("Carrier",1,"Weapon_Gun1"),
    FIXED_GUN("Carrier",2,"Weapon_Gun2"),
    FIXED_GUN("Carrier",3,"Weapon_Gun3"),
    FIXED_GUN("MissileDestroyer",0,"Weapon_Gun0"),
    FIXED_GUN("MissileDestroyer",1,"Weapon_Gun1"),
    FIXED_GUN("MissileDestroyer",2,"Weapon_Gun2"),
    FIXED_GUN("MissileDestroyer",3,"Weapon_Gun3"),
    FIXED_GUN("MissileDestroyer",4,"Weapon_Gun4"),
    FIXED_GUN("MissileDestroyer",5,"Weapon_Gun5"),
    ATTACHED_GUN("AdvanceSupportFrigate",0,"Weapon_Gun0","Weapon_Gun0"),
    ATTACHED_GUN("LightCorvette",0,"Weapon_Gun0","Weapon_Gun0"),
    ATTACHED_GUN("HeavyCorvette",0,"Weapon_Gun0","Weapon_Gun0"),
    ATTACHED_GUN("HeavyCorvette",1,"Weapon_Gun1","Weapon_Gun0"),
    ATTACHED_GUN("MultiGunCorvette",0,"Weapon_Gun0","Weapon_Gun0"),
    ATTACHED_GUN("MultiGunCorvette",1,"Weapon_Gun1","Weapon_Gun0"),
    ATTACHED_GUN("MultiGunCorvette",2,"Weapon_Gun2","Weapon_Gun0"),
    ATTACHED_GUN("MultiGunCorvette",3,"Weapon_Gun3","Weapon_Gun0"),
    ATTACHED_GUN("MultiGunCorvette",4,"Weapon_Gun4","Weapon_Gun0"),
    ATTACHED_GUN("MultiGunCorvette",5,"Weapon_Gun5","Weapon_Gun0"),
    ATTACHED_GUN("StandardFrigate",0,"Weapon_Gun0","Weapon_Gun0"),
    ATTACHED_GUN("StandardFrigate",1,"Weapon_Gun1","Weapon_Gun0"),
    ATTACHED_GUN("StandardFrigate",2,"Weapon_Gun2","Weapon_Gun0"),
    ATTACHED_GUN("StandardFrigate",5,"Weapon_Gun5","Weapon_Gun0"),
    SPLIT_GUN("StandardDestroyer",0,"Weapon_Gun0","Weapon_Gun1"),
    SPLIT_GUN("StandardDestroyer",1,"Weapon_Gun1","Weapon_Gun1"),
    ATTACHED_GUN("StandardDestroyer",2,"Weapon_Gun2","Weapon_Gun0"),
    ATTACHED_GUN("StandardDestroyer",3,"Weapon_Gun3","Weapon_Gun0"),
    ATTACHED_GUN("HeavyCruiser",4,"Weapon_Gun4","Weapon_Gun1"),
    ATTACHED_GUN("HeavyCruiser",5,"Weapon_Gun5","Weapon_Gun1"),
    ATTACHED_GUN("HeavyCruiser",6,"Weapon_Gun6","Weapon_Gun1"),
    ATTACHED_GUN("HeavyCruiser",7,"Weapon_Gun7","Weapon_Gun1"),
    ATTACHED_GUN("HeavyCruiser",8,"Weapon_Gun8","Weapon_Gun1"),
    ATTACHED_GUN("HeavyCruiser",9,"Weapon_Gun9","Weapon_Gun1"),
    ATTACHED_GUN("HeavyCruiser",0,"Weapon_Turret_Ion1@Hardpoint_Turret_Ion1_Position","Weapon_Turret_Ion1"),
    ATTACHED_GUN("HeavyCruiser",1,"Weapon_Turret_Ion1@Hardpoint_Turret_Ion1_Position","Weapon_Turret_Ion1Slave"),
    ATTACHED_GUN("HeavyCruiser",2,"Weapon_Turret_Ion1@Hardpoint_Turret_Ion2_Position","Weapon_Turret_Ion1"),
    ATTACHED_GUN("HeavyCruiser",3,"Weapon_Turret_Ion1@Hardpoint_Turret_Ion2_Position","Weapon_Turret_Ion1Slave"),
    ATTACHED_GUN("RepairCorvette",0,"Hull","Weapon_Gun0"),
    SPLIT_GUN("HeavyDefender",0,"Hull","Weapon_Gun0"),
    SPLIT_GUN("HeavyDefender",1,"Hull","Weapon_Gun1"),
    SPLIT_GUN("HeavyDefender",2,"Hull","Weapon_Gun2"),
    ATTACHED_GUN("LightDefender",0,"Hull","Weapon_Gun0"),
    ATTACHED_GUN("LightDefender",1,"Hull","Weapon_Gun1")
};
#undef ATTACHED_GUN
#undef SPLIT_GUN
#undef FIXED_GUN
#undef HINGE_GUN
int meshNativeVisualBindClassicGuns(HwRmVisualAsset *asset,const char *type,char *error,size_t size)
{
    unsigned i;
    if (!asset || !type) return 0;
    for (i=0;i<sizeof(nativeGuns)/sizeof(nativeGuns[0]);++i)
    {
        const nativeGunMap *gun=&nativeGuns[i];
        if (!strcmp(type,gun->type) && !hwRmVisualAssetBindGun(asset,gun->gun,gun->slot,
            gun->azimuth,gun->declination,gun->recoil,gun->muzzle,error,size)) return 0;
    }
    if (error && size) error[0]=0;
    return 1;
}
