#ifndef OPENHW_CATACLYSM_SCRIPT_IDS_H
#define OPENHW_CATACLYSM_SCRIPT_IDS_H
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define CATA_MISSION1_BRIEFING_TIMER 0x089a6a54u
#define CATA_MISSION1_TARGET_REFRESH_TIMER 0x830719c2u

/* Retail identifier hash. This clears ASCII bit 0x20 on EVERY character,
   including digits; toupper() is not equivalent (MISSION01 is 087e6ef1). */
static inline uint32_t cataScriptHash(const char *name)
{
    uint32_t hash=0,high;
    if(!name)return 0;
    while(*name){
        hash=(hash<<4)+((uint32_t)(unsigned char)*name++ & ~0x20u);
        high=hash&0xf0000000u;
        if(high)hash^=high>>24;
        hash&=~high;
    }
    return hash;
}

/* Recovered 00491d30/00491e30: high bit forces mission/global scope.
   Use stable names in HW1's saved variable/timer stores, never raw pointers. */
static inline void cataScriptScopedKey(uint32_t scope,uint32_t id,char *key,size_t capacity)
{
    if(id&0x80000000u){scope=0;id&=0x7fffffffu;}
    snprintf(key,capacity,"C%08X_%08X",(unsigned)scope,(unsigned)id);
}

/* Engine/script handshake names are shared with the existing cinematic
   runtime. Retail hashes include G_ as well as setting the global-scope bit. */
static inline void cataScriptVariableKey(uint32_t scope,uint32_t id,char *key,size_t capacity)
{
    uint32_t hash=id&0x7fffffffu;
    if((id&0x80000000u) || !scope){
        static const char *shared[]={"PlayNis","PlayNislet","NisCentreDerelict","NisCentreInstance",
            "ShipsHaveArrived","RecentlyLoaded"};
        size_t i;unsigned n;char original[48];
        for(i=0;i<sizeof(shared)/sizeof(shared[0]);i++){
            snprintf(original,sizeof(original),"G_%s",shared[i]);
            if(cataScriptHash(original)==hash){snprintf(key,capacity,"%s",shared[i]);return;}
        }
        for(n=1;n<=32;n++){
            snprintf(original,sizeof(original),"G_Nis%uComplete",n);
            if(cataScriptHash(original)==hash){snprintf(key,capacity,"Nis%uComplete",n);return;}
            snprintf(original,sizeof(original),"G_Nislet%uComplete",n);
            if(cataScriptHash(original)==hash){snprintf(key,capacity,"Nislet%uComplete",n);return;}
        }
    }
    cataScriptScopedKey(scope,id,key,capacity);
}
#endif
