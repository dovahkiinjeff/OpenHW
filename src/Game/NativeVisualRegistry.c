#include "NativeVisualRegistry.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct nativeMeshOwner {
    const void *mesh;
    HwRmVisualAsset *asset;
    float scale;
    unsigned int lod;
    struct nativeMeshOwner *next;
} nativeMeshOwner;
typedef struct nativeShipVisual {
    const void *ship;
    nativeMeshOwner *owner;
    HwRmVisualInstance *instance;
    int presentationOwner;
    unsigned long long token;
    struct nativeShipVisual *next;
} nativeShipVisual;
static nativeMeshOwner *nativeOwners;
static nativeShipVisual *nativeShips;
/* A classic MAD can finish while the ship is offscreen. Retain its last pose
   independently of render instances so newly acquired LODs do not unfold. */
typedef struct nativeShipPose {
    const void *ship;
    char *event;
    double phase;
    struct nativeShipPose *next;
} nativeShipPose;
static nativeShipPose *nativePoses;
static MeshNativePresentationVisit presentationEvents;
static MeshNativeWeaponVisit presentationWeapons;
static void *presentationUser;
static MeshNativePresentationPrepare presentationPrepare;
static unsigned long long nextNativeToken=1;
int meshNativeVisualRestGun(const void* mesh,unsigned gun,float* matrix)
{
    nativeMeshOwner* owner;char error[256];int result;HwRmVisualInstance* instance;
    for(owner=nativeOwners;owner;owner=owner->next)if(owner->mesh==mesh)break;
    if(!owner)return 0;instance=hwRmVisualInstanceCreate(owner->asset);if(!instance)return 0;
    result=hwRmVisualInstanceGunMuzzle(instance,gun,owner->scale,matrix,error,sizeof(error));
    hwRmVisualInstanceFree(instance);return result;
}

int meshNativeVisualRegister(const void *mesh,HwRmVisualAsset *asset,float scale,unsigned int lod)
{
    nativeMeshOwner *owner;
    if (!mesh || !asset || !isfinite(scale) || scale<=0 || scale>100000 || lod>15 || !nextNativeToken) return 0;
    for (owner=nativeOwners;owner;owner=owner->next) if (owner->mesh==mesh) return 0;
    owner=(nativeMeshOwner *)malloc(sizeof(*owner));if (!owner) return 0;
    owner->mesh=mesh;owner->asset=asset;owner->scale=scale;owner->lod=lod;
    owner->next=nativeOwners;nativeOwners=owner;return 1;
}
HwRmVisualInstance *meshNativeVisualAcquire(const void *mesh,const void *ship)
{
    nativeMeshOwner *owner;nativeShipVisual *visual;
    if (!ship || !nextNativeToken) return NULL;
    for (visual=nativeShips;visual;visual=visual->next)
        if (visual->ship==ship && visual->owner->mesh==mesh) return visual->instance;
    for (owner=nativeOwners;owner;owner=owner->next) if (owner->mesh==mesh) break;
    if (!owner) return NULL;
    visual=(nativeShipVisual *)malloc(sizeof(*visual));if (!visual) return NULL;
    visual->instance=hwRmVisualInstanceCreate(owner->asset);
    if (!visual->instance) {free(visual);return NULL;}
    visual->ship=ship;visual->owner=owner;visual->next=nativeShips;nativeShips=visual;
    visual->presentationOwner=0;
    visual->token=nextNativeToken++;
    {
        nativeShipPose *pose;
        for(pose=nativePoses;pose;pose=pose->next) if(pose->ship==ship) {
            char error[256];
            hwRmVisualInstanceSampleEvent(visual->instance,pose->event,pose->phase,error,sizeof(error));
            break;
        }
    }
    return visual->instance;
}
void meshNativeVisualReleaseShip(const void *ship)
{
    nativeShipVisual **link=&nativeShips;
    nativeShipPose **poseLink=&nativePoses;
    while(*poseLink) {
        nativeShipPose *pose=*poseLink;
        if(pose->ship==ship) {*poseLink=pose->next;free(pose->event);free(pose);}
        else poseLink=&pose->next;
    }
    while (*link) {
        nativeShipVisual *visual=*link;
        if (visual->ship==ship) {
            *link=visual->next;hwRmVisualInstanceFree(visual->instance);free(visual);
        } else link=&visual->next;
    }
}
void meshNativeVisualReleaseMesh(const void *mesh)
{
    nativeShipVisual **visualLink=&nativeShips;
    nativeMeshOwner **ownerLink=&nativeOwners;
    while (*visualLink) {
        nativeShipVisual *visual=*visualLink;
        if (visual->owner->mesh==mesh) {
            *visualLink=visual->next;hwRmVisualInstanceFree(visual->instance);free(visual);
        } else visualLink=&visual->next;
    }
    while (*ownerLink) {
        nativeMeshOwner *owner=*ownerLink;
        if (owner->mesh==mesh) {
            *ownerLink=owner->next;hwRmVisualAssetFree(owner->asset);free(owner);return;
        }
        ownerLink=&owner->next;
    }
}
int meshNativeVisualParameters(const void *mesh,float *scale,unsigned int *lod)
{
    nativeMeshOwner *owner;
    if (!scale || !lod) return 0;
    for (owner=nativeOwners;owner;owner=owner->next) if (owner->mesh==mesh) {
        *scale=owner->scale;*lod=owner->lod;return 1;
    }
    return 0;
}
int meshNativeVisualSampleShipEvent(const void *ship,const char *event,double phase)
{
    nativeShipVisual *visual;int updated=0;
    if (!ship || !event || !isfinite(phase) || phase<0 || phase>1) return 0;
    {
        nativeShipPose *pose;
        for(pose=nativePoses;pose;pose=pose->next) if(pose->ship==ship) break;
        if(!pose) {
            pose=(nativeShipPose *)calloc(1,sizeof(*pose));
            if(pose) {pose->ship=ship;pose->next=nativePoses;nativePoses=pose;}
        }
        if(pose) {
            if(!pose->event || strcmp(pose->event,event)) {
                char *copy=(char *)malloc(strlen(event)+1);
                if(copy) {strcpy(copy,event);free(pose->event);pose->event=copy;}
            }
            if(pose->event && !strcmp(pose->event,event)) pose->phase=phase;
        }
    }
    for (visual=nativeShips;visual;visual=visual->next)
        if (visual->ship==ship && hwRmVisualInstanceAnimation(visual->instance))
        {
            char error[256];
            if (hwRmVisualInstanceSampleEvent(visual->instance,event,phase,error,sizeof(error))) ++updated;
        }
    return updated;
}

typedef struct nativePresentationSink {
    nativeShipVisual *visual;
    MeshNativePresentationVisit visit;
    void *user;
} nativePresentationSink;
static void nativePresentationDispatch(void *context,unsigned component,
    const HwRmPresentationEvent *event,double time)
{
    nativePresentationSink *sink=(nativePresentationSink *)context;
    sink->visit(sink->user,sink->visual->ship,sink->visual->instance,
        sink->visual->owner->scale,component,event,time);
}
int meshNativeVisualNotifyAnimation(const void *mesh,const void *ship,const char *event,double phase)
{
    nativeShipVisual *visual,*other;nativePresentationSink sink;char error[256];
    if(!presentationEvents || !event || !isfinite(phase) || phase<0 || phase>1 ||
        !meshNativeVisualAcquire(mesh,ship))return 0;
    for(visual=nativeShips;visual;visual=visual->next)
        if(visual->ship==ship && visual->owner->mesh==mesh)break;
    if(!visual)return 0;
    for(other=nativeShips;other;other=other->next)
        if(other->ship==ship && other!=visual && other->presentationOwner)return 0;
    if(!hwRmVisualInstanceSampleEvent(visual->instance,event,phase,error,sizeof(error)))return 0;
    sink.visual=visual;sink.visit=presentationEvents;sink.user=presentationUser;
    if(!hwRmVisualInstanceAnimationPresentation(visual->instance,event,phase,nativePresentationDispatch,&sink))return 0;
    visual->presentationOwner=1;return 1;
}
int meshNativeVisualFirePresentation(const void *mesh,const void *ship,
    unsigned gun,double clock,MeshNativePresentationVisit visit,void *user)
{
    nativeShipVisual *visual,*other;nativePresentationSink sink;int result;
    if(!visit || !isfinite(clock) || !meshNativeVisualAcquire(mesh,ship))return 0;
    for(visual=nativeShips;visual;visual=visual->next)
        if(visual->ship==ship && visual->owner->mesh==mesh)break;
    if(!visual)return 0;
    /* Never move a running timeline onto a camera-selected LOD. The caller
       must consistently select the first native mesh, as muzzle lookup does. */
    for(other=nativeShips;other;other=other->next)
        if(other->ship==ship && other!=visual && other->presentationOwner)return 0;
    sink.visual=visual;sink.visit=visit;sink.user=user;
    result=hwRmVisualInstanceFirePresentation(visual->instance,gun,clock,
        nativePresentationDispatch,&sink);
    if(result)visual->presentationOwner=1;
    return result;
}
int meshNativeVisualUpdatePresentation(double clock,
    MeshNativePresentationVisit visit,void *user)
{
    nativeShipVisual *visual;int success=1;
    if(!visit || !isfinite(clock))return 0;
    for(visual=nativeShips;visual;visual=visual->next)if(visual->presentationOwner){
        nativePresentationSink sink;sink.visual=visual;sink.visit=visit;sink.user=user;
        if(!hwRmVisualInstanceUpdatePresentation(visual->instance,clock,
            nativePresentationDispatch,&sink))success=0;
    }
    return success;
}
void meshNativeVisualStopPresentation(const void *ship)
{
    nativeShipVisual *visual;
    for(visual=nativeShips;visual;visual=visual->next)
        if(!ship || visual->ship==ship){
            hwRmVisualInstanceStopPresentation(visual->instance);
            visual->presentationOwner=0;
        }
}
void meshNativeVisualSetPresentationSink(MeshNativePresentationVisit events,
    MeshNativeWeaponVisit weapons,void *user)
{
    /* A replacement consumer cannot inherit half-played timelines whose
       activation events went to the previous consumer. */
    meshNativeVisualStopPresentation(NULL);
    presentationEvents=events;presentationWeapons=weapons;presentationUser=user;
    presentationPrepare=NULL;
}
void meshNativeVisualSetPresentationPrepare(MeshNativePresentationPrepare prepare){presentationPrepare=prepare;}
int meshNativeVisualPresentationEnabled(void)
{
    return presentationEvents!=NULL || presentationWeapons!=NULL;
}
int meshNativeVisualNotifyWeaponFire(const void *mesh,const void *ship,
    unsigned gun,double clock)
{
    nativeShipVisual *visual,*other;const HwRmWeaponPresentationSlot *slot;
    int fired=0;
    if(!meshNativeVisualPresentationEnabled() || !isfinite(clock) ||
       !meshNativeVisualAcquire(mesh,ship))return 0;
    for(visual=nativeShips;visual;visual=visual->next)
        if(visual->ship==ship && visual->owner->mesh==mesh)break;
    if(!visual)return 0;
    for(other=nativeShips;other;other=other->next)
        if(other->ship==ship && other!=visual && other->presentationOwner)return 0;
    /* A weaponfire-only gun is valid even without a model animation cue. */
    slot=hwRmWeaponPresentationAt(hwRmVisualInstanceWeaponPresentation(visual->instance,gun),HW_RM_WEAPON_FIRE);
    if(presentationWeapons && slot && (slot->fx[0] || slot->sound[0])){
        presentationWeapons(presentationUser,ship,visual->instance,visual->owner->scale,
            gun,HW_RM_WEAPON_FIRE,slot,clock);
        visual->presentationOwner=1;fired=1;
    }
    if(presentationEvents && meshNativeVisualFirePresentation(mesh,ship,gun,clock,
        presentationEvents,presentationUser))fired=1;
    return fired;
}
int meshNativeVisualPumpPresentation(double clock)
{
    nativeShipVisual *visual;
    if(!isfinite(clock))return 0;
    if(!presentationEvents)return 1;
    if(presentationPrepare)for(visual=nativeShips;visual;visual=visual->next)
        if(visual->presentationOwner&&!presentationPrepare(presentationUser,visual->ship,visual->instance))return 0;
    return meshNativeVisualUpdatePresentation(clock,presentationEvents,presentationUser);
}
unsigned long long meshNativeVisualInstanceToken(const HwRmVisualInstance *instance)
{
    nativeShipVisual *visual;
    if(instance)for(visual=nativeShips;visual;visual=visual->next)
        if(visual->instance==instance)return visual->token;
    return 0;
}
HwRmVisualInstance *meshNativeVisualFindInstance(const void *ship,unsigned long long token,float *scale)
{
    nativeShipVisual *visual;
    if(!ship||!token||!scale)return NULL;
    for(visual=nativeShips;visual;visual=visual->next)
        if(visual->ship==ship&&visual->token==token){*scale=visual->owner->scale;return visual->instance;}
    return NULL;
}
