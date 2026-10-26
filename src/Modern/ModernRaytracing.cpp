#include "DiagnosticPolicy.h"
#include "ModernRaytracing.h"
#include "DustShadowPolicy.h"
#include "OpaqueSurfacePolicy.h"
#include "LosslessTexelPages.h"
#include "MaterialTextureResidency.h"
#include "PreparedMaterialLayout.h"
#include "GpuMemoryAllocator.h"
#include "SamplerFeedbackTexture.h"
#include "WindowsSplash.h"
#include "MotionHistory.h"
/* Query the compatibility clip plane without importing OpenGL types. */
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
extern "C" int hwglGetClipPlaneEye(float equation[4]);
#endif

#if defined(_WIN32)

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <vector>

#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXTex.h>

#include "ModernRaytracingShader.h"
#include "ModernRaytracingFeedbackShader.h"
#include "MaterialMipFeedbackShader.h"

using Microsoft::WRL::ComPtr;

namespace hwmodern
{
namespace
{
constexpr DXGI_FORMAT outputFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr DXGI_FORMAT accumulationFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr DXGI_FORMAT depthFormat = DXGI_FORMAT_R32_FLOAT;
constexpr DXGI_FORMAT motionFormat = DXGI_FORMAT_R16G16_FLOAT;
constexpr unsigned int maximumLights = resolvedSceneLightCapacity;
/* Ship glow maps remain true emissive surfaces in the path shader, but using
   every emissive triangle as an independent next-event point light is
   catastrophic when the camera enters a large illuminated production bay.
   Keep a bounded proxy-light set; visible emission itself is unaffected. */
constexpr unsigned int maximumEmissiveLightsPerVariant = 24;
constexpr unsigned int maximumEmissiveLightsPerInstance = 8;
constexpr unsigned int maximumSceneEmissiveLights = 32;
constexpr unsigned int dxrFrameResourceCount = 3;
constexpr unsigned int rayTypeCount = 2;
constexpr unsigned int shaderIdentifierSize =
    D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

constexpr unsigned int gpuLightAmbient = 0;
constexpr unsigned int gpuLightDirectional = 1;
constexpr unsigned int gpuLightPoint = 2;
constexpr unsigned int gpuLightSpot = 3;
constexpr unsigned int gpuLightLine = 4;
constexpr unsigned int gpuSurfaceHasTexture = 0x80000000u;
constexpr unsigned int gpuSurfaceNativeNormal = 0x40000000u;
constexpr unsigned int gpuSurfaceRmMaterial = 0x20000000u;

template<typename T>
T alignUp(T value, T alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void logFailure(const char *operation, HRESULT result)
{
    HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR %s failed (HRESULT 0x%08lx).\n",
                 operation, static_cast<unsigned long>(result));
}

bool surfaceDiagEnabled()
{
#if HW_ENABLE_DIAGNOSTICS

    static int cached = -1;
    if (cached < 0)
    {
        const char *value = std::getenv("HW_SURFACE_DIAG");
        cached = (value != nullptr && value[0] != '\0' &&
                  std::strcmp(value, "0") != 0) ? 1 : 0;
    }
    return cached != 0;

#else
return false;
#endif
}

struct SurfaceTextureRecord
{
    bool hasEmission = false;
    bool rmMaterial = false;
    bool nativeNormal = false;
    unsigned int minimumAlpha = 0;
    unsigned int textureIndex = 0;
    unsigned int offset = 0;
    unsigned int width = 0;
    unsigned int height = 0;
};

struct GpuSurfaceTexel
{
    unsigned int surfaceRgba;
    unsigned int emissiveRgba;
    unsigned int generatedNormalRgba;
    unsigned int ormRgba;
};

struct GpuTriangleSurface
{
    float baseColor[3];
    float opacity;
    float emissiveColor[3];
    float emissiveIntensity;
    float uv0[2];
    float uv1[2];
    float uv2[2];
    unsigned int textureOffset;
    unsigned int textureWidth;
    unsigned int textureHeight;
    unsigned int flags;
    unsigned int textureIndex;
    unsigned int idleTextureIndex;
    unsigned int idleTextureOffset;
    unsigned int engineBlendEnabled;
};

static_assert(sizeof(GpuSurfaceTexel) == 16,
              "DXR surface-texel layout mismatch");
static_assert(sizeof(GpuTriangleSurface) == 88,
              "DXR triangle-surface layout mismatch");

struct EmissiveLightPrototype
{
    float idleColor[3] = {};
    bool engineBlendEnabled = false;
    float position[3] = {};
    float color[3] = {};
    float radius = 0.0f;
    float intensity = 0.0f;
    float score = 0.0f;
};

struct GeometryRecord
{
    struct SurfaceVariant
    {
        std::vector<GpuTriangleSurface> surfaces;
        /* Preserve the originating GEO material identity even when the
           texture was not ready on the first submitted frame. Late-loaded
           derelict textures can then patch an already-cached surface variant
           instead of leaving it permanently on the flat fallback. */
        std::vector<const void *> materialIdentities;
        std::vector<unsigned int> materialTextureIndices;
        std::vector<EmissiveLightPrototype> emissiveLights;
        ComPtr<ID3D12Resource> buffer;
        bool emissiveLightsBuilt = false;
        bool opaqueForRays = false;
    };

    std::vector<float> positions;
    std::vector<unsigned int> indices;
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> blasScratch;
    ComPtr<ID3D12Resource> blas;
    std::unordered_map<unsigned int, std::unique_ptr<SurfaceVariant>>
        surfaceVariants;
    float boundingRadius = 1.0f;
    bool built = false;
};

struct SceneInstance
{
    motionhistory::StableKey motionIdentity;
    float engineBlend = 0;
    float clipEquationEye[4] = {};
    unsigned int clipEnabled = 0;
    unsigned int historyIdentity = 0;
    GeometryRecord *geometry = nullptr;
    GeometryRecord::SurfaceVariant *surfaceVariant = nullptr;
    float modelView[16] = {};
    float worldTransform[16] = {};
    bool worldTransformValid = false;
    bool surfaceHistoryValid = false;
    float previousModelView[16] = {};
    float baseColor[3] = { 0.6f, 0.6f, 0.6f };
    unsigned int instanceMask = 0x03u;
};

struct PreviousInstanceKey
{
    GeometryRecord *geometry = nullptr;
    GeometryRecord::SurfaceVariant *surfaceVariant = nullptr;
    bool operator==(const PreviousInstanceKey &other) const
    {
        return geometry == other.geometry &&
               surfaceVariant == other.surfaceVariant;
    }
};

struct PreviousInstanceKeyHash
{
    size_t operator()(const PreviousInstanceKey &key) const
    {
        const size_t a = reinterpret_cast<size_t>(key.geometry);
        const size_t b = reinterpret_cast<size_t>(key.surfaceVariant);
        return (a >> 4) ^ (b + 0x9e3779b97f4a7c15ull +
               (a << 6) + (a >> 2));
    }
};

struct GpuInstanceMotion
{
    float previousRows[3][4];
    unsigned int historyIdentity;
    float clipEquationEye[4];
    unsigned int flags;
    float engineBlend;
};

static_assert(sizeof(GpuInstanceMotion) == 76,
              "DXR instance-motion layout mismatch");

/* This layout is shared with ModernRaytracing.hlsl. */
using GpuLight = ResolvedSceneLight;

static_assert(sizeof(GpuLight) == 80, "GpuLight/HLSL layout mismatch");

struct EmissiveLightCandidate
{
    float score = 0.0f;
    GpuLight light = {};
};

struct FrameConstants
{
    float tanHalfFieldOfView;
    float aspectRatio;
    unsigned int lightCount;
    float maximumRayDistance;
    unsigned int accumulationFrame;
    unsigned int samplesPerPixel;
    unsigned int maximumBounces;
    float normalDetailStrength;
    float nearPlane;
    float farPlane;
    unsigned int resetAccumulation;
    unsigned int frameSeed;
    float environmentStrength;
    float surfaceReflectivity;
    float surfaceRoughness;
    float exposure;
    float shadowStrength;
    float shadowReceiverBias;
    float shadowNormalBias;
    float sunShadowAngularRadius;
    unsigned int sunShadowSamples;
    unsigned int localShadowSamples;
    float localShadowAngularRadius;
    float shadowMaximumDistance;
    float contactShadowStrength;
    float contactShadowDistance;
    unsigned int rayReconstructionEnabled;
    unsigned int constantBufferPadding;
    float viewToWorldRow0[4];
    float viewToWorldRow1[4];
    float viewToWorldRow2[4];
    float previousWorldToViewRow0[4];
    float previousWorldToViewRow1[4];
    float previousWorldToViewRow2[4];
    unsigned int environmentIsDualParaboloid;
    unsigned int viewHistoryValid;
    float previousTanHalfFieldOfView;
    float previousAspectRatio;
};

static_assert(sizeof(FrameConstants) == 224,
              "DXR frame constants must match the HLSL cbuffer");

struct LocalRootArguments
{
    D3D12_GPU_VIRTUAL_ADDRESS vertices;
    D3D12_GPU_VIRTUAL_ADDRESS triangles;
    D3D12_GPU_VIRTUAL_ADDRESS surfaces;
};

static_assert(sizeof(LocalRootArguments) == 24,
              "DXR local-root argument layout mismatch");

struct ReusableUploadBuffer
{
    ComPtr<ID3D12Resource> resource;
    unsigned char *mapped = nullptr;
    unsigned long long capacity = 0;
};

struct DxrFrameResources
{
    ReusableUploadBuffer constantsUpload;
    // Texture-copy uploads and one-time BLAS scratch retire with this slot's fence.
    std::vector<ComPtr<ID3D12Resource>> textureStaging;
    std::vector<std::shared_ptr<SamplerFeedbackTexture>> streamingStaging;
    std::vector<std::shared_ptr<SamplerFeedbackTexture>> feedbackTextures;
    unsigned long long feedbackSerial = 0;
    ReusableUploadBuffer instanceUpload;
    ReusableUploadBuffer lightUpload;
    ReusableUploadBuffer instanceMotionUpload;
    ReusableUploadBuffer shaderTable;
    ComPtr<ID3D12Resource> tlasScratch;
    ComPtr<ID3D12Resource> tlas;
    unsigned long long tlasScratchCapacity = 0;
    unsigned long long tlasCapacity = 0;
    unsigned long long tlasTopologyHash = 0;
    unsigned long long tlasContentHash = 0;
    unsigned long long shaderTableSignature = 0;
    bool shaderTableValid = false;
    bool tlasBuilt = false;
};

struct RaytracingState
{
    ComPtr<ID3D12Device5> device;
    GpuMemoryAllocator gpuMemory;
    ID3D12DescriptorHeap *descriptorHeap = nullptr;
    unsigned int descriptorSize = 0;
    unsigned int outputSrvIndex = 0;
    unsigned int outputUavIndex = 0;
    unsigned int accumulationUavIndex = 0;
    unsigned int depthUavIndex = 0;
    unsigned int motionUavIndex = 0;
    unsigned int historyUavIndex = 0;
    unsigned int motionSrvIndex = 9;
    unsigned int environmentSrvIndex = 0;
    unsigned int rrGuideUavBaseIndex = 0;

    ComPtr<ID3D12RootSignature> globalRootSignature;
    ComPtr<ID3D12RootSignature> localRootSignature;
    ComPtr<ID3D12StateObject> stateObject;
    ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
    ComPtr<ID3D12StateObject> feedbackStateObject;
    ComPtr<ID3D12StateObjectProperties> feedbackStateObjectProperties;
    SamplerFeedbackContext samplerFeedback;
    ComPtr<ID3D12RootSignature> materialMipRoot;
    ComPtr<ID3D12PipelineState> materialMipPipeline;
    ComPtr<ID3D12Resource> materialMipRequests;
    ComPtr<ID3D12DescriptorHeap> materialMipClearHeap;
    bool materialMipRequestsActivated = false;

    ComPtr<ID3D12Resource> output;
    ComPtr<ID3D12Resource> accumulation;
    ComPtr<ID3D12Resource> accumulationHistory;
    ComPtr<ID3D12Resource> surfaceHistory;
    ComPtr<ID3D12Resource> surfaceHistoryPrevious;
    unsigned int nextHistoryIdentity = 1;
    ComPtr<ID3D12Resource> depth;
    ComPtr<ID3D12Resource> motion;
    ComPtr<ID3D12Resource> diffuseAlbedo;
    ComPtr<ID3D12Resource> specularAlbedo;
    ComPtr<ID3D12Resource> normalRoughness;
    bool outputShaderReadable = false;
    bool guidanceShaderReadable = false;
    bool rrGuideShaderReadable = false;
    unsigned int width = 0;
    unsigned int height = 0;

    std::unordered_map<const void *, std::unique_ptr<GeometryRecord>> geometries;
    std::vector<SceneInstance> instances;
    std::vector<SceneInstance> previousInstances;
    std::vector<unsigned char> previousInstanceMatched;
    std::unordered_map<PreviousInstanceKey, std::vector<size_t>,
                       PreviousInstanceKeyHash> previousInstanceBuckets;
    motionhistory::StableIndex previousInstanceIdentities;
    motionhistory::StableIndex currentInstanceIdentities;
    bool sceneOpen = false;
    float verticalFieldOfViewDegrees = 60.0f;
    float aspectRatio = 4.0f / 3.0f;
    float viewMatrix[16] = {};
    float previousViewMatrix[16] = {};
    float worldFromViewMatrix[16] = {};
    float previousVerticalFieldOfViewDegrees = 60.0f;
    float previousAspectRatio = 4.0f / 3.0f;
    bool worldFromViewValid = false;
    bool currentViewValid = false;
    bool previousViewValid = false;
    float jitter[2] = {};
    float previousJitter[2] = {};
    bool environmentIsDualParaboloid = false;
    float environmentFade = 1.0f;

    std::array<DxrFrameResources, dxrFrameResourceCount> frames;
    unsigned int activeFrameSlot = 0;
    ComPtr<ID3D12Resource> surfaceTexelUpload;
    std::vector<ComPtr<ID3D12Resource>> materialTextures;
    std::vector<std::shared_ptr<SamplerFeedbackTexture>> streamingMaterials;
    unsigned long long feedbackCaptures = 0, feedbackTrimmedBytes = 0;
    MaterialTextureResidency<dxrFrameResourceCount> materialResidency;
    std::vector<unsigned int> requiredMaterialTextures;
    bool sensorViewActive = false;
    unsigned long long sensorReturnGraceFrame = 0;
    ComPtr<IDXGIAdapter3> memoryBudgetAdapter;
    unsigned long long materialFrameSerial = 0;
    unsigned long long nextMaterialBudgetQuery = 0;
    unsigned long long materialBudgetLimitBytes = ~0ull;
    unsigned long long materialAdapterBudgetBytes = 0;
    unsigned long long materialUploads = 0, materialEvictions = 0;
    std::vector<ComPtr<ID3D12Resource>> retiredSurfaceTexelUploads;
    std::vector<ComPtr<ID3D12Resource>> retiredSurfaceVariantUploads;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescriptions;
    std::vector<GpuInstanceMotion> instanceMotion;
    std::vector<unsigned char> shaderTableBytes;
    std::vector<GpuLight> lightScratch;
    std::vector<const HWModernDynamicLightEmitter *> rankedDynamicLights;
    std::vector<EmissiveLightCandidate> emissiveCandidates;

    unsigned long long previousSceneHash = 0;
    unsigned int accumulationFrame = 0;
    unsigned int frameSeed = 0;
    bool historyReset = true;

    bool active = false;
    bool loggedFirstDispatch = false;
    unsigned long long fullTlasBuilds = 0;
    unsigned long long tlasUpdates = 0;
    unsigned long long tlasReuses = 0;
    unsigned long long uploadBufferAllocations = 0;
    unsigned long long shaderTableRebuilds = 0;
    unsigned long long shaderTableCacheHits = 0;
};

RaytracingState state;
float fxLightingStrength = 1.0f;
unsigned int requestedSamplesPerPixel = 1;
unsigned int requestedMaximumBounces = 3;
bool requestedRayReconstruction = false;
float requestedGeneratedNormalStrength = 1.0f;
float requestedEnvironmentStrength = 1.0f;
float requestedAuthoredLightStrength = 1.0f;
float requestedPrimarySunStrength = 1.0f;
float requestedSkyAmbientStrength = 1.0f;
bool missionSkyAvailable = false;
bool missionSunValid = false;
float missionSunWorldDirection[3] = {0.0f, 0.0f, -1.0f};
float missionSunColor[3] = {1.0f, 1.0f, 1.0f};
float missionAmbientColor[3] = {0.08f, 0.08f, 0.08f};
std::vector<HWModernMissionLightEmitter> missionAuthoringLights;
bool missionAuthoringReplacesMapLights = false;
float requestedSurfaceReflectivity = 1.25f;
float requestedSurfaceRoughness = 0.38f;
float requestedExposure = 1.0f;
float requestedShadowStrength = 1.0f;
float requestedSunShadowAngularRadiusDegrees = 0.35f;
unsigned int requestedSunShadowSamples = 6u;
unsigned int requestedLocalShadowSamples = 2u;
float requestedLocalShadowAngularRadiusDegrees = 0.20f;
float requestedShadowReceiverBias = 0.05f;
float requestedShadowNormalBias = 0.08f;
float requestedContactShadowStrength = 1.0f;
float requestedContactShadowDistance = 1200.0f;
float requestedShadowMaximumDistance = 500000.0f;
std::unordered_map<const void *, SurfaceTextureRecord> surfaceTextures;
std::unordered_map<std::string, SurfaceTextureRecord> surfaceTextureNames;
std::unordered_map<unsigned long long, std::vector<SurfaceTextureRecord>>
    surfaceTextureContents;
unsigned long long surfaceRegistrationCalls = 0;
unsigned long long surfaceRegistrationTexels = 0;
double surfaceRegistrationMilliseconds = 0.0;

std::string normalizedSurfaceTextureKey(const char *textureKey)
{
    std::string key(textureKey != nullptr ? textureKey : "");
    for (char &character : key)
    {
        if (character == '\\') character = '/';
        else if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return key;
}
LosslessTexelPages<GpuSurfaceTexel> surfaceTexels(1);
PreparedMaterialCache materialUploadCache;
std::vector<SurfaceTextureRecord> materialRecords;
bool surfaceTexelsDirty = true;

D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor(unsigned int index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        state.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * state.descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor(unsigned int index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        state.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * state.descriptorSize;
    return handle;
}

void refreshAccumulationUavs()
{
    if (!state.device || !state.accumulation || !state.accumulationHistory ||
        state.descriptorHeap == nullptr)
        return;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = accumulationFormat;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    state.device->CreateUnorderedAccessView(
        state.accumulation.Get(), nullptr, &uav,
        cpuDescriptor(state.accumulationUavIndex));
    state.device->CreateUnorderedAccessView(
        state.accumulationHistory.Get(), nullptr, &uav,
        cpuDescriptor(state.historyUavIndex));
}

void activatePooledResource(ID3D12GraphicsCommandList *commands,
                            ID3D12Resource *resource)
{
    if (!gpuMemoryResourceInfo(resource).placed) return;
    D3D12_RESOURCE_BARRIER alias = {};
    alias.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    alias.Aliasing.pResourceAfter = resource;
    commands->ResourceBarrier(1, &alias);
}

bool createBuffer(unsigned long long size, D3D12_HEAP_TYPE heapType,
                  D3D12_RESOURCE_FLAGS flags,
                  D3D12_RESOURCE_STATES initialState,
                  ComPtr<ID3D12Resource> &resource,
                  GpuMemoryInitialization initialization =
                      GpuMemoryInitialization::PreserveZeroInitialization)
{
    size = std::max<unsigned long long>(size, 256);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = heapType;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = size;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = flags;

    HRESULT result = state.gpuMemory.active()
        ? state.gpuMemory.createResource(heap, D3D12_HEAP_FLAG_NONE,
            description, initialState, nullptr, resource, initialization)
        : state.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
            &description, initialState, nullptr, IID_PPV_ARGS(&resource));
    if (FAILED(result))
    {
        logFailure("buffer allocation", result);
        return false;
    }
    return true;
}

bool createUpload(const void *data, unsigned long long size,
                  ComPtr<ID3D12Resource> &resource)
{
    resource.Reset();
    if (!createBuffer(size, D3D12_HEAP_TYPE_UPLOAD,
                      D3D12_RESOURCE_FLAG_NONE,
                      D3D12_RESOURCE_STATE_GENERIC_READ, resource))
    {
        return false;
    }
    void *mapped = nullptr;
    D3D12_RANGE noRead = { 0, 0 };
    HRESULT result = resource->Map(0, &noRead, &mapped);
    if (FAILED(result))
    {
        logFailure("upload-buffer map", result);
        resource.Reset();
        return false;
    }
    if (data != nullptr && size != 0)
    {
        std::memcpy(mapped, data, static_cast<size_t>(size));
    }
    resource->Unmap(0, nullptr);
    return true;
}

void transitionResource(ID3D12GraphicsCommandList *commands,
                        ID3D12Resource *resource,
                        D3D12_RESOURCE_STATES before,
                        D3D12_RESOURCE_STATES after);

bool createImmutableGeometryBuffer(ID3D12GraphicsCommandList *commands,
                                   const void *data, unsigned long long size,
                                   ComPtr<ID3D12Resource> &resource)
{
    ComPtr<ID3D12Resource> upload, created;
    if (!data || !size || !createUpload(data, size, upload) ||
        !createBuffer(size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST, created,
            GpuMemoryInitialization::FullyWrittenBeforeRead))
        return false;
    // Both BLAS construction and subsequent ray shaders read the same local
    // immutable geometry. Its temporary CPU upload retires with this fence.
    auto &staging = state.frames[state.activeFrameSlot].textureStaging;
    // Complete fallible retention before recording references to either resource.
    staging.push_back(upload);
    // An index-buffer allocation can still fail later in this build. Preserve
    // this destination too if a subsequent retry replaces the owning geometry.
    staging.push_back(created);
    activatePooledResource(commands, created.Get());
    commands->CopyBufferRegion(created.Get(), 0, upload.Get(), 0, size);
    transitionResource(commands, created.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    resource = std::move(created);
    return true;
}

void releaseReusableUpload(ReusableUploadBuffer &upload)
{
    if (upload.resource && upload.mapped != nullptr)
    {
        upload.resource->Unmap(0, nullptr);
    }
    upload.mapped = nullptr;
    upload.capacity = 0;
    upload.resource.Reset();
}

bool updateReusableUpload(const void *data, unsigned long long size,
                          const wchar_t *name,
                          ReusableUploadBuffer &upload)
{
    const unsigned long long required = std::max<unsigned long long>(size, 256);
    if (!upload.resource || upload.capacity < required)
    {
        const unsigned long long previousCapacity = upload.capacity;
        releaseReusableUpload(upload);
        const unsigned long long grown = std::max<unsigned long long>(
            required, std::max<unsigned long long>(64ull * 1024ull,
                                                   previousCapacity * 2ull));
        const unsigned long long capacity = alignUp<unsigned long long>(
            grown, 64ull * 1024ull);
        if (!createBuffer(capacity, D3D12_HEAP_TYPE_UPLOAD,
                          D3D12_RESOURCE_FLAG_NONE,
                          D3D12_RESOURCE_STATE_GENERIC_READ,
                          upload.resource))
        {
            return false;
        }
        D3D12_RANGE noRead = { 0, 0 };
        void *mapped = nullptr;
        HRESULT result = upload.resource->Map(0, &noRead, &mapped);
        if (FAILED(result))
        {
            logFailure("reusable upload-buffer map", result);
            releaseReusableUpload(upload);
            return false;
        }
        upload.mapped = static_cast<unsigned char *>(mapped);
        upload.capacity = capacity;
        if (name != nullptr)
        {
            upload.resource->SetName(name);
        }
        ++state.uploadBufferAllocations;
    }
    if (data != nullptr && size != 0)
    {
        std::memcpy(upload.mapped, data, static_cast<size_t>(size));
    }
    return true;
}

bool ensureAccelerationBuffer(ID3D12GraphicsCommandList *commands,
                              unsigned long long requested,
                              unsigned long long &capacity,
                              D3D12_RESOURCE_STATES initialState,
                              ComPtr<ID3D12Resource> &resource)
{
    requested = alignUp<unsigned long long>(requested, 256);
    if (resource && capacity >= requested)
    {
        return true;
    }
    resource.Reset();
    if (!createBuffer(requested, D3D12_HEAP_TYPE_DEFAULT,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      initialState, resource,
                      GpuMemoryInitialization::FullyWrittenBeforeRead))
    {
        capacity = 0;
        return false;
    }
    state.frames[state.activeFrameSlot].textureStaging.push_back(resource);
    activatePooledResource(commands, resource.Get());
    capacity = requested;
    return true;
}

bool createRootSignatures(void)
{
    D3D12_DESCRIPTOR_RANGE outputRanges[2] = {};
    outputRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputRanges[0].NumDescriptors = 5;
    outputRanges[0].BaseShaderRegister = 0;
    outputRanges[0].OffsetInDescriptorsFromTableStart = 0;
    outputRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputRanges[1].NumDescriptors = 3;
    outputRanges[1].BaseShaderRegister = 5;
    outputRanges[1].OffsetInDescriptorsFromTableStart =
        state.rrGuideUavBaseIndex - state.outputUavIndex;

    D3D12_DESCRIPTOR_RANGE environmentRange = {};
    environmentRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    environmentRange.NumDescriptors = 1;
    environmentRange.BaseShaderRegister = 4;
    environmentRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE materialRange = {};
    materialRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    materialRange.NumDescriptors = materialDescriptorCount;
    materialRange.BaseShaderRegister = 5;
    D3D12_ROOT_PARAMETER globalParameters[11] = {};
    globalParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    globalParameters[0].Descriptor.ShaderRegister = 0;
    globalParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    globalParameters[1].DescriptorTable.NumDescriptorRanges = 2;
    globalParameters[1].DescriptorTable.pDescriptorRanges = outputRanges;
    globalParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    globalParameters[2].Descriptor.ShaderRegister = 1;
    globalParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    globalParameters[3].Descriptor.ShaderRegister = 2;
    globalParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    globalParameters[4].Descriptor.ShaderRegister = 3;
    globalParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[5].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    globalParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    globalParameters[5].DescriptorTable.pDescriptorRanges = &environmentRange;
    globalParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    globalParameters[6].Descriptor.ShaderRegister = 0;
    globalParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    globalParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    globalParameters[7].DescriptorTable.NumDescriptorRanges = 1;
    globalParameters[7].DescriptorTable.pDescriptorRanges = &materialRange;
    for (unsigned int index = 8; index < 10; ++index)
    {
        globalParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        globalParameters[index].Descriptor.ShaderRegister = index;
    }
    globalParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    globalParameters[10].Descriptor.ShaderRegister = 10;
    D3D12_STATIC_SAMPLER_DESC materialSampler = {};
    materialSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    materialSampler.AddressU = materialSampler.AddressV = materialSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    materialSampler.MaxLOD = D3D12_FLOAT32_MAX;
    materialSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    materialSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC globalDescription = {};
    globalDescription.NumParameters = state.samplerFeedback.supported() ? 11 : 10;
    globalDescription.pParameters = globalParameters;
    globalDescription.NumStaticSamplers = 1;
    globalDescription.pStaticSamplers = &materialSampler;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(
        &globalDescription, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &errors);
    if (FAILED(result))
    {
        logFailure("global root-signature serialization", result);
        return false;
    }
    result = state.device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&state.globalRootSignature));
    if (FAILED(result))
    {
        logFailure("global root-signature creation", result);
        return false;
    }

    D3D12_ROOT_PARAMETER localParameters[3] = {};
    localParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    localParameters[0].Descriptor.ShaderRegister = 0;
    localParameters[0].Descriptor.RegisterSpace = 1;
    localParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    localParameters[1].Descriptor.ShaderRegister = 1;
    localParameters[1].Descriptor.RegisterSpace = 1;
    localParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    localParameters[2].Descriptor.ShaderRegister = 2;
    localParameters[2].Descriptor.RegisterSpace = 1;

    D3D12_ROOT_SIGNATURE_DESC localDescription = {};
    localDescription.NumParameters = 3;
    localDescription.pParameters = localParameters;
    localDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    serialized.Reset();
    errors.Reset();
    result = D3D12SerializeRootSignature(
        &localDescription, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &errors);
    if (FAILED(result))
    {
        logFailure("local root-signature serialization", result);
        return false;
    }
    result = state.device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&state.localRootSignature));
    if (FAILED(result))
    {
        logFailure("local root-signature creation", result);
        return false;
    }
    return true;
}

bool createMaterialMipPipeline()
{
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].BaseShaderRegister = 5;
    ranges[0].NumDescriptors = materialDescriptorCount;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].RegisterSpace = 2;
    ranges[1].NumDescriptors = materialDescriptorCount;
    D3D12_ROOT_PARAMETER parameters[3] = {};
    for (unsigned i = 0; i < 2; ++i)
    {
        parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[i].DescriptorTable.NumDescriptorRanges = 1;
        parameters[i].DescriptorTable.pDescriptorRanges = ranges + i;
    }
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[2].Descriptor.ShaderRegister = 0;
    parameters[2].Descriptor.RegisterSpace = 3;
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 3; desc.pParameters = parameters;
    desc.NumStaticSamplers = 1; desc.pStaticSamplers = &sampler;
    ComPtr<ID3DBlob> bytes, errors;
    HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &bytes, &errors);
    if (FAILED(result)) { logFailure("material mip root serialization", result); return false; }
    result = state.device->CreateRootSignature(0, bytes->GetBufferPointer(), bytes->GetBufferSize(),
        IID_PPV_ARGS(&state.materialMipRoot));
    if (FAILED(result)) { logFailure("material mip root creation", result); return false; }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = state.materialMipRoot.Get();
    pipeline.CS = {g_hwMaterialMipFeedbackShader, sizeof(g_hwMaterialMipFeedbackShader)};
    result = state.device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&state.materialMipPipeline));
    if (FAILED(result)) { logFailure("material mip feedback compute pipeline", result); return false; }
    if (!createBuffer(materialDescriptorCount * sizeof(unsigned int), D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        state.materialMipRequests, GpuMemoryInitialization::FullyWrittenBeforeRead)) return false;
    D3D12_DESCRIPTOR_HEAP_DESC clearHeap = {};
    clearHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; clearHeap.NumDescriptors = 1;
    result = state.device->CreateDescriptorHeap(&clearHeap, IID_PPV_ARGS(&state.materialMipClearHeap));
    if (FAILED(result)) { logFailure("material mip clear descriptor", result); return false; }
    D3D12_UNORDERED_ACCESS_VIEW_DESC view = {};
    view.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    view.Format = DXGI_FORMAT_R32_TYPELESS;
    view.Buffer.NumElements = materialDescriptorCount;
    view.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    // ClearUnorderedAccessViewUint requires a raw/typed clear view. The DXR
    // root UAV and compute root SRV still interpret the same DWORD storage.
    view.Buffer.StructureByteStride = 0;
    const auto clearView = state.materialMipClearHeap->GetCPUDescriptorHandleForHeapStart();
    state.device->CreateUnorderedAccessView(state.materialMipRequests.Get(), nullptr, &view, clearView);
    state.device->CopyDescriptorsSimple(1, cpuDescriptor(materialMipRequestDescriptorIndex), clearView,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return true;
}

bool createStateObject(bool feedback = false)
{
    static const wchar_t *libraryExports[] = {
        L"RayGeneration", L"PrimaryMiss", L"ShadowMiss",
        L"PrimaryClosestHit", L"ShadowClosestHit", L"SurfaceAnyHit"
    };
    D3D12_EXPORT_DESC exports[6] = {};
    for (unsigned int index = 0; index < 6; ++index)
    {
        exports[index].Name = libraryExports[index];
    }

    D3D12_DXIL_LIBRARY_DESC library = {};
    library.DXILLibrary.pShaderBytecode = feedback
        ? g_hwModernRaytracingFeedbackLibrary : g_hwModernRaytracingLibrary;
    library.DXILLibrary.BytecodeLength = feedback
        ? sizeof(g_hwModernRaytracingFeedbackLibrary) : sizeof(g_hwModernRaytracingLibrary);
    library.NumExports = 6;
    library.pExports = exports;

    D3D12_HIT_GROUP_DESC primaryHitGroup = {};
    primaryHitGroup.HitGroupExport = L"PrimaryHitGroup";
    primaryHitGroup.ClosestHitShaderImport = L"PrimaryClosestHit";
    primaryHitGroup.AnyHitShaderImport = L"SurfaceAnyHit";
    primaryHitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    D3D12_HIT_GROUP_DESC shadowHitGroup = {};
    shadowHitGroup.HitGroupExport = L"ShadowHitGroup";
    shadowHitGroup.ClosestHitShaderImport = L"ShadowClosestHit";
    shadowHitGroup.AnyHitShaderImport = L"SurfaceAnyHit";
    shadowHitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = 104;
    shaderConfig.MaxAttributeSizeInBytes = 8;
    static const wchar_t *shaderConfigExports[] = {
        L"RayGeneration", L"PrimaryMiss", L"ShadowMiss",
        L"PrimaryHitGroup", L"ShadowHitGroup"
    };

    D3D12_GLOBAL_ROOT_SIGNATURE globalRoot = {
        state.globalRootSignature.Get()
    };
    D3D12_LOCAL_ROOT_SIGNATURE localRoot = {
        state.localRootSignature.Get()
    };
    static const wchar_t *localExports[] = {
        L"PrimaryHitGroup", L"ShadowHitGroup"
    };
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    /* Up to six radiance bounces plus a nested visibility ray. */
    pipelineConfig.MaxTraceRecursionDepth = 7;

    D3D12_STATE_SUBOBJECT subobjects[9] = {};
    subobjects[0] = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &library };
    subobjects[1] = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
                      &primaryHitGroup };
    subobjects[2] = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
                      &shadowHitGroup };
    subobjects[3] = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
                      &shaderConfig };

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderAssociation = {};
    shaderAssociation.pSubobjectToAssociate = &subobjects[3];
    shaderAssociation.NumExports = 5;
    shaderAssociation.pExports = shaderConfigExports;
    subobjects[4] = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
                      &shaderAssociation };
    subobjects[5] = { D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
                      &localRoot };

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION localAssociation = {};
    localAssociation.pSubobjectToAssociate = &subobjects[5];
    localAssociation.NumExports = 2;
    localAssociation.pExports = localExports;
    subobjects[6] = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
                      &localAssociation };
    subobjects[7] = { D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
                      &globalRoot };
    subobjects[8] = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
                      &pipelineConfig };

    D3D12_STATE_OBJECT_DESC description = {};
    description.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    description.NumSubobjects = 9;
    description.pSubobjects = subobjects;
    auto &object = feedback ? state.feedbackStateObject : state.stateObject;
    auto &properties = feedback ? state.feedbackStateObjectProperties : state.stateObjectProperties;
    HRESULT result = state.device->CreateStateObject(
        &description, IID_PPV_ARGS(&object));
    if (FAILED(result))
    {
        logFailure("pipeline creation", result);
        return false;
    }
    result = object.As(&properties);
    if (FAILED(result))
    {
        logFailure("pipeline-properties query", result);
        return false;
    }
    return true;
}

bool buildGeometry(ID3D12GraphicsCommandList4 *commands,
                   GeometryRecord &geometry)
{
    if (geometry.built)
    {
        return true;
    }
    if (!createImmutableGeometryBuffer(commands, geometry.positions.data(),
                      geometry.positions.size() * sizeof(float),
                      geometry.vertexBuffer) ||
        !createImmutableGeometryBuffer(commands, geometry.indices.data(),
                      geometry.indices.size() * sizeof(unsigned int),
                      geometry.indexBuffer))
    {
        return false;
    }

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDescription = {};
    geometryDescription.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    /* Authored cutout/transparent texels are evaluated by SurfaceAnyHit. */
    geometryDescription.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    geometryDescription.Triangles.Transform3x4 = 0;
    geometryDescription.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometryDescription.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDescription.Triangles.IndexCount =
        static_cast<unsigned int>(geometry.indices.size());
    geometryDescription.Triangles.VertexCount =
        static_cast<unsigned int>(geometry.positions.size() / 3);
    geometryDescription.Triangles.IndexBuffer =
        geometry.indexBuffer->GetGPUVirtualAddress();
    geometryDescription.Triangles.VertexBuffer.StartAddress =
        geometry.vertexBuffer->GetGPUVirtualAddress();
    geometryDescription.Triangles.VertexBuffer.StrideInBytes =
        3 * sizeof(float);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &geometryDescription;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    state.device->GetRaytracingAccelerationStructurePrebuildInfo(
        &inputs, &prebuild);
    if (prebuild.ResultDataMaxSizeInBytes == 0)
    {
        return false;
    }
    if (!createBuffer(prebuild.ScratchDataSizeInBytes,
                      D3D12_HEAP_TYPE_DEFAULT,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      geometry.blasScratch,
                      GpuMemoryInitialization::FullyWrittenBeforeRead) ||
        !createBuffer(prebuild.ResultDataMaxSizeInBytes,
                      D3D12_HEAP_TYPE_DEFAULT,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                      geometry.blas,
                      GpuMemoryInitialization::FullyWrittenBeforeRead))
    {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
    activatePooledResource(commands, geometry.blasScratch.Get());
    activatePooledResource(commands, geometry.blas.Get());
    build.Inputs = inputs;
    build.ScratchAccelerationStructureData =
        geometry.blasScratch->GetGPUVirtualAddress();
    build.DestAccelerationStructureData = geometry.blas->GetGPUVirtualAddress();
    commands->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = geometry.blas.Get();
    commands->ResourceBarrier(1, &barrier);
    geometry.built = true;
    // Immutable BLAS geometry never rebuilds in place. Keep scratch alive
    // until this submission completes, then release it with the flight slot
    // instead of retaining one GPU scratch allocation for every loaded mesh.
    state.frames[state.activeFrameSlot].textureStaging.push_back(
        std::move(geometry.blasScratch));
    return true;
}

bool buildSurfaceVariant(GeometryRecord::SurfaceVariant &variant)
{
    if (variant.buffer)
    {
        return true;
    }
    if (variant.surfaces.empty())
    {
        return false;
    }
    // Gather once per immutable variant, including the idle side of engine
    // blends. Late material binding resets variant.buffer and rebuilds this
    // list together with the corrected surface metadata.
    variant.materialTextureIndices.clear();
    for (const auto &surface : variant.surfaces)
        appendMaterialTextureReferences(variant.materialTextureIndices,
            (surface.flags & gpuSurfaceHasTexture) != 0,
            surface.textureIndex, surface.engineBlendEnabled != 0,
            surface.idleTextureIndex, materialRecords.size());
    std::sort(variant.materialTextureIndices.begin(), variant.materialTextureIndices.end());
    variant.materialTextureIndices.erase(std::unique(variant.materialTextureIndices.begin(),
        variant.materialTextureIndices.end()), variant.materialTextureIndices.end());
    return createUpload(variant.surfaces.data(),
                        variant.surfaces.size() * sizeof(GpuTriangleSurface),
                        variant.buffer);
}

void trimMaterialTextures(bool refreshBudget = false, bool requirementsKnown = true)
{
    // Retire completed tile mappings even while Sensors retains the fleet.
    state.samplerFeedback.collectRetiredHeaps();
    // Sensors temporarily replaces the world renderer. An absent DXR scene
    // there is not a discarded fleet: keep its textures ready for the return.
    // One return frame allows requirements to be gathered before trimming.
    if (state.sensorViewActive || state.materialFrameSerial <= state.sensorReturnGraceFrame)
        return;
    const auto residentBytes = state.materialResidency.residentBytes();
    if (state.memoryBudgetAdapter &&
        (refreshBudget || state.materialFrameSerial >= state.nextMaterialBudgetQuery))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO memory = {};
        if (SUCCEEDED(state.memoryBudgetAdapter->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory)))
        {
            state.materialAdapterBudgetBytes = memory.Budget;
            const auto budgetHeadroomBytes = std::max<unsigned long long>(
                512ull * 1024 * 1024, memory.Budget / 16);
            const auto allowed = memory.Budget > budgetHeadroomBytes
                ? memory.Budget - budgetHeadroomBytes : 0;
            // Detached tile heaps remain physically allocated until their
            // mapping-retirement fence completes. They are still our charge,
            // not a new external allocation that warrants another eviction.
            const auto pendingBytes = state.samplerFeedback.pendingRetiredBytes();
            const auto ownedBytes = residentBytes +
                std::min<unsigned long long>(pendingBytes, ~0ull - residentBytes);
            const auto otherUsage = memory.CurrentUsage > ownedBytes
                ? memory.CurrentUsage - ownedBytes : 0;
            state.materialBudgetLimitBytes = allowed > otherUsage
                ? allowed - otherUsage : 0;
        }
        state.nextMaterialBudgetQuery = state.materialFrameSerial + 60;
    }
    // At slot retirement today's scene has not been gathered yet. Enforce
    // actual memory pressure there, not a falsely empty scene target. Retain
    // recently seen views using measured spare GPU memory instead of repeatedly
    // evicting them to a fixed 512 MiB allowance while half the adapter is free.
    const auto target = materialRetentionTarget(state.materialResidency.requiredBytes(),
        state.materialBudgetLimitBytes, state.materialAdapterBudgetBytes, requirementsKnown);
    if (state.materialResidency.residentBytes() <= target) return;

    // The policy supplies every eligible entry in oldest-use order. A partial
    // trim may retain a packed tail, so selecting only enough *whole* victims
    // up front can miss other cold mips that satisfy the deficit without eviction.
    const auto candidates = state.materialResidency.evictionCandidates(0);
    for (const auto index : candidates)
    {
        if (state.materialResidency.residentBytes() <= target) break;
        if (index >= state.streamingMaterials.size()) continue;
        const auto &streaming = state.streamingMaterials[index];
        if (!streaming) continue;
        const auto trimmed = streaming->trimInactive(state.samplerFeedback, true);
        if (trimmed)
        {
            state.materialResidency.resident(index, streaming->residentBytes());
            state.feedbackTrimmedBytes += trimmed;
        }
    }
    // Only discard whole resources if the eligible finer mips could not meet
    // the existing target. Required and in-flight entries remain pinned.
    for (const auto index : candidates)
    {
        if (state.materialResidency.residentBytes() <= target) break;
        if (state.materialResidency.evict(index))
        {
            // All slots that used this index have completed. Leave its unused
            // descriptor unchanged until a future scene requests a reupload;
            // no queued dispatch can observe a rewritten/invalid resource.
            if (index < state.streamingMaterials.size() && state.streamingMaterials[index])
            {
                state.streamingMaterials[index]->releaseInactive(state.samplerFeedback);
                state.streamingMaterials[index].reset();
            }
            state.materialTextures[index].Reset();
            ++state.materialEvictions;
        }
    }
}

bool ensureMaterialTextures(ID3D12GraphicsCommandList *commands, DxrFrameResources &frame)
{
    // t2 remains a harmless compatibility binding. CPU texels are retained
    // for emissive proxies/deduplication, but are no longer sampled over the
    // upload heap by every ray. Material resources live in GPU default memory.
    if (!state.surfaceTexelUpload && !createUpload(nullptr, 256, state.surfaceTexelUpload))
        return false;
    // Registered textures describe the whole loaded fleet. Only material pages
    // referenced by the current DXR scene need full-resolution GPU resources.
    // Null holes retain stable shader descriptor indices without uploading
    // unused ship types or every material from previous missions.
    state.materialTextures.resize(materialRecords.size());
    state.streamingMaterials.resize(materialRecords.size());
    state.materialResidency.resize(materialRecords.size());
    state.requiredMaterialTextures.clear();
    for (const auto &instance : state.instances)
    {
        if (!instance.surfaceVariant) continue;
        for (const auto index : instance.surfaceVariant->materialTextureIndices)
            if (state.materialResidency.require(index))
                state.requiredMaterialTextures.push_back(index);
    }
    trimMaterialTextures();
    bool uploaded = false;
    for (const auto textureIndex : state.requiredMaterialTextures)
    {
        auto streaming = state.streamingMaterials[textureIndex];
        if (state.materialTextures[textureIndex] && (!streaming || streaming->readyForSampling())) continue;
        const bool restoringStreamedTexture = streaming != nullptr;
        const auto &record = materialRecords[textureIndex];
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = record.width; desc.Height = record.height;
        desc.DepthOrArraySize = 4;
        desc.MipLevels = static_cast<UINT16>(materialMipCount(record.width, record.height));
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        const auto allocation = state.device->GetResourceAllocationInfo(0, 1, &desc);
        if (!allocation.SizeInBytes || allocation.SizeInBytes == ~0ull) return false;
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        ComPtr<ID3D12Resource> texture, upload;
        std::vector<SamplerFeedbackMipRange> uploadRanges;
        if (!streaming && state.feedbackStateObject)
        {
            auto candidate = std::make_shared<SamplerFeedbackTexture>();
            if (candidate->create(state.samplerFeedback, desc, textureIndex,
                cpuDescriptor(materialFeedbackDescriptorBase + textureIndex)))
                streaming = std::move(candidate);
        }
        if (streaming)
        {
            // A fresh map allocation can safely fall back before any tile
            // mapping has been queued. Rehydration retains its existing owner
            // and retries without exposing uninitialized mips to a dispatch.
            if (!streaming->mapAll(state.samplerFeedback, uploadRanges))
            {
                if (restoringStreamedTexture) return false;
                state.samplerFeedback.createNullViews(
                    cpuDescriptor(materialFeedbackDescriptorBase + textureIndex),
                    state.descriptorSize, 1);
                streaming.reset();
            }
        }
        if (streaming)
        {
            // Retain every mapped destination through this flight slot even
            // if upload allocation/preparation fails before the first draw.
            frame.streamingStaging.push_back(streaming);
            if (restoringStreamedTexture)
                state.materialResidency.resident(textureIndex, streaming->residentBytes());
            texture = streaming->resource();
            D3D12_RESOURCE_BARRIER alias = {};
            alias.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
            alias.Aliasing.pResourceAfter = texture.Get();
            commands->ResourceBarrier(1, &alias);
        }
        else
        {
            const HRESULT textureResult = state.gpuMemory.active()
                ? state.gpuMemory.createResource(heap, D3D12_HEAP_FLAG_NONE, desc,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, texture,
                    GpuMemoryInitialization::FullyWrittenBeforeRead)
                : state.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
                    &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
            if (FAILED(textureResult)) return false;
            activatePooledResource(commands, texture.Get());
            uploadRanges.push_back({0, desc.MipLevels});
        }
        frame.textureStaging.push_back(texture);
        UINT count = desc.MipLevels * 4u;
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(count);
        UINT64 bytes = 0;
        state.device->GetCopyableFootprints(&desc, 0, count, 0, layouts.data(), nullptr, nullptr, &bytes);
        if (!createUpload(nullptr, bytes, upload)) return false;
        void *mapped = nullptr;
        D3D12_RANGE noRead = {0, 0};
        if (FAILED(upload->Map(0, &noRead, &mapped))) return false;
        const std::uint64_t payloadKey = (static_cast<std::uint64_t>(record.width) << 32) | record.height;
        const PreparedMaterialLayout preparedLayout(record.width, record.height);
        std::vector<PreparedMaterialCache::Range> cachedRanges;
        for (UINT channel = 0; channel < 4; ++channel)
        for (const auto &range : uploadRanges)
        for (UINT mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
        {
            const UINT subresource = channel * desc.MipLevels + mip;
            const auto widthBytes = std::max(1u, record.width >> mip) * 4ull;
            const auto height = std::max(1u, record.height >> mip);
            const auto &layout = layouts[subresource];
            preparedLayout.appendReadRanges(cachedRanges, subresource,
                static_cast<UINT>(widthBytes / 4), height, layout.Offset, layout.Footprint.RowPitch);
        }
        if (!materialUploadCache.read(textureIndex, payloadKey, mapped, bytes, cachedRanges))
        {
            // Only first use (or unavailable disposable cache storage) performs
            // the expensive lossless texel expansion and identical mip filter.
            const auto prepare = [&]() -> bool {
                DirectX::ScratchImage base;
                auto prepared = std::make_shared<DirectX::ScratchImage>();
                auto &mips = *prepared;
                if (FAILED(base.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,
                    record.width, record.height, 4, 1))) return false;
                const DirectX::Image *images[4];
                for (size_t channel = 0; channel < 4; ++channel)
                    images[channel] = base.GetImage(0, channel, 0);
                for (unsigned int y = 0; y < record.height; ++y)
                for (unsigned int x = 0; x < record.width;)
                {
                    const auto texels = surfaceTexels.span(
                        static_cast<size_t>(record.offset) + y * record.width + x, record.width - x);
                    if (texels.size == 0) return false;
                    for (size_t channel = 0; channel < 4; ++channel)
                    for (size_t pixel = 0; pixel < texels.size; ++pixel)
                    {
                        const auto &texel = texels.data[pixel];
                        const unsigned int packed[4] = {texel.surfaceRgba, texel.emissiveRgba,
                            texel.generatedNormalRgba, texel.ormRgba};
                        std::memcpy(images[channel]->pixels + y * images[channel]->rowPitch +
                            (x + pixel) * 4u, &packed[channel], 4);
                    }
                    x += static_cast<unsigned int>(texels.size);
                }
                HRESULT mipResult = S_OK;
                if (desc.MipLevels == 1) mips = std::move(base);
                else mipResult = DirectX::GenerateMipMaps(base.GetImages(), base.GetImageCount(),
                    base.GetMetadata(), DirectX::TEX_FILTER_FANT | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, mips);
                if (FAILED(mipResult)) { logFailure("material mip generation", mipResult); return false; }
                if (mips.GetMetadata().mipLevels != desc.MipLevels) return false;
                bool cacheLayoutValid = preparedLayout.bytes == mips.GetPixelsSize();
                for (UINT channel = 0; channel < 4; ++channel)
                for (UINT mip = 0; mip < desc.MipLevels; ++mip)
                {
                    const auto *image = mips.GetImage(mip, channel, 0);
                    const auto &layout = layouts[channel * desc.MipLevels + mip];
                    cacheLayoutValid = cacheLayoutValid && image->rowPitch == image->width * 4 &&
                        image->pixels == mips.GetPixels() + preparedLayout.offsets[channel * desc.MipLevels + mip];
                    for (size_t y = 0; y < image->height; ++y)
                        std::memcpy(static_cast<unsigned char *>(mapped) + layout.Offset +
                            y * layout.Footprint.RowPitch, image->pixels + y * image->rowPitch, image->width * 4);
                }
                // The writer owns existing ordinary CPU mip memory; it never
                // reads GPU upload memory and never blocks a game frame on
                // writing hundreds of MiB to the disposable cache file.
                if (cacheLayoutValid)
                    materialUploadCache.queue(textureIndex, payloadKey, prepared, mips.GetPixels(), preparedLayout.bytes);
                return true;
            };
            if (!prepare()) { upload->Unmap(0, nullptr); return false; }
        }
        if (restoringStreamedTexture)
            transitionResource(commands, texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_COPY_DEST);
        for (UINT channel = 0; channel < 4; ++channel)
        for (const auto &range : uploadRanges)
        for (UINT mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
        {
            UINT subresource = channel * desc.MipLevels + mip;
            const auto &layout = layouts[subresource];
            D3D12_TEXTURE_COPY_LOCATION destination = {};
            destination.pResource = texture.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.SubresourceIndex = subresource;
            D3D12_TEXTURE_COPY_LOCATION source = {};
            source.pResource = upload.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = layout;
            commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
        upload->Unmap(0, nullptr);
        if (streaming) streaming->uploadsRecorded(uploadRanges);
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        commands->ResourceBarrier(1, &barrier);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = desc.Format;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srv.Texture2DArray.MipLevels = desc.MipLevels;
        srv.Texture2DArray.ArraySize = 4;
        state.device->CreateShaderResourceView(texture.Get(), &srv,
            cpuDescriptor(materialDescriptorBase + record.textureIndex));
        state.materialTextures[textureIndex] = texture;
        state.streamingMaterials[textureIndex] = streaming;
        state.materialResidency.resident(textureIndex,
            streaming ? streaming->residentBytes() : allocation.SizeInBytes);
        ++state.materialUploads;
        uploaded = true;
        frame.textureStaging.push_back(upload);
    }
    if (uploaded) trimMaterialTextures(true);
    surfaceTexelsDirty = false;
    return true;
}

void storeInstanceTransform(float destination[3][4], const float source[16])
{
    destination[0][0] = source[0];
    destination[0][1] = source[4];
    destination[0][2] = source[8];
    destination[0][3] = source[12];
    destination[1][0] = source[1];
    destination[1][1] = source[5];
    destination[1][2] = source[9];
    destination[1][3] = source[13];
    destination[2][0] = source[2];
    destination[2][1] = source[6];
    destination[2][2] = source[10];
    destination[2][3] = source[14];
}

bool buildTopLevel(ID3D12GraphicsCommandList4 *commands,
                   DxrFrameResources &frame)
{
    state.instanceDescriptions.resize(state.instances.size());
    unsigned long long topologyHash = 1469598103934665603ull;
    unsigned long long contentHash = 1469598103934665603ull;
    auto hashBytesLocal = [](unsigned long long &hash,
                             const void *data, size_t size)
    {
        const unsigned char *bytes =
            static_cast<const unsigned char *>(data);
        for (size_t byte = 0; byte < size; ++byte)
        {
            hash ^= bytes[byte];
            hash *= 1099511628211ull;
        }
    };
    const size_t instanceCount = state.instances.size();
    hashBytesLocal(topologyHash, &instanceCount, sizeof(instanceCount));
    hashBytesLocal(contentHash, &instanceCount, sizeof(instanceCount));
    for (size_t index = 0; index < state.instances.size(); ++index)
    {
        const SceneInstance &instance = state.instances[index];
        D3D12_RAYTRACING_INSTANCE_DESC &description =
            state.instanceDescriptions[index];
        description = {};
        storeInstanceTransform(description.Transform, instance.modelView);
        description.InstanceID = static_cast<unsigned int>(index);
        description.InstanceMask = static_cast<UINT8>(
            instance.instanceMask & 0xffu);
        description.InstanceContributionToHitGroupIndex =
            static_cast<unsigned int>(index) * rayTypeCount;
        description.Flags =
            D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
        if (!instance.clipEnabled && instance.surfaceVariant && instance.surfaceVariant->opaqueForRays)
            description.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        if (instance.clipEnabled)
            description.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
        const UINT instanceFlagsForHash = description.Flags;
        hashBytesLocal(contentHash, &instanceFlagsForHash, sizeof(instanceFlagsForHash));
        description.AccelerationStructure =
            instance.geometry->blas->GetGPUVirtualAddress();
        hashBytesLocal(topologyHash, &description.AccelerationStructure,
                       sizeof(description.AccelerationStructure));
        hashBytesLocal(contentHash, &description.AccelerationStructure,
                       sizeof(description.AccelerationStructure));
        hashBytesLocal(contentHash, description.Transform,
                       sizeof(description.Transform));
        /* D3D12_RAYTRACING_INSTANCE_DESC::InstanceMask is an 8-bit
           bit-field. MSVC correctly rejects taking its address or applying
           sizeof to it. Hash a normal byte containing the exact effective
           mask instead. */
        const UINT8 instanceMaskForHash = static_cast<UINT8>(description.InstanceMask);
        hashBytesLocal(contentHash, &instanceMaskForHash,
                       sizeof(instanceMaskForHash));
    }

    /* A flight slot may revisit a completely unchanged scene (paused editor,
       menus, static camera shots).  Reuse that slot's already-built TLAS
       verbatim instead of uploading instance descriptions and issuing a no-op
       refit. */
    if (frame.tlasBuilt && frame.tlas &&
        frame.tlasContentHash == contentHash)
    {
        ++state.tlasReuses;
        return true;
    }
    if (!updateReusableUpload(
            state.instanceDescriptions.data(),
            state.instanceDescriptions.size() * sizeof(state.instanceDescriptions[0]),
            L"Homeworld DXR instance descriptions", frame.instanceUpload))
    {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = static_cast<unsigned int>(
        state.instanceDescriptions.size());
    inputs.InstanceDescs = frame.instanceUpload.resource->GetGPUVirtualAddress();
    inputs.Flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    state.device->GetRaytracingAccelerationStructurePrebuildInfo(
        &inputs, &prebuild);
    ID3D12Resource *previousTlas = frame.tlas.Get();
    const bool tlasNeedsAllocation = !frame.tlas || frame.tlasCapacity <
        alignUp<unsigned long long>(prebuild.ResultDataMaxSizeInBytes, 256);
    const unsigned long long scratchSize = std::max<unsigned long long>(
        prebuild.ScratchDataSizeInBytes,
        prebuild.UpdateScratchDataSizeInBytes);
    if (!ensureAccelerationBuffer(commands,
            scratchSize, frame.tlasScratchCapacity,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, frame.tlasScratch) ||
        !ensureAccelerationBuffer(commands,
            prebuild.ResultDataMaxSizeInBytes, frame.tlasCapacity,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            frame.tlas))
    {
        return false;
    }

    const bool canUpdate = frame.tlasBuilt && !tlasNeedsAllocation &&
        previousTlas == frame.tlas.Get() &&
        frame.tlasTopologyHash == topologyHash;
    if (canUpdate)
    {
        inputs.Flags |=
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
    build.Inputs = inputs;
    build.ScratchAccelerationStructureData =
        frame.tlasScratch->GetGPUVirtualAddress();
    build.DestAccelerationStructureData = frame.tlas->GetGPUVirtualAddress();
    build.SourceAccelerationStructureData = canUpdate
        ? frame.tlas->GetGPUVirtualAddress() : 0;
    commands->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = frame.tlas.Get();
    commands->ResourceBarrier(1, &barrier);
    frame.tlasBuilt = true;
    frame.tlasTopologyHash = topologyHash;
    frame.tlasContentHash = contentHash;
    if (canUpdate)
        ++state.tlasUpdates;
    else
        ++state.fullTlasBuilds;
    return true;
}

void transformPoint(float destination[3], const float source[3],
                    const float matrix[16])
{
    destination[0] = matrix[0] * source[0] + matrix[4] * source[1] +
                     matrix[8] * source[2] + matrix[12];
    destination[1] = matrix[1] * source[0] + matrix[5] * source[1] +
                     matrix[9] * source[2] + matrix[13];
    destination[2] = matrix[2] * source[0] + matrix[6] * source[1] +
                     matrix[10] * source[2] + matrix[14];
}

void transformDirection(float destination[3], const float source[3],
                        const float matrix[16])
{
    destination[0] = matrix[0] * source[0] + matrix[4] * source[1] +
                     matrix[8] * source[2];
    destination[1] = matrix[1] * source[0] + matrix[5] * source[1] +
                     matrix[9] * source[2];
    destination[2] = matrix[2] * source[0] + matrix[6] * source[1] +
                     matrix[10] * source[2];
    const float length = std::sqrt(destination[0] * destination[0] +
                                   destination[1] * destination[1] +
                                   destination[2] * destination[2]);
    if (length > 0.00001f)
    {
        destination[0] /= length;
        destination[1] /= length;
        destination[2] /= length;
    }
}

float degreesToCosine(float degrees)
{
    constexpr float pi = 3.14159265358979323846f;
    return std::cos(degrees * (pi / 180.0f));
}

void unpackRgba(unsigned int packed, float color[4])
{
    color[0] = static_cast<float>(packed & 0xffu) / 255.0f;
    color[1] = static_cast<float>((packed >> 8u) & 0xffu) / 255.0f;
    color[2] = static_cast<float>((packed >> 16u) & 0xffu) / 255.0f;
    color[3] = static_cast<float>((packed >> 24u) & 0xffu) / 255.0f;
}

unsigned int wrappedTexelCoordinate(float value, unsigned int size)
{
    const float wrapped = value - std::floor(value);
    return std::min(size - 1u,
        static_cast<unsigned int>(wrapped * static_cast<float>(size)));
}

bool sampleEmissiveTexel(const GpuTriangleSurface &surface,
                         float barycentric0, float barycentric1,
                         float barycentric2, float rgba[4])
{
    if ((surface.flags & gpuSurfaceHasTexture) == 0 ||
        surface.textureWidth == 0 || surface.textureHeight == 0)
    {
        rgba[0] = surface.emissiveColor[0];
        rgba[1] = surface.emissiveColor[1];
        rgba[2] = surface.emissiveColor[2];
        rgba[3] = surface.emissiveIntensity > 0.0f ? 1.0f : 0.0f;
        return rgba[3] > 0.0f;
    }
    const float u = surface.uv0[0] * barycentric0 +
                    surface.uv1[0] * barycentric1 +
                    surface.uv2[0] * barycentric2;
    const float v = surface.uv0[1] * barycentric0 +
                    surface.uv1[1] * barycentric1 +
                    surface.uv2[1] * barycentric2;
    const unsigned int x = wrappedTexelCoordinate(u, surface.textureWidth);
    const unsigned int y = wrappedTexelCoordinate(v, surface.textureHeight);
    const size_t texelIndex = static_cast<size_t>(surface.textureOffset) +
        static_cast<size_t>(y) * surface.textureWidth + x;
    if (texelIndex >= surfaceTexels.size())
    {
        std::memset(rgba, 0, sizeof(float) * 4);
        return false;
    }
    unpackRgba(surfaceTexels[texelIndex].emissiveRgba, rgba);
    return rgba[3] > 0.0f &&
           (rgba[0] > 0.0f || rgba[1] > 0.0f || rgba[2] > 0.0f);
}

void buildEmissiveLightPrototypes(
    GeometryRecord &geometry, GeometryRecord::SurfaceVariant &variant)
{
    if (variant.emissiveLightsBuilt)
    {
        return;
    }
    variant.emissiveLightsBuilt = true;
    static const float sampleBarycentrics[][3] = {
        { 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f },
        { 0.80f, 0.10f, 0.10f },
        { 0.10f, 0.80f, 0.10f },
        { 0.10f, 0.10f, 0.80f }
    };
    const std::vector<unsigned int> &indices = geometry.indices;
    const std::vector<float> &positions = geometry.positions;
    const size_t triangleCount = std::min(
        variant.surfaces.size(), indices.size() / 3);
    variant.emissiveLights.reserve(std::min<size_t>(
        triangleCount, maximumEmissiveLightsPerVariant));
    for (size_t triangleIndex = 0; triangleIndex < triangleCount;
         ++triangleIndex)
    {
        const GpuTriangleSurface &surface = variant.surfaces[triangleIndex];
        if (surface.emissiveIntensity <= 0.0f)
        {
            continue;
        }
        const unsigned int i0 = indices[triangleIndex * 3 + 0];
        const unsigned int i1 = indices[triangleIndex * 3 + 1];
        const unsigned int i2 = indices[triangleIndex * 3 + 2];
        if (static_cast<size_t>(std::max(i0, std::max(i1, i2))) * 3 + 2 >=
            positions.size())
        {
            continue;
        }
        const float *p0 = &positions[static_cast<size_t>(i0) * 3];
        const float *p1 = &positions[static_cast<size_t>(i1) * 3];
        const float *p2 = &positions[static_cast<size_t>(i2) * 3];
        const float edge0[3] = {
            p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
        };
        const float edge1[3] = {
            p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
        };
        const float crossProduct[3] = {
            edge0[1] * edge1[2] - edge0[2] * edge1[1],
            edge0[2] * edge1[0] - edge0[0] * edge1[2],
            edge0[0] * edge1[1] - edge0[1] * edge1[0]
        };
        const float area = 0.5f * std::sqrt(
            crossProduct[0] * crossProduct[0] +
            crossProduct[1] * crossProduct[1] +
            crossProduct[2] * crossProduct[2]);

        EmissiveLightPrototype best;
        for (const auto &barycentric : sampleBarycentrics)
        {
            float emissive[4];
            if (!sampleEmissiveTexel(surface, barycentric[0],
                                      barycentric[1], barycentric[2],
                                      emissive))
            {
                continue;
            }
            const float luminance = 0.2126f * emissive[0] +
                                    0.7152f * emissive[1] +
                                    0.0722f * emissive[2];
            const float score = luminance * surface.emissiveIntensity *
                                std::max(area, 0.01f);
            if (score <= best.score)
            {
                continue;
            }
            best.score = score;
            best.position[0] = p0[0] * barycentric[0] +
                               p1[0] * barycentric[1] +
                               p2[0] * barycentric[2];
            best.position[1] = p0[1] * barycentric[0] +
                               p1[1] * barycentric[1] +
                               p2[1] * barycentric[2];
            best.position[2] = p0[2] * barycentric[0] +
                               p1[2] * barycentric[1] +
                               p2[2] * barycentric[2];
            best.radius = std::max(
                3.0f, std::sqrt(std::max(area, 1.0f)) * 8.0f);
            best.engineBlendEnabled = surface.engineBlendEnabled != 0;
            float idleEmission[4] = {};
            if (best.engineBlendEnabled) {
                GpuTriangleSurface idleSurface=surface;
                idleSurface.textureOffset=surface.idleTextureOffset;
                sampleEmissiveTexel(idleSurface,barycentric[0],barycentric[1],barycentric[2],idleEmission);
            } else {
                std::copy(emissive,emissive+4,idleEmission);
            }
            std::copy(idleEmission,idleEmission+3,best.idleColor);
            best.color[0] = emissive[0];
            best.color[1] = emissive[1];
            best.color[2] = emissive[2];
            best.intensity = surface.emissiveIntensity * 0.08f;
        }
        if (best.score > 0.0f)
        {
            variant.emissiveLights.push_back(best);
        }
    }

    const auto brighter = [](const EmissiveLightPrototype &left,
                             const EmissiveLightPrototype &right)
    {
        return left.score > right.score;
    };
    if (variant.emissiveLights.size() > maximumEmissiveLightsPerVariant)
    {
        std::nth_element(
            variant.emissiveLights.begin(),
            variant.emissiveLights.begin() + maximumEmissiveLightsPerVariant,
            variant.emissiveLights.end(), brighter);
        variant.emissiveLights.resize(maximumEmissiveLightsPerVariant);
    }
    std::sort(variant.emissiveLights.begin(),
              variant.emissiveLights.end(), brighter);
}

float instanceMaximumScale(const float matrix[16])
{
    float maximum = 1.0f;
    for (unsigned int column = 0; column < 3; ++column)
    {
        const float x = matrix[column * 4 + 0];
        const float y = matrix[column * 4 + 1];
        const float z = matrix[column * 4 + 2];
        maximum = std::max(maximum, std::sqrt(x * x + y * y + z * z));
    }
    return maximum;
}

void appendEmissiveTriangleLights(std::vector<GpuLight> &result)
{
    const size_t remainingLights = maximumLights - result.size();
    if (remainingLights == 0)
    {
        return;
    }
    state.emissiveCandidates.clear();
    for (const SceneInstance &instance : state.instances)
    {
        if (instance.surfaceVariant == nullptr)
        {
            continue;
        }
        buildEmissiveLightPrototypes(
            *instance.geometry, *instance.surfaceVariant);
        const float scale = instanceMaximumScale(instance.modelView);
        const float areaScale = scale * scale;
        unsigned int instanceEmissiveCount = 0;
        for (const EmissiveLightPrototype &prototype :
             instance.surfaceVariant->emissiveLights)
        {
            if (instanceEmissiveCount >= maximumEmissiveLightsPerInstance)
            {
                break;
            }
            EmissiveLightCandidate candidate;
            candidate.score = prototype.score * areaScale;
            candidate.light.type = gpuLightPoint;
            transformPoint(candidate.light.position, prototype.position,
                           instance.modelView);
            candidate.light.radius = prototype.radius * scale;
            candidate.light.color[0] = prototype.color[0];
            candidate.light.color[1] = prototype.color[1];
            candidate.light.color[2] = prototype.color[2];
            if(prototype.engineBlendEnabled) {
                for(unsigned channel=0;channel<3;++channel)
                    candidate.light.color[channel]=prototype.idleColor[channel]+instance.engineBlend*
                        (prototype.color[channel]-prototype.idleColor[channel]);
                float onLuma=0.2126f*prototype.color[0]+0.7152f*prototype.color[1]+0.0722f*prototype.color[2];
                float blendLuma=0.2126f*candidate.light.color[0]+0.7152f*candidate.light.color[1]+0.0722f*candidate.light.color[2];
                candidate.score*=onLuma>0 ? blendLuma/onLuma : 0;
            }
            candidate.light.intensity = prototype.intensity *
                                        fxLightingStrength;
            candidate.light.source =
                static_cast<float>(HW_MODERN_LIGHT_SHIP_EMISSIVE);
            state.emissiveCandidates.push_back(candidate);
            ++instanceEmissiveCount;
        }
    }

    const auto brighter = [](const EmissiveLightCandidate &left,
                             const EmissiveLightCandidate &right)
    {
        return left.score > right.score;
    };
    const size_t emissiveBudget = std::min<size_t>(
        remainingLights, maximumSceneEmissiveLights);
    if (state.emissiveCandidates.size() > emissiveBudget)
    {
        std::nth_element(
            state.emissiveCandidates.begin(),
            state.emissiveCandidates.begin() + emissiveBudget,
            state.emissiveCandidates.end(), brighter);
        state.emissiveCandidates.resize(emissiveBudget);
    }
    std::sort(state.emissiveCandidates.begin(),
              state.emissiveCandidates.end(), brighter);
    for (const EmissiveLightCandidate &candidate : state.emissiveCandidates)
    {
        result.push_back(candidate.light);
    }
}

void buildLights(
    const HWModernMapLightEmitter *mapLights, unsigned int mapLightCount,
    const HWModernDynamicLightEmitter *dynamicLights,
    unsigned int dynamicLightCount, std::vector<GpuLight> &result,
    const float *view = nullptr, bool geometryEmission = true)
{
    if (!view) view=state.viewMatrix;
    result.clear();
    if (result.capacity() < maximumLights)
    {
        result.reserve(maximumLights);
    }

    /* The mission sky owns the global lighting frame. The same large
       coherent bright object that produces god rays is the one and only
       global directional key. It is inserted first so it remains the primary
       shadow-casting source even in scenes with many local emitters. */
    if (missionSunValid && requestedPrimarySunStrength > 0.0001f &&
        result.size() < maximumLights)
    {
        GpuLight sun = {};
        sun.type = gpuLightDirectional;
        transformDirection(sun.direction, missionSunWorldDirection,
                           view);
        /* RTX-0068: SceneLight.direction is a surface-to-light vector. The
           mission sky metadata already stores the world-space direction FROM
           the scene TOWARD the visible key source, exactly matching the HLSL
           convention used by dot(normal, lightVector) and shadow rays. Do not
           negate it here. RTX-0067's extra sign flip illuminated the hemisphere
           facing away from the visible nebula/source. */
        sun.color[0] = missionSunColor[0];
        sun.color[1] = missionSunColor[1];
        sun.color[2] = missionSunColor[2];
        sun.intensity = 1.35f * requestedPrimarySunStrength;
        sun.source = -100.0f;
        result.push_back(sun);
    }
    if (missionSkyAvailable && requestedSkyAmbientStrength > 0.0001f &&
        result.size() < maximumLights)
    {
        GpuLight ambient = {};
        ambient.type = gpuLightAmbient;
        ambient.color[0] = missionAmbientColor[0];
        ambient.color[1] = missionAmbientColor[1];
        ambient.color[2] = missionAmbientColor[2];
        ambient.intensity = 0.85f * requestedSkyAmbientStrength;
        ambient.source = -101.0f;
        result.push_back(ambient);
    }

    /* RTX-0077: live mission-authored point/spot/ambient lights are additive
       and deliberately independent from immutable HSF map lights. They are
       inserted before HSF/dynamic emitters so a lighting artist can rely on
       them even in effect-heavy scenes. */
    for (unsigned int index = 0;
         index < missionAuthoringLights.size() &&
         result.size() < maximumLights; ++index)
    {
        const HWModernMissionLightEmitter &source = missionAuthoringLights[index];
        if (!source.enabled || source.intensity <= 0.0001f)
        {
            continue;
        }
        GpuLight light = {};
        light.color[0] = source.color[0];
        light.color[1] = source.color[1];
        light.color[2] = source.color[2];
        light.intensity = source.intensity * requestedAuthoredLightStrength;
        light.source = -200.0f - static_cast<float>(index);
        if (source.type == HW_MODERN_MISSION_LIGHT_AMBIENT)
        {
            light.type = gpuLightAmbient;
        }
        else
        {
            light.type = source.type == HW_MODERN_MISSION_LIGHT_SPOT
                ? gpuLightSpot : gpuLightPoint;
            transformPoint(light.position, source.position, view);
            transformDirection(light.direction, source.direction, view);
            light.radius = std::max(1.0f, source.radius);
            light.coneCos = degreesToCosine(source.coneAngle);
            light.edgeCos = degreesToCosine(source.coneAngle + source.edgeAngle);
        }
        result.push_back(light);
    }

    for (unsigned int index = 0;
         index < mapLightCount && result.size() < maximumLights; ++index)
    {
        const HWModernMapLightEmitter &source = mapLights[index];
        /* RTX-0079: once the live editor adopts the local HSF emitters, their
           immutable originals must stop contributing or every imported light
           would be doubled. Keep HSF directional/ambient metadata on the
           existing sky policy below. */
        if (missionAuthoringReplacesMapLights && source.type != 0 &&
            source.type != 5)
        {
            continue;
        }
        /* Once the sky provides a real stellar key, legacy HSF directional
           lights remain authoring metadata but no longer compete for the
           global shadow direction. The measured sky mean also replaces HSF
           global ambient; point/spot HSF lights remain as secondary/local
           illumination. */
        if ((missionSunValid && source.type == 0) ||
            (missionSkyAvailable && source.type == 5))
        {
            continue;
        }
        GpuLight light = {};
        light.color[0] = source.color[0];
        light.color[1] = source.color[1];
        light.color[2] = source.color[2];
        light.intensity = std::max(0.0f, source.intensity) *
                          requestedAuthoredLightStrength;
        light.radius = 250000.0f;
        light.coneCos = degreesToCosine(source.coneAngle);
        light.edgeCos = degreesToCosine(
            source.coneAngle + source.edgeAngle);
        if (source.type == 5)
        {
            light.type = gpuLightAmbient;
        }
        else if (source.type == 0)
        {
            light.type = gpuLightDirectional;
            transformDirection(light.direction, source.renderDirection,
                               view);
        }
        else
        {
            light.type = source.type == 2 ? gpuLightSpot : gpuLightPoint;
            transformPoint(light.position, source.renderPosition,
                           view);
            transformDirection(light.direction, source.renderDirection,
                               view);
        }
        result.push_back(light);
    }

    std::vector<const HWModernDynamicLightEmitter *> &rankedDynamicLights =
        state.rankedDynamicLights;
    rankedDynamicLights.clear();
    if (rankedDynamicLights.capacity() < dynamicLightCount)
    {
        rankedDynamicLights.reserve(dynamicLightCount);
    }
    for (unsigned int index = 0; index < dynamicLightCount; ++index)
    {
        rankedDynamicLights.push_back(&dynamicLights[index]);
    }
    std::stable_sort(
        rankedDynamicLights.begin(), rankedDynamicLights.end(),
        [](const HWModernDynamicLightEmitter *left,
           const HWModernDynamicLightEmitter *right)
        {
            const auto priority = [](unsigned int source)
            {
                switch (source)
                {
                    case HW_MODERN_LIGHT_WEAPON_IMPACT: return 7;
                    case HW_MODERN_LIGHT_MUZZLE_FLASH: return 6;
                    case HW_MODERN_LIGHT_EXPLOSION: return 5;
                    case HW_MODERN_LIGHT_ION_BEAM: return 4;
                    case HW_MODERN_LIGHT_WEAPON: return 3;
                    case HW_MODERN_LIGHT_NATIVE_FX: return 3;
                    case HW_MODERN_LIGHT_NAV: return 2;
                    case HW_MODERN_LIGHT_ENGINE: return 1;
                    default: return 0;
                }
            };
            const int leftPriority = priority(left->source);
            const int rightPriority = priority(right->source);
            if (leftPriority != rightPriority)
            {
                return leftPriority > rightPriority;
            }
            return left->intensity * left->radius >
                   right->intensity * right->radius;
        });

    for (const HWModernDynamicLightEmitter *rankedSource :
         rankedDynamicLights)
    {
        if (result.size() >= maximumLights)
        {
            break;
        }
        const HWModernDynamicLightEmitter &source = *rankedSource;
        GpuLight light = {};
        light.type = source.shape == HW_MODERN_LIGHT_LINE
            ? gpuLightLine : gpuLightPoint;
        transformPoint(light.position, source.position, view);
        transformPoint(light.endPosition, source.endPosition, view);
        transformDirection(light.direction, source.direction, view);
        light.radius = std::max(1.0f, source.radius);
        light.color[0] = source.color[0];
        light.color[1] = source.color[1];
        light.color[2] = source.color[2];
        const bool dedicatedWeaponEndpoint =
            source.source == HW_MODERN_LIGHT_MUZZLE_FLASH ||
            source.source == HW_MODERN_LIGHT_WEAPON_IMPACT;
        light.intensity = std::max(0.0f, source.intensity) *
                          (source.source == HW_MODERN_LIGHT_NATIVE_FX ? 1.0f : 0.02f) *
                          (dedicatedWeaponEndpoint ? 1.0f : fxLightingStrength);
        light.source = static_cast<float>(source.source);
        result.push_back(light);
    }

    /* Texture-driven emitters are ranked after authored map and exact dynamic
       lights, ensuring nav/weapon/engine emitters are never displaced by a
       large number of small glow-map triangles. */
    if (geometryEmission) appendEmissiveTriangleLights(result);

    if (result.empty())
    {
        GpuLight fallback = {};
        fallback.type = gpuLightAmbient;
        fallback.color[0] = fallback.color[1] = fallback.color[2] = 0.2f;
        fallback.intensity = 1.0f;
        result.push_back(fallback);
    }
}

bool createShaderTable(DxrFrameResources &frame,
                       D3D12_DISPATCH_RAYS_DESC &dispatch, bool feedback)
{
    const unsigned int rayGenerationOffset = 0;
    const unsigned int rayGenerationSize = shaderIdentifierSize;
    const unsigned int missOffset = alignUp(
        rayGenerationOffset + rayGenerationSize,
        static_cast<unsigned int>(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    const unsigned int missStride = alignUp(
        shaderIdentifierSize,
        static_cast<unsigned int>(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
    const unsigned int missSize = missStride * rayTypeCount;
    const unsigned int hitOffset = alignUp(
        missOffset + missSize,
        static_cast<unsigned int>(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    const unsigned int hitStride = alignUp(
        shaderIdentifierSize + static_cast<unsigned int>(sizeof(LocalRootArguments)),
        static_cast<unsigned int>(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
    const unsigned int hitSize = hitStride * rayTypeCount *
        static_cast<unsigned int>(state.instances.size());
    const unsigned int totalSize = alignUp(
        hitOffset + hitSize, static_cast<unsigned int>(256));

    /* Shader records depend on the state object and per-instance geometry /
       material GPU addresses, but not on transforms.  Cache a table in every
       flight slot and rebuild only when that exact binding topology changes. */
    unsigned long long signature = 1469598103934665603ull;
    auto hashValue = [&signature](unsigned long long value)
    {
        for (unsigned int byte = 0; byte < 8u; ++byte)
        {
            signature ^= static_cast<unsigned char>(value >> (byte * 8u));
            signature *= 1099511628211ull;
        }
    };
    const auto object = feedback ? state.feedbackStateObject.Get() : state.stateObject.Get();
    const auto properties = feedback ? state.feedbackStateObjectProperties.Get() : state.stateObjectProperties.Get();
    hashValue(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(object)));
    hashValue(static_cast<unsigned long long>(state.instances.size()));
    for (const SceneInstance &instance : state.instances)
    {
        hashValue(instance.geometry->vertexBuffer->GetGPUVirtualAddress());
        hashValue(instance.geometry->indexBuffer->GetGPUVirtualAddress());
        hashValue(instance.surfaceVariant->buffer->GetGPUVirtualAddress());
    }

    const bool rebuild = !frame.shaderTableValid ||
        frame.shaderTableSignature != signature || !frame.shaderTable.resource ||
        frame.shaderTable.capacity < totalSize;
    if (rebuild)
    {
        const void *rayGeneration =
            properties->GetShaderIdentifier(L"RayGeneration");
        const void *primaryMiss =
            properties->GetShaderIdentifier(L"PrimaryMiss");
        const void *shadowMiss =
            properties->GetShaderIdentifier(L"ShadowMiss");
        const void *primaryHit =
            properties->GetShaderIdentifier(L"PrimaryHitGroup");
        const void *shadowHit =
            properties->GetShaderIdentifier(L"ShadowHitGroup");
        if (!rayGeneration || !primaryMiss || !shadowMiss || !primaryHit || !shadowHit)
            return false;

        state.shaderTableBytes.resize(totalSize);
        std::fill(state.shaderTableBytes.begin(), state.shaderTableBytes.end(), 0u);
        std::memcpy(state.shaderTableBytes.data() + rayGenerationOffset,
                    rayGeneration, shaderIdentifierSize);
        std::memcpy(state.shaderTableBytes.data() + missOffset,
                    primaryMiss, shaderIdentifierSize);
        std::memcpy(state.shaderTableBytes.data() + missOffset + missStride,
                    shadowMiss, shaderIdentifierSize);

        for (size_t index = 0; index < state.instances.size(); ++index)
        {
            const SceneInstance &instance = state.instances[index];
            unsigned char *primaryRecord = state.shaderTableBytes.data() + hitOffset +
                static_cast<unsigned int>(index) * rayTypeCount * hitStride;
            std::memcpy(primaryRecord, primaryHit, shaderIdentifierSize);
            LocalRootArguments arguments = {};
            arguments.vertices =
                instance.geometry->vertexBuffer->GetGPUVirtualAddress();
            arguments.triangles =
                instance.geometry->indexBuffer->GetGPUVirtualAddress();
            arguments.surfaces =
                instance.surfaceVariant->buffer->GetGPUVirtualAddress();
            std::memcpy(primaryRecord + shaderIdentifierSize,
                        &arguments, sizeof(arguments));

            unsigned char *shadowRecord = primaryRecord + hitStride;
            std::memcpy(shadowRecord, shadowHit, shaderIdentifierSize);
            std::memcpy(shadowRecord + shaderIdentifierSize,
                        &arguments, sizeof(arguments));
        }
        if (!updateReusableUpload(
                state.shaderTableBytes.data(), state.shaderTableBytes.size(),
                L"Homeworld DXR shader table", frame.shaderTable))
        {
            frame.shaderTableValid = false;
            return false;
        }
        frame.shaderTableSignature = signature;
        frame.shaderTableValid = true;
        ++state.shaderTableRebuilds;
    }
    else
    {
        ++state.shaderTableCacheHits;
    }

    const D3D12_GPU_VIRTUAL_ADDRESS base =
        frame.shaderTable.resource->GetGPUVirtualAddress();
    dispatch.RayGenerationShaderRecord.StartAddress =
        base + rayGenerationOffset;
    dispatch.RayGenerationShaderRecord.SizeInBytes = rayGenerationSize;
    dispatch.MissShaderTable.StartAddress = base + missOffset;
    dispatch.MissShaderTable.SizeInBytes = missSize;
    dispatch.MissShaderTable.StrideInBytes = missStride;
    dispatch.HitGroupTable.StartAddress = base + hitOffset;
    dispatch.HitGroupTable.SizeInBytes = hitSize;
    dispatch.HitGroupTable.StrideInBytes = hitStride;
    dispatch.Width = state.width;
    dispatch.Height = state.height;
    dispatch.Depth = 1;
    return true;
}

void transitionResource(ID3D12GraphicsCommandList *commands,
                        ID3D12Resource *resource,
                        D3D12_RESOURCE_STATES before,
                        D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    commands->ResourceBarrier(1, &barrier);
}

void transitionOutput(ID3D12GraphicsCommandList *commands,
                      D3D12_RESOURCE_STATES before,
                      D3D12_RESOURCE_STATES after)
{
    transitionResource(commands, state.output.Get(), before, after);
}

bool uploadInstanceMotion(DxrFrameResources &frame)
{
    state.instanceMotion.resize(state.instances.size());
    for (size_t index = 0; index < state.instances.size(); ++index)
    {
        storeInstanceTransform(state.instanceMotion[index].previousRows,
                               state.instances[index].previousModelView);
        state.instanceMotion[index].historyIdentity = state.instances[index].historyIdentity;
        std::memcpy(state.instanceMotion[index].clipEquationEye,state.instances[index].clipEquationEye,
                    sizeof(state.instanceMotion[index].clipEquationEye));
        state.instanceMotion[index].flags = (state.instances[index].clipEnabled ? 1u : 0u) |
            (state.instances[index].surfaceHistoryValid ? 2u : 0u);
        state.instanceMotion[index].engineBlend=state.instances[index].engineBlend;
    }
    return updateReusableUpload(
        state.instanceMotion.data(),
        state.instanceMotion.size() * sizeof(state.instanceMotion[0]),
        L"Homeworld DXR instance motion", frame.instanceMotionUpload);
}

void hashBytes(unsigned long long &hash, const void *bytes, size_t size)
{
    const unsigned char *source = static_cast<const unsigned char *>(bytes);
    for (size_t index = 0; index < size; ++index)
    {
        hash ^= source[index];
        hash *= 1099511628211ull;
    }
}

unsigned long long buildSceneHash()
{
    unsigned long long hash = 1469598103934665603ull;
    /* Object/camera transforms and dynamic lights are deliberately excluded.
       Their per-pixel motion is reprojected in the ray-generation shader;
       hashing exact floats here used to reset history on virtually every
       gameplay frame and made the indirect pass look like film grain. */
    hashBytes(hash, &state.verticalFieldOfViewDegrees,
              sizeof(state.verticalFieldOfViewDegrees));
    hashBytes(hash, &state.aspectRatio, sizeof(state.aspectRatio));
    return hash;
}
}

void resolveRasterSceneLights(const float view[16],
    const HWModernMapLightEmitter *map,unsigned mapCount,
    const HWModernDynamicLightEmitter *dynamic,unsigned dynamicCount,
    std::vector<ResolvedSceneLight>& output) {
    buildLights(map,mapCount,dynamic,dynamicCount,output,view,false);
}

bool raytracingInitialize(ID3D12Device *device,
                          ID3D12DescriptorHeap *shaderVisibleHeap,
                          unsigned int descriptorSize,
                          unsigned int outputSrvIndex,
                          unsigned int outputUavIndex,
                          unsigned int environmentSrvIndex,
                          unsigned int rrGuideUavBaseIndex,
                          ID3D12CommandQueue *directQueue)
{
    raytracingShutdown();
    if (device == nullptr || shaderVisibleHeap == nullptr || descriptorSize == 0)
    {
        return false;
    }
    HRESULT result = device->QueryInterface(IID_PPV_ARGS(&state.device));
    if (FAILED(result))
    {
        return false;
    }
    // Optional budget feedback. Failure leaves the bounded warm-cache policy
    // active; it does not change texture resolution or disable rendering.
    ComPtr<IDXGIFactory4> memoryFactory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&memoryFactory))))
        memoryFactory->EnumAdapterByLuid(state.device->GetAdapterLuid(),
            IID_PPV_ARGS(&state.memoryBudgetAdapter));
    // Pool only explicitly initialized resources. Histories retain their
    // committed zero-fill contract; allocator failure preserves the old path.
    state.gpuMemory.initialize(state.device.Get(), state.memoryBudgetAdapter.Get());
    state.descriptorHeap = shaderVisibleHeap;
    state.descriptorSize = descriptorSize;
    state.outputSrvIndex = outputSrvIndex;
    state.outputUavIndex = outputUavIndex;
    state.accumulationUavIndex = outputUavIndex + 1;
    state.depthUavIndex = outputUavIndex + 2;
    state.motionUavIndex = outputUavIndex + 3;
    state.historyUavIndex = outputUavIndex + 4;
    state.motionSrvIndex = outputUavIndex + 5;
    state.environmentSrvIndex = environmentSrvIndex;
    state.rrGuideUavBaseIndex = rrGuideUavBaseIndex;
    // Old tools/unsupported devices retain the original lib_6_3 path. The
    // added descriptor range must exist before any feedback resources bind.
    hwWindowsSplashTaskBegin("renderer.dxr.feedback", "Hardware feedback setup");
    if (directQueue && shaderVisibleHeap->GetDesc().NumDescriptors >=
        materialMipRequestDescriptorIndex + 1u &&
        state.samplerFeedback.initialize(device, directQueue, materialDescriptorCount))
    {
        state.samplerFeedback.createNullViews(cpuDescriptor(materialFeedbackDescriptorBase),
            descriptorSize, materialDescriptorCount);
        hwWindowsSplashTaskComplete("renderer.dxr.feedback", "Hardware feedback descriptors ready");
    }
    else hwWindowsSplashTaskSkip("renderer.dxr.feedback", "Hardware feedback unavailable");
    hwWindowsSplashTaskBegin("renderer.dxr.roots", "Raytracing root signatures");
    if (!createRootSignatures())
    {
        raytracingShutdown();
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.dxr.roots", "Raytracing bindings ready");
    hwWindowsSplashTaskBegin("renderer.dxr.pipeline", "Raytracing pipeline");
    if (!createStateObject())
    {
        raytracingShutdown();
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.dxr.pipeline", "Raytracing pipeline ready");
    if (state.samplerFeedback.supported())
    {
        hwWindowsSplashTaskBegin("renderer.dxr.feedback-pipeline", "Raytracing feedback pipelines");
        hwWindowsSplashUpdate("Creating material-feedback compute pipeline");
        const bool mipPipelineReady = createMaterialMipPipeline();
        if (mipPipelineReady)
            hwWindowsSplashUpdate("Creating raytracing feedback capture pipeline");
        if (!mipPipelineReady || !createStateObject(true))
        {
            state.feedbackStateObject.Reset();
            state.feedbackStateObjectProperties.Reset();
            state.materialMipRequests.Reset();
            state.materialMipClearHeap.Reset();
            state.materialMipPipeline.Reset();
            state.materialMipRoot.Reset();
            state.samplerFeedback = {};
            HW_DIAG_FPRINTF(stderr, "[TexturePerf] Sampler feedback pipeline unavailable; full-resolution committed fallback.\n");
            hwWindowsSplashTaskSkip("renderer.dxr.feedback-pipeline", "Optional feedback pipeline unavailable");
        }
        else hwWindowsSplashTaskComplete("renderer.dxr.feedback-pipeline", "Feedback pipelines ready");
    }
    else hwWindowsSplashTaskSkip("renderer.dxr.feedback-pipeline", "Hardware feedback unavailable");
    state.active = true;
    if (state.feedbackStateObject)
        HW_DIAG_FPRINTF(stderr, "[TexturePerf] Exact-quality sampler feedback warm-mip streaming enabled (tier=%u, capture=1/60); active/in-flight materials remain fully resident.\n",
            static_cast<unsigned int>(state.samplerFeedback.tier()));
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] DXR multi-bounce path-tracing pipeline "
                 "created; waiting for scene geometry.\n");
    return true;
}

void raytracingShutdown(void)
{
    const auto &uploadCacheStats = materialUploadCache.stats();
    if (uploadCacheStats.stores || uploadCacheStats.hits || uploadCacheStats.failures)
        HW_DIAG_FPRINTF(stderr, "[TexturePerf] Prepared material cache: %llu stores, %llu restores without texel decode/mip rebuild; %.1f MiB temporary backing, %llu I/O failures.\n",
            uploadCacheStats.stores, uploadCacheStats.hits,
            static_cast<double>(materialUploadCache.storedBytes()) / (1024.0 * 1024.0), uploadCacheStats.failures);
    materialUploadCache.clear();
    if (state.feedbackCaptures)
        HW_DIAG_FPRINTF(stderr, "[TexturePerf] Sampler feedback capture frames=%llu; inactive fine-mip physical heaps retired=%.1f MiB; pending retirement=%.1f MiB.\n",
            state.feedbackCaptures, static_cast<double>(state.feedbackTrimmedBytes) / (1024.0 * 1024.0),
            static_cast<double>(state.samplerFeedback.pendingRetiredBytes()) / (1024.0 * 1024.0));
    if (state.materialUploads)
        HW_DIAG_FPRINTF(stderr,
            "[TexturePerf] DXR material residency=%zu/%zu records %.1f MiB; "
            "%llu full-resolution uploads, %llu fence-safe cache evictions.\n",
            state.materialResidency.residentCount(), materialRecords.size(),
            static_cast<double>(state.materialResidency.residentBytes()) / (1024.0 * 1024.0),
            state.materialUploads, state.materialEvictions);
    if (surfaceRegistrationCalls != 0)
    {
        HW_DIAG_FPRINTF(stderr,
            "[TexturePerf] DXR surface registration=%llu calls %.1f Mtexels %.1f ms; logical atlas=%.1f MiB; lossless CPU backing/cache=%.1f MiB\n",
            surfaceRegistrationCalls,
            static_cast<double>(surfaceRegistrationTexels) / 1000000.0,
            surfaceRegistrationMilliseconds,
            static_cast<double>(surfaceTexels.size() * sizeof(GpuSurfaceTexel)) /
                (1024.0 * 1024.0),
            static_cast<double>(surfaceTexels.storageBytes()) / (1024.0 * 1024.0));
    }
    state.active = false;
    state.sceneOpen = false;
    state.output.Reset();
    state.accumulation.Reset();
    state.accumulationHistory.Reset();
    state.surfaceHistory.Reset();
    state.surfaceHistoryPrevious.Reset();
    state.depth.Reset();
    state.motion.Reset();
    state.diffuseAlbedo.Reset();
    state.specularAlbedo.Reset();
    state.normalRoughness.Reset();
    for (DxrFrameResources &frame : state.frames)
    {
        releaseReusableUpload(frame.instanceUpload);
        releaseReusableUpload(frame.lightUpload);
        releaseReusableUpload(frame.instanceMotionUpload);
        releaseReusableUpload(frame.constantsUpload);
        frame.textureStaging.clear();
        frame.streamingStaging.clear();
        frame.feedbackTextures.clear();
        frame.feedbackSerial = 0;
        releaseReusableUpload(frame.shaderTable);
        frame.tlasScratch.Reset();
        frame.tlas.Reset();
        frame.tlasScratchCapacity = 0;
        frame.tlasCapacity = 0;
        frame.tlasTopologyHash = 0;
        frame.tlasContentHash = 0;
        frame.shaderTableSignature = 0;
        frame.shaderTableValid = false;
        frame.tlasBuilt = false;
    }
    state.activeFrameSlot = 0;
    state.surfaceTexelUpload.Reset();
    state.materialTextures.clear();
    state.streamingMaterials.clear();
    state.samplerFeedback = {};
    state.feedbackCaptures = state.feedbackTrimmedBytes = 0;
    state.materialResidency.reset();
    state.requiredMaterialTextures.clear();
    state.memoryBudgetAdapter.Reset();
    state.sensorViewActive = false;
    state.sensorReturnGraceFrame = 0;
    state.materialFrameSerial = state.nextMaterialBudgetQuery = 0;
    state.materialBudgetLimitBytes = ~0ull;
    state.materialAdapterBudgetBytes = 0;
    state.materialUploads = state.materialEvictions = 0;
    state.retiredSurfaceTexelUploads.clear();
    state.retiredSurfaceVariantUploads.clear();
    state.instanceDescriptions.clear();
    state.instanceMotion.clear();
    state.shaderTableBytes.clear();
    state.lightScratch.clear();
    state.rankedDynamicLights.clear();
    state.emissiveCandidates.clear();
    state.geometries.clear();
    state.instances.clear();
    state.previousInstances.clear();
    state.previousInstanceMatched.clear();
    state.previousInstanceBuckets.clear();
    state.previousInstanceIdentities.clear();
    state.currentInstanceIdentities.clear();
    state.currentViewValid = state.previousViewValid = state.worldFromViewValid = false;
    state.materialMipRequests.Reset();
    state.materialMipClearHeap.Reset();
    state.materialMipPipeline.Reset();
    state.materialMipRoot.Reset();
    state.materialMipRequestsActivated = false;
    state.feedbackStateObjectProperties.Reset();
    state.feedbackStateObject.Reset();
    state.stateObjectProperties.Reset();
    state.stateObject.Reset();
    state.localRootSignature.Reset();
    state.globalRootSignature.Reset();
    state.gpuMemory.reset();
    state.device.Reset();
    state.descriptorHeap = nullptr;
    state.descriptorSize = 0;
    state.width = state.height = 0;
    state.outputShaderReadable = false;
    state.guidanceShaderReadable = false;
    state.rrGuideShaderReadable = false;
    state.previousSceneHash = 0;
    state.accumulationFrame = 0;
    state.frameSeed = 0;
    state.historyReset = true;
    state.loggedFirstDispatch = false;
    state.fullTlasBuilds = 0;
    state.tlasUpdates = 0;
    state.tlasReuses = 0;
    state.uploadBufferAllocations = 0;
    state.shaderTableRebuilds = 0;
    state.shaderTableCacheHits = 0;
    /* CPU texture registrations survive a runtime DXR toggle; only the upload
       resource belongs to this device instance. */
    surfaceTexelsDirty = true;
}

bool raytracingCreateOutput(unsigned int width, unsigned int height)
{
    raytracingReleaseOutput();
    if (!state.active)
    {
        return false;
    }
    width = std::max(1u, width);
    height = std::max(1u, height);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    auto createTexture = [&](DXGI_FORMAT format, const wchar_t *name,
                             ComPtr<ID3D12Resource> &resource)
    {
        D3D12_RESOURCE_DESC texture = {};
        texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture.Width = width;
        texture.Height = height;
        texture.DepthOrArraySize = 1;
        texture.MipLevels = 1;
        texture.Format = format;
        texture.SampleDesc.Count = 1;
        texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texture.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        HRESULT result = state.device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &texture,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
            IID_PPV_ARGS(&resource));
        if (FAILED(result))
        {
            logFailure("path-tracing texture allocation", result);
            return false;
        }
        resource->SetName(name);
        return true;
    };
    if (!createTexture(outputFormat, L"Homeworld path-traced lighting",
                       state.output) ||
        !createTexture(accumulationFormat,
                       L"Homeworld path-tracing accumulation",
                       state.accumulation) ||
        !createTexture(accumulationFormat,
                       L"Homeworld path-tracing history",
                       state.accumulationHistory) ||
        !createTexture(depthFormat, L"Homeworld DLAA depth", state.depth) ||
        !createTexture(motionFormat, L"Homeworld DLAA motion", state.motion) ||
        !createTexture(outputFormat, L"Homeworld RR diffuse albedo", state.diffuseAlbedo) ||
        !createTexture(outputFormat, L"Homeworld RR specular albedo", state.specularAlbedo) ||
        !createTexture(outputFormat, L"Homeworld RR normal roughness", state.normalRoughness))
    {
        raytracingReleaseOutput();
        return false;
    }
    if (!createBuffer(static_cast<UINT64>(width) * height * 16,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, state.surfaceHistory) ||
        !createBuffer(static_cast<UINT64>(width) * height * 16,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, state.surfaceHistoryPrevious))
    {
        raytracingReleaseOutput();
        return false;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = outputFormat;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    state.device->CreateShaderResourceView(
        state.output.Get(), &srv, cpuDescriptor(state.outputSrvIndex));
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = outputFormat;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    state.device->CreateUnorderedAccessView(
        state.output.Get(), nullptr, &uav, cpuDescriptor(state.outputUavIndex));
    uav.Format = accumulationFormat;
    refreshAccumulationUavs();
    uav.Format = depthFormat;
    state.device->CreateUnorderedAccessView(
        state.depth.Get(), nullptr, &uav, cpuDescriptor(state.depthUavIndex));
    uav.Format = motionFormat;
    state.device->CreateUnorderedAccessView(
        state.motion.Get(), nullptr, &uav, cpuDescriptor(state.motionUavIndex));
    D3D12_SHADER_RESOURCE_VIEW_DESC motionSrv = {};
    motionSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    motionSrv.Format = motionFormat;
    motionSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    motionSrv.Texture2D.MipLevels = 1;
    state.device->CreateShaderResourceView(
        state.motion.Get(), &motionSrv, cpuDescriptor(state.motionSrvIndex));
    uav.Format = outputFormat;
    state.device->CreateUnorderedAccessView(
        state.diffuseAlbedo.Get(), nullptr, &uav,
        cpuDescriptor(state.rrGuideUavBaseIndex + 0u));
    state.device->CreateUnorderedAccessView(
        state.specularAlbedo.Get(), nullptr, &uav,
        cpuDescriptor(state.rrGuideUavBaseIndex + 1u));
    state.device->CreateUnorderedAccessView(
        state.normalRoughness.Get(), nullptr, &uav,
        cpuDescriptor(state.rrGuideUavBaseIndex + 2u));
    state.width = width;
    state.height = height;
    state.outputShaderReadable = false;
    state.guidanceShaderReadable = false;
    state.rrGuideShaderReadable = false;
    state.accumulationFrame = 0;
    state.previousSceneHash = 0;
    state.historyReset = true;
    return true;
}

void raytracingReleaseOutput(void)
{
    state.surfaceHistory.Reset();
    state.surfaceHistoryPrevious.Reset();
    state.output.Reset();
    state.accumulation.Reset();
    state.accumulationHistory.Reset();
    state.depth.Reset();
    state.motion.Reset();
    state.diffuseAlbedo.Reset();
    state.specularAlbedo.Reset();
    state.normalRoughness.Reset();
    state.width = state.height = 0;
    state.outputShaderReadable = false;
    state.guidanceShaderReadable = false;
    state.rrGuideShaderReadable = false;
    state.accumulationFrame = 0;
    state.previousSceneHash = 0;
    state.historyReset = true;
}

void raytracingBeginFrame(void)
{
    state.previousJitter[0] = state.jitter[0];
    state.previousJitter[1] = state.jitter[1];
    if (state.currentViewValid)
    {
        std::memcpy(state.previousViewMatrix, state.viewMatrix,
                    sizeof(state.previousViewMatrix));
        state.previousViewValid = true;
        state.previousVerticalFieldOfViewDegrees = state.verticalFieldOfViewDegrees;
        state.previousAspectRatio = state.aspectRatio;
    }
    else
    {
        state.previousViewValid = false;
    }
    state.currentViewValid = false;
    state.worldFromViewValid = false;
    state.previousInstances = state.instances;
    state.previousInstanceMatched.assign(state.previousInstances.size(), 0u);
    state.previousInstanceBuckets.clear();
    state.previousInstanceBuckets.reserve(
        std::max<size_t>(16u, state.previousInstances.size()));
    state.previousInstanceIdentities.clear();
    state.previousInstanceIdentities.reserve(state.previousInstances.size());
    state.currentInstanceIdentities.clear();
    state.currentInstanceIdentities.reserve(state.previousInstances.size());
    for (size_t index = 0; index < state.previousInstances.size(); ++index)
    {
        const SceneInstance &previous = state.previousInstances[index];
        if (previous.motionIdentity.owner != 0)
        {
            state.previousInstanceIdentities.insert(previous.motionIdentity, index);
            continue;
        }
        // Unowned scenery may retain its existing matching policy, but it
        // must never consume the history of an explicitly identified ship.
        PreviousInstanceKey key = {
            previous.geometry, previous.surfaceVariant};
        state.previousInstanceBuckets[key].push_back(index);
    }
    state.instances.clear();
    state.sceneOpen = false;
}

void raytracingSetSensorViewActive(bool active)
{
    if (state.sensorViewActive && !active)
        state.sensorReturnGraceFrame = state.materialFrameSerial + 1;
    state.sensorViewActive = active;
}

void raytracingSetFrameSlot(unsigned int frameSlot)
{
    state.activeFrameSlot = frameSlot % dxrFrameResourceCount;
    // The presenter has waited for this exact flight slot's actual GPU fence.
    // The monotonically increasing token only identifies the completed work;
    // it is never a guessed frame delay used as a replacement for that fence.
    auto &completedFrame = state.frames[state.activeFrameSlot];
    for (const auto &texture : completedFrame.feedbackTextures)
        texture->consumeFeedback(state.activeFrameSlot, completedFrame.feedbackSerial);
    completedFrame.feedbackTextures.clear();
    completedFrame.feedbackSerial = 0;
    completedFrame.streamingStaging.clear();
    completedFrame.textureStaging.clear();
    ++state.materialFrameSerial;
    state.gpuMemory.setCurrentFrameIndex(static_cast<unsigned int>(state.materialFrameSerial));
    state.materialResidency.beginFencedSlot(state.activeFrameSlot);
    // Today's material requirements are not known yet. Keep warm resources
    // ready for returning views; menus still release cold data under real
    // DXGI budget pressure. The known-scene path enforces the 512 MiB cache.
    trimMaterialTextures(false, false);
}

void raytracingSetJitter(float jitterX, float jitterY)
{
    state.jitter[0] = std::max(-0.5f, std::min(jitterX, 0.5f));
    state.jitter[1] = std::max(-0.5f, std::min(jitterY, 0.5f));
}

void raytracingSetFxLightingStrength(float strength)
{
    fxLightingStrength = std::max(0.0f, std::min(strength, 2.0f));
}

void raytracingSetPathSettings(unsigned int samplesPerPixel,
                               unsigned int maximumBounces,
                               float generatedNormalStrength)
{
    requestedSamplesPerPixel = std::max(1u, std::min(samplesPerPixel, 4u));
    requestedMaximumBounces = std::max(1u, std::min(maximumBounces, 6u));
    requestedGeneratedNormalStrength = std::max(
        0.0f, std::min(generatedNormalStrength, 4.0f));
    state.previousSceneHash = 0;
}

void raytracingSetRayReconstructionEnabled(bool enabled)
{
    if (requestedRayReconstruction != enabled)
    {
        requestedRayReconstruction = enabled;
        state.previousSceneHash = 0;
        state.accumulationFrame = 0;
        state.historyReset = true;
    }
}

void raytracingSetEnvironmentMode(bool dualParaboloid, float fade)
{
    const bool newDual = dualParaboloid;
    const float newFade = std::max(0.0f, std::min(fade, 1.0f));
    if (state.environmentIsDualParaboloid != newDual ||
        std::fabs(state.environmentFade - newFade) > 0.0001f)
    {
        state.environmentIsDualParaboloid = newDual;
        state.environmentFade = newFade;
        state.previousSceneHash = 0;
        state.accumulationFrame = 0;
        state.historyReset = true;
    }
}

void raytracingSetLightingTuning(float environmentStrength,
                                float authoredLightStrength,
                                float surfaceReflectivity,
                                float surfaceRoughness,
                                float exposure)
{
    requestedEnvironmentStrength = std::max(
        0.0f, std::min(environmentStrength, 2.0f));
    requestedAuthoredLightStrength = std::max(
        0.0f, std::min(authoredLightStrength, 2.0f));
    requestedSurfaceReflectivity = std::max(
        0.0f, std::min(surfaceReflectivity, 2.0f));
    requestedSurfaceRoughness = std::max(
        0.0f, std::min(surfaceRoughness, 1.0f));
    requestedExposure = std::max(0.5f, std::min(exposure, 2.0f));
    state.previousSceneHash = 0;
}

void raytracingSetSkyLightingStrengths(float primarySunStrength,
                                      float skyAmbientStrength)
{
    const float newSun = std::max(0.0f, std::min(primarySunStrength, 2.0f));
    const float newAmbient = std::max(0.0f, std::min(skyAmbientStrength, 2.0f));
    if (std::fabs(newSun - requestedPrimarySunStrength) > 0.0001f ||
        std::fabs(newAmbient - requestedSkyAmbientStrength) > 0.0001f)
    {
        requestedPrimarySunStrength = newSun;
        requestedSkyAmbientStrength = newAmbient;
        state.previousSceneHash = 0;
    }
}

void raytracingSetShadowSettings(float shadowStrength,
                                 float sunAngularRadiusDegrees,
                                 unsigned int sunSamples,
                                 unsigned int localSamples,
                                 float localAngularRadiusDegrees,
                                 float receiverBias, float normalBias,
                                 float contactStrength,
                                 float contactDistance,
                                 float maximumDistance)
{
    requestedShadowStrength = std::max(0.0f, std::min(shadowStrength, 2.0f));
    requestedSunShadowAngularRadiusDegrees = std::max(0.0f, std::min(sunAngularRadiusDegrees, 5.0f));
    requestedSunShadowSamples = std::max(1u, std::min(sunSamples, 16u));
    requestedLocalShadowSamples = std::max(1u, std::min(localSamples, 8u));
    requestedLocalShadowAngularRadiusDegrees = std::max(0.0f, std::min(localAngularRadiusDegrees, 10.0f));
    requestedShadowReceiverBias = std::max(0.001f, std::min(receiverBias, 2.0f));
    requestedShadowNormalBias = std::max(0.0f, std::min(normalBias, 4.0f));
    requestedContactShadowStrength = std::max(0.0f, std::min(contactStrength, 2.0f));
    requestedContactShadowDistance = std::max(0.0f, std::min(contactDistance, 10000.0f));
    requestedShadowMaximumDistance = std::max(1000.0f, std::min(maximumDistance, 1000000.0f));
    state.previousSceneHash = 0;
}

void raytracingSetMissionSkyLighting(int sourceValid,
                                    const float direction[3],
                                    const float sourceColor[3],
                                    const float ambientColor[3])
{
    const bool newSkyAvailable = ambientColor != nullptr;
    const bool newSunValid = sourceValid != 0 && direction != nullptr &&
                             sourceColor != nullptr;
    bool changed = missionSkyAvailable != newSkyAvailable ||
                   missionSunValid != newSunValid;
    float normalizedDirection[3] = {0.0f, 0.0f, -1.0f};

    if (newSunValid)
    {
        const float length = std::sqrt(direction[0] * direction[0] +
                                       direction[1] * direction[1] +
                                       direction[2] * direction[2]);
        if (length > 0.000001f)
        {
            normalizedDirection[0] = direction[0] / length;
            normalizedDirection[1] = direction[1] / length;
            normalizedDirection[2] = direction[2] / length;
        }
        else
        {
            sourceValid = 0;
        }
    }

    const bool validSunAfterNormalize = sourceValid != 0 && newSunValid;
    changed = changed || missionSunValid != validSunAfterNormalize;
    if (validSunAfterNormalize)
    {
        for (unsigned int channel = 0; channel < 3; ++channel)
        {
            const float newDirection = normalizedDirection[channel];
            const float newColor = std::max(0.0f, std::min(sourceColor[channel], 16.0f));
            changed = changed ||
                std::fabs(missionSunWorldDirection[channel] - newDirection) > 0.0001f ||
                std::fabs(missionSunColor[channel] - newColor) > 0.0001f;
            missionSunWorldDirection[channel] = newDirection;
            missionSunColor[channel] = newColor;
        }
    }
    missionSunValid = validSunAfterNormalize;

    if (newSkyAvailable)
    {
        for (unsigned int channel = 0; channel < 3; ++channel)
        {
            const float newAmbient = std::max(0.0f, std::min(ambientColor[channel], 16.0f));
            changed = changed ||
                std::fabs(missionAmbientColor[channel] - newAmbient) > 0.0001f;
            missionAmbientColor[channel] = newAmbient;
        }
    }
    missionSkyAvailable = newSkyAvailable;
    if (changed)
    {
        state.previousSceneHash = 0;
        if (missionSunValid)
        {
            HW_DIAG_FPRINTF(stderr,
                "[DXR] Mission sky key sourceDir=(%.4f, %.4f, %.4f); "
                "directional key uses scene-to-source convention directly.\n",
                missionSunWorldDirection[0], missionSunWorldDirection[1],
                missionSunWorldDirection[2]);
        }
    }
}

void raytracingSetMissionAuthoringReplacesMapLights(bool replace)
{
    if (missionAuthoringReplacesMapLights != replace)
    {
        missionAuthoringReplacesMapLights = replace;
        state.previousSceneHash = 0;
        HW_DIAG_FPRINTF(stderr,
            "[DXR] HSF local map lights %s by live mission authoring copies.\n",
            replace ? "replaced" : "restored");
    }
}

void raytracingSetMissionAuthoringLights(
    const HWModernMissionLightEmitter *emitters, unsigned int count)
{
    const unsigned int maximumMissionLights = 128u;
    const unsigned int safeCount = emitters != nullptr ?
        std::min(count, maximumMissionLights) : 0u;
    std::vector<HWModernMissionLightEmitter> next;
    next.reserve(safeCount);
    for (unsigned int index = 0; index < safeCount; ++index)
    {
        HWModernMissionLightEmitter light = emitters[index];
        if (light.type != HW_MODERN_MISSION_LIGHT_POINT &&
            light.type != HW_MODERN_MISSION_LIGHT_SPOT &&
            light.type != HW_MODERN_MISSION_LIGHT_AMBIENT)
        {
            light.type = HW_MODERN_MISSION_LIGHT_POINT;
        }
        light.enabled = light.enabled ? 1u : 0u;
        for (unsigned int channel = 0; channel < 3; ++channel)
        {
            light.color[channel] = std::max(0.0f, std::min(light.color[channel], 16.0f));
        }
        light.intensity = std::max(0.0f, std::min(light.intensity, 16.0f));
        light.radius = std::max(1.0f, std::min(light.radius, 1000000.0f));
        light.coneAngle = std::max(1.0f, std::min(light.coneAngle, 89.0f));
        light.edgeAngle = std::max(0.0f, std::min(light.edgeAngle, 89.0f));
        const float directionLength = std::sqrt(
            light.direction[0] * light.direction[0] +
            light.direction[1] * light.direction[1] +
            light.direction[2] * light.direction[2]);
        if (directionLength > 0.000001f)
        {
            light.direction[0] /= directionLength;
            light.direction[1] /= directionLength;
            light.direction[2] /= directionLength;
        }
        else
        {
            light.direction[0] = 0.0f;
            light.direction[1] = 0.0f;
            light.direction[2] = -1.0f;
        }
        next.push_back(light);
    }

    bool changed = next.size() != missionAuthoringLights.size();
    if (!changed && !next.empty())
    {
        changed = std::memcmp(next.data(), missionAuthoringLights.data(),
                              next.size() * sizeof(next[0])) != 0;
    }
    missionAuthoringLights.swap(next);
    if (changed)
    {
        state.previousSceneHash = 0;
        HW_DIAG_FPRINTF(stderr,
            "[DXR] Live mission authoring lights updated: %zu active records.\n",
            missionAuthoringLights.size());
    }
}

float surfaceLuminance(unsigned int packed)
{
    const float red = static_cast<float>(packed & 0xffu) / 255.0f;
    const float green = static_cast<float>((packed >> 8u) & 0xffu) / 255.0f;
    const float blue = static_cast<float>((packed >> 16u) & 0xffu) / 255.0f;
    return 0.2126f * red + 0.7152f * green + 0.0722f * blue;
}

unsigned int packGeneratedNormal(float x, float y, float z)
{
    auto channel = [](float value)
    {
        value = std::max(-1.0f, std::min(value, 1.0f));
        return static_cast<unsigned int>((value * 0.5f + 0.5f) * 255.0f +
                                         0.5f);
    };
    return channel(x) | (channel(y) << 8u) | (channel(z) << 16u) |
           0xff000000u;
}

unsigned int generateSurfaceNormal(const unsigned int *surfaceRgba,
                                   unsigned int width, unsigned int height,
                                   unsigned int x, unsigned int y)
{
    auto luminance = [surfaceRgba, width, height](int sampleX, int sampleY)
    {
        const unsigned int wrappedX = static_cast<unsigned int>(
            (sampleX % static_cast<int>(width) + static_cast<int>(width)) %
            static_cast<int>(width));
        const unsigned int wrappedY = static_cast<unsigned int>(
            (sampleY % static_cast<int>(height) + static_cast<int>(height)) %
            static_cast<int>(height));
        return surfaceLuminance(surfaceRgba[
            static_cast<size_t>(wrappedY) * width + wrappedX]);
    };
    /* A normalized Sobel footprint suppresses single-texel color noise while
       retaining the broad panel seams the generated relief is meant to add. */
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    const float gradientX = (
        luminance(ix + 1, iy - 1) + 2.0f * luminance(ix + 1, iy) +
        luminance(ix + 1, iy + 1) - luminance(ix - 1, iy - 1) -
        2.0f * luminance(ix - 1, iy) - luminance(ix - 1, iy + 1)) * 0.25f;
    const float gradientY = (
        luminance(ix - 1, iy + 1) + 2.0f * luminance(ix, iy + 1) +
        luminance(ix + 1, iy + 1) - luminance(ix - 1, iy - 1) -
        2.0f * luminance(ix, iy - 1) - luminance(ix + 1, iy - 1)) * 0.25f;
    /* Keep the Sobel footprint stable, but retain substantially more of its
       broad panel relief.  The shader still confines this normal to primary
       shading, so added amplitude cannot steer visibility or bounce rays. */
    float normalX = -gradientX * 2.15f;
    float normalY = -gradientY * 2.15f;
    float normalZ = 1.0f;
    const float inverseLength = 1.0f / std::sqrt(
        normalX * normalX + normalY * normalY + normalZ * normalZ);
    normalX *= inverseLength;
    normalY *= inverseLength;
    normalZ *= inverseLength;
    return packGeneratedNormal(normalX, normalY, normalZ);
}

static void classifyOpaqueVariant(GeometryRecord::SurfaceVariant &variant)
{
    variant.opaqueForRays = !variant.surfaces.empty();
    for (size_t index = 0; index < variant.surfaces.size(); ++index)
    {
        const auto &surface = variant.surfaces[index];
        const bool resolved = (surface.flags & gpuSurfaceHasTexture) != 0 &&
            surface.textureIndex < materialRecords.size();
        const bool hasMaterial = resolved || (index < variant.materialIdentities.size() &&
            variant.materialIdentities[index] != nullptr);
        const unsigned int minimumAlpha = resolved ? materialRecords[surface.textureIndex].minimumAlpha : 0u;
        if (!hwmodern::canSkipSurfaceAnyHit(surface.opacity, hasMaterial, resolved, minimumAlpha))
        {
            variant.opaqueForRays = false;
            return;
        }
    }
}

static void bindSurfaceTextureToExistingVariants(
    const void *materialIdentity, const SurfaceTextureRecord &record)
{
    if (materialIdentity == nullptr)
    {
        return;
    }

    bool patchedAny = false;
    for (auto &geometryEntry : state.geometries)
    {
        GeometryRecord &geometry = *geometryEntry.second;
        for (auto &variantEntry : geometry.surfaceVariants)
        {
            GeometryRecord::SurfaceVariant &variant = *variantEntry.second;
            const size_t surfaceCount = std::min(
                variant.surfaces.size(), variant.materialIdentities.size());
            bool patchedVariant = false;
            for (size_t index = 0; index < surfaceCount; ++index)
            {
                if (variant.materialIdentities[index] != materialIdentity)
                {
                    continue;
                }
                GpuTriangleSurface &surface = variant.surfaces[index];
                /* Materialentry storage addresses can be reused after a mesh
                   is freed. Never replace texels on a variant that already
                   has a texture merely because a later mesh happens to reuse
                   the same material pointer. Late binding is only allowed to
                   repair a genuinely flat/unresolved surface. */
                if ((surface.flags & gpuSurfaceHasTexture) != 0u)
                {
                    continue;
                }
                surface.textureOffset = record.offset;
                surface.textureIndex = record.textureIndex;
                surface.textureWidth = record.width;
                surface.textureHeight = record.height;
                surface.flags |= gpuSurfaceHasTexture | (record.nativeNormal ? gpuSurfaceNativeNormal : 0u) | (record.rmMaterial ? gpuSurfaceRmMaterial : 0u);
                patchedVariant = true;
            }
            if (!patchedVariant)
            {
                continue;
            }

            /* Surface buffers are immutable uploads. Keep any already-queued
               buffer alive, then force this variant to rebuild from the
               corrected CPU metadata on its next dispatch. */
            if (variant.buffer)
            {
                state.retiredSurfaceVariantUploads.push_back(variant.buffer);
                variant.buffer.Reset();
            }
            variant.emissiveLights.clear();
            variant.emissiveLightsBuilt = false;
            classifyOpaqueVariant(variant);
            patchedAny = true;
        }
    }

    if (patchedAny)
    {
        /* Texture/material changes invalidate accumulated lighting even when
           the object transforms are identical. */
        state.previousSceneHash = 0;
        state.accumulationFrame = 0;
        state.historyReset = true;
    }
}

void raytracingRegisterSurfaceTexture(
    const void *materialIdentity, unsigned int width, unsigned int height,
    const unsigned int *surfaceRgba, const unsigned int *emissiveRgba,
    const unsigned int *normalRgba, const unsigned int *ormRgba,
    const char *textureKey)
{
#if HW_ENABLE_DIAGNOSTICS
    const auto metricStarted = std::chrono::steady_clock::now();
#endif
    if (materialIdentity == nullptr || width == 0 || height == 0 ||
        surfaceRgba == nullptr || emissiveRgba == nullptr)
    {
        return;
    }

    const std::string namedKey = normalizedSurfaceTextureKey(textureKey);
    if (!namedKey.empty())
    {
        const auto named = surfaceTextureNames.find(namedKey);
        if (named != surfaceTextureNames.end())
        {
            surfaceTextures[materialIdentity] = named->second;
            bindSurfaceTextureToExistingVariants(materialIdentity, named->second);
            return;
        }
    }

    const size_t texelCount = static_cast<size_t>(width) * height;
    if (texelCount > 16u * 1024u * 1024u)
    {
        return;
    }

    unsigned long long contentHash = 1469598103934665603ull;
    auto hashWord = [&contentHash](unsigned int value)
    {
        for (unsigned int byte = 0; byte < 4; ++byte)
        {
            contentHash ^= (value >> (byte * 8u)) & 0xffu;
            contentHash *= 1099511628211ull;
        }
    };
    if (namedKey.empty())
    {
        hashWord(width);
        hashWord(height);
        for (size_t index = 0; index < texelCount; ++index)
        {
            hashWord(surfaceRgba[index]);
            hashWord(emissiveRgba[index]);
            hashWord(normalRgba != nullptr ? normalRgba[index] : 0u);
            hashWord(ormRgba != nullptr ? ormRgba[index] : 0u);
        }
    }

    SurfaceTextureRecord record = {};
    bool foundRecord = false;
    auto contentFound = surfaceTextureContents.find(contentHash);
    if (namedKey.empty() && contentFound != surfaceTextureContents.end())
    {
        for (const SurfaceTextureRecord &existing : contentFound->second)
        {
            if (existing.width != width || existing.height != height)
            {
                continue;
            }
            bool equal = true;
            for (size_t index = 0; index < texelCount; ++index)
            {
                const GpuSurfaceTexel &texel =
                    surfaceTexels[static_cast<size_t>(existing.offset) + index];
                if (texel.surfaceRgba != surfaceRgba[index] ||
                    texel.emissiveRgba != emissiveRgba[index] ||
                    (normalRgba != nullptr &&
                     texel.generatedNormalRgba != normalRgba[index]) ||
                    (ormRgba != nullptr && texel.ormRgba != ormRgba[index]))
                {
                    equal = false;
                    break;
                }
            }
            if (equal)
            {
                record = existing;
                foundRecord = true;
                break;
            }
        }
    }

    if (!foundRecord)
    {
        if (surfaceTexels.size() + texelCount > 0xffffffffu)
        {
            return;
        }
        record.offset = static_cast<unsigned int>(surfaceTexels.size());
        if (materialRecords.size() >= materialDescriptorCount) return;
        record.textureIndex = static_cast<unsigned int>(materialRecords.size());
        record.width = width;
        record.height = height;
        record.minimumAlpha = 255u;
        record.rmMaterial = ormRgba != nullptr && namedKey.find("|rm-material") != std::string::npos;
        record.nativeNormal = namedKey.find("/rmnative/") != std::string::npos ||
            namedKey.compare(0,11,"subsystems/")==0 || namedKey.find("/subsystems/")!=std::string::npos;
        // Keep numeric offsets unchanged while storing completed CPU pages
        // losslessly compressed. Only a bounded decoded cache stays expanded;
        // GPU material textures still receive the exact original texel bits.
        for (size_t index = 0; index < texelCount; ++index)
        {
            GpuSurfaceTexel texel = {};
            texel.surfaceRgba = surfaceRgba[index];
            record.minimumAlpha = std::min(record.minimumAlpha, surfaceRgba[index] >> 24u);
            texel.emissiveRgba = emissiveRgba[index];
            record.hasEmission = record.hasEmission || (texel.emissiveRgba & 0x00ffffffu)!=0;
            const unsigned int x = static_cast<unsigned int>(index % width);
            const unsigned int y = static_cast<unsigned int>(index / width);
            texel.generatedNormalRgba = normalRgba != nullptr
                ? normalRgba[index]
                : (record.nativeNormal ? 0xffff8080u : generateSurfaceNormal(surfaceRgba, width, height, x, y));
            /* Alpha zero marks the legacy/global material fallback. */
            texel.ormRgba = ormRgba != nullptr ? ormRgba[index] : 0x0000ffffu;
            surfaceTexels.push_back(texel);
        }
        materialRecords.push_back(record);
        if (namedKey.empty()) surfaceTextureContents[contentHash].push_back(record);
        surfaceTexelsDirty = true;
    }

    /* Material storage is reused by several late mission GEOs. Do not treat a
       pointer that was registered earlier as permanently authoritative: if the
       same address now resolves to different LIF content, update the mapping
       and patch every cached variant that references it. */
    auto identityFound = surfaceTextures.find(materialIdentity);
    const bool replacedIdentity = identityFound != surfaceTextures.end();
    SurfaceTextureRecord previousRecord = {};
    if (replacedIdentity)
    {
        previousRecord = identityFound->second;
    }
    if (identityFound == surfaceTextures.end())
    {
        surfaceTextures.emplace(materialIdentity, record);
    }
    else
    {
        identityFound->second = record;
    }
    if (surfaceDiagEnabled())
    {
        HW_DIAG_FPRINTF(stderr,
            "[SurfaceDiag][DXR] register id=%p size=%ux%u hash=%llu offset=%u content=%s identity=%s oldOffset=%u oldSize=%ux%u\n",
            materialIdentity, width, height, contentHash, record.offset,
            foundRecord ? "reused" : "new",
            replacedIdentity ? "replaced" : "new",
            previousRecord.offset, previousRecord.width, previousRecord.height);
    }
    bindSurfaceTextureToExistingVariants(materialIdentity, record);
    if (!namedKey.empty())
    {
        surfaceTextureNames[namedKey] = record;
    }
#if HW_ENABLE_DIAGNOSTICS
    ++surfaceRegistrationCalls;
    surfaceRegistrationTexels += texelCount;
    surfaceRegistrationMilliseconds +=
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - metricStarted).count();

#endif
}

bool raytracingTryAliasSurfaceTexture(const void *materialIdentity,
                                      const char *textureKey)
{
    if (materialIdentity == nullptr || textureKey == nullptr || textureKey[0] == 0)
        return false;
    const std::string key = normalizedSurfaceTextureKey(textureKey);
    const auto found = surfaceTextureNames.find(key);
    if (found == surfaceTextureNames.end()) return false;
    surfaceTextures[materialIdentity] = found->second;
    bindSurfaceTextureToExistingVariants(materialIdentity, found->second);
    return true;
}

bool raytracingRasterEmissionSource(const void *materialIdentity,RasterEmissionSource &source)
{
    source={};const auto found=surfaceTextures.find(materialIdentity);
    if(found==surfaceTextures.end() || !found->second.hasEmission)return false;
    const auto&r=found->second;source.token=std::uint64_t(r.textureIndex)+1;
    source.width=r.width;source.height=r.height;return true;
}
bool raytracingReadRasterEmission(std::uint64_t token,std::vector<unsigned char> &rgba)
{
    if(!token || token-1>=materialRecords.size())return false;
    const auto&record=materialRecords[size_t(token-1)];
    const size_t count=size_t(record.width)*record.height;
    if(!record.hasEmission || count>16u*1024u*1024u || record.offset+count>surfaceTexels.size())return false;
    rgba.resize(count*4);
    for(size_t copied=0;copied<count;){
        const auto span=surfaceTexels.span(record.offset+copied,count-copied);
        if(!span.size)return false;
        for(size_t i=0;i<span.size;++i)std::memcpy(rgba.data()+(copied+i)*4,&span.data[i].emissiveRgba,4);
        copied+=span.size;
    }
    return true;
}
void raytracingUnregisterSurfaceTexture(const void *materialIdentity)
{
    if (materialIdentity == nullptr)
    {
        return;
    }
    if (surfaceDiagEnabled())
    {
        const auto found = surfaceTextures.find(materialIdentity);
        if (found != surfaceTextures.end())
        {
            HW_DIAG_FPRINTF(stderr,
                "[SurfaceDiag][DXR] unregister id=%p offset=%u size=%ux%u\n",
                materialIdentity, found->second.offset, found->second.width,
                found->second.height);
        }
        else
        {
            HW_DIAG_FPRINTF(stderr,
                "[SurfaceDiag][DXR] unregister id=%p not-registered\n",
                materialIdentity);
        }
    }
    surfaceTextures.erase(materialIdentity);
}

void raytracingBeginScene(float verticalFieldOfViewDegrees, float aspectRatio,
                          const float viewMatrix[16])
{
    if (!state.active || !state.output || viewMatrix == nullptr)
    {
        return;
    }
    state.verticalFieldOfViewDegrees = verticalFieldOfViewDegrees;
    state.aspectRatio = aspectRatio > 0.01f ? aspectRatio : 1.0f;
    std::memcpy(state.viewMatrix, viewMatrix, sizeof(state.viewMatrix));
    state.worldFromViewValid = motionhistory::inverseAffine(viewMatrix, state.worldFromViewMatrix);
    state.currentViewValid = true;
    state.sceneOpen = true;
}

void raytracingEndScene(void)
{
    state.sceneOpen = false;
}

bool raytracingSceneOpen(void)
{
    return state.active && state.sceneOpen;
}

std::string raytracingMotionSnapshot()
{
    std::ostringstream out;
    out << std::setprecision(9);
    const auto matrix = [&](const float *m) {
        out << '['; for(unsigned i=0;i<16;++i){if(i)out<<',';out<<m[i];} out << ']';
    };
    out << "{\"previousViewValid\":" << (state.previousViewValid?"true":"false")
        << ",\"view\":"; matrix(state.viewMatrix);
    out << ",\"previousView\":"; matrix(state.previousViewMatrix);
    const auto instances = [&](const std::vector<SceneInstance> &list) {
        out << '[';
        for(size_t i=0;i<list.size();++i) {
            const auto &v=list[i]; if(i)out<<',';
            const auto current=state.currentInstanceIdentities.find(v.motionIdentity);
            const auto previous=state.previousInstanceIdentities.find(v.motionIdentity);
            out << "{\"index\":"<<i<<",\"owner\":"<<v.motionIdentity.owner
                <<",\"part\":"<<v.motionIdentity.part<<",\"geometry\":"<<reinterpret_cast<std::uintptr_t>(v.geometry)
                <<",\"variant\":"<<reinterpret_cast<std::uintptr_t>(v.surfaceVariant)
                <<",\"mask\":"<<v.instanceMask<<",\"worldValid\":"<<(v.worldTransformValid?"true":"false")
                <<",\"surfaceHistoryValid\":"<<(v.surfaceHistoryValid?"true":"false")
                <<",\"historyIdentity\":"<<v.historyIdentity
                <<",\"currentIndex\":"<<(current==motionhistory::StableIndex::npos?-1:static_cast<long long>(current))
                <<",\"previousIndex\":"<<(previous==motionhistory::StableIndex::npos?-1:static_cast<long long>(previous))
                <<",\"modelView\":";matrix(v.modelView);
            out <<",\"previousModelView\":";matrix(v.previousModelView);
            out <<",\"world\":";matrix(v.worldTransform);out<<'}';
        }
        out << ']';
    };
    out << ",\"current\":";instances(state.instances);
    out << ",\"previous\":";instances(state.previousInstances);out<<'}';
    return out.str();
}

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
    unsigned long long motionOwner, unsigned long long motionPart)
{
    if (!raytracingSceneOpen() || geometryIdentity == nullptr ||
        vertexPositions == nullptr || vertexCount == 0 ||
        vertexStrideBytes < 3 * sizeof(float) || triangleIndices == nullptr ||
        triangleCount == 0 || triangleStrideBytes < 3 * sizeof(unsigned short) ||
        (triangleSurfaces != nullptr &&
         triangleSurfaceStrideBytes < sizeof(HWModernTriangleSurface)) ||
        modelViewMatrix == nullptr)
    {
        return;
    }

    GeometryRecord *geometry = nullptr;
    auto found = state.geometries.find(geometryIdentity);
    if (found == state.geometries.end())
    {
        auto created = std::make_unique<GeometryRecord>();
        created->positions.resize(static_cast<size_t>(vertexCount) * 3);
        float maximumRadiusSquared = 1.0f;
        for (unsigned int index = 0; index < vertexCount; ++index)
        {
            const float *source = reinterpret_cast<const float *>(
                reinterpret_cast<const unsigned char *>(vertexPositions) +
                static_cast<size_t>(index) * vertexStrideBytes);
            created->positions[index * 3 + 0] = source[0];
            created->positions[index * 3 + 1] = source[1];
            created->positions[index * 3 + 2] = source[2];
            maximumRadiusSquared = std::max(maximumRadiusSquared,
                source[0] * source[0] + source[1] * source[1] +
                source[2] * source[2]);
        }
        created->boundingRadius = std::sqrt(maximumRadiusSquared);
        created->indices.resize(static_cast<size_t>(triangleCount) * 3);
        for (unsigned int index = 0; index < triangleCount; ++index)
        {
            const unsigned short *source =
                reinterpret_cast<const unsigned short *>(
                    reinterpret_cast<const unsigned char *>(triangleIndices) +
                    static_cast<size_t>(index) * triangleStrideBytes);
            created->indices[index * 3 + 0] = source[0];
            created->indices[index * 3 + 1] = source[1];
            created->indices[index * 3 + 2] = source[2];
        }
        geometry = created.get();
        state.geometries.emplace(geometryIdentity, std::move(created));
    }
    else
    {
        geometry = found->second.get();
    }

    GeometryRecord::SurfaceVariant *variant = nullptr;
    auto variantFound = geometry->surfaceVariants.find(surfaceVariant);
    if (variantFound == geometry->surfaceVariants.end())
    {
        auto created = std::make_unique<GeometryRecord::SurfaceVariant>();
        created->surfaces.resize(triangleCount);
        created->materialIdentities.resize(triangleCount, nullptr);
        const void *lastDiagMaterial = nullptr;
        bool firstDiagMaterial = true;
        unsigned int diagTexturedTriangles = 0;
        unsigned int diagMissingTriangles = 0;
        for (unsigned int index = 0; index < triangleCount; ++index)
        {
            GpuTriangleSurface &destination = created->surfaces[index];
            destination.baseColor[0] = baseColor != nullptr ? baseColor[0] : 0.55f;
            destination.baseColor[1] = baseColor != nullptr ? baseColor[1] : 0.55f;
            destination.baseColor[2] = baseColor != nullptr ? baseColor[2] : 0.55f;
            destination.opacity = 1.0f;
            if (triangleSurfaces == nullptr)
            {
                continue;
            }
            const HWModernTriangleSurface *source =
                reinterpret_cast<const HWModernTriangleSurface *>(
                    reinterpret_cast<const unsigned char *>(triangleSurfaces) +
                    static_cast<size_t>(index) * triangleSurfaceStrideBytes);
            std::memcpy(destination.baseColor, source->baseColor,
                        sizeof(destination.baseColor));
            std::memcpy(destination.emissiveColor, source->emissiveColor,
                        sizeof(destination.emissiveColor));
            destination.emissiveIntensity =
                std::max(0.0f, source->emissiveIntensity);
            destination.opacity =
                std::max(0.0f, std::min(source->opacity, 1.0f));
            destination.uv0[0] = source->uv[0];
            destination.uv0[1] = source->uv[1];
            destination.uv1[0] = source->uv[2];
            destination.uv1[1] = source->uv[3];
            destination.uv2[0] = source->uv[4];
            destination.uv2[1] = source->uv[5];
            destination.flags = source->flags;
            created->materialIdentities[index] = source->materialIdentity;
            const auto texture = surfaceTextures.find(source->materialIdentity);
            if (texture != surfaceTextures.end())
            {
                destination.textureOffset = texture->second.offset;
                destination.textureIndex = texture->second.textureIndex;
                destination.textureWidth = texture->second.width;
                destination.textureHeight = texture->second.height;
                destination.flags |= gpuSurfaceHasTexture | (texture->second.nativeNormal ? gpuSurfaceNativeNormal : 0u) | (texture->second.rmMaterial ? gpuSurfaceRmMaterial : 0u);
                const auto idle=surfaceTextures.find(source->idleMaterialIdentity);
                if(idle!=surfaceTextures.end() && idle->second.width==destination.textureWidth && idle->second.height==destination.textureHeight) {
                    destination.engineBlendEnabled=1;
                    destination.idleTextureIndex=idle->second.textureIndex;
                    destination.idleTextureOffset=idle->second.offset;
                }
                ++diagTexturedTriangles;
            }
            else
            {
                ++diagMissingTriangles;
            }
            if (surfaceDiagEnabled() &&
                (firstDiagMaterial || source->materialIdentity != lastDiagMaterial))
            {
                HW_DIAG_FPRINTF(stderr,
                    "[SurfaceDiag][DXR] variant geometry=%p variant=%u tri=%u/%u material=%p texture=%s offset=%u size=%ux%u base=%.3f,%.3f,%.3f uv0=%.5f,%.5f uv1=%.5f,%.5f uv2=%.5f,%.5f flags=0x%08x\n",
                    geometryIdentity, surfaceVariant, index, triangleCount,
                    source->materialIdentity,
                    texture != surfaceTextures.end() ? "yes" : "NO",
                    destination.textureOffset, destination.textureWidth,
                    destination.textureHeight, destination.baseColor[0],
                    destination.baseColor[1], destination.baseColor[2],
                    destination.uv0[0], destination.uv0[1],
                    destination.uv1[0], destination.uv1[1],
                    destination.uv2[0], destination.uv2[1],
                    destination.flags);
                lastDiagMaterial = source->materialIdentity;
                firstDiagMaterial = false;
            }
        }
        if (surfaceDiagEnabled())
        {
            HW_DIAG_FPRINTF(stderr,
                "[SurfaceDiag][DXR] variant-summary geometry=%p variant=%u triangles=%u textured=%u missing=%u\n",
                geometryIdentity, surfaceVariant, triangleCount,
                diagTexturedTriangles, diagMissingTriangles);
        }
        variant = created.get();
        classifyOpaqueVariant(*variant);
        geometry->surfaceVariants.emplace(surfaceVariant, std::move(created));
    }
    else
    {
        variant = variantFound->second.get();
    }

    SceneInstance instance;
    instance.motionIdentity = {motionOwner, motionPart};
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    instance.clipEnabled=hwglGetClipPlaneEye(instance.clipEquationEye);
    instance.engineBlend=triangleSurfaces ? triangleSurfaces->engineBlend : 0.0f;
#endif
    instance.historyIdentity = state.nextHistoryIdentity++;
    if (state.nextHistoryIdentity == 0) state.nextHistoryIdentity = 1;
    instance.geometry = geometry;
    instance.instanceMask = hwmodern::solidShadowMask(instanceMask);
    instance.surfaceVariant = variant;
    std::memcpy(instance.modelView, modelViewMatrix,
                sizeof(instance.modelView));
    instance.worldTransformValid = state.worldFromViewValid &&
        motionhistory::multiplyAffine(state.worldFromViewMatrix, modelViewMatrix, instance.worldTransform);
    /* A new/LOD-switched instance still needs camera motion for FG. Keep a
       fresh surface identity and invalid radiance-history depth until a real
       match exists, but project its current world pose in the previous view. */
    std::memset(instance.previousModelView, 0,
                sizeof(instance.previousModelView));
    if (state.previousViewValid && instance.worldTransformValid)
        motionhistory::multiplyAffine(state.previousViewMatrix, instance.worldTransform, instance.previousModelView);
    // Identified ships match only their exact owner lifetime and mesh part.
    // A crossing/newly visible ship cannot steal a same-model neighbor's pose.
    size_t previousIndex = state.previousInstances.size();
    if (instance.motionIdentity.owner != 0)
    {
        const size_t duplicate = state.currentInstanceIdentities.find(instance.motionIdentity);
        const bool unique = state.currentInstanceIdentities.insert(
            instance.motionIdentity, state.instances.size());
        if (!unique && duplicate < state.instances.size())
        {
            // Fail closed for repeated part keys in the same frame. Both
            // submissions use camera-only motion, never arbitrary draw order.
            SceneInstance &first = state.instances[duplicate];
            first.surfaceHistoryValid = false;
            first.historyIdentity = state.nextHistoryIdentity++;
            if (state.nextHistoryIdentity == 0) state.nextHistoryIdentity = 1;
            std::memset(first.previousModelView, 0, sizeof(first.previousModelView));
            if (state.previousViewValid && first.worldTransformValid)
                motionhistory::multiplyAffine(state.previousViewMatrix,
                    first.worldTransform, first.previousModelView);
        }
        if (unique && state.previousViewValid && instance.worldTransformValid)
        {
            const size_t exact = state.previousInstanceIdentities.find(instance.motionIdentity);
            if (exact < state.previousInstances.size() &&
                !state.previousInstanceMatched[exact] &&
                geometry == state.previousInstances[exact].geometry &&
                state.previousInstances[exact].worldTransformValid)
                previousIndex = exact;
        }
    }
    else
    {
        // Preserve existing unowned scenery behavior in a separate bucket.
        // Identified ships never fall through to this proximity fallback.
        const PreviousInstanceKey matchKey = {geometry, variant};
        const auto bucket = state.previousInstanceBuckets.find(matchKey);
        if (state.previousViewValid && instance.worldTransformValid &&
            bucket != state.previousInstanceBuckets.end())
            previousIndex = motionhistory::nearest(instance.worldTransform, state.previousInstances,
                bucket->second, state.previousInstanceMatched);
    }
    if (previousIndex < state.previousInstances.size())
    {
        const SceneInstance &previous = state.previousInstances[previousIndex];
        instance.surfaceHistoryValid = geometry == previous.geometry &&
            variant == previous.surfaceVariant;
        if (instance.surfaceHistoryValid) instance.historyIdentity = previous.historyIdentity;
        state.previousInstanceMatched[previousIndex] = 1u;
        std::memcpy(instance.previousModelView,
                    state.previousInstances[previousIndex].modelView,
                    sizeof(instance.previousModelView));
    }
    if (baseColor != nullptr)
    {
        std::memcpy(instance.baseColor, baseColor,
                    sizeof(instance.baseColor));
    }
    state.instances.push_back(instance);
}

bool raytracingRecord(ID3D12GraphicsCommandList *commands,
                      ID3D12Resource *environmentFrame,
                      const HWModernMapLightEmitter *mapLights,
                      unsigned int mapLightCount,
                      const HWModernDynamicLightEmitter *dynamicLights,
                      unsigned int dynamicLightCount)
{
    if (!state.active || !state.output || commands == nullptr ||
        environmentFrame == nullptr ||
        state.instances.empty())
    {
        return false;
    }
    ComPtr<ID3D12GraphicsCommandList4> rayCommands;
    HRESULT result = commands->QueryInterface(IID_PPV_ARGS(&rayCommands));
    if (FAILED(result))
    {
        logFailure("command-list query", result);
        return false;
    }
    DxrFrameResources &frame = state.frames[state.activeFrameSlot];
    for (SceneInstance &instance : state.instances)
    {
        if (!buildGeometry(rayCommands.Get(), *instance.geometry) ||
            instance.surfaceVariant == nullptr ||
            !buildSurfaceVariant(*instance.surfaceVariant))
        {
            return false;
        }
    }
    if (!ensureMaterialTextures(commands, frame) || !uploadInstanceMotion(frame))
    {
        return false;
    }
    if (!buildTopLevel(rayCommands.Get(), frame))
    {
        return false;
    }

    buildLights(mapLights, mapLightCount, dynamicLights, dynamicLightCount,
                state.lightScratch);
    const std::vector<GpuLight> &lights = state.lightScratch;
    const unsigned long long sceneHash = buildSceneHash();
    const bool resetHistory = sceneHash != state.previousSceneHash;
    const unsigned nextAccumulationFrame = resetHistory ? 0u :
        std::min(state.accumulationFrame + 1u, 63u);
    const unsigned nextFrameSeed = state.frameSeed + 1u;
    if (!updateReusableUpload(
            lights.data(), lights.size() * sizeof(GpuLight),
            L"Homeworld DXR light list", frame.lightUpload))
    {
        return false;
    }
    constexpr float pi = 3.14159265358979323846f;
    FrameConstants constants = {};
    constants.tanHalfFieldOfView = std::tan(
        state.verticalFieldOfViewDegrees * (pi / 360.0f));
    constants.aspectRatio = state.aspectRatio;
    constants.lightCount = static_cast<unsigned int>(lights.size());
    constants.maximumRayDistance = 500000.0f;
    constants.accumulationFrame = nextAccumulationFrame;
    constants.samplesPerPixel = requestedSamplesPerPixel;
    constants.maximumBounces = requestedMaximumBounces;
    constants.normalDetailStrength = requestedGeneratedNormalStrength;
    constants.nearPlane = 0.05f;
    constants.farPlane = constants.maximumRayDistance;
    constants.resetAccumulation = resetHistory ? 1u : 0u;
    constants.frameSeed = nextFrameSeed;
    constants.environmentStrength = requestedEnvironmentStrength;
    constants.surfaceReflectivity = requestedSurfaceReflectivity;
    constants.surfaceRoughness = requestedSurfaceRoughness;
    constants.exposure = requestedExposure;
    constants.shadowStrength = requestedShadowStrength;
    constants.shadowReceiverBias = requestedShadowReceiverBias;
    constants.shadowNormalBias = requestedShadowNormalBias;
    constants.sunShadowAngularRadius = requestedSunShadowAngularRadiusDegrees * (pi / 180.0f);
    constants.sunShadowSamples = requestedSunShadowSamples;
    constants.localShadowSamples = requestedLocalShadowSamples;
    constants.localShadowAngularRadius = requestedLocalShadowAngularRadiusDegrees * (pi / 180.0f);
    constants.shadowMaximumDistance = requestedShadowMaximumDistance;
    constants.contactShadowStrength = requestedContactShadowStrength;
    constants.contactShadowDistance = requestedContactShadowDistance;
    constants.rayReconstructionEnabled = requestedRayReconstruction ? 1u : 0u;
    constants.constantBufferPadding = 0u;

    /* The ray-traced scene is submitted in current view space.  These compact
       rotation rows let the shader recover a camera-invariant world direction
       for mission-sky lookup and reproject sky misses into the previous view. */
    constants.viewToWorldRow0[0] = state.viewMatrix[0];
    constants.viewToWorldRow0[1] = state.viewMatrix[1];
    constants.viewToWorldRow0[2] = state.viewMatrix[2];
    constants.viewToWorldRow0[3] = state.jitter[0];
    constants.viewToWorldRow1[0] = state.viewMatrix[4];
    constants.viewToWorldRow1[1] = state.viewMatrix[5];
    constants.viewToWorldRow1[2] = state.viewMatrix[6];
    constants.viewToWorldRow1[3] = state.jitter[1];
    constants.viewToWorldRow2[0] = state.viewMatrix[8];
    constants.viewToWorldRow2[1] = state.viewMatrix[9];
    constants.viewToWorldRow2[2] = state.viewMatrix[10];
    constants.viewToWorldRow2[3] = state.environmentFade;

    const float *previousView = state.previousViewValid ?
        state.previousViewMatrix : state.viewMatrix;
    constants.previousWorldToViewRow0[0] = previousView[0];
    constants.previousWorldToViewRow0[1] = previousView[4];
    constants.previousWorldToViewRow0[2] = previousView[8];
    constants.previousWorldToViewRow0[3] = state.previousJitter[0];
    constants.previousWorldToViewRow1[0] = previousView[1];
    constants.previousWorldToViewRow1[1] = previousView[5];
    constants.previousWorldToViewRow1[2] = previousView[9];
    constants.previousWorldToViewRow1[3] = state.previousJitter[1];
    constants.previousWorldToViewRow2[0] = previousView[2];
    constants.previousWorldToViewRow2[1] = previousView[6];
    constants.previousWorldToViewRow2[2] = previousView[10];
    constants.previousWorldToViewRow2[3] = 0.0f;
    constants.environmentIsDualParaboloid =
        state.environmentIsDualParaboloid ? 1u : 0u;
    constants.viewHistoryValid = state.previousViewValid ? 1u : 0u;
    constants.previousTanHalfFieldOfView = state.previousViewValid ?
        std::tan(state.previousVerticalFieldOfViewDegrees * (pi / 360.0f)) : constants.tanHalfFieldOfView;
    constants.previousAspectRatio = state.previousViewValid ? state.previousAspectRatio : constants.aspectRatio;

    if (!updateReusableUpload(&constants, sizeof(constants),
        L"Homeworld DXR constants", frame.constantsUpload)) return false;

    bool captureFeedback = state.feedbackStateObject && state.materialFrameSerial % 60u == 0;
    std::vector<std::shared_ptr<SamplerFeedbackTexture>> captureTextures;
    if (captureFeedback)
        for (const auto index : state.requiredMaterialTextures)
        {
            const auto &texture = state.streamingMaterials[index];
            if (texture && texture->prepareFeedback(state.samplerFeedback, state.activeFrameSlot))
            {
                state.materialResidency.resident(index, texture->residentBytes());
                captureTextures.push_back(texture);
            }
        }
    captureFeedback = !captureTextures.empty();
    D3D12_DISPATCH_RAYS_DESC dispatch = {};
    if (!createShaderTable(frame, dispatch, captureFeedback))
    {
        return false;
    }

    // Commit temporal state only after every fallible upload/table allocation.
    // A failed preparation cannot suppress the next scene-history reset.
    state.historyReset = resetHistory;
    state.accumulationFrame = nextAccumulationFrame;
    state.previousSceneHash = sceneHash;
    state.frameSeed = nextFrameSeed;
    if (state.outputShaderReadable)
    {
        transitionOutput(commands, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    if (state.guidanceShaderReadable)
    {
        transitionResource(commands, state.depth.Get(),
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transitionResource(commands, state.motion.Get(),
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    if (state.rrGuideShaderReadable)
    {
        transitionResource(commands, state.diffuseAlbedo.Get(),
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transitionResource(commands, state.specularAlbedo.Get(),
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transitionResource(commands, state.normalRoughness.Get(),
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    rayCommands->SetPipelineState1(captureFeedback ? state.feedbackStateObject.Get() : state.stateObject.Get());
    rayCommands->SetComputeRootSignature(state.globalRootSignature.Get());
    ID3D12DescriptorHeap *heaps[] = { state.descriptorHeap };
    rayCommands->SetDescriptorHeaps(1, heaps);
    rayCommands->SetComputeRootShaderResourceView(
        0, frame.tlas->GetGPUVirtualAddress());
    rayCommands->SetComputeRootDescriptorTable(
        1, gpuDescriptor(state.outputUavIndex));
    rayCommands->SetComputeRootShaderResourceView(
        2, frame.lightUpload.resource->GetGPUVirtualAddress());
    rayCommands->SetComputeRootShaderResourceView(
        3, state.surfaceTexelUpload->GetGPUVirtualAddress());
    rayCommands->SetComputeRootShaderResourceView(
        4, frame.instanceMotionUpload.resource->GetGPUVirtualAddress());
    rayCommands->SetComputeRootDescriptorTable(
        5, gpuDescriptor(state.environmentSrvIndex));
    rayCommands->SetComputeRootConstantBufferView(6,
        frame.constantsUpload.resource->GetGPUVirtualAddress());
    rayCommands->SetComputeRootDescriptorTable(7, gpuDescriptor(materialDescriptorBase));
    rayCommands->SetComputeRootUnorderedAccessView(8, state.surfaceHistory->GetGPUVirtualAddress());
    rayCommands->SetComputeRootUnorderedAccessView(9, state.surfaceHistoryPrevious->GetGPUVirtualAddress());
    if (captureFeedback)
        rayCommands->SetComputeRootUnorderedAccessView(10, state.materialMipRequests->GetGPUVirtualAddress());
    if (!state.environmentIsDualParaboloid)
    {
        transitionResource(commands, environmentFrame,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    if (captureFeedback)
    {
        if (!state.materialMipRequestsActivated)
        {
            activatePooledResource(commands, state.materialMipRequests.Get());
            state.materialMipRequestsActivated = true;
        }
        const UINT clear[4] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
        commands->ClearUnorderedAccessViewUint(gpuDescriptor(materialMipRequestDescriptorIndex),
            state.materialMipClearHeap->GetCPUDescriptorHandleForHeapStart(),
            state.materialMipRequests.Get(), clear, 0, nullptr);
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = state.materialMipRequests.Get();
        commands->ResourceBarrier(1, &barrier);
    }
    if (captureFeedback)
        for (const auto index : state.requiredMaterialTextures)
        {
            const auto &texture = state.streamingMaterials[index];
            if (texture && std::find(captureTextures.begin(), captureTextures.end(), texture) != captureTextures.end())
                texture->beginFeedback(commands, gpuDescriptor(materialFeedbackDescriptorBase + index), state.activeFrameSlot);
        }
    rayCommands->DispatchRays(&dispatch);
    if (captureFeedback)
    {
        // Reduce the exact requested whole-material minimum with hardware
        // sampler feedback in a compute pass. Direct feedback instructions in
        // DXR hit shaders are deliberately absent (driver hang in validation).
        transitionResource(commands, state.materialMipRequests.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        commands->SetPipelineState(state.materialMipPipeline.Get());
        commands->SetComputeRootSignature(state.materialMipRoot.Get());
        commands->SetComputeRootDescriptorTable(0, gpuDescriptor(materialDescriptorBase));
        commands->SetComputeRootDescriptorTable(1, gpuDescriptor(materialFeedbackDescriptorBase));
        commands->SetComputeRootShaderResourceView(2, state.materialMipRequests->GetGPUVirtualAddress());
        commands->Dispatch((materialDescriptorCount + 63u) / 64u, 1, 1);
        transitionResource(commands, state.materialMipRequests.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame.feedbackSerial = state.materialFrameSerial;
        for (const auto &texture : captureTextures)
            if (texture->endFeedback(rayCommands.Get(), state.activeFrameSlot))
            {
                texture->feedbackSubmitted(state.activeFrameSlot, frame.feedbackSerial);
                frame.feedbackTextures.push_back(texture);
            }
        ++state.feedbackCaptures;
    }

    D3D12_RESOURCE_BARRIER uavBarriers[7] = {};
    ID3D12Resource *uavResources[7] = {
        state.output.Get(), state.accumulation.Get(),
        state.depth.Get(), state.motion.Get(),
        state.diffuseAlbedo.Get(), state.specularAlbedo.Get(),
        state.normalRoughness.Get()
    };
    for (unsigned int index = 0; index < 7; ++index)
    {
        uavBarriers[index].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarriers[index].UAV.pResource = uavResources[index];
    }
    commands->ResourceBarrier(7, uavBarriers);
    D3D12_RESOURCE_BARRIER metadataBarriers[2] = {};
    metadataBarriers[0].Type = metadataBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    metadataBarriers[0].UAV.pResource = state.surfaceHistory.Get();
    metadataBarriers[1].UAV.pResource = state.surfaceHistoryPrevious.Get();
    commands->ResourceBarrier(2, metadataBarriers);
    std::swap(state.surfaceHistory, state.surfaceHistoryPrevious);

    /* RTX-AAA: ping-pong accumulation/history instead of copying a full
       R16G16B16A16_FLOAT frame every dispatch. The resource written through
       u1 becomes next frame's immutable u4 history; the old history resource
       becomes the next write target. Both remain UAV state, so this removes
       two transitions plus a full-resolution CopyResource per frame. */
    std::swap(state.accumulation, state.accumulationHistory);
    refreshAccumulationUavs();
    transitionOutput(commands, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transitionResource(commands, state.depth.Get(),
                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(commands, state.motion.Get(),
                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(commands, state.diffuseAlbedo.Get(),
                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(commands, state.specularAlbedo.Get(),
                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(commands, state.normalRoughness.Get(),
                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if (!state.environmentIsDualParaboloid)
    {
        transitionResource(commands, environmentFrame,
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    state.outputShaderReadable = true;
    state.guidanceShaderReadable = true;
    state.rrGuideShaderReadable = true;

    if (!state.loggedFirstDispatch)
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] DXR path tracing active: up to %u "
                     "radiance bounces, motion-reprojected denoising, frame-"
                     "scrambled temporal sampling, mission environment emission, generated "
                     "detail normals, %zu mesh instances, "
                     "%zu BLAS records, %zu lights, "
                     "%zu authored surface texels at %ux%u. Alpha-tested "
                     "shadow rays are live.\n",
                     requestedMaximumBounces, state.instances.size(), state.geometries.size(),
                     lights.size(),
                     surfaceTexels.size() - 1,
                     state.width, state.height);
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] DXR accumulation history uses ping-pong UAVs; "
                     "full-frame history CopyResource disabled.\n");
        state.loggedFirstDispatch = true;
    }
    if ((state.frameSeed % 600u) == 1u)
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] DXR optimization telemetry: %llu TLAS reuses, "
            "%llu TLAS refits, "
            "%llu full TLAS builds, %llu reusable upload allocations, "
            "%llu shader-table rebuilds, %llu shader-table cache hits, "
            "%zu cached emissive candidates.\n",
            state.tlasReuses, state.tlasUpdates, state.fullTlasBuilds,
            state.uploadBufferAllocations, state.shaderTableRebuilds,
            state.shaderTableCacheHits, state.emissiveCandidates.size());
        HW_DIAG_FPRINTF(stderr,
            "[TexturePerf] DXR material residency=%zu/%zu records %.1f MiB "
            "(%zu required this frame); uploads=%llu evictions=%llu.\n",
            state.materialResidency.residentCount(), materialRecords.size(),
            static_cast<double>(state.materialResidency.residentBytes()) / (1024.0 * 1024.0),
            state.requiredMaterialTextures.size(), state.materialUploads, state.materialEvictions);
        const auto prepared = materialUploadCache.stats();
        HW_DIAG_FPRINTF(stderr,
            "[TexturePerf] Prepared mip cache: stores=%llu restores=%llu failures=%llu; "
            "retention target=%.1f MiB material limit=%.1f MiB.\n",
            prepared.stores, prepared.hits, prepared.failures,
            static_cast<double>(materialRetentionTarget(state.materialResidency.requiredBytes(),
                state.materialBudgetLimitBytes, state.materialAdapterBudgetBytes, true)) / (1024.0 * 1024.0),
            static_cast<double>(state.materialBudgetLimitBytes) / (1024.0 * 1024.0));
    }
    return true;
}

bool raytracingActive(void)
{
    return state.active && state.output.Get() != nullptr;
}

RaytracingCacheStats raytracingCacheStats(void)
{
    return {state.materialUploads, state.materialEvictions,
        state.feedbackTrimmedBytes, state.materialResidency.residentBytes()};
}

bool raytracingInitialized(void)
{
    return state.active;
}
ID3D12Resource *raytracingDepthResource(void)
{
    return state.guidanceShaderReadable ? state.depth.Get() : nullptr;
}

ID3D12Resource *raytracingMotionResource(void)
{
    return state.guidanceShaderReadable ? state.motion.Get() : nullptr;
}

ID3D12Resource *raytracingDiffuseAlbedoResource(void)
{
    return state.rrGuideShaderReadable ? state.diffuseAlbedo.Get() : nullptr;
}

ID3D12Resource *raytracingSpecularAlbedoResource(void)
{
    return state.rrGuideShaderReadable ? state.specularAlbedo.Get() : nullptr;
}

ID3D12Resource *raytracingNormalRoughnessResource(void)
{
    return state.rrGuideShaderReadable ? state.normalRoughness.Get() : nullptr;
}

float raytracingFieldOfViewDegrees(void)
{
    return state.verticalFieldOfViewDegrees;
}

float raytracingAspectRatio(void)
{
    return state.aspectRatio;
}
bool raytracingHistoryReset(void)
{
    return state.historyReset;
}

unsigned long long raytracingSunVisibilityScene(float viewRows[12])
{
    auto &frame = state.frames[state.activeFrameSlot];
    if (!state.active || !frame.tlasBuilt || !frame.tlas || !viewRows) return 0;
    for (unsigned int row=0; row<3; ++row)
        for (unsigned int col=0; col<4; ++col)
            viewRows[row*4+col]=state.viewMatrix[col*4+row];
    return frame.tlas->GetGPUVirtualAddress();
}

}

#endif
