#ifndef HW_NATIVE_VISUAL_SCOPE_H
#define HW_NATIVE_VISUAL_SCOPE_H
#include <string.h>
/* Exact existing identities, not prefixes that enable other races. Derelict
   package identities are qualified because their names overlap fleet ships. */
static int hwRmNativeR2Type(const char *name)
{
    static const char *types[]={"AdvanceSupportFrigate","AttackBomber","Carrier","CloakGenerator",
        "DefenseFighter","DFGFrigate","GravWellGenerator","HeavyCorvette","HeavyCruiser",
        "HeavyDefender","HeavyInterceptor","IonCannonFrigate","LightCorvette","LightInterceptor",
        "MinelayerCorvette","MissileDestroyer","Mothership","MultiGunCorvette","Probe",
        "ProximitySensor","RepairCorvette","ResearchShip","ResourceCollector","ResourceController",
        "SalCapCorvette","SensorArray","StandardDestroyer","StandardFrigate","TargetDrone"};
    unsigned i;if(name) for(i=0;i<sizeof(types)/sizeof(types[0]);++i) if(!strcmp(name,types[i])) return 1;
    return 0;
}
static int hwRmNativeP1Type(const char *name)
{
    static const char *types[]={"P1Fighter","P1StandardCorvette","P1MissileCorvette","P1IonArrayFrigate","P1Mothership"};
    unsigned i;if(name) for(i=0;i<sizeof(types)/sizeof(types[0]);++i) if(!strcmp(name,types[i])) return 1;
    return 0;
}
static int hwRmNativeTraderType(const char *name)
{return name && (!strcmp(name,"FloatingCity") || !strcmp(name,"ResearchStation") || !strcmp(name,"Ghostship") || !strcmp(name,"JunkYardHQ") || !strcmp(name,"MiningBase"));}
static int hwRmNativeP3Type(const char *name)
{return name && !strcmp(name,"P3Megaship");}
static int hwRmNativeP2Type(const char *name)
{
    static const char *types[]={"P2Swarmer","P2AdvanceSwarmer","P2FuelPod","P2MultiBeamFrigate","P2Mothership"};
    unsigned i;if(name) for(i=0;i<sizeof(types)/sizeof(types[0]);++i) if(!strcmp(name,types[i])) return 1;
    return 0;
}
static int hwRmNativeDerelictType(const char *name)
{
    static const char *types[]={"Crate","FragmentPanel0a","FragmentPanel0b","FragmentPanel0c",
        "FragmentPanel1","FragmentPanel2","FragmentPanel3","FragmentStrut","PrisonShipOld","PrisonShipNew",
        "Scaffold","Scaffold_scarred","ScaffoldFingerA_scarred","ScaffoldFingerB_scarred","Shipwreck",
        "AdvanceSupportFrigate","Carrier","DDDFrigate","HeavyCorvette","HeavyCruiser","IonCannonFrigate",
        "LightCorvette","MinelayerCorvette","MultiGunCorvette","RepairCorvette","ResourceController",
        "SalCapCorvette","StandardFrigate","Junk0_antenna","Junk0_fin1","Junk0_fin2","Junk0_GunAmmo",
        "Junk0_panel","Junk0_sensors","Junk1_partA","Junk1_partB","Junk1_shell","Junk1_strut",
        "Junk2_panelA","Junk2_panelB","Junk2_panelC","Junk2_panelD","Junk2_shipwreck","Junk3_Boiler",
        "Junk3_BoilerCasing","M13PanelA","M13PanelB","M13PanelC","LifeBoat"};
    unsigned i;if(name) for(i=0;i<sizeof(types)/sizeof(types[0]);++i) if(!strcmp(name,types[i])) return 1;
    return 0;
}
#endif
