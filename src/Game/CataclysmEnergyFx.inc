#include "../Modern/CataclysmData.h"
static etglod *cataEnergyTables[2][4][EGT_NumberGunTypes],*hw1EnergyTables[2][4][EGT_NumberGunTypes];
static color cataEnergyColors[2][4],hw1EnergyColors[2][4];
static bool32 cataEnergyLoaded,cataEnergyActive;
static const sdword cataEnergySounds[]={GS_SmallEnergyCannon,GS_MediumEnergyCannon,GS_LargeEnergyCannon,GS_VeryLargeEnergyCannon};
static void etgCopyEnergyTables(etglod *table[2][4][EGT_NumberGunTypes],color colors[2][4],bool32 restore)
{
    sdword r,g,e;for(r=0;r<2;r++)for(g=0;g<4;g++){
        if(restore)etgBulletColor[r][cataEnergySounds[g]]=colors[r][g];
        else colors[r][g]=etgBulletColor[r][cataEnergySounds[g]];
        for(e=0;e<EGT_NumberGunTypes;e++){
            if(restore)etgGunEventTable[r][cataEnergySounds[g]][e]=table[r][g][e];
            else table[r][g][e]=etgGunEventTable[r][cataEnergySounds[g]][e];
        }
    }
}
void etgSelectCataclysmEnergyFx(bool32 enabled)
{
    sdword r,g,e;if(enabled==cataEnergyActive)return;
    if(enabled){
        if(!cataEnergyLoaded){
            if(!hwCataEnergyFxScript())dbgFatalf(DBG_Loc,"Owned Cataclysm energy effects are unavailable");
            etgCopyEnergyTables(hw1EnergyTables,hw1EnergyColors,FALSE);
            for(r=0;r<2;r++)for(g=0;g<4;g++)for(e=0;e<EGT_NumberGunTypes;e++)
                etgGunEventTable[r][cataEnergySounds[g]][e]=NULL;
            scriptSet("Cataclysm\\ETG\\","Cata_energy.script",etgScriptParseTable);
            etgCopyEnergyTables(cataEnergyTables,cataEnergyColors,FALSE);cataEnergyLoaded=TRUE;
        }else etgCopyEnergyTables(cataEnergyTables,cataEnergyColors,TRUE);
    }else etgCopyEnergyTables(hw1EnergyTables,hw1EnergyColors,TRUE);
    cataEnergyActive=enabled;
}
static void etgFreeCataclysmEnergyFx(void)
{
    sdword r,g,e;if(cataEnergyActive)etgSelectCataclysmEnergyFx(FALSE);
    if(cataEnergyLoaded)for(r=0;r<2;r++)for(g=0;g<4;g++)for(e=0;e<EGT_NumberGunTypes;e++)
        if(cataEnergyTables[r][g][e])memFree(cataEnergyTables[r][g][e]);
    memset(cataEnergyTables,0,sizeof(cataEnergyTables));cataEnergyLoaded=FALSE;
}
