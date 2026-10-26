#ifndef HW_MODERN_RAYTRACING_H
#define HW_MODERN_RAYTRACING_H

#include "ModernGraphics.h"
#include "ResolvedSceneLighting.h"
#include "RasterEmissionSource.h"

#if defined(_WIN32)

#include <d3d12.h>
#include <string>

namespace hwmodern
{
// Same authoring, source priority, units and coordinate conversion as DXR.
// Call on the frozen render thread before dispatching a ray worker.
void resolveRasterSceneLights(const float view[16],
    const HWModernMapLightEmitter *map,unsigned mapCount,
    const HWModernDynamicLightEmitter *dynamic,unsigned dynamicCount,
    std::vector<ResolvedSceneLight>& output);
constexpr unsigned int materialDescriptorBase = 128;
constexpr unsigned int materialDescriptorCount = 32768;
constexpr unsigned int materialFeedbackDescriptorBase = materialDescriptorBase + materialDescriptorCount;
constexpr unsigned int materialMipRequestDescriptorIndex = materialFeedbackDescriptorBase + materialDescriptorCount;

bool raytracingInitialize(ID3D12Device *device,
                          ID3D12DescriptorHeap *shaderVisibleHeap,
                          unsigned int descriptorSize,
                          unsigned int outputSrvIndex,
                          unsigned int outputUavIndex,
                          unsigned int environmentSrvIndex,
                          unsigned int rrGuideUavBaseIndex,
                          ID3D12CommandQueue *directQueue = nullptr);
void raytracingShutdown(void);

bool raytracingCreateOutput(unsigned int width, unsigned int height);
void raytracingReleaseOutput(void);

void raytracingBeginFrame(void);
void raytracingSetFrameSlot(unsigned int frameSlot);
void raytracingSetSensorViewActive(bool active);
void raytracingSetJitter(float jitterX, float jitterY);
void raytracingSetFxLightingStrength(float strength);
void raytracingSetPathSettings(unsigned int samplesPerPixel,
                               unsigned int maximumBounces,
                               float generatedNormalStrength);
void raytracingSetRayReconstructionEnabled(bool enabled);
void raytracingSetEnvironmentMode(bool dualParaboloid, float fade);
void raytracingSetLightingTuning(float environmentStrength,
                                float authoredLightStrength,
                                float surfaceReflectivity,
                                float surfaceRoughness,
                                float exposure);
void raytracingSetSkyLightingStrengths(float primarySunStrength,
                                      float skyAmbientStrength);
void raytracingSetShadowSettings(float shadowStrength,
                                 float sunAngularRadiusDegrees,
                                 unsigned int sunSamples,
                                 unsigned int localSamples,
                                 float localAngularRadiusDegrees,
                                 float receiverBias, float normalBias,
                                 float contactStrength,
                                 float contactDistance,
                                 float maximumDistance);
void raytracingSetMissionSkyLighting(int sourceValid,
                                    const float direction[3],
                                    const float sourceColor[3],
                                    const float ambientColor[3]);
void raytracingSetMissionAuthoringLights(
    const HWModernMissionLightEmitter *emitters, unsigned int count);
void raytracingSetMissionAuthoringReplacesMapLights(bool replace);
void raytracingRegisterSurfaceTexture(
    const void *materialIdentity, unsigned int width, unsigned int height,
    const unsigned int *surfaceRgba, const unsigned int *emissiveRgba,
    const unsigned int *normalRgba, const unsigned int *ormRgba,
    const char *textureKey = nullptr);
bool raytracingTryAliasSurfaceTexture(const void *materialIdentity,
                                      const char *textureKey);
void raytracingUnregisterSurfaceTexture(const void *materialIdentity);
bool raytracingRasterEmissionSource(const void *materialIdentity,RasterEmissionSource &source);
bool raytracingReadRasterEmission(std::uint64_t token,std::vector<unsigned char> &rgba);
void raytracingBeginScene(float verticalFieldOfViewDegrees, float aspectRatio,
                          const float viewMatrix[16]);
void raytracingEndScene(void);
bool raytracingSceneOpen(void);
// Explicit visual-capture only; no traversal or formatting on normal frames.
std::string raytracingMotionSnapshot();

void raytracingSubmitGeometry(
    const void *geometryIdentity,
    const float *vertexPositions, unsigned int vertexCount,
    unsigned int vertexStrideBytes,
    const unsigned short *triangleIndices, unsigned int triangleCount,
    unsigned int triangleStrideBytes,
    unsigned int surfaceVariant,
    const HWModernTriangleSurface *triangleSurfaces,
    unsigned int triangleSurfaceStrideBytes,
    const float modelViewMatrix[16],
    const float baseColor[3],
    unsigned int instanceMask,
    unsigned long long motionOwner = 0, unsigned long long motionPart = 0);

/* Records BLAS/TLAS work and DispatchRays. The caller selects a frame slot
   whose fence has completed before invoking this function. Returns true when
   the output contains a complete path-traced lighting image in pixel-shader
   state, plus depth and motion guidance for native-resolution DLAA. */
bool raytracingRecord(ID3D12GraphicsCommandList *commands,
                      ID3D12Resource *environmentFrame,
                      const HWModernMapLightEmitter *mapLights,
                      unsigned int mapLightCount,
                      const HWModernDynamicLightEmitter *dynamicLights,
                      unsigned int dynamicLightCount);

struct RaytracingCacheStats
{
    unsigned long long uploads, evictions, trimmedBytes, residentBytes;
};
// Render-thread snapshot of existing counters; no resource query or cache scan.
RaytracingCacheStats raytracingCacheStats(void);

bool raytracingInitialized(void);
bool raytracingActive(void);
ID3D12Resource *raytracingDepthResource(void);
ID3D12Resource *raytracingMotionResource(void);
ID3D12Resource *raytracingDiffuseAlbedoResource(void);
ID3D12Resource *raytracingSpecularAlbedoResource(void);
ID3D12Resource *raytracingNormalRoughnessResource(void);
float raytracingFieldOfViewDegrees(void);
float raytracingAspectRatio(void);
bool raytracingHistoryReset(void);
// View-space TLAS is usable only after successful recording for this frame.
unsigned long long raytracingSunVisibilityScene(float viewRows[12]);

}

#endif

#endif
