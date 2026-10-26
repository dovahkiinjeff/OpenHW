#include "DiagnosticPolicy.h"
#include "FGEffectGuides.h"

#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace hwmodern
{
namespace
{
using Microsoft::WRL::ComPtr;
constexpr unsigned int slotCount = 3;
constexpr unsigned int descriptorsPerSlot = 6;
constexpr D3D12_RESOURCE_STATES motionRead =
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

const char effectGuideShader[] = R"(
Texture2D<float> OpaqueDepth : register(t0);
Texture2D<float2> OpaqueMotion : register(t1);
Texture2D<float> NativeEffectDepth : register(t2);
Texture2D<float2> CloudDistanceOpacity : register(t3);
RWTexture2D<float> EffectDepth : register(u0);
RWTexture2D<float2> EffectMotion : register(u1);
cbuffer GuideConstants : register(b0)
{
    float4 CurrentPosition;
    float4 CurrentRight;
    float4 CurrentUp;
    float4 CurrentForward;
    float4 PreviousPosition;
    float4 PreviousRight;
    float4 PreviousUp;
    float4 PreviousForward;
    uint4 Dimensions; // render width/height, unused
    float4 Projection; // current tanHalf/aspect, previous tanHalf/aspect
    uint4 Flags; // native present, cloud present, camera history valid, unused
};
static const float NearPlane = 0.05;
static const float FarPlane = 500000.0;
float DeviceDepth(float viewZ)
{
    return saturate(FarPlane * (viewZ - NearPlane) /
                    (viewZ * (FarPlane - NearPlane)));
}
[numthreads(8, 8, 1)]
void MergeEffectGuides(uint3 tid : SV_DispatchThreadID)
{
    uint2 pixel = tid.xy;
    if (any(pixel >= Dimensions.xy)) return;
    float baseDepth = OpaqueDepth.Load(int3(pixel, 0));
    float2 baseMotion = OpaqueMotion.Load(int3(pixel, 0));
    // Valid opaque guides pass through unchanged wherever no visible effect
    // is selected. These are separate resources: SR input data is read-only.
    EffectDepth[pixel] = baseDepth;
    EffectMotion[pixel] = baseMotion;

    float2 uv = (float2(pixel) + 0.5) / float2(Dimensions.xy);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float3 ray = CurrentForward.xyz +
        CurrentRight.xyz * (ndc.x * Projection.x * Projection.y) +
        CurrentUp.xyz * (ndc.y * Projection.x);
    float selectedZ = FarPlane + 1.0;
    bool selected = false;
    bool selectedNative = false;
    bool selectedCore = false;
    if (Flags.x != 0)
    {
        uint width, height;
        NativeEffectDepth.GetDimensions(width, height);
        // Integer pixel-center mapping avoids reciprocal rounding selecting
        // the neighboring full-output texel at exact half-resolution edges.
        uint2 samplePixel = min(((pixel * 2 + 1) * uint2(width, height)) /
            (Dimensions.xy * 2), uint2(width - 1, height - 1));
        samplePixel.y = height - 1 - samplePixel.y; // native target is bottom-up
        float encoded = NativeEffectDepth.Load(int3(samplePixel, 0));
        bool core = isfinite(encoded) && encoded <= -2.0;
        float z = core ? -1000000.0 / encoded : encoded;
        if ((core || !(baseDepth < 1.0)) && isfinite(z) &&
            z >= NearPlane && z < FarPlane && DeviceDepth(z) <= baseDepth)
        {
            selectedZ = z;
            selected = selectedNative = true;
            selectedCore = core;
        }
    }
    // Cloud anchors describe camera motion only. Even a dense foreground
    // cloud must not replace a moving hull or a selected native effect guide.
    if (Flags.y != 0 && !selectedNative && !(baseDepth < 1.0))
    {
        uint width, height;
        CloudDistanceOpacity.GetDimensions(width, height);
        uint2 samplePixel = min(((pixel * 2 + 1) * uint2(width, height)) /
            (Dimensions.xy * 2), uint2(width - 1, height - 1));
        float2 cloud = CloudDistanceOpacity.Load(int3(samplePixel, 0));
        // Against sky, real 8% coverage supplies its finite anchor.
        float z = cloud.x * dot(normalize(ray), CurrentForward.xyz);
        if (all(isfinite(cloud)) && cloud.y >= 0.08 && cloud.y <= 1.0 &&
            z >= NearPlane && z < selectedZ && DeviceDepth(z) <= baseDepth)
        {
            selectedZ = z;
            selected = true;
        }
    }
    if (!selected) return;

    float2 motion = 0.0;
    if (Flags.z != 0)
    {
        float3 world = CurrentPosition.xyz + ray * selectedZ;
        // Finite anchors MUST include camera translation. Treating them as
        // sky directions introduces large false flow while orbiting a target.
        float3 previousRelative = world - PreviousPosition.xyz;
        float previousZ = dot(previousRelative, PreviousForward.xyz);
        if (previousZ >= NearPlane)
        {
            float2 previousNdc = float2(
                dot(previousRelative, PreviousRight.xyz) / (previousZ * Projection.z * Projection.w),
                dot(previousRelative, PreviousUp.xyz) / (previousZ * Projection.z));
            float2 previousUv = float2(previousNdc.x * 0.5 + 0.5, 0.5 - previousNdc.y * 0.5);
            motion = (previousUv - uv) * float2(Dimensions.xy);
            if (!all(isfinite(motion))) motion = 0.0;
        }
    }
    // Native ribbon/glow motion needs a finite depth anchor: with sky depth,
    // AMD's interpolation rejects this otherwise correct finite motion and
    // duplicates/breaks thin trails. Clouds remain non-solid sky guides.
    // Protect the entire opaque 3x3 dilation neighborhood, not just this
    // pixel, so a nearby ribbon cannot displace a moving hull's own vectors.
    if (selectedNative)
    {
        bool opaqueNeighbour = false;
        [unroll] for (int y = -1; y <= 1; ++y)
            [unroll] for (int x = -1; x <= 1; ++x)
            {
                int2 neighbour = clamp(int2(pixel) + int2(x, y),
                    int2(0, 0), int2(Dimensions.xy) - 1);
                opaqueNeighbour = opaqueNeighbour ||
                    (OpaqueDepth.Load(int3(neighbour, 0)) < 1.0);
            }
        // A covering core owns this pixel, including over hulls/asteroids.
        // Other transparent effects must still protect the hull neighborhood.
        if (selectedCore || !opaqueNeighbour) EffectDepth[pixel] = DeviceDepth(selectedZ);
    }
    EffectMotion[pixel] = motion;
}
)";

struct Constants
{
    float current[4][4] = {}, previous[4][4] = {};
    unsigned int dimensions[4] = {};
    float projection[4] = {};
    unsigned int flags[4] = {};
};
static_assert(sizeof(Constants) == 44 * sizeof(unsigned int), "HLSL constant layout");

struct Slot
{
    ComPtr<ID3D12Resource> depth, motion;
    unsigned int width = 0, height = 0;
    bool readable = false;
};
struct State
{
    ComPtr<ID3D12Device> device;
    ComPtr<IUnknown> identity;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12DescriptorHeap> heap;
    Slot slots[slotCount];
    FGEffectGuideCamera previous;
    unsigned int descriptorSize = 0, currentSlot = slotCount;
    unsigned int previousWidth = 0, previousHeight = 0;
    bool history = false, failed = false;
} state;

bool failed(const char *operation, HRESULT result)
{
    HW_DIAG_FPRINTF(stderr, "[FrameGeneration] Effect guide %s failed (0x%08lX); interpolation suspended.\n",
        operation, static_cast<unsigned long>(result));
    state.failed = true;
    state.history = false;
    state.currentSlot = slotCount;
    return false;
}

bool validCamera(const FGEffectGuideCamera &camera)
{
    for (unsigned int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(camera.position[axis]) || !std::isfinite(camera.right[axis]) ||
            !std::isfinite(camera.up[axis]) || !std::isfinite(camera.forward[axis])) return false;
    const auto dot = [](const float *a, const float *b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };
    return std::isfinite(camera.verticalFovRadians) && camera.verticalFovRadians > 0.01f &&
        camera.verticalFovRadians < 3.13f && std::isfinite(camera.aspect) && camera.aspect > 0.0001f &&
        std::abs(dot(camera.right, camera.right) - 1.0f) < 0.02f &&
        std::abs(dot(camera.up, camera.up) - 1.0f) < 0.02f &&
        std::abs(dot(camera.forward, camera.forward) - 1.0f) < 0.02f &&
        std::abs(dot(camera.right, camera.up)) < 0.02f &&
        std::abs(dot(camera.right, camera.forward)) < 0.02f &&
        std::abs(dot(camera.up, camera.forward)) < 0.02f;
}

bool initialize(ID3D12Device *device)
{
    ComPtr<IUnknown> identity;
    if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(&identity)))) return false;
    if (state.device) return identity.Get() == state.identity.Get() && !state.failed;
    state.device = device; state.identity = identity;
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, 0};
    ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[3] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &ranges[0]};
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1, &ranges[1]};
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[2].Constants = {0, 0, sizeof(Constants) / sizeof(unsigned int)};
    D3D12_ROOT_SIGNATURE_DESC description = {};
    description.NumParameters = 3; description.pParameters = parameters;
    ComPtr<ID3DBlob> rootBlob, errors, shader;
    HRESULT result = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1,
        &rootBlob, &errors);
    if (FAILED(result)) return failed("root serialization", result);
    result = device->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
        IID_PPV_ARGS(&state.root));
    if (FAILED(result)) return failed("root creation", result);
    result = D3DCompile(effectGuideShader, std::strlen(effectGuideShader), "FGEffectGuides", nullptr,
        nullptr, "MergeEffectGuides", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shader, &errors);
    if (FAILED(result))
    {
        if (errors) HW_DIAG_FPRINTF(stderr, "[FrameGeneration] %s\n", static_cast<const char *>(errors->GetBufferPointer()));
        return failed("shader compilation", result);
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = state.root.Get();
    pipeline.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
    result = device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&state.pipeline));
    if (FAILED(result)) return failed("pipeline creation", result);
    D3D12_DESCRIPTOR_HEAP_DESC heap = {};
    heap.NumDescriptors = slotCount * descriptorsPerSlot;
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&state.heap));
    if (FAILED(result)) return failed("descriptor allocation", result);
    state.descriptorSize = device->GetDescriptorHandleIncrementSize(heap.Type);
    return true;
}

bool texture(ID3D12Resource *resource, DXGI_FORMAT format, unsigned int &width, unsigned int &height)
{
    if (!resource) { width = height = 0; return false; }
    const auto description = resource->GetDesc();
    if (description.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || description.Format != format ||
        description.Width == 0 || description.Width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        description.Height == 0 || description.DepthOrArraySize != 1 || description.SampleDesc.Count != 1)
        return false;
    width = static_cast<unsigned int>(description.Width); height = description.Height;
    return true;
}

bool allocate(Slot &slot, unsigned int width, unsigned int height)
{
    if (slot.depth && slot.width == width && slot.height == height) return true;
    D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = width; description.Height = height;
    description.DepthOrArraySize = 1; description.MipLevels = 1;
    description.SampleDesc.Count = 1; description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    description.Format = DXGI_FORMAT_R32_FLOAT;
    ComPtr<ID3D12Resource> depth, motion;
    HRESULT result = state.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&depth));
    if (FAILED(result)) return failed("depth allocation", result);
    description.Format = DXGI_FORMAT_R16G16_FLOAT;
    result = state.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&motion));
    if (FAILED(result)) return failed("motion allocation", result);
    depth->SetName(L"AMD FG effect depth"); motion->SetName(L"AMD FG effect pixel motion");
    slot.depth = depth; slot.motion = motion; slot.width = width; slot.height = height; slot.readable = false;
    return true;
}
}

bool fgEffectGuidesPrepare(ID3D12GraphicsCommandList *commands, ID3D12Device *device,
    unsigned int flightSlot, ID3D12Resource *opaqueDepth, ID3D12Resource *opaquePixelMotion,
    ID3D12Resource *nativeFxLinearDepth, ID3D12Resource *cloudDistanceOpacity,
    const FGEffectGuideCamera &camera, bool reset)
{
    state.currentSlot = slotCount;
    unsigned int width = 0, height = 0, motionWidth = 0, motionHeight = 0;
    unsigned int nativeWidth = 0, nativeHeight = 0, cloudWidth = 0, cloudHeight = 0;
    if (!commands || flightSlot >= slotCount || !validCamera(camera) ||
        !texture(opaqueDepth, DXGI_FORMAT_R32_FLOAT, width, height) ||
        !texture(opaquePixelMotion, DXGI_FORMAT_R16G16_FLOAT, motionWidth, motionHeight) ||
        width != motionWidth || height != motionHeight ||
        (nativeFxLinearDepth && !texture(nativeFxLinearDepth, DXGI_FORMAT_R32_FLOAT, nativeWidth, nativeHeight)) ||
        (cloudDistanceOpacity && !texture(cloudDistanceOpacity, DXGI_FORMAT_R32G32_FLOAT, cloudWidth, cloudHeight)))
    { state.history = false; return false; }
    if (!initialize(device) || !allocate(state.slots[flightSlot], width, height))
    { state.history = false; return false; }

    auto cpu = state.heap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(flightSlot) * descriptorsPerSlot * state.descriptorSize;
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT};
    ID3D12Resource *inputs[] = {opaqueDepth, opaquePixelMotion, nativeFxLinearDepth, cloudDistanceOpacity};
    for (unsigned int index = 0; index < 4; ++index)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = formats[index]; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(inputs[index], &srv, cpu);
        cpu.ptr += state.descriptorSize;
    }
    Slot &slot = state.slots[flightSlot];
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; uav.Format = DXGI_FORMAT_R32_FLOAT;
    device->CreateUnorderedAccessView(slot.depth.Get(), nullptr, &uav, cpu);
    cpu.ptr += state.descriptorSize;
    uav.Format = DXGI_FORMAT_R16G16_FLOAT;
    device->CreateUnorderedAccessView(slot.motion.Get(), nullptr, &uav, cpu);

    const bool history = state.history && !reset && camera.frameId != 0 &&
        camera.frameId == state.previous.frameId + 1 && width == state.previousWidth && height == state.previousHeight;
    Constants constants;
    const auto copyCamera = [](float (&out)[4][4], const FGEffectGuideCamera &source) {
        std::copy_n(source.position, 3, out[0]); std::copy_n(source.right, 3, out[1]);
        std::copy_n(source.up, 3, out[2]); std::copy_n(source.forward, 3, out[3]);
    };
    copyCamera(constants.current, camera); copyCamera(constants.previous, history ? state.previous : camera);
    constants.dimensions[0] = width; constants.dimensions[1] = height;
    constants.projection[0] = std::tan(camera.verticalFovRadians * 0.5f);
    constants.projection[1] = camera.aspect;
    constants.projection[2] = history ? std::tan(state.previous.verticalFovRadians * 0.5f) : constants.projection[0];
    constants.projection[3] = history ? state.previous.aspect : camera.aspect;
    constants.flags[0] = nativeFxLinearDepth != nullptr;
    constants.flags[1] = cloudDistanceOpacity != nullptr;
    constants.flags[2] = history;

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition = {slot.depth.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        slot.readable ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
    barriers[1].Transition = {slot.motion.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        slot.readable ? motionRead : D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
    if (slot.readable) commands->ResourceBarrier(2, barriers);
    ID3D12DescriptorHeap *heap = state.heap.Get(); commands->SetDescriptorHeaps(1, &heap);
    commands->SetComputeRootSignature(state.root.Get()); commands->SetPipelineState(state.pipeline.Get());
    auto gpu = state.heap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += static_cast<UINT64>(flightSlot) * descriptorsPerSlot * state.descriptorSize;
    commands->SetComputeRootDescriptorTable(0, gpu);
    gpu.ptr += 4ull * state.descriptorSize;
    commands->SetComputeRootDescriptorTable(1, gpu);
    commands->SetComputeRoot32BitConstants(2, sizeof(Constants) / sizeof(unsigned int), &constants, 0);
    commands->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.StateAfter = motionRead;
    commands->ResourceBarrier(2, barriers);
    slot.readable = true;
    state.previous = camera; state.previousWidth = width; state.previousHeight = height;
    state.history = true; state.currentSlot = flightSlot;
    return true;
}

ID3D12Resource *fgEffectGuidesDepth(void)
{ return state.currentSlot < slotCount ? state.slots[state.currentSlot].depth.Get() : nullptr; }
ID3D12Resource *fgEffectGuidesMotion(void)
{ return state.currentSlot < slotCount ? state.slots[state.currentSlot].motion.Get() : nullptr; }
void fgEffectGuidesReleaseOutputs(void)
{
    for (auto &slot : state.slots) slot = {};
    state.history = false; state.currentSlot = slotCount;
    if (state.pipeline && state.heap) state.failed = false;
}
void fgEffectGuidesReleaseDevice(void) { state = {}; }
}
