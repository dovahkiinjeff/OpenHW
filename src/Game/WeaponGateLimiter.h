#ifndef OPENHW_WEAPON_GATE_LIMITER_H
#define OPENHW_WEAPON_GATE_LIMITER_H
#include <string.h>
#define WEAPON_GATE_SLOTS 512
#define WEAPON_GATE_PER_SECOND 16
#define WEAPON_GATE_TOTAL 2048
typedef struct WeaponGateSample {
    unsigned id,gun,reason,valid;float created,next;
} WeaponGateSample;
typedef struct WeaponGateLimiter {
    WeaponGateSample samples[WEAPON_GATE_SLOTS];
    float window,last;unsigned used,total;
} WeaponGateLimiter;
static int weaponGateAllow(WeaponGateLimiter *limit,unsigned id,unsigned gun,
    unsigned reason,float created,float now,float interval)
{
    unsigned start,i;WeaponGateSample *freeSlot=NULL;
    if(now<limit->last)memset(limit,0,sizeof(*limit));
    limit->last=now;
    if(now-limit->window>=1){limit->window=now;limit->used=0;}
    if(limit->used>=WEAPON_GATE_PER_SECOND || limit->total>=WEAPON_GATE_TOTAL)return 0;
    start=(id*16777619u+gun*31u+reason)&(WEAPON_GATE_SLOTS-1);
    for(i=0;i<16;i++){
        WeaponGateSample *s=&limit->samples[(start+i)&(WEAPON_GATE_SLOTS-1)];
        if(s->valid && s->id==id && s->gun==gun && s->reason==reason && s->created==created){
            if(now<s->next)return 0;
            freeSlot=s;break;
        }
        if(!freeSlot && (!s->valid || now>=s->next))freeSlot=s;
    }
    /* A full cache drops a sample. It must never turn off throttling. */
    if(!freeSlot)return 0;
    freeSlot->id=id;freeSlot->gun=gun;freeSlot->reason=reason;
    freeSlot->created=created;freeSlot->next=now+interval;freeSlot->valid=1;
    ++limit->used;++limit->total;return 1;
}
#endif
