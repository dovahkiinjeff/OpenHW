#ifndef OPENHW_PRESENTATION_EVENTS_H
#define OPENHW_PRESENTATION_EVENTS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmPresentationEvents HwRmPresentationEvents;
/* SM is the source spawn-wrapper switch; its rendering meaning is unverified. */
enum { HW_RM_EVENT_ENTITY_SCALE=1, HW_RM_EVENT_NLIPS=2, HW_RM_EVENT_SM=4,
       HW_RM_EVENT_RADIUS_SCALE=8 };
typedef struct HwRmPresentationTimeline {
    const char *name;
    int parent;
    float duration, minimum, maximum;
    unsigned loop;
} HwRmPresentationTimeline;
typedef struct HwRmPresentationEvent {
    unsigned timeline, flags;
    float time, scale; /* time is authored normalized phase, or -1 activation sentinel */
    const char *markers, *fx, *sound, *gameEvent;
    unsigned orphan, lengthMode, other, transform;
    float length; /* mode 0=numeric, 1=entity, 2=radius, 3=distance */
    const char *otherMarker, *orphanText;
} HwRmPresentationEvent;
/* Immutable data owned by the native ship/subsystem asset. No Lua execution
   and no ability to modify HW1 movement, stats, collision or damage. */
HwRmPresentationEvents *hwRmPresentationEventsRead(const void *bytes,size_t size,
    char *error,size_t capacity);
void hwRmPresentationEventsFree(HwRmPresentationEvents *events);
unsigned hwRmPresentationTimelineCount(const HwRmPresentationEvents *events);
unsigned hwRmPresentationEventCount(const HwRmPresentationEvents *events);
const HwRmPresentationTimeline *hwRmPresentationTimelineAt(const HwRmPresentationEvents *events,unsigned index);
const HwRmPresentationEvent *hwRmPresentationEventAt(const HwRmPresentationEvents *events,unsigned index);
int hwRmPresentationFindTimeline(const HwRmPresentationEvents *events,const char *name);
/* from/to are elapsed seconds; event phase is multiplied by timeline duration.
   Visit authored events crossed by the existing engine clock. Initial from=-1
   includes time zero. Negative-time activation events stay explicit and are
   never converted into ordinary bursts. Callbacks receive immutable data. */
typedef void (*HwRmPresentationVisit)(void *user,const HwRmPresentationEvent *event,double scheduledTime);
int hwRmPresentationVisitInterval(const HwRmPresentationEvents *events,unsigned timeline,
    double from,double to,HwRmPresentationVisit visit,void *user);
/* Existing HW1 MAD phase owns this interval. Do not loop a source timeline
   across its endpoint or start a second animation clock. from=-1 includes 0;
   activation is explicitly requested only when the owning animation starts. */
int hwRmPresentationVisitPhase(const HwRmPresentationEvents *events,unsigned timeline,
    double from,double to,int activation,HwRmPresentationVisit visit,void *user);
/* Resolve first, then select one valid marker (RM 0064e9b9), never fan out.
   Resolver must be stable and side-effect free. -1 means no resolved marker.
   The caller supplies a random sample so this layer does not alter HW1 RNG. */
typedef int (*HwRmPresentationResolveMarker)(void *user,const char *name);
int hwRmPresentationSelectMarker(const HwRmPresentationEvent *event,unsigned randomSample,
    HwRmPresentationResolveMarker resolve,void *user);
/* Per-owner playback; uses the caller's existing clock, not a second game
   clock. Source data must outlive this handle. Start replaces a previous cue
   and emits negative-time activation events; phase zero is visited on the
   first advancing update. No callback may mutate/free this player. */
typedef struct HwRmPresentationPlayer HwRmPresentationPlayer;
HwRmPresentationPlayer *hwRmPresentationPlayerCreate(const HwRmPresentationEvents *events);
void hwRmPresentationPlayerFree(HwRmPresentationPlayer *player);
int hwRmPresentationPlayerStart(HwRmPresentationPlayer *player,const char *timeline,
    double clock,HwRmPresentationVisit visit,void *user);
int hwRmPresentationPlayerUpdate(HwRmPresentationPlayer *player,double clock,
    HwRmPresentationVisit visit,void *user);
void hwRmPresentationPlayerStop(HwRmPresentationPlayer *player);
#ifdef __cplusplus
}
#endif
#endif
