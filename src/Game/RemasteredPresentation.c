#include "../Modern/DiagnosticPolicy.h"
#include "RemasteredPresentation.h"
#include "NativeVisualRegistry.h"
#include "RemasteredSoundBank.h"
#include "RemasteredSoundPlayback.h"
#include "RemasteredFxRenderer.h"
#include "RemasteredPcmStream.h"
#include "File.h"
#include "Memory.h"
#include "Mesh.h"
#include "Ships.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "mainrgn.h"
#include "Vector.h"
#include "SoundEvent.h"
#include "mainswitches.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct PresentationAnchor {
    uint64_t handle,nativeToken;
    ShipID owner;
    float created;
    unsigned component,gun,random;
    int isGun,stateTimeline;
    const void *projectile;
    uint64_t projectileToken;
    int impact;
    unsigned deathIndex;
    uint64_t deathToken;
    char markers[4097];
} PresentationAnchor;
enum {PRESENTATION_ANCHORS=8192};
static PresentationAnchor anchors[PRESENTATION_ANCHORS];
static unsigned freeAnchors[PRESENTATION_ANCHORS],freeAnchorCount;
static uint64_t nextAnchor=1;
static unsigned presentationRandom=0x7f4a7c15u,diagnostics;
static HwRmSoundBank *soundBank;
static HwRmSoundPlayback *soundPlayback;
static HwRmFxGraph *fxGraph;
static HwRmFxResources *fxResources;
static HwRmFxPlayback *fxPlayback;
static HwRmWeaponPresentation *ionWeapons[2];
typedef struct IonCharge {const void *ship,*target;float created,start,last;unsigned gun;} IonCharge;
static IonCharge ionCharges[2048];
static int availableFx(void *user,const char *choices);
/* Legacy-model ships have no visual package to own a weapon binding. Their
   ion presentation uses the same native RM .wf reader, never ETG artwork. */
static int ionGun(const void *owner,unsigned index){
    const Ship *ship=(const Ship*)owner;const GunStatic *gun;
    if(!ship||!ship->gunInfo||index>=(unsigned)ship->gunInfo->numGuns)return 0;
    gun=ship->gunInfo->guns[index].gunstatic;
    return gun&&(gun->gunsoundtype==GS_LargeIonCannon||gun->gunsoundtype==GS_VeryLargeIonCannon);
}
enum {PROJECTILE_CUES=8192};
typedef struct CapturedCue {const char *fx,*sound;float clampMinimum,clampMaximum;unsigned options;} CapturedCue;
typedef struct ProjectileCue {const void *key;float scale,frame[16],velocity[3],length,impactFrame[16],impactVelocity[3];uint64_t token;unsigned random;int hit,started,visual,beam,impactVisual,defaultIonFire;CapturedCue slots[HW_RM_WEAPON_SLOT_COUNT];} ProjectileCue;
static ProjectileCue projectileCues[PROJECTILE_CUES];
static uint64_t nextProjectileToken=1;
enum {DEATH_PRESENTATIONS=512};
typedef struct DeathPresentation {HwRmVisualInstance *instance;matrix rotation;vector position,velocity;
    double last,end,hullEnd,start;float scale;uint64_t token;unsigned references;int visual;void *hull;} DeathPresentation;
static DeathPresentation deaths[DEATH_PRESENTATIONS];
static uint64_t nextDeathToken=1;
static void resetDeaths(void);
static void updateDeaths(void);
static unsigned projectileHash(const void *key){return (unsigned)(((uintptr_t)key>>4)*2654435761u)&(PROJECTILE_CUES-1);}
static ProjectileCue *projectileCue(const void *key,int create){
    unsigned start=projectileHash(key),i;
    if(!key)return NULL;
    for(i=0;i<PROJECTILE_CUES;++i){ProjectileCue *entry=&projectileCues[(start+i)&(PROJECTILE_CUES-1)];
        if(entry->key==key)return entry;
        if(!entry->key){if(create){memset(entry,0,sizeof(*entry));entry->key=key;return entry;}return NULL;}}
    return NULL;
}
static void eraseProjectile(ProjectileCue *entry){
    unsigned hole=(unsigned)(entry-projectileCues),scan=(hole+1)&(PROJECTILE_CUES-1),visited=0;
    /* Back-shift the cluster instead of accumulating tombstones. Long battles
       must not turn every projectile lookup into an 8192-slot scan. */
    while(projectileCues[scan].key&&visited++<PROJECTILE_CUES-1){unsigned home=projectileHash(projectileCues[scan].key);
        if(((scan-home)&(PROJECTILE_CUES-1))>=((scan-hole)&(PROJECTILE_CUES-1))){projectileCues[hole]=projectileCues[scan];hole=scan;}
        scan=(scan+1)&(PROJECTILE_CUES-1);}
    memset(&projectileCues[hole],0,sizeof(projectileCues[hole]));
}

static void diagnostic(const char *cue,const char *reason)
{
    if(diagnostics++<64)HW_DIAG_FPRINTF(stderr,"[NativePresentation] %s: %s\n",cue?cue:"",reason);
}
static void resetAnchors(void){unsigned i;memset(anchors,0,sizeof(anchors));freeAnchorCount=0;
    for(i=PRESENTATION_ANCHORS;i--;)freeAnchors[freeAnchorCount++]=i;}
static PresentationAnchor *anchorByHandle(uint64_t handle){unsigned slot=(unsigned)(handle&65535u);
    return slot&&slot<=PRESENTATION_ANCHORS&&anchors[slot-1].handle==handle?&anchors[slot-1]:NULL;}
static PresentationAnchor *newAnchor(void){PresentationAnchor *anchor;unsigned slot;
    if(!freeAnchorCount||nextAnchor>(UINT64_MAX>>16))return NULL;
    slot=freeAnchors[--freeAnchorCount];anchor=&anchors[slot];memset(anchor,0,sizeof(*anchor));anchor->handle=(nextAnchor++<<16)|(slot+1);return anchor;}
static Ship *anchorOwner(const PresentationAnchor *anchor)
{
    Node *node;Ship *ship=ShipIDtoShip(anchor->owner,FALSE);
    if(ship&&ship->timeCreated==anchor->created&&!(ship->flags&(SOF_Dead|SOF_Hide)))return ship;
    for(node=universe.ShipList.head;node;node=node->next){ship=(Ship*)listGetStructOfNode(node);
        if(ship->shipID.shipNumber==anchor->owner.shipNumber&&ship->timeCreated==anchor->created&&!(ship->flags&(SOF_Dead|SOF_Hide)))return ship;}
    return NULL;
}
static void markerFrame(const float localMatrix[16],matrix *rotation,const vector *position,float output[16]){
    vector local,world;unsigned column;float converted[9];
    local.x=localMatrix[12];local.y=localMatrix[13];local.z=localMatrix[14];matMultiplyMatByVec(&world,rotation,&local);vecAddTo(world,*position);
    memset(output,0,16*sizeof(float));output[12]=world.x;output[13]=world.y;output[14]=world.z;output[15]=1;
    for(column=0;column<3;++column){vector axis,rotated;axis.x=localMatrix[column*4];axis.y=localMatrix[column*4+1];axis.z=localMatrix[column*4+2];
        matMultiplyMatByVec(&rotated,rotation,&axis);converted[column*3]=rotated.x;converted[column*3+1]=rotated.y;converted[column*3+2]=rotated.z;}
    for(column=0;column<3;++column){output[column]=-converted[3+column];output[4+column]=converted[column];output[8+column]=converted[6+column];}
}
static int resolveFrame(void *user,uint64_t handle,float output[16],float velocity[3])
{
    PresentationAnchor *anchor=anchorByHandle(handle);(void)user;
    if(anchor){
        Ship *ship;HwRmVisualInstance *instance;
        float scale,matrix[16];char error[256];HwRmPresentationEvent event={0};
        if(anchor->deathToken){DeathPresentation *death=&deaths[anchor->deathIndex];
            if(!death->instance||death->token!=anchor->deathToken)return 0;
            event.markers=anchor->markers;
            if(!hwRmVisualInstancePresentationMarker(death->instance,anchor->component,&event,anchor->random,death->scale,matrix,error,sizeof(error)))return 0;
            markerFrame(matrix,&death->rotation,&death->position,output);memcpy(velocity,&death->velocity,3*sizeof(float));return 1;}
        if(anchor->projectile){ProjectileCue *entry=projectileCue(anchor->projectile,0);
            if(!entry||entry->token!=anchor->projectileToken)return 0;
            memcpy(output,anchor->impact?entry->impactFrame:entry->frame,16*sizeof(float));
            memcpy(velocity,anchor->impact?entry->impactVelocity:entry->velocity,3*sizeof(float));return 1;}
        ship=anchorOwner(anchor);
        if(!ship||(instance=meshNativeVisualFindInstance(ship,anchor->nativeToken,&scale))==NULL)return 0;
        if(anchor->stateTimeline>=0&&hwRmVisualInstancePresentationState(instance,anchor->component,(unsigned)anchor->stateTimeline)==0)return 0;
        if(anchor->isGun){
            if(!hwRmVisualInstanceGunMuzzle(instance,anchor->gun,scale,matrix,error,sizeof(error)))return 0;
        }else{
            event.markers=anchor->markers;
            if(!hwRmVisualInstancePresentationMarker(instance,anchor->component,&event,anchor->random,scale,matrix,error,sizeof(error))){
                /* The RM HW1 collector event names front_00/marker_beam, but
                   its imported HOD exposes the nozzle as Weapon_Gun0_Muzzle. */
                if(ship->shiptype!=ResourceCollector || anchor->component!=0 ||
                   (strcmp(anchor->markers,"front_00") && strcmp(anchor->markers,"marker_beam") &&
                    strcmp(anchor->markers,"top_00") && strcmp(anchor->markers,"top_03")) ||
                   !hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(instance),0,
                       "Weapon_Gun0_Muzzle",scale,NULL,NULL,matrix,error,sizeof(error)))return 0;
            }
        }
        markerFrame(matrix,&ship->rotinfo.coordsys,&ship->posinfo.position,output);
        if(anchor->isGun && ship->gunInfo && anchor->gun<(unsigned)ship->gunInfo->numGuns){
            /* The muzzle joint locates the effect, but a gimballed weapon can
               aim inside its cone without rotating that joint (no barrel).
               Orient FX +Z along gameplay aim, keeping the muzzle's roll. */
            vector forward;float right[3],up[3],norm,dot;
            matMultiplyMatByVec(&forward,&ship->rotinfo.coordsys,&ship->gunInfo->guns[anchor->gun].gunheading);
            norm=sqrtf(forward.x*forward.x+forward.y*forward.y+forward.z*forward.z);
            if(isfinite(norm)&&norm>0.000001f){
                forward.x/=norm;forward.y/=norm;forward.z/=norm;
                dot=output[0]*forward.x+output[1]*forward.y+output[2]*forward.z;
                right[0]=output[0]-dot*forward.x;right[1]=output[1]-dot*forward.y;right[2]=output[2]-dot*forward.z;
                norm=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
                if(norm<0.000001f){
                    right[0]=output[5]*forward.z-output[6]*forward.y;
                    right[1]=output[6]*forward.x-output[4]*forward.z;
                    right[2]=output[4]*forward.y-output[5]*forward.x;
                    norm=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
                }
                if(norm>0.000001f){unsigned i;for(i=0;i<3;++i)right[i]/=norm;
                    up[0]=forward.y*right[2]-forward.z*right[1];up[1]=forward.z*right[0]-forward.x*right[2];up[2]=forward.x*right[1]-forward.y*right[0];
                    memcpy(output,right,sizeof(right));memcpy(output+4,up,sizeof(up));
                    output[8]=forward.x;output[9]=forward.y;output[10]=forward.z;
                }
            }
        }
        velocity[0]=ship->posinfo.velocity.x;velocity[1]=ship->posinfo.velocity.y;velocity[2]=ship->posinfo.velocity.z;return 1;
    }
    return 0;
}
static int resolveAnchor(void *user,uint64_t handle,float output[3]){
    float frame[16],velocity[3];if(!resolveFrame(user,handle,frame,velocity))return 0;
    output[0]=frame[12];output[1]=frame[13];output[2]=frame[14];return 1;
}
static int resolveFxFrame(void *user,uint64_t handle,float output[16],float velocity[3],float *length){PresentationAnchor *anchor;
    if(!resolveFrame(user,handle,output,velocity))return 0;
    anchor=anchorByHandle(handle);
    if(anchor&&anchor->projectile&&!anchor->impact){ProjectileCue *entry=projectileCue(anchor->projectile,0);if(entry&&length)*length=entry->length;}
    return 1;
}
static void releaseAnchor(void *user,uint64_t handle)
{
    PresentationAnchor *anchor=anchorByHandle(handle);(void)user;
    if(anchor){unsigned slot=(unsigned)(anchor-anchors);
        if(anchor->deathToken){DeathPresentation *death=&deaths[anchor->deathIndex];if(death->token==anchor->deathToken&&death->references)--death->references;}
        memset(anchor,0,sizeof(*anchor));freeAnchors[freeAnchorCount++]=slot;}
}
static PresentationAnchor *allocateAnchor(const Ship *ship,HwRmVisualInstance *instance,unsigned component,const char *markers,unsigned gun,int isGun){
    PresentationAnchor *anchor;uint64_t token;
    if(!ship||!instance)return NULL;
    if(!markers)markers="";if(strlen(markers)>=sizeof(anchors[0].markers))return NULL;
    token=meshNativeVisualInstanceToken(instance);if(!token)return NULL;
    anchor=newAnchor();if(!anchor)return NULL;anchor->nativeToken=token;anchor->stateTimeline=-1;
    strcpy(anchor->markers,markers);anchor->owner=ship->shipID;anchor->created=ship->timeCreated;anchor->component=component;anchor->gun=gun;anchor->isGun=isGun;
    presentationRandom=presentationRandom*1664525u+1013904223u;anchor->random=presentationRandom;return anchor;
}
static int precache(void *user,const char *resource){(void)user;return rmPcmEffectPrecache(resource);}
static int play(void *user,int clip,float volume,float pan){(void)user;return rmPcmEffectPlay(clip,volume,pan);}
static int update(void *user,int voice,float volume,float pan){(void)user;return rmPcmEffectUpdate(voice,volume,pan);}
static int active(void *user,int voice){(void)user;return rmPcmEffectActive(voice);}
static void stop(void *user,int voice){(void)user;rmPcmEffectStop(voice);}
static int prepare(void *user,const void *ship,HwRmVisualInstance *instance){(void)user;return meshRemasteredSynchronizeInstance(ship,instance);}
static void attachedSound(const Ship *ship,HwRmVisualInstance *instance,unsigned component,
    const char *markers,unsigned gun,int isGun,const char *cue,int stateTimeline)
{
    uint64_t handle;float position[3];char error[256];PresentationAnchor *anchor;
    if(!soundPlayback||!cue||!*cue)return;
    /* Reap finished voices before allocating from the attachment budget. */
    hwRmSoundPlaybackAdvance(soundPlayback,universe.totaltimeelapsed);
    anchor=allocateAnchor(ship,instance,component,markers,gun,isGun);if(!anchor){diagnostic(cue,"sound attachment unavailable");return;}
    anchor->stateTimeline=stateTimeline;handle=anchor->handle;
    if(!resolveAnchor(NULL,handle,position)||!hwRmSoundPlaybackPlay(soundPlayback,cue,anchor->random,position,handle,error,sizeof(error))){
        diagnostic(cue,"native sound selection, attachment or voice unavailable");releaseAnchor(NULL,handle);
    }
}
static void attachedFx(const Ship *ship,HwRmVisualInstance *instance,unsigned component,const char *markers,unsigned gun,int isGun,
    const char *cue,float scale,float length,int orphan,int stateTimeline){
    PresentationAnchor *anchor;float frame[16],velocity[3];uint64_t handle;unsigned random;
    if(!fxPlayback||!cue||!*cue)return;
    anchor=allocateAnchor(ship,instance,component,markers,gun,isGun);if(!anchor){diagnostic(cue,"FX attachment unavailable");return;}
    anchor->stateTimeline=stateTimeline;handle=anchor->handle;random=anchor->random;
    if(!resolveFrame(NULL,handle,frame,velocity)){releaseAnchor(NULL,handle);diagnostic(cue,"FX marker unavailable");return;}
    if(orphan){releaseAnchor(NULL,handle);handle=0;}
    if(!hwRmFxPlaybackSpawn(fxPlayback,cue,random,universe.totaltimeelapsed,frame,velocity,scale,length,handle)){
        if(handle)releaseAnchor(NULL,handle);diagnostic(cue,"native FX graph or playback capacity unavailable");}
}
static void modelEvent(void *user,const void *owner,HwRmVisualInstance *instance,float scale,unsigned component,
    const HwRmPresentationEvent *event,double clock)
{
    const Ship *ship=(const Ship*)owner;
    const HwRmPresentationEvents *events=hwRmVisualInstancePresentation(instance,component);
    const HwRmPresentationTimeline *timeline=events?hwRmPresentationTimelineAt(events,event->timeline):NULL;
    (void)user;(void)scale;(void)clock;
    int stateTimeline=hwRmVisualInstancePresentationState(instance,component,event->timeline)<0?-1:(int)event->timeline;
    unsigned gun=0;int muzzle=hwRmVisualInstanceEventGun(instance,component,event,&gun);
    attachedSound(ship,instance,component,event->markers,0,0,event->sound,stateTimeline);
    /* The original HW1 resourcing effect already follows the collector and
       its resource. Keep RM event audio, but never layer the RM harvest mesh
       over the upscaled ETG artwork, including with older staged events. */
    if(ship->shiptype!=ResourceCollector||!timeline||strncmp(timeline->name,"Resourcing",10))
        attachedFx(ship,instance,component,event->markers,gun,muzzle,event->fx,
            scale*((event->flags&(HW_RM_EVENT_ENTITY_SCALE|HW_RM_EVENT_RADIUS_SCALE))?1:event->scale),
            event->lengthMode==0?event->length*scale:0,event->orphan,stateTimeline);
}
static void weaponEvent(void *user,const void *owner,HwRmVisualInstance *instance,float scale,unsigned gun,
    unsigned slot,const HwRmWeaponPresentationSlot *presentation,double clock)
{
    (void)user;(void)scale;(void)slot;(void)clock;
    attachedSound((const Ship*)owner,instance,0,NULL,gun,1,presentation->sound,-1);
    attachedFx((const Ship*)owner,instance,0,NULL,gun,1,presentation->fx,scale,0,0,-1);
}
float rmPresentationIonChargeRemaining(const void *owner,unsigned gun)
{
    const Ship *ship=(const Ship *)owner;
    unsigned i;
    float now=universe.totaltimeelapsed;
    if(!ship) return 0;
    for(i=0;i<2048;i++)
    {
        const IonCharge *charge=&ionCharges[i];
        if(charge->ship==owner && charge->created==ship->timeCreated && charge->gun==gun &&
           now>=charge->last && now-charge->last<=1.0f)
            return max(0.0f,2.0f-(now-charge->start));
    }
    return 0;
}
int rmPresentationPrepareIonShot(const void *owner,unsigned gun,const void *target){
    const Ship *ship=(const Ship*)owner;unsigned i,first,freeSlot=2048;float now=universe.totaltimeelapsed,scale;
    HwRmVisualInstance *instance;
    if(!ionGun(owner,gun))return 1;
    /* The HW1-race RM ion definitions use a two-second anticipation phase.
       This is a firing gate, independent of audio/graphics availability. */
    first=(unsigned)(((uintptr_t)owner>>4)+gun*31u)&2047u;
    for(i=0;i<2048;++i){unsigned slot=(first+i)&2047u;IonCharge *charge=&ionCharges[slot];
        if(charge->ship==owner&&charge->created==ship->timeCreated&&charge->gun==gun){
            if(charge->target==target&&now>=charge->last&&now-charge->last<=1.0f){
                charge->last=now;
                if(now-charge->start<2.0f)return 0;
                memset(charge,0,sizeof(*charge));return 2;
            }
            freeSlot=slot;break;
        }
        if(freeSlot==2048&&(!charge->ship||now<charge->last||now-charge->last>1.0f))freeSlot=slot;
    }
    if(freeSlot==2048){diagnostic("ion charge","charge capacity exhausted");return 0;}
    ionCharges[freeSlot]=(IonCharge){owner,target,ship->timeCreated,now,now,gun};
    meshRemasteredNotifyWeaponFire((void*)owner,gun,now);
    instance=meshRemasteredInstance(owner,&scale);
    if(instance&&!hwRmVisualInstanceGunHasFx(instance,gun,availableFx,NULL))
        attachedFx(ship,instance,0,NULL,gun,1,"ion_beam_charge_combo_kus",scale,0,0,-1);
    return 0;
}
typedef struct DamageDispatch {const Ship *ship;HwRmVisualInstance *instance;float scale;} DamageDispatch;
static void damageEvent(void *user,unsigned component,const HwRmPresentationEvent *event,double clock){
    DamageDispatch *dispatch=(DamageDispatch*)user;
    modelEvent(NULL,dispatch->ship,dispatch->instance,dispatch->scale,component,event,clock);
}
static void updateDamage(void){Node *node;
    for(node=universe.ShipList.head;node;node=node->next){Ship *ship=(Ship*)listGetStructOfNode(node);DamageDispatch dispatch;unsigned component;float magnitude;
        if(ship->flags&(SOF_Dead|SOF_Hide)||!ship->staticinfo||ship->staticinfo->maxhealth<=0)continue;
        dispatch.instance=meshRemasteredInstance(ship,&dispatch.scale);if(!dispatch.instance)continue;
        dispatch.ship=ship;magnitude=1.0f-ship->health/ship->staticinfo->maxhealth;
        if(magnitude<0)magnitude=0;if(magnitude>1)magnitude=1;
        meshRemasteredSynchronizeInstance(ship,dispatch.instance);
        for(component=0;component<256;++component){const HwRmPresentationEvents *events=hwRmVisualInstancePresentation(dispatch.instance,component);
            if(!events)break;
            if(hwRmPresentationFindTimeline(events,"Damage")>=0)
                hwRmVisualInstanceSetPresentationState(dispatch.instance,component,"Damage",magnitude,universe.totaltimeelapsed,damageEvent,&dispatch);
            if(ship->shiptype==ResourceCollector && hwRmPresentationFindTimeline(events,"Resourcing")>=0)
                hwRmVisualInstanceSetPresentationState(dispatch.instance,component,"Resourcing",
                    ship->rceffect?1.0f:0.0f,universe.totaltimeelapsed,damageEvent,&dispatch);
        }
    }
}
static void *readPresentationBank(const char *name,sdword maximum,sdword *size){void *bytes=NULL;
    if(!fileExists((char*)name,0))return NULL;
    *size=fileSizeGet((char*)name,0);if(*size<=0||*size>maximum){diagnostic(name,"invalid presentation bank size");return NULL;}
    if(fileLoadAlloc((char*)name,&bytes,FF_ReturnNULLOnFail)!=*size||!bytes){if(bytes)memFree(bytes);diagnostic(name,"presentation bank read failed");return NULL;}
    return bytes;
}
static int startupFx(void){void *bytes=NULL;sdword size;char error[256];
    HwRmFxPlaybackOps ops={resolveFxFrame,releaseAnchor,rmFxRendererDraw};
    const char *graph="RemasteredFX\\Native\\effects.fxbank",*resources="RemasteredFX\\Native\\effects.fxresources";
    if(!fileExists((char*)graph,0)||!fileExists((char*)resources,0))return 0;
    bytes=readPresentationBank(graph,64*1024*1024,&size);if(!bytes)return 0;
    fxGraph=hwRmFxGraphRead(bytes,(size_t)size,error,sizeof(error));memFree(bytes);bytes=NULL;if(!fxGraph){diagnostic(graph,error);return 0;}
    bytes=readPresentationBank(resources,64*1024*1024,&size);if(!bytes)goto failed;
    fxResources=hwRmFxResourcesRead(bytes,(size_t)size,error,sizeof(error));memFree(bytes);if(!fxResources){diagnostic(resources,error);goto failed;}
    if(!rmFxRendererStartup(fxResources))goto failed;
    fxPlayback=hwRmFxPlaybackCreate(fxGraph,&ops,NULL);if(fxPlayback)return 1;
failed:
    rmFxRendererShutdown();hwRmFxResourcesFree(fxResources);fxResources=NULL;hwRmFxGraphFree(fxGraph);fxGraph=NULL;return 0;
}
void rmPresentationStartup(void)
{
    const char *path="RemasteredAudio\\sfx\\effects.sfxbank";void *bytes=NULL;sdword size;char error[256];
    HwRmSoundPlaybackOps ops={precache,play,update,active,stop,resolveAnchor,releaseAnchor};
    rmPresentationShutdown();diagnostics=0;
    startupFx();
    {unsigned i;const char *names[]={"RemasteredFX/WeaponFire/ionbeam.wf","RemasteredFX/WeaponFire/kus_ionbeam.wf"};
    for(i=0;i<2;++i){bytes=readPresentationBank(names[i],1024*1024,&size);
        if(bytes){ionWeapons[i]=hwRmWeaponPresentationRead(bytes,(size_t)size,error,sizeof(error));memFree(bytes);bytes=NULL;
            if(!ionWeapons[i])diagnostic(names[i],error);}}}
    if(fxPlayback){meshNativeVisualSetPresentationSink(modelEvent,weaponEvent,NULL);meshNativeVisualSetPresentationPrepare(prepare);}
    if(!enableSFX||!fileExists((char*)path,0))return;
    size=fileSizeGet((char*)path,0);if(size<=0||size>16*1024*1024){diagnostic(path,"invalid native sound bank size");return;}
    if(fileLoadAlloc((char*)path,&bytes,0)!=size||!bytes){if(bytes)memFree(bytes);diagnostic(path,"native sound bank read failed");return;}
    soundBank=hwRmSoundBankRead(bytes,(size_t)size,error,sizeof(error));memFree(bytes);
    if(!soundBank){diagnostic(path,error);return;}
    soundPlayback=hwRmSoundPlaybackCreate(soundBank,&ops,NULL,error,sizeof(error));
    if(!soundPlayback){diagnostic(path,error);hwRmSoundBankFree(soundBank);soundBank=NULL;return;}
    meshNativeVisualSetPresentationSink(modelEvent,weaponEvent,NULL);
    meshNativeVisualSetPresentationPrepare(prepare);
    HW_DIAG_FPRINTF(stderr,"[NativePresentation] RM sound playback enabled; samples precached before gameplay.\n");
}
void rmPresentationReset(void)
{
    memset(ionCharges,0,sizeof(ionCharges));
    hwRmSoundPlaybackStop(soundPlayback);hwRmFxPlaybackReset(fxPlayback);resetDeaths();resetAnchors();
    memset(projectileCues,0,sizeof(projectileCues));
}
void rmPresentationShutdown(void)
{
    memset(ionCharges,0,sizeof(ionCharges));
    hwRmWeaponPresentationFree(ionWeapons[0]);hwRmWeaponPresentationFree(ionWeapons[1]);ionWeapons[0]=ionWeapons[1]=NULL;
    if(soundPlayback||fxPlayback)meshNativeVisualSetPresentationSink(NULL,NULL,NULL);
    hwRmFxPlaybackFree(fxPlayback);fxPlayback=NULL;rmFxRendererShutdown();hwRmFxResourcesFree(fxResources);fxResources=NULL;hwRmFxGraphFree(fxGraph);fxGraph=NULL;
    hwRmSoundPlaybackFree(soundPlayback);soundPlayback=NULL;hwRmSoundBankFree(soundBank);soundBank=NULL;
    resetDeaths();
    resetAnchors();
    memset(projectileCues,0,sizeof(projectileCues));
}
void rmPresentationUpdate(void)
{
    vector forward,right;unsigned i;Node *node;
    if(!soundPlayback&&!fxPlayback)return;
    updateDamage();updateDeaths();
    for(node=universe.BulletList.head;node;node=node->next)rmPresentationTrackProjectile(listGetStructOfNode(node));
    for(node=universe.MissileList.head;node;node=node->next)rmPresentationTrackProjectile(listGetStructOfNode(node));
    if(mrCamera){vecSub(forward,mrCamera->lookatpoint,mrCamera->eyeposition);vecCrossProduct(right,forward,mrCamera->upvector);
        hwRmSoundPlaybackListener(soundPlayback,(const float*)&mrCamera->eyeposition,(const float*)&right);}
    for(i=0;i<PRESENTATION_ANCHORS;++i)if(anchors[i].handle){
        if(anchors[i].projectile||anchors[i].deathToken)continue;
        Ship *ship=anchorOwner(&anchors[i]);float scale;HwRmVisualInstance *instance;
        if(ship&&(instance=meshNativeVisualFindInstance(ship,anchors[i].nativeToken,&scale))!=NULL)
            meshRemasteredSynchronizeInstance(ship,instance);
    }
    hwRmSoundPlaybackAdvance(soundPlayback,universe.totaltimeelapsed);
    hwRmFxPlaybackUpdate(fxPlayback,universe.totaltimeelapsed);
}
void rmPresentationHullContact(const float position[3])
{
    char error[256];
    /* Reuse an installed RM hull-hit cue; the native SFX bus and spatial
       attenuation apply. Contact debouncing belongs to collision handling. */
    if(soundPlayback && enableSFX)
        hwRmSoundPlaybackPlay(soundPlayback,"weapon/megaship/impact/weapon_megaship_hull_defence_gun_hit",
            (unsigned)(universe.totaltimeelapsed*1000.0f),position,0,error,sizeof(error));
}
void rmPresentationDraw(Camera *camera){unsigned i;if(!camera)return;
    for(i=0;i<DEATH_PRESENTATIONS;++i)if(deaths[i].instance&&deaths[i].hull&&universe.totaltimeelapsed<deaths[i].hullEnd)
        meshRemasteredDeathHullDraw(deaths[i].hull,deaths[i].instance,&deaths[i].rotation,&deaths[i].position);
    if(fxPlayback){rmFxRendererBegin(camera);hwRmFxPlaybackDraw(fxPlayback);rmFxRendererEnd();}}
bool32 rmPresentationOwnsGunSound(const void *ship,unsigned gun)
{
    return soundPlayback&&(meshRemasteredGunHasSound(ship,gun)||ionGun(ship,gun));
}
static int availableFx(void *user,const char *choices){(void)user;return fxGraph&&hwRmFxGraphSelect(fxGraph,choices,0)>=0;}
bool32 rmPresentationOwnsGunFx(const void *ship,unsigned gun){
    float scale;HwRmVisualInstance *instance;if(!fxPlayback)return FALSE;
    instance=meshRemasteredInstance(ship,&scale);
    return (instance&&hwRmVisualInstanceGunHasFx(instance,gun,availableFx,NULL))||
        (ionGun(ship,gun)&&ionWeapons[((const Ship*)ship)->shiprace==R1]);
}
static bool32 ownsHarvestCue(const void *owner,int visual){
    const Ship *ship=(const Ship*)owner;HwRmVisualInstance *instance;const HwRmPresentationEvents *events;
    float scale;int timeline;unsigned i;
    if(!ship||ship->shiptype!=ResourceCollector||!(visual?fxPlayback:soundPlayback))return FALSE;
    instance=meshRemasteredInstance(ship,&scale);if(!instance)return FALSE;
    events=hwRmVisualInstancePresentation(instance,0);if(!events)return FALSE;
    timeline=hwRmPresentationFindTimeline(events,"Resourcing");if(timeline<0)return FALSE;
    for(i=0;i<hwRmPresentationEventCount(events);++i){const HwRmPresentationEvent *event=hwRmPresentationEventAt(events,i);
        if(event->timeline!=(unsigned)timeline)continue;
        if(visual){if(event->fx&&*event->fx&&hwRmFxGraphSelect(fxGraph,event->fx,0)>=0)return TRUE;}
        else if(event->sound&&*event->sound&&hwRmSoundBankFind(soundBank,event->sound)>=0)return TRUE;
    }
    return FALSE;
}
bool32 rmPresentationOwnsHarvestFx(const void *ship){(void)ship;return FALSE;}
bool32 rmPresentationOwnsHarvestSound(const void *ship){return ownsHarvestCue(ship,0);}
void rmPresentationCaptureProjectile(const void *projectile,const void *ship,unsigned gun){
    float scale;unsigned slot;ProjectileCue *entry;const HwRmWeaponPresentation *weapon;HwRmVisualInstance *instance;
    if(!soundPlayback&&!fxPlayback)return;
    scale=1;instance=meshRemasteredInstance(ship,&scale);
    weapon=instance?hwRmVisualInstanceWeaponPresentation(instance,gun):NULL;
    if(!weapon&&ionGun(ship,gun))weapon=ionWeapons[((const Ship*)ship)->shiprace==R1];
    if(!weapon)return;
    entry=projectileCue(projectile,1);if(!entry){diagnostic("projectile","presentation snapshot capacity exhausted");return;}
    memset(entry,0,sizeof(*entry));entry->key=projectile;entry->scale=scale;entry->token=nextProjectileToken++;presentationRandom=presentationRandom*1664525u+1013904223u;entry->random=presentationRandom;
    for(slot=0;slot<HW_RM_WEAPON_SLOT_COUNT;++slot){const HwRmWeaponPresentationSlot *source=hwRmWeaponPresentationAt(weapon,slot);int index;
        if(!source)continue;
        entry->slots[slot].clampMinimum=source->clampMinimum;entry->slots[slot].clampMaximum=source->clampMaximum;entry->slots[slot].options=source->authoredOptions;
        index=hwRmFxGraphSelect(fxGraph,source->fx,entry->random+slot);
        if(index>=0)entry->slots[slot].fx=hwRmFxGraphAt(fxGraph,(unsigned)index)->name;
        index=hwRmSoundBankFind(soundBank,source->sound);
        if(index>=0)entry->slots[slot].sound=hwRmSoundBankAt(soundBank,(unsigned)index)->name;
    }
    if(ionGun(ship,gun)&&!instance){
        entry->defaultIonFire=1;
        int index=hwRmFxGraphFind(fxGraph,"ion_beam_charge_combo_kus");
        if(index>=0)entry->slots[HW_RM_WEAPON_FIRE].fx=hwRmFxGraphAt(fxGraph,(unsigned)index)->name;
        index=hwRmSoundBankFind(soundBank,"WEAPON/FRIGATE/FIRE/WEAPON_FRIGATE_ION_CANNON_BEAM");
        if(index>=0)entry->slots[HW_RM_WEAPON_FIRE].sound=hwRmSoundBankAt(soundBank,(unsigned)index)->name;
    }
}
static uint64_t projectileAnchor(const ProjectileCue *entry,int impact){PresentationAnchor *anchor=newAnchor();
    if(!anchor)return 0;anchor->projectile=entry->key;anchor->projectileToken=entry->token;anchor->impact=impact;return anchor->handle;
}
void rmPresentationTrackProjectile(const void *projectile){
    ProjectileCue *entry=projectileCue(projectile,0);const SpaceObjRot *object=(const SpaceObjRot*)projectile;unsigned column;vector axis;uint64_t anchor;char error[256];
    if(!entry)return;
    if(object->objtype==OBJ_BulletType&&((const Bullet*)projectile)->bulletType==BULLET_Beam){entry->beam=1;entry->length=((const Bullet*)projectile)->beamtraveldist;}
    memset(entry->frame,0,sizeof(entry->frame));entry->frame[15]=1;
    for(column=0;column<3;++column){
        const float *rotation=(const float*)&object->rotinfo.coordsys;
        axis.x=rotation[column*3];axis.y=rotation[column*3+1];axis.z=rotation[column*3+2];
        if(column==0){entry->frame[4]=axis.x;entry->frame[5]=axis.y;entry->frame[6]=axis.z;}
        else if(column==1){entry->frame[0]=-axis.x;entry->frame[1]=-axis.y;entry->frame[2]=-axis.z;}
        else {entry->frame[8]=axis.x;entry->frame[9]=axis.y;entry->frame[10]=axis.z;}}
    memcpy(entry->frame+12,&object->posinfo.position,3*sizeof(float));memcpy(entry->velocity,&object->posinfo.velocity,3*sizeof(float));
    if(entry->started)return;entry->started=1;
    if(entry->defaultIonFire&&fxPlayback&&entry->slots[HW_RM_WEAPON_FIRE].fx&&(anchor=projectileAnchor(entry,0))!=0){
        if(!hwRmFxPlaybackSpawn(fxPlayback,entry->slots[HW_RM_WEAPON_FIRE].fx,entry->random,universe.totaltimeelapsed,entry->frame,entry->velocity,entry->scale,0,anchor))releaseAnchor(NULL,anchor);}
    if(entry->defaultIonFire&&soundPlayback&&entry->slots[HW_RM_WEAPON_FIRE].sound&&(anchor=projectileAnchor(entry,0))!=0){
        if(!hwRmSoundPlaybackPlay(soundPlayback,entry->slots[HW_RM_WEAPON_FIRE].sound,entry->random,entry->frame+12,anchor,error,sizeof(error)))releaseAnchor(NULL,anchor);}
    if(fxPlayback&&entry->slots[HW_RM_WEAPON_BULLET].fx&&(anchor=projectileAnchor(entry,0))!=0){
        entry->visual=hwRmFxPlaybackSpawn(fxPlayback,entry->slots[HW_RM_WEAPON_BULLET].fx,entry->random,universe.totaltimeelapsed,entry->frame,entry->velocity,entry->scale,entry->length,anchor);
        if(!entry->visual)releaseAnchor(NULL,anchor);}
    if(soundPlayback&&entry->slots[HW_RM_WEAPON_BULLET].sound&&(anchor=projectileAnchor(entry,0))!=0){
        if(!hwRmSoundPlaybackPlay(soundPlayback,entry->slots[HW_RM_WEAPON_BULLET].sound,entry->random,entry->frame+12,anchor,error,sizeof(error)))releaseAnchor(NULL,anchor);}
}
bool32 rmPresentationOwnsProjectileFx(const void *projectile){return projectileCue(projectile,0)!=NULL;}
static int worldCue(ProjectileCue *entry,unsigned slot,const float position[3],const float heading[3],const float velocity[3],float magnitude){
    const CapturedCue *cue=&entry->slots[slot];float matrix[16]={0},norm,right[3],up[3];char error[256];int visual=0;uint64_t anchor=0;
    if(!position||!heading||!velocity||!isfinite(magnitude))return 0;
    if(cue->options&HW_RM_WEAPON_HAS_CLAMP)magnitude=fmaxf(cue->clampMinimum,fminf(cue->clampMaximum,magnitude));
    /* Native FX forward is +Z. Preserve HW1's actual shot/contact heading. */
    norm=sqrtf(heading[0]*heading[0]+heading[1]*heading[1]+heading[2]*heading[2]);if(norm<=.000001f)return 0;
    matrix[8]=heading[0]/norm;matrix[9]=heading[1]/norm;matrix[10]=heading[2]/norm;
    if(fabsf(matrix[10])<.99f){right[0]=-matrix[9];right[1]=matrix[8];right[2]=0;}
    else {right[0]=matrix[10];right[1]=0;right[2]=-matrix[8];}
    norm=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);right[0]/=norm;right[1]/=norm;right[2]/=norm;
    up[0]=matrix[9]*right[2]-matrix[10]*right[1];up[1]=matrix[10]*right[0]-matrix[8]*right[2];up[2]=matrix[8]*right[1]-matrix[9]*right[0];
    memcpy(matrix,right,3*sizeof(float));memcpy(matrix+4,up,3*sizeof(float));memcpy(matrix+12,position,3*sizeof(float));matrix[15]=1;
    if(entry->beam&&slot==HW_RM_WEAPON_HIT){
        memcpy(entry->impactFrame,matrix,sizeof(matrix));memcpy(entry->impactVelocity,velocity,3*sizeof(float));
        if(entry->hit)return entry->impactVisual;
        if(cue->fx&&fxPlayback){anchor=projectileAnchor(entry,1);if(!anchor){diagnostic(cue->fx,"beam contact attachment capacity exhausted");return 0;}}
    }
    if(cue->fx&&fxPlayback){visual=hwRmFxPlaybackSpawn(fxPlayback,cue->fx,entry->random+slot,universe.totaltimeelapsed,matrix,velocity,entry->scale*magnitude,0,anchor);
        if(!visual&&anchor)releaseAnchor(NULL,anchor);}
    if(entry->beam&&slot==HW_RM_WEAPON_HIT)entry->impactVisual=visual;
    if(cue->sound&&soundPlayback&&!hwRmSoundPlaybackPlay(soundPlayback,cue->sound,entry->random+slot,position,0,error,sizeof(error)))diagnostic(cue->sound,error);
    return visual;
}
bool32 rmPresentationProjectileImpact(const void *projectile,const float position[3],const float heading[3],const float velocity[3],float damage){
    ProjectileCue *entry=projectileCue(projectile,0);if(!entry)return FALSE;
    /* 004ba0a1 -> 004752c3 -> 004b9dbb: the positive damage magnitude
       is clamped before becoming effect scale. Never clamp each layer's
       authored radius, multiply by the victim's radius, or change damage. */
    /* Ownership is the captured weapon definition, not successful allocation
       of a particular FX instance. Empty slots intentionally draw nothing;
       neither they nor a full FX pool should resurrect Classic effects. */
    worldCue(entry,damage>0?HW_RM_WEAPON_HIT:HW_RM_WEAPON_NOPENETRATE,position,heading,velocity,damage>0?damage:1);
    entry->hit=1;return TRUE;
}
void rmPresentationReleaseProjectile(const void *projectile,const float position[3],const float heading[3],const float velocity[3]){
    ProjectileCue *entry=projectileCue(projectile,0);if(!entry)return;
    if(!entry->hit)worldCue(entry,HW_RM_WEAPON_MISS,position,heading,velocity,1);
    eraseProjectile(entry);
}
static uint64_t deathAnchor(DeathPresentation *death,unsigned component,const char *markers,unsigned random){PresentationAnchor *anchor;
    if(!markers)markers="";if(strlen(markers)>=sizeof(anchors[0].markers))return 0;
    anchor=newAnchor();if(!anchor)return 0;anchor->deathIndex=(unsigned)(death-deaths);anchor->deathToken=death->token;
    anchor->component=component;anchor->random=random;strcpy(anchor->markers,markers);++death->references;return anchor->handle;
}
static void deathEvent(void *user,unsigned component,const HwRmPresentationEvent *event,double scheduled){
    DeathPresentation *death=(DeathPresentation*)user;float frame[16],velocity[3];uint64_t anchor;unsigned random;char error[256];(void)scheduled;
    presentationRandom=presentationRandom*1664525u+1013904223u;random=presentationRandom;
    if(fxPlayback&&event->fx&&*event->fx&&(anchor=deathAnchor(death,component,event->markers,random))!=0){
        if(resolveFrame(NULL,anchor,frame,velocity)){
            float scale=death->scale*((event->flags&(HW_RM_EVENT_ENTITY_SCALE|HW_RM_EVENT_RADIUS_SCALE))?1:event->scale);
            if(event->orphan){releaseAnchor(NULL,anchor);anchor=0;}
            if(!hwRmFxPlaybackSpawn(fxPlayback,event->fx,random,universe.totaltimeelapsed,frame,velocity,scale,event->lengthMode==0?event->length*death->scale:0,anchor)&&anchor)releaseAnchor(NULL,anchor);
        }else releaseAnchor(NULL,anchor);
    }
    if(soundPlayback&&event->sound&&*event->sound&&(anchor=deathAnchor(death,component,event->markers,random))!=0){
        if(!resolveFrame(NULL,anchor,frame,velocity)||!hwRmSoundPlaybackPlay(soundPlayback,event->sound,random,frame+12,anchor,error,sizeof(error)))releaseAnchor(NULL,anchor);
    }
}
bool32 rmPresentationShipDeath(const void *owner){
    const Ship *ship=(const Ship*)owner;HwRmVisualInstance *source;DeathPresentation *death;unsigned i,component;float scale;double duration=0,hullDuration=0;int visual=0,found=0;
    if((!fxPlayback&&!soundPlayback)||!ship||!nextDeathToken)return FALSE;
    source=meshRemasteredInstance(ship,&scale);if(!source)return FALSE;
    for(component=0;component<256;++component){const HwRmPresentationEvents *events=hwRmVisualInstancePresentation(source,component);unsigned kind;
        if(!events)continue;
        for(kind=0;kind<2;++kind){int timeline=hwRmPresentationFindTimeline(events,kind?"Deathtrail":"Death");
            if(timeline<0)continue;found=1;duration=fmax(duration,hwRmPresentationTimelineAt(events,(unsigned)timeline)->duration);
            if(!kind)hullDuration=fmax(hullDuration,hwRmPresentationTimelineAt(events,(unsigned)timeline)->duration);
            for(i=0;i<hwRmPresentationEventCount(events);++i){const HwRmPresentationEvent *event=hwRmPresentationEventAt(events,i);
                if(event->timeline==(unsigned)timeline&&fxPlayback&&hwRmFxGraphSelect(fxGraph,event->fx,0)>=0)visual=1;}}}
    if(!found)return FALSE;
    for(i=0;i<DEATH_PRESENTATIONS&&deaths[i].instance;++i);if(i==DEATH_PRESENTATIONS){diagnostic("Death","detached presentation capacity exhausted");return FALSE;}
    meshRemasteredSynchronizeInstance(ship,source);death=&deaths[i];memset(death,0,sizeof(*death));
    death->instance=hwRmVisualInstanceSnapshot(source);if(!death->instance)return FALSE;
    death->token=nextDeathToken++;death->scale=scale;death->rotation=ship->rotinfo.coordsys;death->position=ship->posinfo.position;death->velocity=ship->posinfo.velocity;
    death->last=death->start=universe.totaltimeelapsed;death->end=death->last+duration;
    death->hullEnd=death->last+hullDuration;death->visual=visual;
    death->hull=meshRemasteredDeathHullCreate(ship);
    for(component=0;component<256;++component){const HwRmPresentationEvents *events=hwRmVisualInstancePresentation(death->instance,component);
        if(!events)continue;
        if(hwRmPresentationFindTimeline(events,"Death")>=0)
            hwRmVisualInstanceStartPresentation(death->instance,component,"Death",death->last,deathEvent,death);
        if(hwRmPresentationFindTimeline(events,"Deathtrail")>=0)
            hwRmVisualInstanceStartPresentation(death->instance,component,"Deathtrail",death->last,deathEvent,death);
    }
    /* The snapshot owns this death timeline, including sound-only timelines.
       The original Ship no longer waits for an ETG deletion callback. */
    return TRUE;
}
static void updateDeaths(void){unsigned i;double clock=universe.totaltimeelapsed;
    for(i=0;i<DEATH_PRESENTATIONS;++i)if(deaths[i].instance){DeathPresentation *death=&deaths[i];float dt=(float)(clock-death->last);if(dt<0)continue;
        death->position.x+=death->velocity.x*dt;death->position.y+=death->velocity.y*dt;death->position.z+=death->velocity.z*dt;death->last=clock;
        if(death->hull&&death->hullEnd>death->start){char error[256];double phase=fmin(1.,(clock-death->start)/(death->hullEnd-death->start));
            /* Ships with a source Death MAD clip deform on the same timeline.
               Missing clips leave their current pose intact. */
            hwRmVisualInstanceSampleSourceClip(death->instance,"Death",phase,error,sizeof(error));}
        if(clock<=death->end)hwRmVisualInstanceUpdatePresentation(death->instance,clock,deathEvent,death);
        else {hwRmVisualInstanceUpdatePresentation(death->instance,death->end,deathEvent,death);hwRmVisualInstanceStopPresentation(death->instance);}
        if(clock>=death->hullEnd&&death->hull){meshRemasteredDeathHullFree(death->hull);death->hull=NULL;}
        if(clock>=death->end&&!death->references){hwRmVisualInstanceFree(death->instance);memset(death,0,sizeof(*death));}
    }
}
static void resetDeaths(void){unsigned i;for(i=0;i<DEATH_PRESENTATIONS;++i){meshRemasteredDeathHullFree(deaths[i].hull);hwRmVisualInstanceFree(deaths[i].instance);memset(&deaths[i],0,sizeof(deaths[i]));}}
