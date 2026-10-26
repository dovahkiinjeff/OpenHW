#ifndef HW_REMASTERED_SUBSYSTEM_H
#define HW_REMASTERED_SUBSYSTEM_H
#include "RemasteredShipHod.h"
#include "RemasteredShipMad.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmSubsystemLayer HwRmSubsystemLayer;
typedef struct HwRmSubsystemInstance HwRmSubsystemInstance;
enum HwRmSubsystemRole { HW_RM_HULL=0, HW_RM_TURRET=1, HW_RM_ENGINE=2, HW_RM_ATTACHMENT=3 };
typedef struct HwRmSubsystemDraw {
    unsigned int component, role;
    const char *slot;
    const HwRmHodMesh *mesh;
    const HwRmHodPart *part;
    const HwRmHodMaterial *material;
    /* Column-major RM-vertex-to-HW1-ship transform. The renderer applies the
       owning ship's world transform afterward, NOT a second model scale. */
    float matrix[16];
} HwRmSubsystemDraw;
typedef void (*HwRmSubsystemDrawCallback)(void *user, const HwRmSubsystemDraw *draw);
typedef struct HwRmSubsystemNode {
    unsigned int index, component, role;
    int parent; /* -1 = ship-space root; otherwise node index, not traversal order */
    const char *slot, *jointName;
    float localMatrix[16];
    /* Joint nodes have no part. Geometry nodes are identity children of their
       authored joint; vertexMatrix maps their raw RM vertices into HW units. */
    const HwRmHodPart *part;
    const HwRmHodMaterial *material;
    float vertexMatrix[16];
} HwRmSubsystemNode;
typedef void (*HwRmSubsystemNodeCallback)(void *user, const HwRmSubsystemNode *node);
/* Return 1 to replace a local joint matrix, 0 to use its authored rest pose.
   Rest/replacement matrices use HW1 axes/units, so gun/MAD adapters don't have
   to pretend RM pivots match classic pivots. The callback is instance-local. */
typedef int (*HwRmSubsystemPoseCallback)(void *user, unsigned int component,
    unsigned int joint, const char *slot, const char *jointName,
    const float *restMatrix, float *replacementMatrix);

/* Scenes are borrowed immutable data. Keep them alive until the layer and all
   its instances have been released. Components are visual parts of ONE HW1
   ship, not new Ship objects, build options, collision bodies or save identities. */
HwRmSubsystemLayer *hwRmSubsystemLayerCreate(const HwRmHodScene *hull,
                                             char *error, size_t errorSize);
int hwRmSubsystemLayerAdd(HwRmSubsystemLayer *layer, const char *slot,
    const char *hullAnchor, unsigned int role, const HwRmHodScene *model,
    char *error, size_t errorSize);
void hwRmSubsystemLayerFree(HwRmSubsystemLayer *layer);
int hwRmSubsystemLayerMount(HwRmSubsystemLayer *layer,const char *slot,const float *matrix);
int hwRmSubsystemLayerAuthorJoint(HwRmSubsystemLayer *layer,const char *slot,const char *joint,const char *parent,const float *matrix);
unsigned int hwRmSubsystemLayerCount(const HwRmSubsystemLayer *layer);
int hwRmSubsystemLayerFindComponent(const HwRmSubsystemLayer *layer,const char *slot);
const HwRmHodScene *hwRmSubsystemLayerScene(const HwRmSubsystemLayer *layer,unsigned int component);
int hwRmSubsystemLayerFindJoint(const HwRmSubsystemLayer *layer,
    const char *slot, const char *jointName, unsigned int *component, unsigned int *joint);
/* Creating the first instance freezes the definition. Shared source models
   still have independent turret poses/visibility for every component/ship. */
HwRmSubsystemInstance *hwRmSubsystemInstanceCreate(HwRmSubsystemLayer *layer);
/* Retain the shared layer and copy only current pose/visibility controls. */
HwRmSubsystemInstance *hwRmSubsystemInstanceSnapshot(const HwRmSubsystemInstance *instance);
void hwRmSubsystemInstanceFree(HwRmSubsystemInstance *instance);
int hwRmSubsystemSetVisible(HwRmSubsystemInstance *instance, unsigned int component, int visible);
/* Sample one explicitly selected native clip into this ship/component only.
   phase=0/1 gives exact authored endpoints; caller owns elapsed time, pause,
   reversal and HW1 state mapping. Missing/ambiguous tracks fail atomically.
   The sampled pose is copied, so MAD data need not outlive this call. */
int hwRmSubsystemSampleAnimation(HwRmSubsystemInstance *instance,
    unsigned int component, const HwRmMad *mad, unsigned int clip, double phase,
    char *error, size_t errorSize);
/* Compose explicitly chosen arm/door clips atomically into one component.
   Independent parts can share a time-normalized gameplay event. Overlapping
   tracks must produce identical poses; conflicting clips fail without changes.
   No automatic clip aliases, clocks, state changes or last-writer-wins blending. */
int hwRmSubsystemSampleAnimations(HwRmSubsystemInstance *instance,
    unsigned int component, const HwRmMad *mad, const unsigned int *clips,
    const double *phases, unsigned int count, char *error, size_t errorSize);
typedef struct HwRmAnimationTarget {
    const char *sourceJoint;
    unsigned int component;
    const char *targetJoint;
} HwRmAnimationTarget;
/* Explicit source-track routing into attached component local frames. This
   does NOT compensate differing rest frames: callers must audit that contract.
   Unmapped tracks use defaultComponent with their exact original name. All
   affected components update atomically; unrelated component poses persist. */
int hwRmSubsystemSampleMappedAnimations(HwRmSubsystemInstance *instance,
    unsigned int defaultComponent, const HwRmMad *mad, const unsigned int *clips,
    const double *phases, unsigned int count, const HwRmAnimationTarget *targets,
    unsigned int targetCount, char *error, size_t errorSize);
int hwRmSubsystemClearAnimation(HwRmSubsystemInstance *instance, unsigned int component);
/* Existing HW1 gun controls, applied AFTER authored rest/animation in the
   converted local pivot frame: azimuth about HW X, declination about HW Y,
   recoil along negative local forward Z in HW units. Exact joint names only. */
int hwRmSubsystemSetJointControl(HwRmSubsystemInstance *instance,
    unsigned int component, const char *jointName, float azimuth,
    float declination, float recoil, char *error, size_t errorSize);
int hwRmSubsystemClearJointControls(HwRmSubsystemInstance *instance, unsigned int component);
typedef struct HwRmJointControlUpdate {
    unsigned int component;
    const char *jointName;
    float azimuth, declination, recoil;
} HwRmJointControlUpdate;
/* All-or-nothing update; failed validation/allocation preserves every pivot. */
int hwRmSubsystemSetJointControls(HwRmSubsystemInstance *instance,
    const HwRmJointControlUpdate *updates, unsigned int count, char *error, size_t errorSize);
/* Select each source mesh's nearest available LOD <= requested LOD; if none,
   use its minimum available level. Validate the entire pose before submitting
   any draw, so a bad joint doesn't publish a partially composed ship. */
int hwRmSubsystemVisit(HwRmSubsystemInstance *instance, unsigned int lod, float modelScale,
    HwRmSubsystemPoseCallback pose, void *poseUser,
    HwRmSubsystemDrawCallback draw, void *drawUser, char *error, size_t errorSize);
/* Exact authored marker/pivot lookup for weapon, docking, engine, repair and
   salvage adapters. Result is joint-to-HW1-ship space, including attachment
   anchors and current instance animation, but not the raw vertex basis or the
   owning ship world transform. Ambiguous names fail; output is unchanged. */
int hwRmSubsystemJointTransform(HwRmSubsystemInstance *instance,
    unsigned int component, const char *jointName, float modelScale,
    HwRmSubsystemPoseCallback pose, void *poseUser, float *matrix,
    char *error, size_t errorSize);
/* Component owner frame for events without a marker. This is not a guessed
   joint named Root: attached models inherit their live hull mounting frame. */
int hwRmSubsystemComponentTransform(HwRmSubsystemInstance *instance,
    unsigned int component,float modelScale,float *matrix,char *error,size_t errorSize);
/* Animated attachment frame without gun controls; does not mutate the instance. */
int hwRmSubsystemNeutralJointTransform(HwRmSubsystemInstance *instance,
    unsigned int component, const char *jointName, float *matrix,
    char *error, size_t errorSize);
/* Legacy renderer bridge. Preserve empty pivot/marker joints and put indexed
   geometry below them, rather than baking every moving part into the hull.
   Reject exceeding maxNodes before invoking callbacks (HW1 iObject is 8-bit).
   Node data is borrowed only for the duration of the callback. */
int hwRmSubsystemVisitHierarchy(HwRmSubsystemInstance *instance, unsigned int lod,
    float modelScale, unsigned int maxNodes, HwRmSubsystemNodeCallback visit,
    void *user, char *error, size_t errorSize);
#ifdef __cplusplus
}
#endif
#endif
