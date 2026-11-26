#include "StatScript.h"
#include "Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
static float speed=7,track=8,turn=9;
real32 FIGHTER_BANK=.6f,MASSIVESHIP_BANK=.7f,MASSIVESHIP_TURNPITCH=.3f,MASSIVESHIP_DESCENDPITCH=.4f;
static int loads,recalculations;
void dbgFatalf(char *file,sdword line,char *format,...){(void)file;(void)line;fprintf(stderr,"%s\n",format);exit(1);}
static void storeFloat(char *directory,char *field,void *value){(void)directory;*(float*)value=(float)atof(field);}
void scriptSetSinAngCB(char *directory,char *field,void *value){(void)directory;*(float*)value=sinf((float)atof(field)*.017453292519943295f);}
void scriptSetAngCB(char *directory,char *field,void *value){(void)directory;*(float*)value=(float)atof(field)*.017453292519943295f;}
static void precalc(char *directory,char *field,void *value){(void)directory;(void)field;(void)value;++recalculations;}
static void forbiddenRows(char *directory,char *field,void *value){(void)directory;(void)field;(void)value;abort();}
scriptEntry AIShipTweaks[]={{"Speed",storeFloat,&speed},{"rowDetails",forbiddenRows,NULL},
    {"AISHIP_OTHER_CALCULATIONS",precalc,NULL},{NULL,NULL,NULL}};
scriptEntry AITrackTweaks[]={{"Track",storeFloat,&track},{NULL,NULL,NULL}};
scriptEntry FlightmanTweaks[]={{"Turn",storeFloat,&turn},{NULL,NULL,NULL}};
int hwCataDataReady(void){return 1;}
void scriptSet(char *directory,char *filename,scriptEntry *table)
{
    scriptEntry *entry;(void)directory;
    if(strncmp(filename,"Cataclysm\\",10))abort();
    ++loads;
    for(entry=table;entry->name;entry++){
        if(!strcmp(entry->name,"FIGHTER_BANK")||!strncmp(entry->name,"BIGSHIP_",8))abort();
        entry->setVarCB(NULL,"30",entry->dataPtr);
    }
}
#include "../src/Game/CataclysmAI.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);return 1;}} while(0)
int main(void)
{
    float original[]={speed,track,turn,FIGHTER_BANK,MASSIVESHIP_BANK,MASSIVESHIP_TURNPITCH,MASSIVESHIP_DESCENDPITCH};
    int cycle;
    cataAISelectProfile(FALSE);CHECK(loads==0);
    for(cycle=0;cycle<3;cycle++){
        cataAISelectProfile(TRUE);CHECK(loads==3*(cycle+1));
        CHECK(speed==30&&track==30&&turn==30);
        CHECK(fabsf(FIGHTER_BANK-.99f)<.00001f&&fabsf(MASSIVESHIP_BANK-.17f)<.00001f);
        CHECK(fabsf(MASSIVESHIP_TURNPITCH-.34906587f)<.00001f&&fabsf(MASSIVESHIP_DESCENDPITCH-.17453294f)<.00001f);
        cataAISelectProfile(TRUE);CHECK(loads==3*(cycle+1));
        cataAISelectProfile(FALSE);
        {float restored[]={speed,track,turn,FIGHTER_BANK,MASSIVESHIP_BANK,MASSIVESHIP_TURNPITCH,MASSIVESHIP_DESCENDPITCH};
            CHECK(!memcmp(original,restored,sizeof(original)));}
        CHECK(recalculations==2*(cycle+1));
    }
    puts("PASS: Cataclysm AI tuning loads once, preserves per-ship row identities and restores HW1 exactly.");return 0;
}
