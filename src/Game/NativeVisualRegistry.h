#ifndef HW_NATIVE_VISUAL_REGISTRY_H
#define HW_NATIVE_VISUAL_REGISTRY_H
#include "RemasteredVisualAsset.h"
struct meshdata;
#ifdef __cplusplus
extern "C" {
#endif
/* Runtime-only side tables: no Ship struct/layout/save-file changes.
   Register takes asset ownership only on success. Registry access runs on the
   existing single-threaded ship/mesh lifecycle path, never GPU worker threads. */
int meshNativeVisualRegister(const void *mesh, HwRmVisualAsset *asset,
    float scale, unsigned int lod);
HwRmVisualInstance *meshNativeVisualAcquire(const void *mesh, const void *ship);
void meshNativeVisualReleaseShip(const void *ship);
void meshNativeVisualReleaseMesh(const void *mesh);
int meshNativeVisualParameters(const void *mesh, float *scale, unsigned int *lod);
int meshNativeVisualRestGun(const void *mesh,unsigned gun,float *matrix);
int meshNativeVisualRestMarker(const void *mesh,const char *semantic,float *matrix);
/* Called by the existing HW1 MAD clock. No native animation scheduler exists. */
int meshNativeVisualSampleShipEvent(const void *ship, const char *event, double phase);
/* Presentation uses one explicitly selected mesh per ship, independent of the
   currently rendered LOD. Callbacks may inspect the instance but must not
   mutate the registry or start/stop playback while dispatch is in progress. */
typedef void (*MeshNativePresentationVisit)(void *user,const void *ship,
    HwRmVisualInstance *instance,float scale,unsigned component,
    const HwRmPresentationEvent *event,double scheduledTime);
int meshNativeVisualFirePresentation(const void *mesh,const void *ship,
    unsigned gun,double clock,MeshNativePresentationVisit visit,void *user);
int meshNativeVisualUpdatePresentation(double clock,
    MeshNativePresentationVisit visit,void *user);
void meshNativeVisualStopPresentation(const void *ship);
/* Engine presentation consumer. No sink means no playback is started and no
   classic visual/audio behavior is suppressed. Borrowed data is valid only
   during the callback; consumers must use ship lifecycle identities for FX. */
typedef void (*MeshNativeWeaponVisit)(void *user,const void *ship,
    HwRmVisualInstance *instance,float scale,unsigned gun,unsigned slot,
    const HwRmWeaponPresentationSlot *presentation,double clock);
void meshNativeVisualSetPresentationSink(MeshNativePresentationVisit events,
    MeshNativeWeaponVisit weapons,void *user);
typedef int (*MeshNativePresentationPrepare)(void *user,const void *ship,HwRmVisualInstance *instance);
void meshNativeVisualSetPresentationPrepare(MeshNativePresentationPrepare prepare);
int meshNativeVisualPresentationEnabled(void);
int meshNativeVisualNotifyWeaponFire(const void *mesh,const void *ship,
    unsigned gun,double clock);
int meshNativeVisualNotifyAnimation(const void *mesh,const void *ship,const char *event,double phase);
int meshNativeVisualPumpPresentation(double clock);
/* Tokens never survive a mesh reload. Delayed consumers reacquire by token
   rather than retaining a potentially freed native-instance pointer. */
unsigned long long meshNativeVisualInstanceToken(const HwRmVisualInstance *instance);
HwRmVisualInstance *meshNativeVisualFindInstance(const void *ship,unsigned long long token,float *scale);
#ifdef __cplusplus
}
#endif
#endif
