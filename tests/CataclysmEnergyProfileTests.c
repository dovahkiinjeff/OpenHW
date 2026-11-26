#include "ETG.h"
#include "StatScript.h"
#include "Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
etglod *etgGunEventTable[NUM_RACES][NUM_GUN_SOUND_TYPES][EGT_NumberGunTypes];
color etgBulletColor[NUM_RACES][NUM_GUN_SOUND_TYPES];
scriptEntry etgScriptParseTable[]={{NULL,NULL,NULL}};
static int loads,freed;
void dbgFatalf(char *file,sdword line,char *format,...){(void)file;(void)line;fprintf(stderr,"%s\n",format);abort();}
void memFree(void *p){++freed;free(p);}
const char *hwCataEnergyFxScript(void){return "owned fixture";}
void scriptSet(char *directory,char *filename,scriptEntry *table);
#include "../src/Game/CataclysmEnergyFx.inc"
void scriptSet(char *directory,char *filename,scriptEntry *table)
{
    int r,g,e;(void)table;
    if(strcmp(directory,"Cataclysm\\ETG\\")||strcmp(filename,"Cata_energy.script"))abort();
    ++loads;for(r=0;r<2;r++)for(g=0;g<4;g++){
        etgBulletColor[r][cataEnergySounds[g]]=99;
        for(e=0;e<EGT_NumberGunTypes;e++)
            etgGunEventTable[r][cataEnergySounds[g]][e]=calloc(1,sizeof(etglod));
    }
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void)
{
    int r,g,e,cycle;etglod original={0};etglod *saved[2][4][EGT_NumberGunTypes];
    for(r=0;r<NUM_RACES;r++)for(g=0;g<NUM_GUN_SOUND_TYPES;g++){
        etgBulletColor[r][g]=42;
        for(e=0;e<EGT_NumberGunTypes;e++)etgGunEventTable[r][g][e]=&original;
    }
    for(cycle=0;cycle<3;cycle++){
        etgSelectCataclysmEnergyFx(TRUE);CHECK(loads==1);
        for(r=0;r<2;r++)for(g=0;g<4;g++)for(e=0;e<EGT_NumberGunTypes;e++){
            etglod *value=etgGunEventTable[r][cataEnergySounds[g]][e];
            CHECK(value&&value!=&original);
            if(!cycle)saved[r][g][e]=value;else CHECK(saved[r][g][e]==value);
            CHECK(etgBulletColor[r][cataEnergySounds[g]]==99);
        }
        CHECK(etgGunEventTable[R1][GS_LargeProjectile][0]==&original);
        etgSelectCataclysmEnergyFx(FALSE);
        for(r=0;r<NUM_RACES;r++)for(g=0;g<NUM_GUN_SOUND_TYPES;g++){
            CHECK(etgBulletColor[r][g]==42);
            for(e=0;e<EGT_NumberGunTypes;e++)CHECK(etgGunEventTable[r][g][e]==&original);
        }
    }
    etgSelectCataclysmEnergyFx(TRUE);etgFreeCataclysmEnergyFx();
    CHECK(freed==2*4*EGT_NumberGunTypes);
    CHECK(etgGunEventTable[R1][GS_LargeEnergyCannon][0]==&original);
    puts("PASS: owned energy profile caches independently, restores HW1 exactly and releases its tables.");return 0;
}
