#include "CataclysmAI.h"
#include "StatScript.h"
#include "Debug.h"
#include "../Modern/CataclysmData.h"
#include <string.h>

extern scriptEntry AIShipTweaks[],AITrackTweaks[],FlightmanTweaks[];
extern real32 FIGHTER_BANK,MASSIVESHIP_BANK,MASSIVESHIP_TURNPITCH,MASSIVESHIP_DESCENDPITCH;
typedef struct CataAITuningValue {void *address;ubyte value[4];} CataAITuningValue;
static CataAITuningValue baseline[256];
static unsigned baselineCount;
static bool32 cataProfile;

static void remember(void *address)
{
    unsigned i;if(!address)return;
    for(i=0;i<baselineCount;i++)if(baseline[i].address==address)return;
    if(baselineCount>=256)dbgFatalf(DBG_Loc,"Cataclysm AI tuning snapshot capacity exceeded");
    baseline[baselineCount].address=address;
    memcpy(baseline[baselineCount++].value,address,4);
}
static unsigned scalarTable(scriptEntry *target,scriptEntry *source)
{
    unsigned count=0;
    for(;source->name;source++)if(source->dataPtr){
        if(count>=127)dbgFatalf(DBG_Loc,"Cataclysm AI script table capacity exceeded");
        remember(source->dataPtr);target[count++]=*source;
    }
    memset(&target[count],0,sizeof(*target));return count;
}
void cataAISelectProfile(bool32 enabled)
{
    unsigned i;scriptEntry flight[128],tracking[128],manoeuvres[128];
    scriptEntry *entry;
    if(enabled==cataProfile)return;
    if(enabled){
        if(!hwCataDataReady())dbgFatalf(DBG_Loc,"Cataclysm NPC AI requires its owned game data");
        baselineCount=0;
        scalarTable(flight,AIShipTweaks);
        remember(&FIGHTER_BANK);
        remember(&MASSIVESHIP_BANK);remember(&MASSIVESHIP_TURNPITCH);remember(&MASSIVESHIP_DESCENDPITCH);
        /* Retail does not bind FIGHTER_BANK or BIGSHIP_* from the shipped
           script. Keep its defaults rather than invent aliases. Massive-ship
           defaults verified in owned executable at 0074d700/710/720. */
        FIGHTER_BANK=.99f;
        MASSIVESHIP_BANK=.17f;
        MASSIVESHIP_TURNPITCH=.3490658700466156f;
        MASSIVESHIP_DESCENDPITCH=.1745329350233078f;
        scalarTable(tracking,AITrackTweaks);scalarTable(manoeuvres,FlightmanTweaks);
        /* rowDetails is handled per original ship identity. Loading it into
           HW1's type-only array would merge Kushan and Taiidan priorities. */
        scriptSet(NULL,"Cataclysm\\AIShip.script",flight);
        scriptSet(NULL,"Cataclysm\\AITrack.script",tracking);
        scriptSet(NULL,"Cataclysm\\Flightman.script",manoeuvres);
    }else{
        for(i=0;i<baselineCount;i++)memcpy(baseline[i].address,baseline[i].value,4);
        baselineCount=0;
    }
    /* Recalculate reciprocals after loading or restoring the scalar values. */
    for(entry=AIShipTweaks;entry->name;entry++)
        if(!strcmp(entry->name,"AISHIP_OTHER_CALCULATIONS")){
            entry->setVarCB(NULL,"dothem",NULL);break;
        }
    cataProfile=enabled;
}
