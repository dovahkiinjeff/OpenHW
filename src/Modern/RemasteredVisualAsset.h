#ifndef HW_REMASTERED_VISUAL_ASSET_H
#define HW_REMASTERED_VISUAL_ASSET_H
#include "RemasteredSubsystem.h"
#include "RemasteredPresentationEvents.h"
#include "RemasteredWeaponPresentation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Own immutable parsed models and their dependency layer together. Instance
   handles are runtime-only visual state, never Ship/savegame identities. */
typedef struct HwRmVisualAsset HwRmVisualAsset;
typedef struct HwRmVisualInstance HwRmVisualInstance;
/* A detached presentation snapshot retains immutable HOD/event assets and
   current joint pose, but no ship pointer or active weapon/MAD timelines. */
HwRmVisualInstance *hwRmVisualInstanceSnapshot(const HwRmVisualInstance *instance);
/* Play an exact source MAD clip on a detached visual instance. The caller
   supplies its existing timeline phase; no gameplay animation is started. */
int hwRmVisualInstanceSampleSourceClip(HwRmVisualInstance *instance,const char *clip,
    double phase,char *error,size_t capacity);
HwRmVisualAsset *hwRmVisualAssetCreate(const void *hod, size_t hodSize,
    const void *mad, size_t madSize, char *error, size_t errorSize);
/* Explicit hull-local MAD track aliases. Target component must be zero here:
   attached models do not exist yet. Source data and names are copied/validated;
   the ordinary create call retains strict no-alias validation. */
HwRmVisualAsset *hwRmVisualAssetCreateWithAnimationTargets(const void *hod, size_t hodSize,
    const void *mad, size_t madSize, const HwRmAnimationTarget *targets,
    unsigned int targetCount, char *error, size_t errorSize);
/* Optional explicit clip allowlist: excluded source clips cannot be bound.
   Used for legacy conversion clips whose integrated rig no longer exists. */
HwRmVisualAsset *hwRmVisualAssetCreateWithAnimationClips(const void *hod, size_t hodSize,
    const void *mad, size_t madSize, const HwRmAnimationTarget *targets,
    unsigned int targetCount, const char *const *clips, unsigned int clipCount,
    char *error, size_t errorSize);
/* Definitions freeze at first instance. A repeated model key shares its HOD
   only when its exact source bytes match; slots/anchors remain independent. */
int hwRmVisualAssetAttach(HwRmVisualAsset *asset, const char *modelKey,
    const char *slot, const char *anchor, unsigned int role,
    const void *hod, size_t hodSize, char *error, size_t errorSize);
void hwRmVisualAssetFree(HwRmVisualAsset *asset);
int hwRmVisualAssetBindPresentation(HwRmVisualAsset *asset,const char *slot,
    const void *bytes,size_t size,char *error,size_t capacity);
const HwRmPresentationEvents *hwRmVisualAssetPresentation(const HwRmVisualAsset *asset,unsigned component);
const HwRmPresentationEvents *hwRmVisualInstancePresentation(const HwRmVisualInstance *instance,unsigned component);
/* Each ship and attached component owns independent timeline playback.
   Callbacks must not mutate/free the instance. Existing HW1 time is supplied
   explicitly; playback never changes simulation, gun cadence or damage. */
typedef void (*HwRmVisualPresentationVisit)(void *user,unsigned component,
    const HwRmPresentationEvent *event,double scheduledTime);
int hwRmVisualInstanceStartPresentation(HwRmVisualInstance *instance,unsigned component,
    const char *timeline,double clock,HwRmVisualPresentationVisit visit,void *user);
int hwRmVisualInstanceUpdatePresentation(HwRmVisualInstance *instance,double clock,
    HwRmVisualPresentationVisit visit,void *user);
void hwRmVisualInstanceStopPresentation(HwRmVisualInstance *instance);
/* HW1 supplies the state magnitude (zero disables). Parent/child threshold
   changes start/stop visual timelines without restarting unchanged states. */
int hwRmVisualInstanceSetPresentationState(HwRmVisualInstance *instance,unsigned component,
    const char *state,float magnitude,double clock,HwRmVisualPresentationVisit visit,void *user);
/* -1: not state-controlled, 0: disabled, 1: enabled. */
int hwRmVisualInstancePresentationState(const HwRmVisualInstance *instance,unsigned component,unsigned timeline);
/* Re-evaluate the selected source marker against this instance's current
   animated/turret pose. Output is component-to-ship, not a frozen fire-time
   world position. Selection never consumes the gameplay RNG. */
int hwRmVisualInstancePresentationMarker(HwRmVisualInstance *instance,unsigned component,
    const HwRmPresentationEvent *event,unsigned randomSample,float scale,
    float *matrix,char *error,size_t capacity);
/* Read source material/texture data by the draw's component and material.
   Texture bytes are borrowed from the asset, never generated on disk. */
const HwRmHodMaterial *hwRmVisualAssetMaterial(const HwRmVisualAsset *asset,
    unsigned int component,unsigned int material);
int hwRmVisualAssetTexture(HwRmVisualAsset *asset,unsigned int component,
    unsigned int material,const char *parameter,unsigned int mip,
    HwRmHodTextureView *view,char *error,size_t errorSize);
int hwRmVisualAssetMount(HwRmVisualAsset *asset,const char *slot,const float *matrix);
int hwRmVisualAssetAuthorJoint(HwRmVisualAsset *asset,const char *slot,const char *joint,const char *parent,const float *matrix);
/* Bind an existing gameplay event explicitly to one or several source clips.
   Validate/copy names before the asset freezes; never infer case/name aliases. */
int hwRmVisualAssetBindAnimation(HwRmVisualAsset *asset, const char *event,
    const char *const *sourceClips, unsigned int count, char *error, size_t errorSize);
/* Explicit HW1 gun-index -> native component/pivots. NULL axes are fixed.
   Muzzle must exist; shared models are addressed by attachment slot, not by
   guessing their internally reused Gun0/Gun1 names. Definitions freeze. */
int hwRmVisualAssetBindGun(HwRmVisualAsset *asset, unsigned int gun,
    const char *slot, const char *azimuthJoint, const char *declinationJoint,
    const char *recoilJoint, const char *muzzleJoint, char *error, size_t errorSize);
/* Exact source fire-animation name, owned by the gun's existing component.
   Never infer an animation from a gameplay gun number or shared model name. */
int hwRmVisualAssetBindGunPresentation(HwRmVisualAsset *asset,unsigned gun,
    const char *timeline,char *error,size_t capacity);
int hwRmVisualInstanceFirePresentation(HwRmVisualInstance *instance,unsigned gun,
    double clock,HwRmVisualPresentationVisit visit,void *user);
int hwRmVisualAssetBindWeaponPresentation(HwRmVisualAsset *asset,unsigned gun,
    const void *bytes,size_t size,char *error,size_t capacity);
const HwRmWeaponPresentation *hwRmVisualInstanceWeaponPresentation(const HwRmVisualInstance *instance,unsigned gun);
int hwRmVisualInstanceGunHasSound(const HwRmVisualInstance *instance,unsigned gun);
/* Resolve only an exact muzzle event on the gun's own firing timeline.
   Engine, vent and damage markers must retain their authored orientation. */
int hwRmVisualInstanceEventGun(const HwRmVisualInstance *instance,unsigned component,
    const HwRmPresentationEvent *event,unsigned *gun);
typedef int (*HwRmVisualFxAvailable)(void *user,const char *choices);
int hwRmVisualInstanceGunHasFx(const HwRmVisualInstance *instance,unsigned gun,
    HwRmVisualFxAvailable available,void *user);
unsigned int hwRmVisualInstanceGunCount(const HwRmVisualInstance *instance);
int hwRmVisualInstanceGunIndex(const HwRmVisualInstance *instance,
    unsigned int ordinal, unsigned int *gun);
int hwRmVisualInstanceSetGun(HwRmVisualInstance *instance, unsigned int gun,
    float azimuth, float declination, float recoil, char *error, size_t errorSize);
typedef struct HwRmGunControl {
    unsigned int gun;
    float azimuth, declination, recoil;
} HwRmGunControl;
int hwRmVisualInstanceSetGuns(HwRmVisualInstance *instance,
    const HwRmGunControl *guns, unsigned int count, char *error, size_t errorSize);
typedef struct HwRmGunAim {
    unsigned int gun;
    float heading[3], recoil;
} HwRmGunAim;
/* Heading is the existing gameplay aim in HW1 ship space, not HW1 Euler angles. */
int hwRmVisualInstanceAimGuns(HwRmVisualInstance *instance,
    const HwRmGunAim *guns, unsigned int count, char *error, size_t errorSize);
int hwRmVisualInstanceGunMuzzle(HwRmVisualInstance *instance, unsigned int gun,
    float scale, float *matrix, char *error, size_t errorSize);
/* Named hull-local DOCK endpoint in HW1 coordinates. Read-only; eligibility,
   reservations and docking state remain owned by the original simulation. */
int hwRmVisualInstanceDockEndpoint(const HwRmVisualInstance *instance,
    const char *pathName, float scale, float position[3]);
/* Explicit old engine/dock/repair/salvage semantic -> native joint. Aliases
   are immutable definitions, queried through the same animated gun hierarchy. */
int hwRmVisualAssetBindMarker(HwRmVisualAsset *asset, const char *semantic,
    const char *slot, const char *jointName, char *error, size_t errorSize);
int hwRmVisualInstanceMarker(HwRmVisualInstance *instance, const char *semantic,
    float scale, float *matrix, char *error, size_t errorSize);
HwRmVisualInstance *hwRmVisualInstanceCreate(HwRmVisualAsset *asset);
void hwRmVisualInstanceFree(HwRmVisualInstance *instance);
HwRmSubsystemInstance *hwRmVisualInstanceSubsystem(HwRmVisualInstance *instance);
const HwRmMad *hwRmVisualInstanceAnimation(const HwRmVisualInstance *instance);
int hwRmVisualInstanceSampleEvent(HwRmVisualInstance *instance, const char *event,
    double phase, char *error, size_t errorSize);
/* Dispatch the explicitly bound source clips' event timelines from the HW1
   MAD phase. Repeated samples do not replay cues; gun timelines are separate. */
int hwRmVisualInstanceAnimationPresentation(HwRmVisualInstance *instance,const char *event,
    double phase,HwRmVisualPresentationVisit visit,void *user);
#ifdef __cplusplus
}
#endif
#endif
