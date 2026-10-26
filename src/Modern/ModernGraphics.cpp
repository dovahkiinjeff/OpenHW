#include "DiagnosticPolicy.h"
#include "ModernGraphics.h"
#include "CloudRenderSettings.h"
#include "ModernDlaa.h"
#include "ModernUpscaler.h"
#include "ModernFrameGeneration.h"
#if defined(HW_ENABLE_D3D12_BACKEND)
#include "FGEffectGuides.h"
#include "FGZoomCapture.h"
#endif
#include "ModernRaytracing.h"
#include "TextureResolution.h"
#include "BenchmarkStats.h"
#include "FrameWorker.h"
#include "PresentedFrameRate.h"
#include "WindowsDiagnostics.h"
#include "WindowsSplash.h"
#include "WindowsSplash.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

static HWModernCloudRenderSettings cloudRenderSettings = hwModernCloudRenderSettingsDefaults();
static float cloudSimulationTime = 0.0f;

extern "C" void hwModernGraphicsGetCloudRenderSettings(HWModernCloudRenderSettings *settings)
{
    if (settings) *settings = cloudRenderSettings;
}
extern "C" void hwModernGraphicsSetCloudRenderSettings(const HWModernCloudRenderSettings *settings)
{
    cloudRenderSettings = hwModernCloudRenderSettingsSanitize(settings);
}
extern "C" void hwModernGraphicsResetCloudRenderSettings(void)
{
    cloudRenderSettings = hwModernCloudRenderSettingsDefaults();
    cloudSimulationTime = 0.0f;
}
extern "C" void hwModernGraphicsSetCloudSimulationTime(float seconds)
{
    if (std::isfinite(seconds) && seconds >= 0.0f) cloudSimulationTime = seconds;
}

static std::chrono::steady_clock::time_point performanceClockNow()
{
    return hwDiagnosticsDisabled() ? std::chrono::steady_clock::time_point{}
                                   : std::chrono::steady_clock::now();
}

static hwmodern::PresentedFrameRate presentedFrameRate;
static void recordPresentedFrame(unsigned int count = 1)
{
    presentedFrameRate.presented(std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count(), count);
#if defined(_WIN32)
    if (hwWindowsSplashIsActive() && hwWindowsSplashTaskIsRunning("main.firstframe")) {
        if (hwModernGraphicsIsD3D11()) {
            // A successful fallback frame resolves the unused D3D12 path,
            // including any failed D3D12 task that led to the fallback.
#define HW_STARTUP_TASK(id,label) if (std::strncmp(id,"renderer.",9)==0) hwWindowsSplashTaskSkip(id,"Using the D3D11 renderer");
#include "WindowsStartupTasks.inc"
#undef HW_STARTUP_TASK
        } else {
            hwWindowsSplashSkipPending("renderer.","Optional rendering paths are not needed for this startup");
        }
        hwWindowsSplashTaskComplete("main.firstframe","First main menu frame presented / ready");
        hwWindowsSplashFinish();
    }
#endif
}
extern "C" float hwModernGraphicsGetFramesPerSecond(void)
{
    return presentedFrameRate.value();
}

#if defined(_WIN32)

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <DirectXTex.h>
#ifndef HW_ENABLE_D3D12_NATIVE_RASTER
#include <GL/gl.h>
#else
#include <SDL2/SDL_opengl.h>
#include "LegacyD3D12GL.h"
#include "LegacyD3D12Renderer.h"
#include "LegacyD3D11Renderer.h"
#endif
#include <wrl/client.h>
#if defined(HW_ENABLE_D3D12_BACKEND)
#include "DustSunVisibilityShader.h"
#include "DustShadowPolicy.h"
#include "DustTileBounds.h"
#include "OpenMWCloudNoise.h"
#endif

#ifndef GL_PACK_ROW_LENGTH
#define GL_PACK_ROW_LENGTH 0x0D02
#endif

using Microsoft::WRL::ComPtr;

namespace
{
void logTextureLoadMetrics();
constexpr UINT frameBufferCount = 3;
constexpr UINT gpuTimestampCount = 10;
constexpr UINT maximumVolumetricDustVolumes = 256;
constexpr UINT particleGridValuesPerBrick = 4913;
constexpr UINT maximumParticleGridValues = maximumVolumetricDustVolumes * particleGridValuesPerBrick;
constexpr UINT maximumDustVolumeRecords = maximumVolumetricDustVolumes + (maximumParticleGridValues + 43) / 44;
constexpr UINT maximumVolumetricDustLights = 12;
constexpr UINT maximumVolumetricDustWakes = 12;
constexpr UINT sceneDepthDescriptorIndex = 13;
constexpr UINT dustDescriptorBase = 14;
constexpr UINT dustDescriptorsPerFrame = 10;
constexpr UINT dustTileColumns = 16, dustTileRows = 9;
constexpr UINT dustTileWords = 8;
constexpr UINT dustFroxelNearUavDescriptorIndex =
    dustDescriptorBase + frameBufferCount * dustDescriptorsPerFrame;
constexpr UINT dustFroxelFarUavDescriptorIndex =
    dustFroxelNearUavDescriptorIndex + 1u;
constexpr UINT dustFroxelDescriptorCount = dustFroxelFarUavDescriptorIndex + 1u;
constexpr UINT rrGuideUavDescriptorBase = dustFroxelDescriptorCount;
constexpr UINT rrGuideUavDescriptorCount = 3u;
constexpr UINT rayEnvironmentDescriptorIndex =
    rrGuideUavDescriptorBase + rrGuideUavDescriptorCount;
// Separate stable SRV and sampler-feedback UAV ranges for authored materials.
constexpr UINT presenterDescriptorCount = 128u + 2u * 32768u + 1u;
/* Shared dust cache storage is world-aligned, not camera/frustum aligned.
   The physical texture is deliberately larger than the active grid so the
   active dimensions can preserve roughly cubic world-space voxels without
   reallocating GPU resources as authored clouds change. */
constexpr UINT dustWorldGridTextureDimension = 160u;
constexpr UINT dustWorldGridActiveDimension = 144u;
/* Present the final image as ordinary SDR sRGB.  The previous FP16 scRGB
   swapchain was correct for HDR-capable displays, but OBS Game Capture reads
   the swapchain before the Windows compositor tone-maps it; on an SDR setup
   that made the recording substantially darker than the visible game. */
constexpr DXGI_FORMAT presenterFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
constexpr DXGI_FORMAT sceneFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
#else
constexpr DXGI_FORMAT sceneFormat = presenterFormat;
#endif

float halton(unsigned int index, unsigned int base)
{
    float result = 0.0f;
    float fraction = 1.0f;
    while (index != 0u)
    {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

void temporalJitter(unsigned long long frameIndex, float &x, float &y)
{
    /* A centered low-discrepancy sequence supplies the sub-pixel coverage
       DLSS/DLAA expects. Thirty-two phases also avoid a very short repeat in
       the aggressive Performance and Ultra Performance modes. */
    const unsigned int phase =
        static_cast<unsigned int>(frameIndex % 32u) + 1u;
    x = halton(phase, 2u) - 0.5f;
    y = halton(phase, 3u) - 0.5f;
}

const char presenterShader[] = R"(
Texture2D<float4> SourceFrame : register(t0);
Texture2D<float4> RayLighting : register(t1);
Texture2D<float4> WorldFrame : register(t2);
Texture2D<float4> FilteredWorld : register(t3);
Texture2D<float2> MotionVectors : register(t4);
Texture2D<float4> BackgroundFrame : register(t5);
Texture2D<float2> OpenMWCloudNoise : register(t6);
Texture2D<float4> OccluderFrame : register(t7);
Texture2D<float> SceneDepth : register(t8);

struct DustVolume
{
    uint enabled;
    uint shape;
    uint shapeSeed;
    uint flags;
    float3 position;
    float density;
    float3 size;
    float scattering;
    float3 color;
    float absorption;
    float anisotropy;
    float coverage;
    float noiseScale;
    float noiseDetail;
    float shapeVariation;
    float3 rotationDegrees;
    /* RTX sparse/froxel path: CPU-precomputed invariants. Keeping these in the
       structured volume record avoids millions of redundant sin/cos/hash
       evaluations while building the shared 3D field. */
    float2 rotationXSinCos;
    float2 rotationYSinCos;
    float2 rotationZSinCos;
    float2 precomputedPadding;
    float3 seedOffset;
    float seedPadding;
    float4 openmwControl; // quality, detail, enabled motion speed, billow
    float4 openmwTime; // simulation seconds, reserved
};

struct DustLight
{
    float3 position;
    uint type;
    float3 direction;
    float radius;
    float3 color;
    float intensity;
    float coneCos;
    float edgeCos;
    float2 padding;
};

struct DustWake
{
    float3 position;
    float radius;
    float3 direction;
    float length;
    float strength;
    float bowStrength;
    float recovery;
    float padding;
};

StructuredBuffer<DustVolume> DustVolumes : register(t9);
StructuredBuffer<DustLight> DustLights : register(t10);
StructuredBuffer<DustWake> DustWakes : register(t11);
Texture3D<float4> DustFroxelField : register(t12);
Texture2D<float4> PrimaryAlbedoGuide : register(t13);
Texture3D<float4> DustFarFroxelField : register(t14);
Texture2D<float4> DustScreenField : register(t15);
Texture2D<float4> DustScreenHistory : register(t16);
StructuredBuffer<uint> DustTileMasks : register(t17);
#ifndef DUST_RAY_QUERY
Texture2D<float> NativeEffectDepthGuide : register(t18);
#endif
#ifdef DUST_FROXEL_COMPUTE
RWTexture3D<float4> DustFroxelFieldRW : register(u0);
#ifdef DUST_RAY_QUERY
RaytracingAccelerationStructure DustShadowScene : register(t18);
cbuffer DustShadowTransform : register(b1)
{
    float4 DustShadowViewRow0;
    float4 DustShadowViewRow1;
    float4 DustShadowViewRow2;
    float4 DustShadowControl; // enabled, ray range, world-space bias, reserved
};
groupshared float DustSunVisibility[8];
#endif
#endif
SamplerState SourceSampler : register(s0);
SamplerState OpenMWNoiseSampler : register(s1);

cbuffer PresenterConstants : register(b0)
{
    uint RayOverlayEnabled;
    uint AntiAliasingMode;
    uint SceneCompositeEnabled;
    uint FrameIndex;
    float ChromaticAberration;
    float MotionBlur;
    float FilmGrain;
    float GodRays;
    float2 GodRayCenter;
    uint ScenePrepass;
    uint StreamlineResolvedScene;
    float OutputDither;
    float3 GodRaySourceColor;
    uint GodRaySourceExternal;
    float Bloom;
    float2 PresenterPadding;
    uint DustVolumeCount;
    uint DustLightCount;
    uint DustRenderEnabled;
    uint DustWakeCount;
    float4 DustCameraPositionMaxDistance;
    float4 DustRayUv00;
    float4 DustRayUv10;
    float4 DustRayUv01SunStrength;
    float4 DustSunDirectionValid;
    float4 DustSunColor;
    float4 DustAmbientColorStrength;
    /* World-space shared-volume cache.  .w stores active X/Y dimensions;
       PresenterPadding.x stores active Z.  Keeping these at the end preserves
       the existing constants while staying within the D3D12 root DWORD budget. */
    float4 DustGridMinWidth;
    float4 DustGridSizeHeight;
    uint DisplayControls;
    /* Depth reconstruction reuses otherwise-unused .w components so this
       cbuffer remains at the proven 60-DWORD size.  D3D12 root signatures
       have a 64-DWORD total budget and this shader already uses three
       descriptor tables in addition to the root constants.
       DustRayUv00.w = projection P[10]
       DustRayUv10.w = projection P[14]
       DustSunColor.w = exact native-raster depth available this frame. */
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VertexOutput PresentVS(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float2 corner = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0),
                             0.0, 1.0);
    output.texcoord = float2(corner.x, 1.0 - corner.y);
    return output;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float Noise(float2 position)
{
    float value = dot(position + float2(FrameIndex * 17u, FrameIndex * 31u),
                      float2(12.9898, 78.233));
    return frac(sin(value) * 43758.5453);
}

float3 LinearToSrgb(float3 linearRgb)
{
    linearRgb = max(linearRgb, 0.0);
    float3 low = linearRgb * 12.92;
    float3 high = 1.055 * pow(linearRgb, 1.0 / 2.4) - 0.055;
    return lerp(high, low, step(linearRgb, 0.0031308));
}

float3 DisplayToLinear(float3 displayRgb)
{
    displayRgb = max(displayRgb, 0.0);
    float3 low = displayRgb / 12.92;
    float3 high = pow((displayRgb + 0.055) / 1.055, 2.4);
    return lerp(high, low, step(displayRgb, 0.04045));
}

float3 ToneMapSky(float3 radiance)
{
    /* Leave ordinary authored gradients untouched in scene-linear space, then
       roll only high radiance smoothly into the SDR display range. */
    const float shoulderStart = 0.62;
    float3 excess = max(radiance - shoulderStart, 0.0);
    float3 shoulder = shoulderStart + (1.0 - shoulderStart) *
        (1.0 - exp(-excess / (1.0 - shoulderStart)));
    return lerp(radiance, shoulder, step(shoulderStart, radiance));
}

float3 SkyToDisplay(float3 radiance)
{
    return saturate(LinearToSrgb(ToneMapSky(max(radiance, 0.0))));
}

float3 ApplySkyCoverage(float2 uv, float3 raw, float coverage)
{
    uv = saturate(uv);
    float3 backgroundLinear = BackgroundFrame.Sample(SourceSampler, uv).rgb;
    float3 backgroundDisplay = SkyToDisplay(backgroundLinear);
    /* Legacy RGB is already display-referred, while the mission environment
       below it is scene-linear HDR. Alpha in the native world target is now
       explicit foreground coverage. Replace only the uncovered fraction of
       the linear background with its display-space equivalent. This is exact
       for both standard SRC_ALPHA compositing and additive FX (whose alpha
       channel deliberately preserves the destination coverage). */
    return raw + (backgroundDisplay - backgroundLinear) *
                 (1.0 - saturate(coverage));
}

float2 RasterSceneUv(float2 uv)
{
    // Only the reconstruction prepass uses GodRayCenter as a raster UV offset.
    // Lens flares use its ordinary sun position only outside that prepass.
    // Native raster/sky/cloud colour is unjittered; DXR guides already include
    // projection jitter and must keep their original coordinates.
    return saturate(uv + (ScenePrepass == 1u ? GodRayCenter : float2(0.0, 0.0)));
}

float NativeTrailCoreMask(float2 uv)
{
#ifdef DUST_RAY_QUERY
    return 0.0;
#else
    uint width, height;
    NativeEffectDepthGuide.GetDimensions(width, height);
    if (width == 0 || height == 0) return 0.0;
    uint2 pixel = min(uint2(saturate(uv) * float2(width, height)), uint2(width - 1, height - 1));
    float encoded = NativeEffectDepthGuide.Load(int3(pixel, 0));
    return isfinite(encoded) && encoded <= -2.0 ? 1.0 : 0.0;
#endif
}

float3 SceneSample(float2 uv)
{
    uv = RasterSceneUv(uv);
    float3 raw = (AntiAliasingMode == 1
        ? FilteredWorld.Sample(SourceSampler, uv)
        : WorldFrame.Sample(SourceSampler, uv)).rgb;
    float coverage = WorldFrame.Sample(SourceSampler, uv).a;
    return ApplySkyCoverage(uv, raw, coverage);
}


float3 BackgroundDisplaySample(float2 uv)
{
    return SkyToDisplay(BackgroundFrame.Sample(SourceSampler, saturate(uv)).rgb);
}

float3 BloomExtract(float2 uv)
{
    float3 sampleColor = SceneSample(saturate(uv));
    float luminance = Luminance(sampleColor);
    /* A soft knee keeps ordinary ship paint out of bloom while allowing
       engines, weapon FX, suns and bright sky regions to spread. */
    float contribution = smoothstep(0.62, 0.90, luminance);
    return sampleColor * contribution;
}

float3 BloomScene(float2 uv)
{
    uint width;
    uint height;
    WorldFrame.GetDimensions(width, height);
    float2 texel = 1.0 / float2(max(1u, width), max(1u, height));
    float3 bloom = BloomExtract(uv) * 3.0;
    float weight = 3.0;

    /* Thirteen full-resolution taps approximate a broad bloom kernel without
       introducing another render target or a multi-pass synchronization
       point into the already-fast single-pass presenter. */
    bloom += BloomExtract(uv + texel * float2( 3.0,  0.0)) * 1.50;
    bloom += BloomExtract(uv + texel * float2(-3.0,  0.0)) * 1.50;
    bloom += BloomExtract(uv + texel * float2( 0.0,  3.0)) * 1.50;
    bloom += BloomExtract(uv + texel * float2( 0.0, -3.0)) * 1.50;
    weight += 6.0;

    bloom += BloomExtract(uv + texel * float2( 5.0,  5.0));
    bloom += BloomExtract(uv + texel * float2(-5.0,  5.0));
    bloom += BloomExtract(uv + texel * float2( 5.0, -5.0));
    bloom += BloomExtract(uv + texel * float2(-5.0, -5.0));
    weight += 4.0;

    bloom += BloomExtract(uv + texel * float2( 13.0,  0.0)) * 0.65;
    bloom += BloomExtract(uv + texel * float2(-13.0,  0.0)) * 0.65;
    bloom += BloomExtract(uv + texel * float2( 0.0,  13.0)) * 0.65;
    bloom += BloomExtract(uv + texel * float2( 0.0, -13.0)) * 0.65;
    weight += 2.6;
    return bloom / weight;
}

float3 FxaaScene(float2 uv)
{
    uint width;
    uint height;
    WorldFrame.GetDimensions(width, height);
    float2 texel = 1.0 / float2(max(1u, width), max(1u, height));
    float3 center = SceneSample(uv);
    float3 northwest = SceneSample(uv + texel * float2(-1.0, -1.0));
    float3 northeast = SceneSample(uv + texel * float2(1.0, -1.0));
    float3 southwest = SceneSample(uv + texel * float2(-1.0, 1.0));
    float3 southeast = SceneSample(uv + texel * float2(1.0, 1.0));
    float lumaCenter = Luminance(center);
    float lumaMinimum = min(lumaCenter, min(min(Luminance(northwest),
        Luminance(northeast)), min(Luminance(southwest),
        Luminance(southeast))));
    float lumaMaximum = max(lumaCenter, max(max(Luminance(northwest),
        Luminance(northeast)), max(Luminance(southwest),
        Luminance(southeast))));
    if (lumaMaximum - lumaMinimum < max(0.035, lumaMaximum * 0.10))
    {
        return center;
    }
    float2 direction;
    direction.x = -((Luminance(northwest) + Luminance(northeast)) -
                    (Luminance(southwest) + Luminance(southeast)));
    direction.y = ((Luminance(northwest) + Luminance(southwest)) -
                   (Luminance(northeast) + Luminance(southeast)));
    float reduction = max((Luminance(northwest) + Luminance(northeast) +
        Luminance(southwest) + Luminance(southeast)) * 0.03125, 0.0078125);
    float inverseMinimum = 1.0 / (min(abs(direction.x), abs(direction.y)) +
                                  reduction);
    direction = clamp(direction * inverseMinimum, -8.0, 8.0) * texel;
    float3 blendA = 0.5 * (
        SceneSample(uv + direction * (1.0 / 3.0 - 0.5)) +
        SceneSample(uv + direction * (2.0 / 3.0 - 0.5)));
    float3 blendB = blendA * 0.5 + 0.25 * (
        SceneSample(uv + direction * -0.5) +
        SceneSample(uv + direction * 0.5));
    float lumaB = Luminance(blendB);
    return (lumaB < lumaMinimum || lumaB > lumaMaximum) ? blendA : blendB;
}

float4 RaySourceSample(float2 rayUv)
{
    return AntiAliasingMode >= 3
        ? FilteredWorld.SampleLevel(SourceSampler, saturate(rayUv), 0)
        : RayLighting.SampleLevel(SourceSampler, saturate(rayUv), 0);
}

float4 FilteredRayLighting(float2 displayUv)
{
    uint width;
    uint height;
    if (AntiAliasingMode >= 3)
    {
        FilteredWorld.GetDimensions(width, height);
    }
    else
    {
        RayLighting.GetDimensions(width, height);
    }
    float2 texel = 1.0 / float2(max(1u, width), max(1u, height));
    float2 centerUv = float2(displayUv.x, 1.0 - displayUv.y);
    float4 center = RaySourceSample(centerUv);
    if (center.a < 0.0)
    {
        return center;
    }
    float3 total = center.rgb * 4.0;
    float totalWeight = 4.0;
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }
            float2 offset = float2((float)x, (float)-y) * texel;
            float4 sampleValue = RaySourceSample(centerUv + offset);
            float depthTolerance = max(0.025, abs(center.a) * 0.008);
            if (sampleValue.a >= 0.0 &&
                abs(sampleValue.a - center.a) <= depthTolerance)
            {
                float weight = 1.0 / (1.0 + (float)(x * x + y * y));
                total += sampleValue.rgb * weight;
                totalWeight += weight;
            }
        }
    }
    return float4(lerp(center.rgb, total / totalWeight, 0.88), center.a);
}

/* Reconstruct glow coverage without nearest-texel blocks or filtering across
   foreground silhouettes. Use raw primary depth, independent of AA/upscaling. */
float FilteredEmissionProtection(float2 uv)
{
    uint width, height, depthWidth, depthHeight;
    PrimaryAlbedoGuide.GetDimensions(width, height);
    RayLighting.GetDimensions(depthWidth, depthHeight);
    float2 size = float2(width, height);
    int2 centerPixel = clamp(int2(saturate(uv) * size), int2(0,0), int2(width-1,height-1));
    int2 depthPixel = clamp(int2(saturate(uv) * float2(depthWidth,depthHeight)),
                           int2(0,0), int2(depthWidth-1,depthHeight-1));
    float centerDepth = RayLighting.Load(int3(depthPixel,0)).a;
    if (centerDepth < 0.0) return 0.0;
    float2 position = uv * size - 0.5;
    int2 basePixel = int2(floor(position));
    float2 fraction = frac(position);
    float total = 0.0, weightSum = 0.0;
    [unroll] for (int y=0; y<2; ++y)
    {
        [unroll] for (int x=0; x<2; ++x)
        {
            int2 p = clamp(basePixel + int2(x,y), int2(0,0), int2(width-1,height-1));
            float2 tapUv = (float2(p)+0.5)/size;
            int2 dp = min(int2(tapUv*float2(depthWidth,depthHeight)), int2(depthWidth-1,depthHeight-1));
            float depth = RayLighting.Load(int3(dp,0)).a;
            if (depth >= 0.0 && abs(depth-centerDepth) <= max(0.025,abs(centerDepth)*0.008))
            {
                float weight = (x ? fraction.x : 1.0-fraction.x) *
                               (y ? fraction.y : 1.0-fraction.y);
                total += PrimaryAlbedoGuide.Load(int3(p,0)).a * weight;
                weightSum += weight;
            }
        }
    }
    return saturate(weightSum > 0.0001 ? total/weightSum :
        PrimaryAlbedoGuide.Load(int3(centerPixel,0)).a);
}

static const uint DUST_SHAPE_SPHERE = 0u;
static const uint DUST_SHAPE_BOX = 1u;
static const uint DUST_SHAPE_ELLIPSOID = 2u;
static const uint DUST_SHAPE_ROPE = 3u;
static const uint DUST_RECEIVE_MISSION_KEY = 1u;
static const uint DUST_RECEIVE_LOCAL_LIGHTS = 2u;
static const uint DUST_CAST_VOLUMETRIC_SHADOW = 4u;
/* Bit 7 marks the new per-volume wake-strength encoding. Bits 3..6 store a
   4-bit 0..2x value. Legacy records without the marker retain 1.0x wakes. */
static const uint DUST_WAKE_STRENGTH_MARKER = 0x80u;
static const uint DUST_WAKE_STRENGTH_SHIFT = 3u;
static const uint DUST_WAKE_STRENGTH_MASK = 0x0fu;
/* High flag bits are runtime-only relevance masks populated by the CPU.
   Authoring flags remain in the low byte. This avoids walking every wake and
   local light at every raymarch sample for every cloud. */
static const uint DUST_WAKE_MASK_SHIFT = 8u;
static const uint DUST_LOCAL_LIGHT_MASK_SHIFT = 20u;
static const uint DUST_RELEVANCE_MASK = 0x0fffu;
static const float DUST_PI = 3.14159265358979323846;
/* Early termination is only a performance decision. Do not quantize a dark
   cloud to black: the old two-percent snap exposed a moving opacity contour. */
static const float DUST_EARLY_OUT_TRANSMITTANCE = 0.002;

uint DustHash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float DustHashCell(int3 cell, uint seed)
{
    uint h = DustHash(seed ^ (asuint(cell.x) * 0x9e3779b9u));
    h = DustHash(h ^ (asuint(cell.y) * 0x85ebca6bu));
    h = DustHash(h ^ (asuint(cell.z) * 0xc2b2ae35u));
    return (float)(h & 0x00ffffffu) / 16777216.0;
}

float DustValueNoise(float3 p, uint seed)
{
    int3 base = int3(floor(p));
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = DustHashCell(base + int3(0,0,0), seed);
    float n100 = DustHashCell(base + int3(1,0,0), seed);
    float n010 = DustHashCell(base + int3(0,1,0), seed);
    float n110 = DustHashCell(base + int3(1,1,0), seed);
    float n001 = DustHashCell(base + int3(0,0,1), seed);
    float n101 = DustHashCell(base + int3(1,0,1), seed);
    float n011 = DustHashCell(base + int3(0,1,1), seed);
    float n111 = DustHashCell(base + int3(1,1,1), seed);
    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    return lerp(lerp(nx00, nx10, f.y), lerp(nx01, nx11, f.y), f.z);
}

float3 DustNoiseDomain(float3 p)
{
    /* Fixed non-axis-aligned basis. Sampling trilinear value noise directly
       in authored XYZ exposes its cell planes as crosses and tube-like bands
       when density and lighting contrast are pushed hard. */
    return float3(
        dot(p, float3( 0.00,  0.82,  0.57)),
        dot(p, float3(-0.92,  0.22, -0.32)),
        dot(p, float3(-0.39, -0.52,  0.76)));
}

float3 DustSeedOffset(uint seed)
{
    return float3(
        (float)(DustHash(seed ^ 0xa341316cu) & 1023u),
        (float)(DustHash(seed ^ 0xc8013ea4u) & 1023u),
        (float)(DustHash(seed ^ 0xad90777du) & 1023u)) * (1.0 / 37.0);
}

struct DustRotationContext
{
    float2 xSinCos;
    float2 ySinCos;
    float2 zSinCos;
};

DustRotationContext DustBuildRotationContext(DustVolume volume)
{
    DustRotationContext result;
    result.xSinCos = volume.rotationXSinCos;
    result.ySinCos = volume.rotationYSinCos;
    result.zSinCos = volume.rotationZSinCos;
    return result;
}

float3 DustWorldToLocalVectorFast(DustRotationContext rotation,
                                  float3 worldVector)
{
    float s = rotation.zSinCos.x;
    float c = rotation.zSinCos.y;
    worldVector = float3(worldVector.x*c - worldVector.y*s,
                         worldVector.x*s + worldVector.y*c,
                         worldVector.z);
    s = rotation.ySinCos.x;
    c = rotation.ySinCos.y;
    worldVector = float3(worldVector.x*c + worldVector.z*s,
                         worldVector.y,
                        -worldVector.x*s + worldVector.z*c);
    s = rotation.xSinCos.x;
    c = rotation.xSinCos.y;
    return float3(worldVector.x,
                  worldVector.y*c - worldVector.z*s,
                  worldVector.y*s + worldVector.z*c);
}

float3 DustBoundsHalfSize(DustVolume volume)
{
    float3 halfSize = max(volume.size * 0.5, float3(1.0, 1.0, 1.0));
    if (volume.shape == DUST_SHAPE_SPHERE)
    {
        float radius = max(1.0, (halfSize.x + halfSize.y + halfSize.z) / 3.0);
        halfSize = float3(radius, radius, radius);
    }
    /* Domain-warped density can protrude beyond the unwarped authoring shape.
       Expand only the ray-intersection bounds so the randomized silhouette is
       never sliced off by an invisible axis-aligned plane. */
    float variation = saturate(volume.shapeVariation * 0.75);
    return halfSize * (1.0 + 0.38 * variation);
}

float DustFilteredNoise(float3 p, uint seed, float footprint)
{
    float resolved = 1.0 - smoothstep(0.25, 1.0, footprint);
    if (resolved <= 0.0) return 0.5;
    return lerp(0.5, DustValueNoise(p, seed), resolved);
}

float DustShapeEdgeFiltered(DustVolume volume, DustRotationContext rotation,
                    float3 seedOffset, float3 worldPosition, float footprint)
{
    float3 halfSize = max(volume.size * 0.5, float3(1.0, 1.0, 1.0));
    float3 volumeLocalPosition = DustWorldToLocalVectorFast(
        rotation, worldPosition - volume.position);
    float3 local;
    if (volume.shape == DUST_SHAPE_ROPE)
    {
        /* A true swept capsule: local Z is the tendril path and XY is its
           soft radial envelope.  This has no box corners or ellipsoid waist. */
        float radius = max(1.0, min(halfSize.x, halfSize.y));
        float halfLine = max(0.0, halfSize.z - radius);
        float3 q = volumeLocalPosition;
        q.z -= clamp(q.z, -halfLine, halfLine);
        local = q / radius;
    }
    else if (volume.shape == DUST_SHAPE_SPHERE)
    {
        float radius = max(1.0, (halfSize.x + halfSize.y + halfSize.z) / 3.0);
        local = volumeLocalPosition / radius;
    }
    else
    {
        local = volumeLocalPosition / halfSize;
    }

    float edge = (volume.shape == DUST_SHAPE_BOX || volume.shape == 4u)
        ? 1.0 - max(abs(local.x), max(abs(local.y), abs(local.z)))
        : 1.0 - length(local);

    float variation = saturate(volume.shapeVariation * 0.75);
    if (variation > 0.0001)
    {
        /* The former sin/cos vector warp had exact zero/crest planes. Under
           strong lighting those planes became a cross, and its small maximum
           displacement still left the underlying ellipsoid plainly visible.

           Treat the selected primitive only as an authoring coordinate system
           and displace its signed boundary with low-frequency, rotated 3D
           value noise. There are no analytic axial planes, while the zero-mean
           displacement preserves the cloud's authored position and scale. */
        float3 boundaryP = DustNoiseDomain(volumeLocalPosition) *
            max(volume.noiseScale * 0.115, 0.000002) + seedOffset * 0.29;
        float boundaryFootprint = footprint * max(volume.noiseScale * 0.115, 0.000002);
        float boundary0 = DustFilteredNoise(
            boundaryP, volume.shapeSeed ^ 0x85ebca6bu, boundaryFootprint);
        float boundary1 = DustFilteredNoise(
            boundaryP * 2.07 + float3(11.3, 23.7, 7.9),
            volume.shapeSeed ^ 0xc2b2ae35u, boundaryFootprint * 2.07);
        float boundary2 = DustFilteredNoise(
            boundaryP * 4.19 + float3(31.1, 5.3, 17.7),
            volume.shapeSeed ^ 0x27d4eb2fu, boundaryFootprint * 4.19);
        float displacement =
            ((boundary0 * 0.57 + boundary1 * 0.29 + boundary2 * 0.14) - 0.5) *
            2.0;
        /* Large enough to erase the mathematical primitive at the high end,
           but smoothly proportional so existing low-variation clouds retain
           their established silhouette. */
        edge += displacement * (0.72 * variation);
    }
    return edge;
}

float DustShapeEdge(DustVolume volume, DustRotationContext rotation,
                    float3 seedOffset, float3 worldPosition)
{
    return DustShapeEdgeFiltered(volume, rotation, seedOffset, worldPosition, 0.0);
}

/* Conservative lower bound on distance to any potentially dense randomized
   core. Unlike DustShapeEdge this intentionally does not evaluate the domain
   warp's sin/cos field. It subtracts the maximum possible warp analytically,
   so a positive result is guaranteed empty enough for contribution testing. */
float DustConservativeOutsideWorld(DustVolume volume,
                                   DustRotationContext rotation,
                                   float3 authoredHalfSize,
                                   float smallestHalfAxis,
                                   float3 worldPosition)
{
    float3 localWorld = DustWorldToLocalVectorFast(
        rotation, worldPosition - volume.position);
    float3 local = localWorld / max(authoredHalfSize, float3(1.0, 1.0, 1.0));
    float variation = saturate(volume.shapeVariation * 0.75);
    /* Must cover the complete signed-boundary displacement used by
       DustShapeEdge so empty-space skipping can never cut off a noisy lobe. */
    float warpAxis = 0.72 * variation;
    float outsideNormalized;
    if (volume.shape == DUST_SHAPE_ROPE)
    {
        float radius = max(1.0, min(authoredHalfSize.x, authoredHalfSize.y));
        float halfLine = max(0.0, authoredHalfSize.z - radius);
        float3 q = localWorld;
        q.z -= clamp(q.z, -halfLine, halfLine);
        outsideNormalized = max(0.0, length(q) / radius - (1.0 + warpAxis));
        smallestHalfAxis = radius;
    }
    else if (volume.shape == DUST_SHAPE_BOX || volume.shape == 4u)
    {
        float maxAxis = max(abs(local.x), max(abs(local.y), abs(local.z)));
        outsideNormalized = max(0.0, maxAxis - (1.0 + warpAxis));
    }
    else
    {
        const float sqrtThree = 1.7320508075688772;
        outsideNormalized = max(0.0,
            length(local) - (1.0 + warpAxis * sqrtThree));
    }
    return outsideNormalized * smallestHalfAxis;
}

float3 DustSafePerpendicular(float3 direction)
{
    float3 reference = abs(direction.y) < 0.85 ? float3(0.0, 1.0, 0.0)
                                                : float3(1.0, 0.0, 0.0);
    return normalize(cross(direction, reference));
}

void DustApplyShipWakes(float3 worldPosition, uint wakeMask,
                        float volumeWakeStrength,
                        out float3 densitySamplePosition,
                        out float densityMultiplier)
{
    densitySamplePosition = worldPosition;
    densityMultiplier = 1.0;
    [loop]
    for (uint wakeIndex = 0u; wakeIndex < DustWakeCount; ++wakeIndex)
    {
        if ((wakeMask & (1u << wakeIndex)) == 0u) continue;
        DustWake wake = DustWakes[wakeIndex];
        float wakeStrength = wake.strength * volumeWakeStrength;
        float bowStrength = wake.bowStrength * volumeWakeStrength;
        if (wakeStrength <= 0.001 || wake.radius <= 1.0 || wake.length <= 1.0)
            continue;

        float3 direction = normalize(wake.direction);
        float3 relative = worldPosition - wake.position;
        float axial = dot(relative, direction);
        if (axial > wake.radius * 2.4 ||
            axial < -(wake.length + wake.radius * 2.0))
            continue;

        float segmentAxial = clamp(axial, -wake.length, 0.0);
        float3 axisPoint = wake.position + direction * segmentAxial;
        float3 radial = worldPosition - axisPoint;
        float radialDistance = length(radial);
        if (radialDistance > wake.radius * 2.4)
            continue;

        float3 radialDirection = radialDistance > 0.001
            ? radial / radialDistance
            : DustSafePerpendicular(direction);
        float headFade = 1.0 - smoothstep(0.0, wake.radius * 1.6, axial);
        float tailFade = smoothstep(
            -(wake.length + wake.radius),
            -(wake.length - wake.radius * 0.35), axial);
        float segmentMask = saturate(headFade * tailFade);
        float core = (1.0 - smoothstep(
            wake.radius * 0.22, wake.radius * 1.10, radialDistance)) *
            segmentMask * wakeStrength;

        /* Advect the sampled density away from the ship path.  This moves the
           existing 3D noise field rather than drawing a second FX sprite. */
        float3 tangentCross = cross(direction, radialDirection);
        float tangentLength = length(tangentCross);
        float3 tangent = tangentLength > 0.001
            ? tangentCross / tangentLength
            : DustSafePerpendicular(direction);
        float swirl = sin(dot(worldPosition, float3(0.0011, 0.0017, 0.0013)) +
                          dot(wake.position, float3(0.0007, 0.0013, 0.0019)));
        densitySamplePosition -= radialDirection * (wake.radius * 0.34 * core);
        densitySamplePosition -= tangent * (wake.radius * 0.11 * core * swirl);

        /* The evacuated centre recovers smoothly as wake.strength decays. */
        densityMultiplier *= 1.0 - 0.84 * saturate(core);

        /* Dust pushed out of the centre builds a soft shoulder around the
           wake, preserving mass visually instead of merely erasing density. */
        float sideBand = 1.0 - saturate(
            abs(radialDistance - wake.radius * 1.20) /
            max(1.0, wake.radius * 0.58));
        densityMultiplier += 0.24 * sideBand * segmentMask * wakeStrength;

        /* A short-lived compressed bow wave sits just ahead of the moving
           hull.  Older trail records lose this much faster than their wake. */
        float3 bowCenter = wake.position + direction * wake.radius * 0.80;
        float bowDistance = length(worldPosition - bowCenter);
        float bowBand = 1.0 - saturate(
            abs(bowDistance - wake.radius * 1.05) /
            max(1.0, wake.radius * 0.52));
        float bowFront = smoothstep(-wake.radius * 0.25,
                                    wake.radius * 1.50, axial);
        densityMultiplier += 0.38 * bowBand * bowFront * bowStrength;
    }
    densityMultiplier = clamp(densityMultiplier, 0.04, 1.75);
}

float DustTargetStepLength(DustVolume volume)
{
    /* RTX-0089: the previous 18-slice march divided every current ray segment
       into 18 pieces.  Rotating the camera changed both the segment length and
       the phase of every sample, so a perfectly static 3D field appeared to
       boil against its editor bounds.  Choose a WORLD-UNIT step from the
       authored volume/noise instead.  It stays constant as the camera moves. */
    float scale = max(volume.noiseScale, 0.000005);
    float fineFeatureSize = 1.0 / max(scale * 1.65, 0.000001);
    float3 authoredSize = max(volume.size, float3(1.0, 1.0, 1.0));
    float minimumAxis = min(authoredSize.x, min(authoredSize.y, authoredSize.z));
    // A converted brick is a 17^3 trilinear field, not procedural fine noise.
    // Resolve at least two samples per voxel, regardless of camera distance.
    if (volume.shape == 4u) return minimumAxis / 64.0;
    return clamp(min(fineFeatureSize * 0.48, minimumAxis / 18.0),
                 45.0, 220.0);
}

float DustSoftEnvelope(float edge)
{
    /* The authored sphere/box/ellipsoid is a density coordinate system, not
       a clipping primitive. Preserve the familiar interior feather, then add
       a smooth asymptotic exterior tail so density can cross the authoring
       boundary without ever meeting a mathematical wall. */
    float authoredMask = smoothstep(-0.06, 0.20, edge);
    float outside = max(0.0, -edge);
    float exteriorTail = 0.35 * exp(-6.0 * outside * outside);
    return saturate(authoredMask + (1.0 - authoredMask) * exteriorTail);
}

float DustVolumeWakeStrength(uint flags)
{
    if ((flags & DUST_WAKE_STRENGTH_MARKER) == 0u) return 1.0;
    uint code = (flags >> DUST_WAKE_STRENGTH_SHIFT) & DUST_WAKE_STRENGTH_MASK;
    if (code <= 8u) return (float)code / 8.0;
    return 1.0 + (float)(code - 8u) / 7.0;
}

float DustHighCoverageFill(float authoredCoverage)
{
    /* Coverage 1.0 is the established/default response. The upper half of the
       0..2 editor range is an intentional artist override: progressively fill
       noise voids until coverage 2.0 produces a continuous interior medium. */
    float t = saturate(authoredCoverage - 1.0);
    return t * t * (3.0 - 2.0 * t);
}

float DustMaterialCoefficientScale(DustVolume volume)
{
    // A particle brick's packed peak is normalization, not a material override.
    if (volume.shape == 4u) return 1.0;
    /* Keep the proven/default range unchanged, but give high-end authoring a
       physically useful optical-depth response. This scales sigmaT and sigmaS
       together, so scattering albedo/hue are preserved while thick materials
       actually become opaque. */
    float densityHigh = max(0.0, volume.density - 1.0);
    float materialExtinction = max(0.0, volume.absorption + volume.scattering);
    float extinctionHigh = max(0.0, materialExtinction - 1.15);
    return (1.0 + densityHigh * 0.30) *
           (1.0 + extinctionHigh * 0.12);
}

float DustDistanceFade(DustVolume volume, float3 cameraPosition)
{
    /* Authored clouds are permanent world volumes. Camera distance must never
       weaken them or select a cheaper-looking representation. */
    return 1.0;
}

float ParticleGridValue(uint index)
{
    DustVolume packed = DustVolumes[256u + index / 44u];
    uint lane=index%44u;float4 values=0.0;
    switch(lane/4u) {
        case 0:values=asfloat(uint4(packed.enabled,packed.shape,packed.shapeSeed,packed.flags));break;
        case 1:values=float4(packed.position,packed.density);break;
        case 2:values=float4(packed.size,packed.scattering);break;
        case 3:values=float4(packed.color,packed.absorption);break;
        case 4:values=float4(packed.anisotropy,packed.coverage,packed.noiseScale,packed.noiseDetail);break;
        case 5:values=float4(packed.shapeVariation,packed.rotationDegrees);break;
        case 6:values=float4(packed.rotationXSinCos,packed.rotationYSinCos);break;
        case 7:values=float4(packed.rotationZSinCos,packed.precomputedPadding);break;
        case 8:values=float4(packed.seedOffset,packed.seedPadding);break;
        case 9:values=packed.openmwControl;break;
        default:values=packed.openmwTime;break;
    }
    return values[lane&3u];
}
// HLSL adaptation of zesterer/openmw-volumetric-clouds, clouds.omwfx.
// Pinned upstream: c8830bcd2de0f6e7355f91880308426203eded3c.
// See ../ThirdParty/OpenMWVolumetricClouds/PROVENANCE.md for attribution,
// intentional changes, and the unresolved upstream redistribution license.
// Host owns finite-volume placement, envelope/grid support, authored color,
// compositing, ray bounds, and motion-vector production.
#ifndef OPENHW_OPENMW_CLOUD_KERNEL
#define OPENHW_OPENMW_CLOUD_KERNEL

// Original red channels, not sRGB: noise2.png in R; noise3.png in G.



float OpenMWCloudHashOne(int q)
{
    // Preserve the upstream modern-GLSL signed int result and 32-bit overflow.
    // Explicit unsigned arithmetic gives DXC/FXC defined wrapping semantics.
    uint bits = ((396128171u * asuint(q)) ^ 388370683u) * 204766743u;
    return float(asint(bits)) * (1.0 / 4294967295.0);
}

float OpenMWCloudNoise3D(float3 p)
{
    p.z *= 15.0;
    int z = (int)floor(p.z);
    float2 offset0 = float2(OpenMWCloudHashOne(z), OpenMWCloudHashOne(z + 73));
    float2 offset1 = float2(OpenMWCloudHashOne(z + 1), OpenMWCloudHashOne(z + 74));
    return lerp(OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset0, 0.0).r,
                OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset1, 0.0).r,
                frac(p.z));
}

float OpenMWCloudBillowNoise3D(float3 p)
{
    p.z *= 15.0;
    int z = (int)floor(p.z);
    float2 offset0 = float2(OpenMWCloudHashOne(z), OpenMWCloudHashOne(z + 73));
    float2 offset1 = float2(OpenMWCloudHashOne(z + 1), OpenMWCloudHashOne(z + 74));
    return lerp(OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset0, 0.0).g,
                OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset1, 0.0).g,
                frac(p.z));
}

float OpenMWCloudResolvedWeight(float footprint)
{
    // The source LUT is 128 texels per XY repeat and 15 slices per Z unit.
    // Only unresolved detail fades, with no camera/time-dependent random phase.
    return 1.0 - smoothstep(0.5, 2.0, max(0.0, footprint) * 128.0);
}

float3 OpenMWCloudAdvectedPosition(float3 p, float simulationTime, float speed)
{
    // Motion is a single rigid translation. No swirl, curl, rotation, changing
    // shape offsets, or opposite-moving octaves. Speed zero is exactly static.
    if (speed != 0.0)
        p.x += simulationTime * speed * 0.003;
    return p;
}

float OpenMWCloudPattern(float3 p, float coverage, float detail,
                        float billowStrength, float simulationTime,
                        float motionSpeed, float footprint)
{
    // p is a fixed local/world-space position times the authored noise scale;
    // upstream uses world position / 500000 for the large cloud scale.
    p = OpenMWCloudAdvectedPosition(p, simulationTime, motionSpeed);
    coverage = saturate(coverage);
    detail = clamp(detail, 0.0, 3.0);
    billowStrength = max(0.0, billowStrength);
    float3 largePosition = p * float3(0.55, 0.55, 1.0);
    float cloudF = OpenMWCloudNoise3D(largePosition + float3(0.0, 0.0, 0.01));
    float cloudF2 = cloudF;
    if (detail >= 2.0)
        cloudF2 = OpenMWCloudNoise3D(largePosition - float3(0.0, 0.0, 0.01));

    float cloudLarge = (cloudF + cloudF2) * 0.5 - 1.0 + coverage;
    if (cloudLarge <= -0.1)
        return 0.0;

    float smallNoise = 0.0;
    if (detail >= 1.0)
    {
        float resolved = OpenMWCloudResolvedWeight(footprint * (500000.0 / 120000.0));
        if (resolved > 0.0)
            smallNoise += (OpenMWCloudBillowNoise3D(p * (500000.0 / 120000.0)) - 0.5) * resolved;
    }
    if (detail >= 3.0)
    {
        float resolved = OpenMWCloudResolvedWeight(footprint * (500000.0 / 30000.0));
        if (resolved > 0.0)
            smallNoise += (OpenMWCloudBillowNoise3D(p * (500000.0 / 30000.0)) - 0.5) * 0.25 * resolved;
    }

    // Retain the source's coverage threshold, billow erosion, and power-five
    // shaping. Host density supplies world-space extinction; there is no sky
    // altitude band, weather control, water-height dependency, or camera fade.
    float cloudFactor = max(cloudLarge - smallNoise * 0.1 * billowStrength, 0.0) * 3.0;
    float cloudFactor2 = cloudFactor * cloudFactor;
    return cloudFactor2 * cloudFactor2 * cloudFactor;
}

float OpenMWCloudTransmission(float extinction, float segmentLength)
{
    // Source pow(0.9999, length * density * 10000), expressed exponentially.
    // This coefficient is -log(0.9999) * 10000, not an arbitrary contrast curve.
    return exp(-1.0000500033335834 * max(0.0, extinction) * max(0.0, segmentLength));
}

float OpenMWCloudSamplingFactor(float samplingQuality)
{
    return pow(1.25, clamp(samplingQuality, -5.0, 10.0));
}

float OpenMWCloudLogSpace(float distance, float samplingQuality)
{
    return log(max(distance, 0.000001)) * OpenMWCloudSamplingFactor(samplingQuality);
}

float OpenMWCloudWorldSpace(float logarithmicDistance, float samplingQuality)
{
    return exp(logarithmicDistance / OpenMWCloudSamplingFactor(samplingQuality));
}

float OpenMWCloudSegmentBoundary(float entryDistance, float exitDistance,
                                uint segmentIndex, uint segmentCount)
{
    // Finite-volume version of the source logarithmic ray distribution. The
    // shifted domain supports a camera inside a cloud (entry=0), and bounded
    // segment count guarantees the final segment reaches the entire exit.
    float entry = max(0.0, entryDistance);
    float exit = max(entry, exitDistance);
    float fraction = min((float)segmentIndex / (float)max(1u, segmentCount), 1.0);
    float shift = max(1.0, (exit - entry) * 0.25);
    float logEntry = log(shift + entry);
    float logExit = log(shift + exit);
    return min(exit, max(entry, exp(lerp(logEntry, logExit, fraction)) - shift));
}

#endif // OPENHW_OPENMW_CLOUD_KERNEL

float OpenMWVolumePattern(DustVolume volume, float3 worldPosition, float footprint)
{
    // A storage brick must not acquire its own noise phase or seam. Particle
    // formations use shared world coordinates; authored primitives keep their
    // existing rotation and seed without changing their placement or bounds.
    float3 p = volume.shape == 4u ? worldPosition :
        DustWorldToLocalVectorFast(DustBuildRotationContext(volume), worldPosition-volume.position);
    float scale = max(volume.noiseScale, 0.0000001) / 64.0;
    p *= scale;
    if (volume.shape != 4u) p += volume.seedOffset * 0.01;
    // Particle records use `coverage` for baked kernel softness. It is not
    // procedural cloud coverage: feeding softness=2 into the OpenMW power-five
    // pattern flattened it against the old cap and restored the smooth grid.
    bool particleField = volume.shape == 4u;
    float coverage = particleField ? saturate(volume.openmwTime.y) :
        lerp(0.70, 1.0, saturate(volume.coverage * 0.5));
    float billow = volume.openmwControl.w *
        (particleField ? clamp(volume.noiseDetail,0.0,2.0) : 1.0);
    float pattern = OpenMWCloudPattern(p, coverage, volume.openmwControl.y,
        billow, volume.openmwTime.x, volume.openmwControl.z,
        max(0.0, footprint) * scale);
    if (particleField)
    {
        // The baked field owns placement and particle-count density; OpenMW
        // owns its cloud texture. Zero noise stays a hole, with no residual
        // smooth-fog baseline. Smooth compression avoids hard flat plateaus
        // while leaving each brick's independently normalized density linear.
        float contrast = lerp(0.6,1.8,saturate(volume.shapeVariation*0.5));
        float shaped = pow(max(pattern,0.0),contrast);
        return 2.0*shaped/(1.0+shaped);
    }
    float strength = saturate(volume.noiseDetail * 0.65 + volume.shapeVariation * 0.30);
    return lerp(1.0, min(pattern, 2.0), strength);
}

float ParticleDensityAt(DustVolume volume, float3 worldPosition, float footprint = 0.0)
{
    float3 q = (worldPosition - volume.position) / max(volume.size,1.0) + 0.5;
    if (any(q < 0.0) || any(q > 1.0)) return 0.0;
    q *= 16.0;
    uint3 cell = min(uint3(floor(q)), uint3(15,15,15));
    float3 f = q - float3(cell);
    float density = 0.0;
    [loop] for (uint corner=0u;corner<8u;++corner) {
        uint x=corner&1u,y=(corner>>1u)&1u,z=(corner>>2u)&1u;
        uint index = ((cell.z+z)*17u+cell.y+y)*17u+cell.x+x;
        density += ParticleGridValue(volume.shapeSeed+index) *
            (x?f.x:1.0-f.x)*(y?f.y:1.0-f.y)*(z?f.z:1.0-f.z);
    }
    // Preserve the baked support/density field. OpenMW adds texture within it;
    // neither camera motion nor optional animation can move the brick bounds.
    if (density <= 0.0) return 0.0;
    return volume.density * density * OpenMWVolumePattern(volume,worldPosition,footprint);
}
float DustDensityAt(DustVolume volume, DustRotationContext rotation,
                    float3 seedOffset, uint wakeMask, float3 worldPosition,
                    float sampleStepLength)
{
    if (volume.shape == 4u) return ParticleDensityAt(volume,worldPosition,sampleStepLength);
    float edgeMask = DustSoftEnvelope(DustShapeEdge(volume,rotation,seedOffset,worldPosition));
    if (max(0.0,volume.density)*edgeMask <= 0.00001) return 0.0;
    return max(0.0,volume.density)*edgeMask*OpenMWVolumePattern(volume,worldPosition,sampleStepLength);
}
float DustLargeVolumeDensityAt(DustVolume volume, DustRotationContext rotation,
                    float3 seedOffset, uint wakeMask, float3 worldPosition,
                    float sampleStepLength)
{
    return DustDensityAt(volume,rotation,seedOffset,0u,worldPosition,sampleStepLength);
}
float DustCoarseDensityAt(DustVolume volume, DustRotationContext rotation,
                          float3 seedOffset, float3 worldPosition, float footprint)
{
    return DustDensityAt(volume,rotation,seedOffset,0u,worldPosition,footprint);
}

float DustPhaseHG(float anisotropy, float cosineTheta)
{
    float g = clamp(anisotropy, -0.92, 0.92);
    float g2 = g * g;
    float denom = max(0.015, 1.0 + g2 - 2.0 * g * cosineTheta);
    /* denom * sqrt(denom) is exactly denom^1.5 and avoids a general pow. */
    return (1.0 - g2) / (4.0 * DUST_PI * denom * sqrt(denom));
}

float DustKeyTransmittance(DustVolume volume, DustRotationContext rotation,
                           float3 seedOffset, float3 samplePosition,
                           float3 directionToSource)
{
    if ((volume.flags & DUST_CAST_VOLUMETRIC_SHADOW) == 0u) return 1.0;
    float3 halfSize = DustBoundsHalfSize(volume);
    float shadowLength = length(halfSize) * 1.35;
    const uint shadowSteps = 3u;
    float stepLength = shadowLength / (float)shadowSteps;
    float opticalDepth = 0.0;
    [unroll]
    for (uint i = 0u; i < shadowSteps; ++i)
    {
        float3 p = samplePosition + directionToSource *
            (stepLength * ((float)i + 0.75));
        float footprint = max(volume.size.x, max(volume.size.y, volume.size.z)) > 22000.0
            ? stepLength : 0.0;
        float density = DustCoarseDensityAt(volume, rotation, seedOffset, p, footprint);
        opticalDepth += density * stepLength;
    }
    float extinction = max(0.0, volume.absorption + volume.scattering);
    return exp(-opticalDepth * extinction * 0.00022 *
               DustMaterialCoefficientScale(volume));
}

float3 DustLightingWithDirectAt(DustVolume volume, DustRotationContext rotation,
                      float3 seedOffset, uint localLightMask,
                      float3 samplePosition, float3 cameraRayDirection,
                      float solidSunVisibility, out float3 directSunLighting)
{
    directSunLighting = 0.0;
    /* Cloud lighting is authored radiance, not a pre-tonemapped glow value.
       Keep source RGB/intensity linear here so F12 changes remain visible and
       hue survives into the medium. The final volumetric contribution gets a
       hue-preserving shoulder after integration instead of crushing each light
       before scattering. */
    float3 lighting = DustAmbientColorStrength.rgb *
                      DustAmbientColorStrength.a * 0.68;
    if ((volume.flags & DUST_RECEIVE_MISSION_KEY) != 0u &&
        DustSunDirectionValid.w > 0.5 &&
        DustRayUv01SunStrength.w > 0.0001)
    {
        float3 toSun = normalize(DustSunDirectionValid.xyz);
        float phase = DustPhaseHG(volume.anisotropy,
                                  dot(toSun, cameraRayDirection));
        float shadow = DustKeyTransmittance(
            volume, rotation, seedOffset, samplePosition, toSun);
        /* Dense clouds still receive a small multiple-scattering/key fill so
           self-shadowing creates depth instead of driving the interior to
           neutral black. This fill carries the authored key color. */
        float effectiveShadow = 0.18 + shadow * 0.82;
        // Solid geometry blocks the DIRECT key term inside the medium.
        // Leaving this argument unused made the queried ship shadows vanish:
        // only a faint separate atmosphere remained behind dense cloud fog.
        float geometryVisibility = saturate(solidSunVisibility);
        directSunLighting = DustSunColor.rgb * DustRayUv01SunStrength.w *
                    effectiveShadow * (0.18 + phase * 2.60);
        lighting += directSunLighting * geometryVisibility;
        /* Single scattering drives a forward-scattering cloud almost black
           when viewed away from the key light. Real dusty media recycle some
           of that energy through higher-order scattering. This conservative
           isotropic term prevents black cut-outs while retaining directional
           highlights and authored absorption. */
        float scatteringAlbedo = saturate(volume.scattering /
            max(0.001, volume.scattering + volume.absorption));
        lighting += DustSunColor.rgb * DustRayUv01SunStrength.w *
                    (0.20 * scatteringAlbedo * scatteringAlbedo);
        /* Additional shaft radiance belongs to the actual medium. Geometry
           visibility modulates direct light, not density or extinction. Keep
           authored ambient/multiple scattering intact in shadow. */
        // Ambient and multiple scattering stay unshadowed. Only direct key
        // radiance follows solid geometry; density/extinction never change.
    }

    if ((volume.flags & DUST_RECEIVE_LOCAL_LIGHTS) != 0u &&
        localLightMask != 0u)
    {
        [loop]
        for (uint lightIndex = 0u; lightIndex < DustLightCount; ++lightIndex)
        {
            if ((localLightMask & (1u << lightIndex)) == 0u) continue;
            DustLight light = DustLights[lightIndex];
            float3 delta = light.position - samplePosition;
            float distanceSquared = dot(delta, delta);
            float radiusSquared = light.radius * light.radius;
            if (distanceSquared <= 0.000001 || distanceSquared >= radiusSquared)
                continue;
            /* Reject out-of-range lights before paying for sqrt. */
            float distanceToLight = sqrt(distanceSquared);
            float3 toLight = delta / distanceToLight;
            float range = saturate(1.0 - distanceToLight / light.radius);
            float attenuation = range * range;
            if (light.type == 3u)
            {
                float spotCosine = dot(-toLight, light.direction);
                float innerCosine = max(light.coneCos, light.edgeCos);
                float outerCosine = min(light.coneCos, light.edgeCos);
                attenuation *= smoothstep(outerCosine, innerCosine, spotCosine);
            }
            float phase = DustPhaseHG(volume.anisotropy,
                                      dot(toLight, cameraRayDirection));
            /* Preserve the actual authored RGB and intensity. The previous
               per-light compression asymptoted near 1.2, so 2x/4x/8x lights
               became almost indistinguishable inside clouds. */
            float3 emitted = max(light.color, 0.0) * max(light.intensity, 0.0);
            lighting += emitted * attenuation * (0.12 + phase * 1.80);
        }
    }
    return max(lighting, 0.0);
}

float3 DustLightingAt(DustVolume volume, DustRotationContext rotation,
                      float3 seedOffset, uint localLightMask,
                      float3 samplePosition, float3 cameraRayDirection,
                      float solidSunVisibility = 1.0)
{
    float3 directSunLighting;
    return DustLightingWithDirectAt(volume,rotation,seedOffset,localLightMask,
        samplePosition,cameraRayDirection,solidSunVisibility,directSunLighting);
}

float3 DustToneVolumetricContribution(float3 radiance, float transmittance)
{
    float opacity = saturate(1.0 - transmittance);
    // Sources are authored display colors converted to linear before
    // integration. No light tint, albedo dimming, shoulder or white-point cap:
    // white stays white, black stays black, with density-controlled coverage.
    return min(max(radiance, 0.0), opacity.xxx);
}

float3 DustWorldGridActiveDimensions()
{
    return max(float3(DustGridMinWidth.w, DustGridSizeHeight.w,
                      PresenterPadding.x), float3(1.0, 1.0, 1.0));
}

float3 DustWorldGridTextureCoordinate(float3 worldPosition)
{
    uint textureWidth, textureHeight, textureDepth;
    DustFroxelField.GetDimensions(textureWidth, textureHeight, textureDepth);
    float3 gridSize = max(DustGridSizeHeight.xyz,
                          float3(0.0001, 0.0001, 0.0001));
    float3 gridUv = saturate(
        (worldPosition - DustGridMinWidth.xyz) / gridSize);
    float3 activeDimensions = DustWorldGridActiveDimensions();

    /* The physical cache is a persistent 160^3 allocation while only the
       active prefix is written this frame. Clamp to the centres of the first
       and last live texels so hardware trilinear filtering can never blend
       with inactive storage at a positive active-grid face. */
    float3 activeCell = gridUv * activeDimensions;
    activeCell = clamp(
        activeCell,
        float3(0.5, 0.5, 0.5),
        max(float3(0.5, 0.5, 0.5),
            activeDimensions - float3(0.5, 0.5, 0.5)));
    return activeCell /
        float3(max(1u, textureWidth), max(1u, textureHeight),
               max(1u, textureDepth));
}

bool DustRayWorldGridInterval(float3 rayOrigin, float3 rayDirection,
                              out float entryDistance,
                              out float exitDistance)
{
    float3 boundsMin = DustGridMinWidth.xyz;
    float3 boundsMax = boundsMin + DustGridSizeHeight.xyz;
    float tMin = 0.0;
    float tMax = 3.0e30;

    [unroll]
    for (uint axis = 0u; axis < 3u; ++axis)
    {
        float origin = rayOrigin[axis];
        float direction = rayDirection[axis];
        float minimumValue = boundsMin[axis];
        float maximumValue = boundsMax[axis];
        if (abs(direction) < 0.0000001)
        {
            if (origin < minimumValue || origin > maximumValue)
            {
                entryDistance = 0.0;
                exitDistance = 0.0;
                return false;
            }
            continue;
        }
        float inverseDirection = 1.0 / direction;
        float a = (minimumValue - origin) * inverseDirection;
        float b = (maximumValue - origin) * inverseDirection;
        float nearDistance = min(a, b);
        float farDistance = max(a, b);
        tMin = max(tMin, nearDistance);
        tMax = min(tMax, farDistance);
        if (tMax < tMin)
        {
            entryDistance = 0.0;
            exitDistance = 0.0;
            return false;
        }
    }
    entryDistance = tMin;
    exitDistance = tMax;
    return tMax > max(0.0, tMin);
}

void DustSunBasis(out float3 u, out float3 v)
{
    float3 sun = normalize(DustSunDirectionValid.xyz);
    u = normalize(cross(sun, abs(sun.z)<0.9 ? float3(0,0,1) : float3(0,1,0)));
    v = cross(sun,u);
}

#ifdef DUST_FROXEL_COMPUTE
#ifdef DUST_RAY_QUERY
// Orthographic sunlight: one nearest surface per light-aligned column.
// Keeping a column straight avoids stair steps through a world-axis grid.
[numthreads(8,8,1)]
void BuildDustSunColumnsCS(uint3 cell : SV_DispatchThreadID)
{
    uint width,height,depth;
    DustFroxelFieldRW.GetDimensions(width,height,depth);
    bool farCascade = PresenterPadding.y == 4.0;
    uint3 active = farCascade ? uint3(width,height,depth) :
        min(uint3(width,height,depth),uint3(DustGridMinWidth.w,
            DustGridSizeHeight.w,PresenterPadding.x));
    if (any(cell.xy >= active.xy)) return;
    float3 sun = normalize(DustSunDirectionValid.xyz);
    float3 u,v;
    DustSunBasis(u,v);
    float3 size = DustGridSizeHeight.xyz*(farCascade ? 4.0 : 1.0);
    float3 center = DustGridMinWidth.xyz+DustGridSizeHeight.xyz*0.5;
    float2 xy = ((float2(cell.xy)+0.5)/float2(active.xy)-0.5)*size.xy;
    float range = max(DustShadowControl.y,size.z*0.5+1.0);
    float4 origin = float4(center+u*xy.x+v*xy.y+sun*range,1.0);
    RayDesc ray;
    ray.Origin = float3(dot(DustShadowViewRow0,origin),
        dot(DustShadowViewRow1,origin),dot(DustShadowViewRow2,origin));
    ray.Direction = normalize(float3(dot(DustShadowViewRow0.xyz,-sun),
        dot(DustShadowViewRow1.xyz,-sun),dot(DustShadowViewRow2.xyz,-sun)));
    ray.TMin = 0.01;
    ray.TMax = range+size.z*0.5+1.0;
    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(DustShadowScene,RAY_FLAG_NONE,0x02u,ray);
    while (query.Proceed()) {}
    float surface = query.CommittedStatus()==COMMITTED_NOTHING ?
        -range-size.z : range-query.CommittedRayT();
    // Independent shaft visibility excludes only cloud-contained asteroids.
    // Alpha keeps ALL solid blockers for mission-key shadows inside dust.
    RayQuery<RAY_FLAG_FORCE_OPAQUE> shaftQuery;
    shaftQuery.TraceRayInline(DustShadowScene,RAY_FLAG_NONE,0x04u,ray);
    while (shaftQuery.Proceed()) {}
    float shaftSurface = shaftQuery.CommittedStatus()==COMMITTED_NOTHING ?
        -range-size.z : range-shaftQuery.CommittedRayT();
    [loop] for (uint z=0;z<active.z;++z)
    {
        float columnZ = ((float(z)+0.5)/float(active.z)-0.5)*size.z;
        float visibility = columnZ+DustShadowControl.z >= surface ? 1.0 : 0.0;
        float shaftVisibility = columnZ+DustShadowControl.z >= shaftSurface ? 1.0 : 0.0;
        DustFroxelFieldRW[uint3(cell.xy,z)] = float4(shaftVisibility,0,0,visibility);
    }
}
#endif
[numthreads(4, 4, 4)]
void BuildDustFroxelCS(uint3 cell : SV_DispatchThreadID,
                       uint3 localCell : SV_GroupThreadID)
{
    uint textureWidth, textureHeight, textureDepth;
    DustFroxelFieldRW.GetDimensions(textureWidth, textureHeight, textureDepth);
    bool buildFarCascade = PresenterPadding.y == 2.0 || PresenterPadding.y == 4.0;
    uint3 activeDimensions = buildFarCascade
        ? uint3(textureWidth, textureHeight, textureDepth)
        : uint3(max(1.0, DustGridMinWidth.w),
                max(1.0, DustGridSizeHeight.w),
                max(1.0, PresenterPadding.x));
    activeDimensions = min(activeDimensions,
        uint3(textureWidth, textureHeight, textureDepth));

    float3 gridUv = (float3(cell) + 0.5) / float3(activeDimensions);
    float3 nearCenter = DustGridMinWidth.xyz + DustGridSizeHeight.xyz * 0.5;
    float3 gridSize = DustGridSizeHeight.xyz *
                      (buildFarCascade ? 4.0 : 1.0);
    float3 gridMinimum = buildFarCascade
        ? nearCenter - gridSize * 0.5
        : DustGridMinWidth.xyz;
    float solidSunVisibility = 1.0;
#ifdef DUST_RAY_QUERY
    /* Eight fixed world samples per 4^3 group, rather than a ray per pixel
       or per volume. No temporal random phase: rotating the camera cannot
       stamp detached screen-space copies of a ship. */
    uint shadowSlot = (localCell.x / 2u) + (localCell.y / 2u)*2u +
                      (localCell.z / 2u)*4u;
    if (all((localCell & 1u) == 0u))
    {
        float visibility = 1.0;
        if (DustShadowControl.x > 0.5 &&
            DustSunDirectionValid.w > 0.5)
        {
            float3 sourceDirection = normalize(DustSunDirectionValid.xyz);
            float3 shadowPoint = gridMinimum + (float3(cell) + 1.0) /
                           float3(activeDimensions) * gridSize;
            shadowPoint += sourceDirection * DustShadowControl.z;
            RayDesc ray;
            float4 p = float4(shadowPoint, 1.0);
            ray.Origin = float3(dot(DustShadowViewRow0,p),
                dot(DustShadowViewRow1,p),dot(DustShadowViewRow2,p));
            ray.Direction = normalize(float3(dot(DustShadowViewRow0.xyz,sourceDirection),
                dot(DustShadowViewRow1.xyz,sourceDirection),
                dot(DustShadowViewRow2.xyz,sourceDirection)));
            ray.TMin = 0.01;
            ray.TMax = DustShadowControl.y;
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
                     RAY_FLAG_FORCE_OPAQUE> query;
            query.TraceRayInline(DustShadowScene, RAY_FLAG_NONE, 0x02u, ray);
            while (query.Proceed()) {}
            visibility = query.CommittedStatus() == COMMITTED_NOTHING ? 1.0 : 0.0;
        }
        DustSunVisibility[shadowSlot] = visibility;
    }
    GroupMemoryBarrierWithGroupSync();
    solidSunVisibility = DustSunVisibility[shadowSlot];
#endif
    if (any(cell >= activeDimensions)) return;
    // Visibility-only mode feeds the live procedural renderer. Never cache
    // its density or colour: that would change authored clouds.
    if (PresenterPadding.y >= 3.0)
    {
        DustFroxelFieldRW[cell] = float4(0.0,0.0,0.0,solidSunVisibility);
        return;
    }
    float3 worldPosition = gridMinimum + gridUv * gridSize;
    float3 voxelSize = gridSize / float3(activeDimensions);
    float sampleStepLength = max(1.0,
        max(voxelSize.x, max(voxelSize.y, voxelSize.z)));
    float3 cameraVector = worldPosition - DustCameraPositionMaxDistance.xyz;
    float cameraVectorLengthSquared = dot(cameraVector, cameraVector);
    float3 cameraRayDirection = cameraVectorLengthSquared > 0.000001
        ? cameraVector * rsqrt(cameraVectorLengthSquared)
        : float3(0.0, 0.0, 1.0);

    float3 scatteringSource = 0.0;
    float extinction = 0.0;
    [loop]
    for (uint volumeIndex = 0u; volumeIndex < DustVolumeCount; ++volumeIndex)
    {
        DustVolume volume = DustVolumes[volumeIndex];
        if (volume.enabled == 0u || volume.density <= 0.00001) continue;

        DustRotationContext rotation = DustBuildRotationContext(volume);
        float volumeDistanceFade = DustDistanceFade(
            volume, DustCameraPositionMaxDistance.xyz);
        if (volumeDistanceFade <= 0.0001) continue;
        float3 authoredHalfSize = max(volume.size * 0.5,
            float3(1.0, 1.0, 1.0));
        if (volume.shape == DUST_SHAPE_SPHERE)
        {
            float radius = max(1.0,
                (authoredHalfSize.x + authoredHalfSize.y + authoredHalfSize.z) /
                3.0);
            authoredHalfSize = float3(radius, radius, radius);
        }
        float smallestHalfAxis = min(authoredHalfSize.x,
            min(authoredHalfSize.y, authoredHalfSize.z));
        float outsideWorldDistance = DustConservativeOutsideWorld(
            volume, rotation, authoredHalfSize, smallestHalfAxis, worldPosition);
        if (outsideWorldDistance > 0.0)
        {
            float envelopeUpper = DustSoftEnvelope(
                -outsideWorldDistance / max(1.0, smallestHalfAxis));
            if (max(0.0, volume.density) * envelopeUpper * 1.75 <= 0.00001)
                continue;
        }

        uint wakeMask = (volume.flags >> DUST_WAKE_MASK_SHIFT) &
                        DUST_RELEVANCE_MASK;
        uint localLightMask = (volume.flags >> DUST_LOCAL_LIGHT_MASK_SHIFT) &
                              DUST_RELEVANCE_MASK;
        float density = DustDensityAt(
            volume, rotation, volume.seedOffset, wakeMask, worldPosition,
            sampleStepLength) * volumeDistanceFade;
        if (density <= 0.00001) continue;

        float coefficientScale = 0.00034 *
            DustMaterialCoefficientScale(volume);
        float sigmaT = density * max(0.00001,
            volume.absorption + volume.scattering) * coefficientScale;
        scatteringSource += DisplayToLinear(saturate(volume.color)) * sigmaT;
        extinction += sigmaT;
    }

    /* A direct procedural continuation now begins at the clipmap exit, so the
       cached medium must remain physically continuous through its last cell.
       Fading the cache itself was the distance-dependent disappearance. */
    DustFroxelFieldRW[cell] = float4(scatteringSource, extinction);
}
#endif

float3 ApplyVolumetricDustFroxel(float2 uv, float3 scene,
                                 float sceneDistance,
                                 out float outputTransmittance)
{
    outputTransmittance = 1.0;
    float3 rayVector = DustRayUv00.xyz +
        uv.x * (DustRayUv10.xyz - DustRayUv00.xyz) +
        uv.y * (DustRayUv01SunStrength.xyz - DustRayUv00.xyz);
    float3 rayDirection = normalize(rayVector);
    float3 rayOrigin = DustCameraPositionMaxDistance.xyz;
    float entryDistance;
    float exitDistance;
    if (!DustRayWorldGridInterval(rayOrigin, rayDirection,
                                  entryDistance, exitDistance))
        return scene;
    float startDistance = max(0.0, entryDistance);
    float fullEndDistance = exitDistance;
    float endDistance = min(fullEndDistance, sceneDistance);
    if (endDistance <= startDistance) return scene;

    float3 activeDimensions = DustWorldGridActiveDimensions();
    float3 voxelSize = DustGridSizeHeight.xyz / activeDimensions;
    float nominalStepLength = max(1.0,
        min(voxelSize.x, min(voxelSize.y, voxelSize.z)) * 0.90);

    /* IMPORTANT: choose the integration lattice from the full world-cache
       interval, never from opaque-scene depth. Previously a ship shortened the
       span, which changed every sample position/step length along that pixel.
       The cloud therefore changed numerically at the ship silhouette and could
       flip from dark/purple to bright/white with angle. Scene depth may stop
       the lattice, but it no longer defines the lattice. */
    float fullSpan = fullEndDistance - startDistance;
    const uint maximumIntegrationSteps = 192u;
    uint stepCount = max(1u, (uint)ceil(fullSpan / nominalStepLength));
    stepCount = min(stepCount, maximumIntegrationSteps);
    float integrationLength = fullSpan / max(1.0, (float)stepCount);

    float3 accumulated = 0.0;
    float transmittance = 1.0;
    [loop]
    for (uint stepIndex = 0u; stepIndex < stepCount; ++stepIndex)
    {
        float segmentStart = startDistance +
            (float)stepIndex * integrationLength;
        if (segmentStart >= endDistance) break;
        float segmentEnd = min(segmentStart + integrationLength, endDistance);
        float segmentLength = segmentEnd - segmentStart;
        if (segmentLength <= 0.000001) break;
        float sampleDistance = segmentStart + segmentLength * 0.5;
        float3 worldPosition = rayOrigin + rayDirection * sampleDistance;
        float3 textureCoordinate = DustWorldGridTextureCoordinate(worldPosition);
        float4 medium = DustFroxelField.SampleLevel(
            SourceSampler, textureCoordinate, 0.0);
        float extinction = max(0.0, medium.a);
        if (extinction <= 0.00000001) continue;
        float opticalDepth = extinction * segmentLength;
        float stepTransmittance = exp(-opticalDepth);
        float alpha = 1.0 - stepTransmittance;
        float3 sourceFunction = max(medium.rgb, 0.0) /
            max(extinction, 0.00000001);
        accumulated += transmittance * sourceFunction * alpha;
        transmittance *= stepTransmittance;
        if (transmittance < DUST_EARLY_OUT_TRANSMITTANCE) break;
    }
    accumulated = DustToneVolumetricContribution(accumulated, transmittance);
    outputTransmittance = transmittance;
    return max(scene * transmittance + accumulated, 0.0);
}

float3 DustFarGridSize()
{
    return DustGridSizeHeight.xyz * 4.0;
}

float3 DustFarGridMinimum()
{
    float3 nearCenter = DustGridMinWidth.xyz + DustGridSizeHeight.xyz * 0.5;
    return nearCenter - DustFarGridSize() * 0.5;
}

bool DustRayFarGridInterval(float3 rayOrigin, float3 rayDirection,
                            out float entryDistance, out float exitDistance)
{
    float3 boundsMin = DustFarGridMinimum();
    float3 boundsMax = boundsMin + DustFarGridSize();
    float tMin = 0.0;
    float tMax = 3.0e30;
    [unroll]
    for (uint axis = 0u; axis < 3u; ++axis)
    {
        if (abs(rayDirection[axis]) < 0.0000001)
        {
            if (rayOrigin[axis] < boundsMin[axis] ||
                rayOrigin[axis] > boundsMax[axis])
            {
                entryDistance = 0.0;
                exitDistance = 0.0;
                return false;
            }
            continue;
        }
        float inverseDirection = 1.0 / rayDirection[axis];
        float a = (boundsMin[axis] - rayOrigin[axis]) * inverseDirection;
        float b = (boundsMax[axis] - rayOrigin[axis]) * inverseDirection;
        tMin = max(tMin, min(a, b));
        tMax = min(tMax, max(a, b));
        if (tMax < tMin)
        {
            entryDistance = 0.0;
            exitDistance = 0.0;
            return false;
        }
    }
    entryDistance = tMin;
    exitDistance = tMax;
    return tMax > max(0.0, tMin);
}

float3 ApplyVolumetricDustFarFroxel(float2 uv, float3 scene,
                                    float sceneDistance,
                                    float minimumDistance)
{
    float3 rayVector = DustRayUv00.xyz +
        uv.x * (DustRayUv10.xyz - DustRayUv00.xyz) +
        uv.y * (DustRayUv01SunStrength.xyz - DustRayUv00.xyz);
    float3 rayDirection = normalize(rayVector);
    float3 rayOrigin = DustCameraPositionMaxDistance.xyz;
    float entryDistance;
    float exitDistance;
    if (!DustRayFarGridInterval(rayOrigin, rayDirection,
                                entryDistance, exitDistance))
        return scene;
    float startDistance = max(minimumDistance, max(0.0, entryDistance));
    float endDistance = min(exitDistance, sceneDistance);
    if (endDistance <= startDistance) return scene;

    uint width, height, depth;
    DustFarFroxelField.GetDimensions(width, height, depth);
    float3 gridSize = DustFarGridSize();
    float3 gridMinimum = DustFarGridMinimum();
    float3 voxelSize = gridSize /
        float3(max(1u, width), max(1u, height), max(1u, depth));
    float stepLength = max(1.0,
        min(voxelSize.x, min(voxelSize.y, voxelSize.z)) * 0.90);
    uint stepCount = min(128u, max(1u,
        (uint)ceil((endDistance - startDistance) / stepLength)));
    float integrationLength = (endDistance - startDistance) /
                              max(1.0, (float)stepCount);
    float3 accumulated = 0.0;
    float transmittance = 1.0;
    [loop]
    for (uint stepIndex = 0u; stepIndex < stepCount; ++stepIndex)
    {
        float sampleDistance = startDistance +
            ((float)stepIndex + 0.5) * integrationLength;
        float3 worldPosition = rayOrigin + rayDirection * sampleDistance;
        float3 gridUv = saturate((worldPosition - gridMinimum) / gridSize);
        float3 dimensions = float3(max(1u, width), max(1u, height),
                                   max(1u, depth));
        float3 cell = clamp(gridUv * dimensions, 0.5, dimensions - 0.5);
        float4 medium = DustFarFroxelField.SampleLevel(
            SourceSampler, cell / dimensions, 0.0);
        float extinction = max(0.0, medium.a);
        if (extinction <= 0.00000001) continue;
        float stepTransmittance = exp(-extinction * integrationLength);
        float alpha = 1.0 - stepTransmittance;
        float3 sourceFunction = max(medium.rgb, 0.0) /
            max(extinction, 0.00000001);
        accumulated += transmittance * sourceFunction * alpha;
        transmittance *= stepTransmittance;
        if (transmittance < DUST_EARLY_OUT_TRANSMITTANCE) break;
    }
    accumulated = DustToneVolumetricContribution(accumulated, transmittance);
    return max(scene * transmittance + accumulated, 0.0);
}

float DustVisibleSceneDistance(float2 uv, float3 rayVector)
{
    if (DustSunColor.w < 0.5) return DustCameraPositionMaxDistance.w;
    uint depthWidth, depthHeight;
    SceneDepth.GetDimensions(depthWidth, depthHeight);
    if (depthWidth == 0u || depthHeight == 0u) return DustCameraPositionMaxDistance.w;
    /* LegacyD3D12GL negates clip-space Y for BOTH color and depth. Its
       compatibility depth therefore uses presenter UVs directly, unlike the
       top-down cloud render target. Do not mirror the opaque silhouette. */
    float2 depthUv = uv;
    uint2 pixel = min(uint2(depthUv * float2(depthWidth, depthHeight)),
                      uint2(depthWidth - 1u, depthHeight - 1u));
    float depth = SceneDepth.Load(int3(pixel, 0));
    /* Background domes are deliberately drawn with depth testing disabled,
       but legacy/native state restoration can still leave their extreme-far
       raster depth in a small number of pixels.  Never terminate a volume on
       that far-plane fringe: doing so projects the dome's triangle topology
       into otherwise smooth dust as a large cross.  Real ships, asteroids and
       debris sit comfortably in front of this guard band. */
    if (depth >= 0.99990) return DustCameraPositionMaxDistance.w;

    /* Native raster converts OpenGL clip Z to D3D [0,1] with
       zD3D = 0.5*(zGL+w). Reconstruct the original GL NDC Z and solve the
       exact perspective equation using P[10]/P[14]. This is the actual visible
       raster depth, never DXR/colour/alpha inference. */
    float ndcZ = depth * 2.0 - 1.0;
    float denominator = ndcZ + DustRayUv00.w;
    if (abs(denominator) <= 0.000001) return DustCameraPositionMaxDistance.w;
    float viewDepth = DustRayUv10.w / denominator;
    if (viewDepth <= 0.0 || viewDepth >= DustCameraPositionMaxDistance.w)
        return DustCameraPositionMaxDistance.w;
    float rayDistance = viewDepth * length(rayVector);
    /* Bias termination slightly toward the camera to eliminate bright dust
       halos leaking around an opaque hull/asteroid due to depth quantization. */
    return max(0.0, rayDistance - max(4.0, rayDistance * 0.001));
}

bool DustRayVolumeInterval(DustVolume volume,
                           DustRotationContext rotation,
                           float3 rayOrigin, float3 rayDirection,
                           out float entryDistance,
                           out float exitDistance)
{
    entryDistance = 0.0;
    exitDistance = 0.0;
    /* DustSoftEnvelope is already below the integrator's useful precision by
       2.6 authored radii.  Intersect that conservative, rotated support so a
       ray begins before every visible tail and finishes after it.  Unlike the
       old iteration watchdog, these endpoints belong to the authored WORLD
       volume and cannot form a camera-facing cutoff plane. */
    const float supportRadius = volume.shape == 4u ? 1.0 : 2.6;
    float3 halfSize = max(volume.size * 0.5, float3(1.0, 1.0, 1.0));
    if (volume.shape == DUST_SHAPE_SPHERE)
    {
        float radius = max(1.0,
            (halfSize.x + halfSize.y + halfSize.z) / 3.0);
        halfSize = float3(radius, radius, radius);
    }
    float3 localOrigin = DustWorldToLocalVectorFast(
        rotation, rayOrigin - volume.position) / halfSize;
    float3 localDirection = DustWorldToLocalVectorFast(
        rotation, rayDirection) / halfSize;
    // Normalize the quadratic's units: a fixed 1e-12 cutoff on 1/size^2
    // incorrectly rejected valid rays as clouds grew beyond a million units.
    float directionScale = max(abs(localDirection.x),
        max(abs(localDirection.y), abs(localDirection.z)));
    localDirection /= max(directionScale, 1.0e-30);

    if (volume.shape == DUST_SHAPE_ROPE)
    {
        // Reject empty swept-line tails before native-resolution classification
        // or marching. Do NOT change the original interval below: contributing
        // rays retain their exact old sample lattice and integration lengths.
        float radius = max(1.0, min(halfSize.x, halfSize.y));
        float support = volume.seedPadding;
        float3 tightHalf = min(halfSize * supportRadius,
            float3(support, support, max(0.0, halfSize.z-radius)+support));
        float3 limit = tightHalf / halfSize;
        float tightMin = -3.0e30;
        float tightMax = 3.0e30;
        [unroll] for (uint axis=0u;axis<3u;++axis)
        {
            if (abs(localDirection[axis]) < 1.0e-10)
            {
                if (abs(localOrigin[axis]) > limit[axis])
                { return false; }
            }
            else
            {
                float a=(-limit[axis]-localOrigin[axis])/localDirection[axis];
                float b=( limit[axis]-localOrigin[axis])/localDirection[axis];
                tightMin=max(tightMin,min(a,b));
                tightMax=min(tightMax,max(a,b));
            }
        }
        if (tightMax <= max(0.0,tightMin))
        { return false; }
    }

    if (volume.shape != DUST_SHAPE_BOX && volume.shape != DUST_SHAPE_ROPE && volume.shape != 4u)
    {
        float a = dot(localDirection, localDirection);
        float b = dot(localOrigin, localDirection);
        float c = dot(localOrigin, localOrigin) -
                  supportRadius * supportRadius;
        float discriminant = b * b - a * c;
        if (a <= 1.0e-12 || discriminant < 0.0)
        {
            entryDistance = 0.0;
            exitDistance = 0.0;
            return false;
        }
        float root = sqrt(discriminant);
        float q = -b - (b >= 0.0 ? root : -root);
        float t0 = abs(q) > 1.0e-20 ? q / a : -b / a;
        float t1 = abs(q) > 1.0e-20 ? c / q : t0;
        entryDistance = min(t0, t1) / directionScale;
        exitDistance = max(t0, t1) / directionScale;
        return exitDistance > max(0.0, entryDistance);
    }

    float tMin = -3.0e30;
    float tMax = 3.0e30;
    [unroll]
    for (uint axis = 0u; axis < 3u; ++axis)
    {
        if (abs(localDirection[axis]) < 1.0e-10)
        {
            if (abs(localOrigin[axis]) > supportRadius)
            {
                entryDistance = 0.0;
                exitDistance = 0.0;
                return false;
            }
            continue;
        }
        float inverseDirection = 1.0 / localDirection[axis];
        float a = (-supportRadius - localOrigin[axis]) * inverseDirection;
        float b = ( supportRadius - localOrigin[axis]) * inverseDirection;
        tMin = max(tMin, min(a, b));
        tMax = min(tMax, max(a, b));
    }
    entryDistance = tMin / directionScale;
    exitDistance = tMax / directionScale;
    return tMax > max(0.0, tMin);
}

float DustProceduralSunVisibility(float3 worldPosition, bool shafts = false)
{
    if (PresenterPadding.y != 3.0) return 1.0;
    float3 center = DustGridMinWidth.xyz + DustGridSizeHeight.xyz*0.5;
    float3 u,v;
    DustSunBasis(u,v);
    float3 offset = worldPosition-center;
    float3 local = float3(dot(offset,u),dot(offset,v),
        dot(offset,normalize(DustSunDirectionValid.xyz)));
    float3 nearUv = local/max(DustGridSizeHeight.xyz,0.0001)+0.5;
    // Blend coverage at cascade edges; a hard near/far switch changes the
    // filtered silhouette instantly when the camera resizes the light grid.
    float3 farUv = local/(DustGridSizeHeight.xyz*4.0)+0.5;
    float farVisibility = 1.0;
    if (all(farUv >= 0.0) && all(farUv <= 1.0))
    {
        float4 farSample = DustFarFroxelField.SampleLevel(SourceSampler,
            clamp(farUv,0.00625,0.99375),0);
        float farEdge = max(abs(farUv.x-0.5),
            max(abs(farUv.y-0.5),abs(farUv.z-0.5)))*2.0;
        farVisibility = lerp(saturate(shafts ? farSample.r : farSample.a),
            1.0,smoothstep(0.8,1.0,farEdge));
    }
    if (all(nearUv >= 0.0) && all(nearUv <= 1.0))
    {
        uint width,height,depth;
        DustFroxelField.GetDimensions(width,height,depth);
        float3 active = float3(DustGridMinWidth.w,DustGridSizeHeight.w,PresenterPadding.x);
        float4 visibility = DustFroxelField.SampleLevel(SourceSampler,
            clamp(nearUv*active,0.5,active-0.5)/float3(width,height,depth),0);
        float nearEdge = max(abs(nearUv.x-0.5),
            max(abs(nearUv.y-0.5),abs(nearUv.z-0.5)))*2.0;
        return lerp(saturate(shafts ? visibility.r : visibility.a),
            farVisibility,smoothstep(0.8,1.0,nearEdge));
    }
    return farVisibility;
}

// Resolve visibility at the light-grid scale, independently of the much
// coarser density march. A point sample held across a density cell projects
// repeated silhouettes when a view ray crosses a narrow ship shadow.
float DustShadowStepLength(float3 rayDirection)
{
    float3 u,v;
    DustSunBasis(u,v);
    float3 cellSize = DustGridSizeHeight.xyz /
        max(float3(DustGridMinWidth.w,DustGridSizeHeight.w,PresenterPadding.x),1.0);
    float3 rate = abs(float3(dot(rayDirection,u),dot(rayDirection,v),
        dot(rayDirection,normalize(DustSunDirectionValid.xyz)))) / max(cellSize,0.001);
    return 0.5 / max(max(rate.x,rate.y),max(rate.z,0.000001));
}

float DustSegmentSunVisibility(float3 center, float3 rayDirection, float length)
{
    if (PresenterPadding.y != 3.0) return 1.0;
    uint count = (uint)clamp(ceil(length / DustShadowStepLength(rayDirection)),1.0,128.0);
    float visibility = 0.0;
    [loop] for (uint i=0u;i<count;++i)
        visibility += DustProceduralSunVisibility(center + rayDirection *
            ((((float)i+0.5)/(float)count-0.5)*length));
    return visibility/(float)count;
}

float3 AtmosphericSunScattering(float3 rayOrigin, float3 rayDirection,
                                float sceneDistance)
{
    if (GodRays <= 0.0001 || DustSunDirectionValid.w < 0.5 ||
        PresenterPadding.y != 3.0) return 0.0;
    // Occlusion is local to each world-space sample. Covering the sun's screen
    // position must not extinguish the illuminated shafts beside a silhouette.
    // The phase is continuous: no per-pixel cone can cut rays into a disk.
    float cosine = dot(normalize(DustSunDirectionValid.xyz),normalize(rayDirection));
    /* The existing phase response is exactly zero away from the sun. Reject
       that case BEFORE the visibility march, not after dozens of samples. */
    float phaseFloor = DustPhaseHG(0.35,0.0);
    float directionalPhase = saturate(
        (DustPhaseHG(0.35,cosine)-phaseFloor) /
        max(0.0001,DustPhaseHG(0.35,1.0)-phaseFloor));
    if (directionalPhase <= 0.0) return 0.0;
    // Integrate a soft spherical region around the viewed fleet, not merely
    // the first 20k units from the camera. Zoom must not leave the ships and
    // their shadows behind. This adds light only, never scene extinction.
    float3 center = DustGridMinWidth.xyz + DustGridSizeHeight.xyz*0.5;
    float radius = DustGridSizeHeight.x*0.48;
    float3 offset = rayOrigin-center;
    float b = dot(offset,rayDirection);
    float discriminant = b*b-dot(offset,offset)+radius*radius;
    if (discriminant <= 0.0) return 0.0;
    float root = sqrt(discriminant);
    float entry = max(0.0,-b-root);
    float exit = min(max(0.0,sceneDistance),-b+root);
    if (exit <= entry) return 0.0;
    uint shadowSteps = (uint)clamp(ceil((exit-entry) /
        DustShadowStepLength(rayDirection)),1.0,1024.0);
    float stepLength = (exit-entry)/(float)shadowSteps;
    float contrastLength = 0.0;
    float3 sunDirection = normalize(DustSunDirectionValid.xyz);
    float3 referenceAxis = abs(sunDirection.z)<0.9 ? float3(0,0,1) : float3(0,1,0);
    float3 shaftU = normalize(cross(sunDirection,referenceAxis));
    float3 shaftV = cross(sunDirection,shaftU);
    // Subtract the locally smooth scattering pedestal already represented by
    // sky lighting. Only excess light alongside a geometry-shadowed beam is
    // added. An entirely unoccluded field contributes EXACTLY zero, so raising
    // beam contrast cannot wash the whole image out again.
    float referenceRadius = min(radius*0.25,
        DustGridSizeHeight.x/max(DustGridMinWidth.w,1.0)*1.5);
    [loop] for (uint step=0u; step<shadowSteps; ++step)
    {
        float3 p = rayOrigin + rayDirection*(entry+((float)step+0.5)*stepLength);
        float envelope = 1.0-smoothstep(0.65,1.0,length(p-center)/radius);
        if (envelope <= 0.0) continue;
        float visibility = DustProceduralSunVisibility(p,true);
        // Reference visibility is nonnegative: a fully blocked sample cannot
        // contribute positive contrast. Skip four lookups with exactly zero loss.
        if (visibility <= 0.0) continue;
        float reference = 0.25 * (
            DustProceduralSunVisibility(p+shaftU*referenceRadius,true) +
            DustProceduralSunVisibility(p-shaftU*referenceRadius,true) +
            DustProceduralSunVisibility(p+shaftV*referenceRadius,true) +
            DustProceduralSunVisibility(p-shaftV*referenceRadius,true));
        contrastLength += max(0.0,visibility-reference)*envelope*stepLength;
    }
    // No isotropic pedestal: looking away from the source adds zero haze.
    float3 sourceColor = max(DustSunColor.rgb,0.0);
    float sourcePeak = max(sourceColor.r,max(sourceColor.g,sourceColor.b));
    float sourceResponse = 1.0-exp(-sourcePeak *
        max(DustRayUv01SunStrength.w,0.0)*0.25);
    float beamResponse = 1.0-exp(-contrastLength/max(radius*0.08,0.0001));
    float3 sourceTint = sourceColor/max(sourcePeak,0.0001);
    // Packed mission tint reuses the flag word; no root-signature growth.
    if ((GodRaySourceExternal & 4u) != 0u)
        sourceTint = float3((GodRaySourceExternal >> 8u)&255u,
            (GodRaySourceExternal >> 16u)&255u,(GodRaySourceExternal >> 24u)&255u)/255.0;
    // This field is SCENE-LINEAR. The presenter converts it to display once.
    // Encoding this contribution separately lifted even small energies into
    // a white veil. Bound energy even with extremely bright authored keys.
    return sourceTint * (0.18 * sourceResponse * max(GodRays,0.0) *
        directionalPhase * beamResponse);
}

float3 ApplyVolumetricDust(float2 uv, float3 scene, bool forceDirect,
                           out float outputTransmittance)
{
    outputTransmittance = 1.0;
    if (DustRenderEnabled == 0u) return scene;

    /* HARD WORLD-LOCK FIX: the three presenter-UV camera-plane vectors are
       rebuilt from the ACTUAL world render view matrix each frame inside
       ModernGraphics.cpp.  The legacy external corner-ray callback may still
       run for compatibility, but it is overwritten before dust presentation. */
    float3 rayVector = DustRayUv00.xyz +
        uv.x * (DustRayUv10.xyz - DustRayUv00.xyz) +
        uv.y * (DustRayUv01SunStrength.xyz - DustRayUv00.xyz);
    float3 rayDirection = normalize(rayVector);
    float3 rayOrigin = DustCameraPositionMaxDistance.xyz;
    /* Screen-space quarter-resolution rays cover a larger world footprint as
       FOV changes. Feed that continuous footprint into density filtering so
       zooming changes resolved detail smoothly instead of making fine noise
       appear to pop between ray samples. Derivatives are evaluated here,
       before any divergent volume/march control flow. */
    uint dustWidth, dustHeight;
    DustScreenField.GetDimensions(dustWidth, dustHeight);
    if (forceDirect) SceneDepth.GetDimensions(dustWidth, dustHeight);
    // Analytic footprint also works in selective edge-resolve branches,
    // where screen derivatives across divergent pixel quads are undefined.
    float angularPixelFootprint = max(
        length(normalize(rayVector + (DustRayUv10.xyz - DustRayUv00.xyz) /
            max(1u, dustWidth)) - rayDirection),
        length(normalize(rayVector + (DustRayUv01SunStrength.xyz - DustRayUv00.xyz) /
            max(1u, dustHeight)) - rayDirection));
    /* The hard world lock remains authoritative for where density exists.
       Geometry now affects only the far integration distance, using the exact
       D32 depth attachment produced by the visible native raster pass. */
    float sceneDistance = DustVisibleSceneDistance(uv, rayVector);
    float3 atmosphericLight = AtmosphericSunScattering(
        rayOrigin,rayDirection,sceneDistance);

    /* The high-resolution clipmap owns the near field. Beyond its ray exit,
       continue with the exact procedural marcher so distant clouds remain
       visible without diluting near-field cache resolution. */
    bool froxelAvailable = PresenterPadding.y == 1.0 && !forceDirect;
    float nearExitDistance = 0.0;
    if (froxelAvailable)
    {
        float cacheEntryDistance;
        float cacheExitDistance;
        if (DustRayWorldGridInterval(rayOrigin, rayDirection,
                                     cacheEntryDistance, cacheExitDistance))
            nearExitDistance = max(0.0, cacheExitDistance);
    }

    float3 accumulated = 0.0;
    float transmittance = 1.0;
    uint2 tile = min(uint2(saturate(uv) * float2(16, 9)), uint2(15, 8));
    uint tileBase = (tile.y * 16u + tile.x) * 8u;
    uint remaining[8];
    [unroll] for(uint word=0u;word<8u;++word)remaining[word]=DustTileMasks[tileBase+word];
    [loop]
    for (uint candidateIndex = 0u; candidateIndex < DustVolumeCount; ++candidateIndex)
    {
        // The existing per-volume cutoff already discards all later volumes.
        // Stop before sorting them; preserve its exact threshold and image.
        if (transmittance < 0.008) break;
        // Composite bricks in this pixel ray's entry order, independent of
        // camera-distance sorting and the packed density-grid offsets.
        uint volumeIndex=0xffffffffu;
        float nearestEntry=3.0e30,nearestExit=0.0;
        float selectedEntry=0.0,selectedExit=0.0;
        [loop] for(uint word=0u;word<8u;++word){
            uint pending=remaining[word];
            [loop] while(pending!=0u){
                uint bit=firstbitlow(pending),index=word*32u+bit;
                pending &= pending-1u;
                if(index>=DustVolumeCount){remaining[word]&=~(1u<<bit);continue;}
                DustVolume candidate=DustVolumes[index];float entry,exit;
                if(candidate.enabled==0u||candidate.density<=0.00001||
                   !DustRayVolumeInterval(candidate,DustBuildRotationContext(candidate),rayOrigin,rayDirection,entry,exit)||
                   min(sceneDistance,exit)<=max(0.0,entry)){
                    remaining[word]&=~(1u<<bit);continue;
                }
                float clippedEntry=max(0.0,entry);
                if(clippedEntry<nearestEntry || (clippedEntry==nearestEntry&&exit<nearestExit)){
                    nearestEntry=clippedEntry;nearestExit=exit;volumeIndex=index;
                    selectedEntry=entry;selectedExit=exit;
                }
            }
        }
        if(volumeIndex==0xffffffffu)break;
        remaining[volumeIndex/32u]&=~(1u<<(volumeIndex%32u));
        DustVolume volume = DustVolumes[volumeIndex];
        if (volume.enabled == 0u || volume.density <= 0.00001 ||
            transmittance < 0.008) continue;
        // The editor color is the cloud's actual color, independent of scene
        // illumination. Convert once per volume, not once per march sample.
        float3 authoredCloudColor = DisplayToLinear(saturate(volume.color));
        /* Rotation and hash-derived phase are invariant for every ray sample
           in this volume. RTX-AAA computes them once instead of repeating
           trigonometry/hash work in density and shadow probes. */
        DustRotationContext rotation = DustBuildRotationContext(volume);
        // Reject empty/geometry-occluded support intervals before evaluating
        // distance fade, material cadence, noise phase or lighting invariants.
        // Selection tested this exact volume/ray already. Keep its raw entry
        // (which can be negative inside the volume), not the sort's clipped key.
        float volumeEntryDistance = selectedEntry;
        float volumeExitDistance = selectedExit;
        float startDistance = max(
            froxelAvailable ? nearExitDistance : 0.0,
            max(0.0, volumeEntryDistance));
        float endDistance = min(sceneDistance, volumeExitDistance);
        if (endDistance <= startDistance) continue;
        float volumeDistanceFade = DustDistanceFade(volume, rayOrigin);
        if (volumeDistanceFade <= 0.0001) continue;
        float3 seedOffset = volume.seedOffset;
        // Finite-volume adaptation of OpenMW's logarithmic ray integration.
        // Every quality setting spans the complete clipped interval, including
        // camera-inside rays and very large particle bricks. No step-budget
        // truncation, camera-dependent noise phase, or changing cloud bounds.
        // Map the complete editor range to 32..256 steps; no dead high-end
        // settings caused by hitting the budget before the slider reaches 10.
        uint segmentCount = (uint)clamp(ceil(64.0 *
            OpenMWCloudSamplingFactor(volume.openmwControl.x * 0.62125674)), 24.0, 256.0);
        float extinctionCoefficient = max(0.00001,
            volume.absorption + volume.scattering) * 0.00034 * DustMaterialCoefficientScale(volume);
        float segmentStart = startDistance;
        [loop] for (uint stepIndex=0u; stepIndex<segmentCount; ++stepIndex)
        {
            if (segmentStart >= endDistance) break;
            // Opaque geometry clips the integration, never its underlying
            // sample lattice. A passing ship cannot redistribute cloud noise
            // or change the footprint of every sample in front of its hull.
            float latticeEnd = stepIndex+1u == segmentCount ? volumeExitDistance :
                OpenMWCloudSegmentBoundary(startDistance,volumeExitDistance,stepIndex+1u,segmentCount);
            float segmentEnd = min(endDistance,latticeEnd);
            float integrationLength = max(0.0,segmentEnd-segmentStart);
            float currentDistance = (segmentStart+segmentEnd)*0.5;
            float3 samplePosition = rayOrigin+rayDirection*currentDistance;
            float footprint = max(integrationLength,currentDistance*angularPixelFootprint);
            float density = DustDensityAt(volume,rotation,seedOffset,0u,samplePosition,footprint)*volumeDistanceFade;
            if (density > 0.00001 && integrationLength > 0.0)
            {
                    float stepTransmittance = OpenMWCloudTransmission(density*extinctionCoefficient,integrationLength);
                    float alpha = 1.0-stepTransmittance;
                    float3 inScattering = authoredCloudColor*alpha;
                    accumulated += transmittance * inScattering;
                    transmittance *= stepTransmittance;
                    if (transmittance < DUST_EARLY_OUT_TRANSMITTANCE) break;
            }
            segmentStart = segmentEnd;
        }
    }
    /* The presenter is display-referred. A gentle shoulder keeps bright local
       lights photographic without turning dense dust into a flat white blob. */
    accumulated = DustToneVolumetricContribution(accumulated, transmittance);
    float3 composite = max(scene * transmittance + accumulated +
        atmosphericLight * transmittance, 0.0);
    if (froxelAvailable)
    {
        float nearTransmittance;
        composite = ApplyVolumetricDustFroxel(
            uv, composite, sceneDistance, nearTransmittance);
        outputTransmittance = transmittance * nearTransmittance;
        return composite;
    }
    outputTransmittance = transmittance;
    return composite;
}

float4 DustScreenPS(VertexOutput input) : SV_Target
{
    float transmittance;
    float3 scattering = ApplyVolumetricDust(
        input.texcoord, 0.0, false, transmittance);
    /* A volume has no single surface depth. Reprojecting it with opaque-scene
       motion vectors produced copied cloud fragments, so temporal reuse stays
       disabled. */
    return float4(scattering, saturate(transmittance));
}

bool TryReuseClearAtmosphere(float2 uv, out float3 scattering)
{
    scattering = 0.0;
    if ((GodRaySourceExternal & 16u) == 0u) return false;
    uint width,height,depthWidth,depthHeight;
    DustScreenField.GetDimensions(width,height);
    SceneDepth.GetDimensions(depthWidth,depthHeight);
    float2 fieldUv = float2(uv.x,1.0-uv.y);
    float2 pixel = fieldUv * float2(width,height)-0.5;
    int2 first = int2(floor(pixel));
    float2 fraction = frac(pixel);
    uint2 centerPixel = min(uint2(saturate(uv)*float2(depthWidth,depthHeight)),
        uint2(depthWidth-1u,depthHeight-1u));
    float centerDepth = SceneDepth.Load(int3(centerPixel,0));
    bool centerGeometry = centerDepth < 0.99990;
    float centerDenominator = centerDepth*2.0-1.0+DustRayUv00.w;
    // Reuse only a complete clear-medium, depth-compatible bilinear footprint.
    // Any foreground/background boundary, thin hull, or cloud sample falls
    // back to the original native-ray integration. No temporal history is used.
    [unroll] for (uint y=0u;y<2u;++y)
    {
        [unroll] for (uint x=0u;x<2u;++x)
        {
            int2 samplePixel = clamp(first+int2(x,y),int2(0,0),int2(width,height)-1);
            float4 value = DustScreenField.Load(int3(samplePixel,0));
            if (value.a < 1.0) return false;
            float2 sampleUv = (float2(samplePixel)+0.5)/float2(width,height);
            uint2 depthPixel = min(uint2(float2(sampleUv.x,1.0-sampleUv.y)*
                float2(depthWidth,depthHeight)),uint2(depthWidth-1u,depthHeight-1u));
            float sampleDepth = SceneDepth.Load(int3(depthPixel,0));
            bool sampleGeometry = sampleDepth < 0.99990;
            float sampleDenominator = sampleDepth*2.0-1.0+DustRayUv00.w;
            float relativeError = abs(centerDenominator-sampleDenominator)/
                max(abs(centerDenominator),0.000001);
            if (sampleGeometry != centerGeometry || (centerGeometry && relativeError >= 0.001))
                return false;
            float weight = (x==0u ? 1.0-fraction.x : fraction.x)*
                           (y==0u ? 1.0-fraction.y : fraction.y);
            scattering += value.rgb*weight;
        }
    }
    return true;
}

float4 FilteredDustScreen(float2 uv)
{
    // Harvestable dust is sparse and temporary. Resolve its pixels at native
    // resolution instead of magnifying quarter-resolution cells at distance.
    // Re-evaluate the complete sorted medium on these pixels so overlapping
    // map clouds retain their established depth/order and ship occlusion.
    float3 nativeRay = DustRayUv00.xyz +
        uv.x * (DustRayUv10.xyz - DustRayUv00.xyz) +
        uv.y * (DustRayUv01SunStrength.xyz - DustRayUv00.xyz);
    float3 nativeDirection = normalize(nativeRay);
    float nativeSceneDistance = DustVisibleSceneDistance(uv,nativeRay);
    uint2 nativeTile = min(uint2(saturate(uv)*float2(16,9)),uint2(15,8));
    uint nativeTileBase = (nativeTile.y*16u+nativeTile.x)*8u;
    bool intersectsMedium = false;
    [loop] for (uint word=0u;word<8u;++word)
    {
        uint candidates= DustTileMasks[nativeTileBase+word];
        [loop] while(candidates!=0u)
        {
            uint index=word*32u+firstbitlow(candidates);
            candidates &= candidates-1u;
            if(index>=DustVolumeCount) continue;
            DustVolume volume=DustVolumes[index];
            if(volume.enabled==0u || volume.density<=0.00001) continue;
            float entry,exit;
            if(DustRayVolumeInterval(volume,DustBuildRotationContext(volume),
                DustCameraPositionMaxDistance.xyz,nativeDirection,entry,exit) &&
               min(exit,nativeSceneDistance)>max(0.0,entry))
            {
                intersectsMedium = true;
                // Bit 1 of enabled is runtime-only harvestable classification.
                // Keep harvestable dust native-detail and preserve edge repair.
                if ((volume.enabled&2u)!=0u)
                {
                    float transmission;
                    float3 scattering=ApplyVolumetricDust(uv,0.0,true,transmission);
                    return float4(scattering,transmission);
                }
            }
        }
    }
    /* A close hull/hangar can terminate the ray before any cloud. No medium
       on that exact ray segment means identity compositing, even at a depth
       discontinuity. Avoid both the filter and its expensive native remarch
       rather than repairing nonexistent foreground fog. */
    if (!intersectsMedium)
    {
        float3 cachedAtmosphere;
        if (TryReuseClearAtmosphere(uv,cachedAtmosphere))
            return float4(cachedAtmosphere,1.0);
        return float4(AtmosphericSunScattering(DustCameraPositionMaxDistance.xyz,
            nativeDirection,nativeSceneDistance),1.0);
    }
    uint width, height;
    uint depthWidth, depthHeight;
    DustScreenField.GetDimensions(width, height);
    SceneDepth.GetDimensions(depthWidth, depthHeight);
    float2 texel = 1.0 / float2(max(1u, width), max(1u, height));
    uint2 dimensions = uint2(max(1u, width), max(1u, height));
    // Render targets are top-down; presenter UVs are bottom-up. Cloud color
    // and the depth used to generate it must address the SAME screen pixel.
    float2 fieldUv = float2(uv.x, 1.0 - uv.y);
    int2 centerPixel = int2(min(uint2(fieldUv * float2(dimensions)),
                                dimensions - 1u));
    float2 depthUv = uv;
    uint2 centerDepthPixel = min(uint2(depthUv * float2(depthWidth, depthHeight)),
        uint2(depthWidth - 1u, depthHeight - 1u));
    float centerDepth = SceneDepth.Load(int3(centerDepthPixel, 0));
    bool centerHasGeometry = centerDepth < 0.99990;
    // A compatible coarse sample is not necessarily the correct fog at an
    // opaque silhouette. Resolve the whole depth-discontinuity footprint at
    // native resolution, on BOTH sides, rather than changing filter weights
    // as the silhouette moves through quarter-resolution cells.
    bool resolveNativeEdge = false;
    [unroll] for (int edgeY = -1; edgeY <= 1; ++edgeY)
    {
        [unroll] for (int edgeX = -1; edgeX <= 1; ++edgeX)
        {
            int2 edgePixel = clamp(int2(centerDepthPixel) + int2(edgeX, edgeY),
                int2(0, 0), int2(depthWidth, depthHeight) - 1);
            float edgeDepth = SceneDepth.Load(int3(edgePixel, 0));
            resolveNativeEdge = resolveNativeEdge ||
                ((edgeDepth < 0.99990) != centerHasGeometry);
        }
    }
    // Throughout the complete filter footprint, every candidate passes depth,
    // including the center. The old unconditional center weight retained
    // background fog on foreground hull pixels.
    float4 total = 0.0;
    float totalWeight = 0.0;
    const float2 offsets[9] = {
        float2(-1.0,  0.0), float2(1.0,  0.0),
        float2( 0.0, -1.0), float2(0.0,  1.0),
        float2(-1.0, -1.0), float2(1.0, -1.0),
        float2(-1.0,  1.0), float2(1.0,  1.0), float2(0.0, 0.0)};
    [unroll]
    for (uint index = 0u; index < 9u; ++index)
    {
        float2 offset = offsets[index];
        int2 samplePixel = clamp(centerPixel + int2(offset),
                                 int2(0, 0), int2(dimensions) - 1);
        float2 sampleUv = (float2(samplePixel) + 0.5) * texel;
        float4 sampleValue = DustScreenField.Load(int3(samplePixel, 0));
        uint2 depthPixel = min(uint2(float2(sampleUv.x, 1.0 - sampleUv.y) *
            float2(depthWidth, depthHeight)), uint2(depthWidth - 1u, depthHeight - 1u));
        float sampleDepth = SceneDepth.Load(int3(depthPixel, 0));
        // Test discrete samples BEFORE blending. Bilinear prefiltering can
        // already mix background and hull fog and cannot be undone afterward.
        float depthWeight;
        {
            bool sampleHasGeometry = sampleDepth < 0.99990;
            float centerDenominator = centerDepth * 2.0 - 1.0 + DustRayUv00.w;
            float sampleDenominator = sampleDepth * 2.0 - 1.0 + DustRayUv00.w;
            float relativeDepthError = abs(centerDenominator - sampleDenominator) /
                max(abs(centerDenominator), 0.000001);
            resolveNativeEdge = resolveNativeEdge ||
                sampleHasGeometry != centerHasGeometry ||
                (centerHasGeometry && relativeDepthError >= 0.02);
            depthWeight = sampleHasGeometry == centerHasGeometry &&
                (!centerHasGeometry || relativeDepthError < 0.02) ? 1.0 : 0.0;
        }
        float2 pixelDelta = float2(samplePixel) + 0.5 - fieldUv * float2(dimensions);
        float spatialWeight = exp(-2.0 * dot(pixelDelta, pixelDelta));
        float weight = depthWeight * spatialWeight;
        total += sampleValue * weight;
        totalWeight += weight;
    }
    if (!resolveNativeEdge && totalWeight > 0.0001) return total / totalWeight;
    // Thin geometry can have no compatible low-resolution sample. Resolve
    // just that pixel rather than borrowing fog from behind the object.
    float transmission;
    float3 scattering = ApplyVolumetricDust(uv, 0.0, true, transmission);
    return float4(scattering, transmission);
}

float4 PresentPS(VertexOutput input) : SV_Target
{
    /* The Streamline prepass is HUD-less. The display pass samples SourceFrame
       so classic UI remains native-resolution and outside temporal history. */
    float4 source = ScenePrepass != 0
        ? WorldFrame.Sample(SourceSampler, input.texcoord)
        : SourceFrame.Sample(SourceSampler, input.texcoord);
    if (SceneCompositeEnabled != 0)
    {
        float4 world = WorldFrame.Sample(SourceSampler, input.texcoord);
        // Native alpha records foreground coverage, not a visible UI change.
        // Treating an alpha-only change as UI punched rectangular holes in
        // the complete scene/cloud composite. Preserve actual UI by RGB.
        float3 difference = abs(source.rgb - world.rgb);
        float largestDifference = max(max(difference.r, difference.g), difference.b);
        /* The prepass deliberately renders every scene pixel. The final pass
           preserves pixels changed after the world snapshot as native UI. */
        if (ScenePrepass != 0 || largestDifference <= (1.5 / 255.0))
        {
            /* Streamline resources use native D3D top-down texture
               orientation. The presenter's TEXCOORD convention is inverted
               for legacy/native-raster snapshots, so sample the reconstructed
               Streamline output through the matching top-down UV. */
            float2 streamlineUv = float2(input.texcoord.x,
                                         1.0 - input.texcoord.y);
            float3 scene = StreamlineResolvedScene != 0 && ScenePrepass != 1
                ? FilteredWorld.Sample(SourceSampler, streamlineUv).rgb
                : SceneSample(input.texcoord);
            if (StreamlineResolvedScene == 0 && AntiAliasingMode == 2)
            {
                scene = FxaaScene(input.texcoord);
            }
            if (StreamlineResolvedScene == 0 || ScenePrepass == 1)
            {
            if (MotionBlur > 0.0001 && RayOverlayEnabled != 0)
            {
                uint motionWidth;
                uint motionHeight;
                MotionVectors.GetDimensions(motionWidth, motionHeight);
                /* Motion guidance is native D3D top-down. Convert it to the
                   presenter's legacy bottom-up UV convention only for this
                   raster post effect; Streamline consumes the stored field
                   directly. */
                float2 motionUv = float2(input.texcoord.x,
                                         1.0 - input.texcoord.y);
                float2 motion = MotionVectors.Sample(
                    SourceSampler, motionUv).xy /
                    float2(max(1u, motionWidth), max(1u, motionHeight));
                motion.y = -motion.y;
                motion *= MotionBlur;
                float motionLimit = 32.0 / max(1.0, (float)motionWidth);
                motion = clamp(motion, -motionLimit, motionLimit);
                float3 blurred = 0.0;
                [unroll]
                for (uint sampleIndex = 0; sampleIndex < 7; ++sampleIndex)
                {
                    float along = ((float)sampleIndex / 6.0) - 0.5;
                    blurred += SceneSample(input.texcoord + motion * along);
                }
                scene = lerp(scene, blurred / 7.0, MotionBlur);
            }

            float rayCoverage = 0.0;
            float emissiveSurfaceMask = 0.0;
            if (RayOverlayEnabled != 0)
            {
                float4 ray = FilteredRayLighting(input.texcoord);
                if (ray.a >= 0.0)
                {
                    rayCoverage = 1.0;
                    /* Preserve texture and team colors while replacing most
                       fixed-function light with visibility-tested DXR. */
                    /* RayLighting is already tone mapped.  Keep its hybrid
                       modulation bounded so indirect variance cannot become a
                       large exposure jump in the native raster image. */
                    float3 unshadowedRaster = scene;
                    float3 rayFactor = 0.06 + 1.58 * ray.rgb;
                    float3 shadowedScene =
                        scene * rayFactor + 0.015 * ray.rgb;

                    /* Foreground additive/emissive FX are raster effects, not
                       shadow receivers.  When a trail/beam is drawn over an
                       opaque ship, the DXR primary ray still hits that ship;
                       blindly applying its shadow factor to the completed
                       raster pixel makes the luminous FX darken with the hull.
                       Compare the raster result against the ray-space primary
                       albedo and protect only energy that sits well above a
                       generously lit opaque-surface ceiling.  Ordinary hull
                       texture/light variation remains DXR-shadowed. */
                    float2 guideUv = float2(input.texcoord.x,
                                             1.0 - input.texcoord.y);
                    float3 primaryAlbedo = PrimaryAlbedoGuide.Sample(
                        SourceSampler, guideUv).rgb;
                    emissiveSurfaceMask = FilteredEmissionProtection(guideUv);
                    float3 opaqueCeiling = primaryAlbedo * 1.65 + 0.08;
                    float3 fxExcess = max(unshadowedRaster - opaqueCeiling, 0.0);
                    float scenePeak = max(unshadowedRaster.r,
                        max(unshadowedRaster.g, unshadowedRaster.b));
                    float sceneFloor = min(unshadowedRaster.r,
                        min(unshadowedRaster.g, unshadowedRaster.b));
                    float fxPeak = max(fxExcess.r,
                        max(fxExcess.g, fxExcess.b));
                    float fxLuma = Luminance(fxExcess);
                    float sceneLuma = Luminance(unshadowedRaster);
                    float chroma = scenePeak - sceneFloor;
                    float coloredFx =
                        smoothstep(0.08, 0.28, fxPeak) *
                        smoothstep(0.10, 0.30, chroma) *
                        smoothstep(0.52, 0.82, scenePeak);
                    float whiteHotFx =
                        smoothstep(0.12, 0.34, fxLuma) *
                        smoothstep(0.78, 0.96, sceneLuma);
                    float emissiveFxProtection =
                        max(saturate(max(coloredFx, whiteHotFx)),
                            NativeTrailCoreMask(RasterSceneUv(input.texcoord)));
                    scene = lerp(shadowedScene, unshadowedRaster,
                                 max(emissiveFxProtection,emissiveSurfaceMask));
                }
            }

            /* Screen-space radial shafts removed: sunlight now scatters in
               the world-space dust grid, with geometry visibility. */

            if (DustRenderEnabled != 0u)
            {
                float4 dust = FilteredDustScreen(RasterSceneUv(input.texcoord));
                float transmission = saturate(dust.a);
                float3 medium = max(dust.rgb, 0.0);
                /* Apply extinction to scene-linear radiance. Multiplying the
                   display/gamma encoded BTG directly exaggerates tiny vertex
                   interpolation differences that are invisible in the clean
                   sky and makes its triangulation appear through a cloud. */
                float3 linearScene = DisplayToLinear(scene);
                scene = LinearToSrgb(max(linearScene * transmission + medium, 0.0));
            }

            // Glow remains self-lit, but foreground dust attenuates it along
            // with the hull. Never restore the whole raster pixel after dust.

            if (Bloom > 0.0001)
            {
                /* Scene-only bloom: this branch runs only for pixels that
                   still match the pre-UI world capture, so menus/HUD text are
                   never blurred or brightened. 100% is intentionally modest;
                   the F12 control can drive the multiplier to 400%. */
                scene += BloomScene(input.texcoord) * (0.34 * Bloom);
            }

            if (ChromaticAberration > 0.0001)
            {
                float2 radial = input.texcoord - 0.5;
                float2 offset = radial * (0.006 * ChromaticAberration);
                float3 separated;
                separated.r = SceneSample(input.texcoord + offset).r;
                separated.g = scene.g;
                separated.b = SceneSample(input.texcoord - offset).b;
                scene = lerp(scene, separated, ChromaticAberration);
            }

            }

            /* Optical lens ghosts are camera effects, not atmospheric shafts.
               Final display only: neither AA history nor the cloud field
               receives ghosts. Occlusion uses explicit coverage/depth, never
               a colour difference or a radial copy of scene geometry. */
            // Bit 0: visible source. Bit 1: independent graphics toggle.
            if (ScenePrepass != 1 && (GodRaySourceExternal & 3u) == 3u)
            {
                float sourceEdge = min(min(GodRayCenter.x,1.0-GodRayCenter.x),
                    min(GodRayCenter.y,1.0-GodRayCenter.y));
                float visibility = 0.0;
                uint width,height;
                WorldFrame.GetDimensions(width,height);
                float2 texel = 1.0/float2(max(1u,width),max(1u,height));
                [unroll] for (int y=-1;y<=1;++y)
                [unroll] for (int x=-1;x<=1;++x)
                {
                    float2 p=GodRayCenter+float2(x,y)*texel*2.0;
                    float covered=WorldFrame.SampleLevel(SourceSampler,p,0).a;
                    if (RayOverlayEnabled != 0u)
                        covered=max(covered,RayLighting.SampleLevel(SourceSampler,
                            float2(p.x,1.0-p.y),0).a>=0.0 ? 1.0:0.0);
                    visibility += 1.0-saturate(covered);
                }
                visibility *= smoothstep(0.0,0.06,sourceEdge)/9.0;
                float aspect=(float)width/max(1.0,(float)height);
                float2 axis=0.5-GodRayCenter;
                float3 ghosts=0.0;
                [unroll] for (uint ghost=0u;ghost<4u;++ghost)
                {
                    float along=0.45+0.55*(float)ghost;
                    float2 center=GodRayCenter+axis*along;
                    float2 d=(input.texcoord-center)*float2(aspect,1.0);
                    float radius=0.016+0.009*(float)ghost;
                    float r=length(d)/radius;
                    float disk=exp(-r*r*3.0);
                    float ring=exp(-pow((r-0.82)*12.0,2.0))*0.10;
                    float3 tint=ghost==1u ? float3(0.55,0.80,1.0) :
                        (ghost==2u ? float3(1.0,0.65,0.40) : float3(0.70,1.0,0.85));
                    ghosts += tint*(disk+ring)/(1.0+(float)ghost);
                }
                float2 sunDelta=(input.texcoord-GodRayCenter)*float2(aspect,1.0);
                float halo=exp(-dot(sunDelta,sunDelta)*750.0)*0.20;
                float3 flareColor=SkyToDisplay(max(GodRaySourceColor,0.0));
                scene += flareColor*(ghosts*0.10+halo)*visibility;
            }

            /* Grain/dither are final-display operations and must not enter
               Streamline temporal history. */
            if (ScenePrepass != 1 && FilmGrain > 0.0001)
            {
                float grain = Noise(input.position.xy) - 0.5;
                float luminance = saturate(Luminance(scene));
                float blackProtection = smoothstep(0.015, 0.12, luminance);
                float highlightProtection =
                    1.0 - smoothstep(0.68, 0.98, luminance);
                float midtoneResponse =
                    0.35 + 0.65 * (1.0 - abs(luminance * 2.0 - 1.0));
                float response = blackProtection * highlightProtection *
                                 midtoneResponse;
                scene += grain * (0.075 * FilmGrain * response);
            }
            source.rgb = max(scene, 0.0);
        }
    }
    /* scRGB is linear. The classic/presenter composition is display-referred,
       so linearize exactly once at the final visible boundary. */
    if (ScenePrepass != 1)
    {
        float brightness = float(DisplayControls & 65535u) / 1000.0;
        float gamma = float(DisplayControls >> 16u) / 1000.0;
        float3 display = max(source.rgb, 0.0) * brightness;
        display = pow(display, 1.0 / max(gamma, 0.01));
        /* The UNORM swapchain has no automatic sRGB encode. Write the
           display-referred values directly and advertise Rec.709 below. */
        source.rgb = saturate(display);
    }
    return source;
}
)";

// Keep the normal presenter source byte-for-byte stable: these checked edits
// extend the exact same cloud marcher only for the optional FG MRT shader.
// Changing dead helper text in presenterShader would otherwise invalidate all
// four existing warm shader-cache entries and recompile the large presenter.
struct CloudGuideShaderEdit { const char *before; const char *after; };
const CloudGuideShaderEdit cloudGuideShaderEdits[] = {
    {R"(float3 ApplyVolumetricDust(float2 uv, float3 scene, bool forceDirect,
                           out float outputTransmittance)
{
)", R"(float3 ApplyVolumetricDustGuided(float2 uv, float3 scene, bool forceDirect,
                           out float outputTransmittance, out float2 cloudGuide)
{
    cloudGuide = 0.0;
    float cloudGuideDistanceSum = 0.0;
    float cloudGuideWeightSum = 0.0;
)"},
    {R"(                    accumulated += transmittance * inScattering;
                    transmittance *= stepTransmittance;)",
     R"(                    accumulated += transmittance * inScattering;
                    float cloudGuideWeight = transmittance * alpha;
                    cloudGuideDistanceSum += cloudGuideWeight * currentDistance;
                    cloudGuideWeightSum += cloudGuideWeight;
                    transmittance *= stepTransmittance;)"},
    {R"(    accumulated = DustToneVolumetricContribution(accumulated, transmittance);
    float3 composite = max(scene * transmittance + accumulated +
        atmosphericLight * transmittance, 0.0);)",
     R"(    if (cloudGuideWeightSum > 0.000001)
    {
        float distance = cloudGuideDistanceSum / cloudGuideWeightSum;
        if (isfinite(distance) && distance > 0.0)
            cloudGuide = float2(distance, saturate(cloudGuideWeightSum));
    }
    accumulated = DustToneVolumetricContribution(accumulated, transmittance);
    float3 composite = max(scene * transmittance + accumulated +
        atmosphericLight * transmittance, 0.0);)"},
    {R"(float4 DustScreenPS(VertexOutput input) : SV_Target
)", R"(float3 ApplyVolumetricDust(float2 uv, float3 scene, bool forceDirect,
                           out float outputTransmittance)
{
    float2 unusedGuide;
    return ApplyVolumetricDustGuided(uv, scene, forceDirect,
        outputTransmittance, unusedGuide);
}

float4 DustScreenPS(VertexOutput input) : SV_Target
)"}
};
const char cloudGuideShaderEntry[] = R"(
struct DustScreenGuidedOutput
{
    float4 color : SV_Target0;
    float2 guide : SV_Target1;
};
DustScreenGuidedOutput DustScreenGuidedPS(VertexOutput input)
{
    DustScreenGuidedOutput output;
    float transmittance;
    float3 scattering = ApplyVolumetricDustGuided(
        input.texcoord, 0.0, false, transmittance, output.guide);
    output.color = float4(scattering, saturate(transmittance));
    return output;
}
)";

bool makeCloudFrameGenerationShaderSource(std::string &source)
{
    source.assign(presenterShader, sizeof(presenterShader) - 1);
    for (const auto &edit : cloudGuideShaderEdits)
    {
        const size_t at = source.find(edit.before);
        if (at == std::string::npos ||
            source.find(edit.before, at + 1) != std::string::npos)
        {
            // A changed march must be reviewed, never silently instrumented
            // at a different site or compiled into an incomplete guide.
            HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Cloud FG shader extension marker mismatch; cloud guide disabled.\n");
            source.clear();
            return false;
        }
        source.replace(at, std::strlen(edit.before), edit.after);
    }
    source.append(cloudGuideShaderEntry, sizeof(cloudGuideShaderEntry) - 1);
    return true;
}

struct GpuDustVolume
{
    unsigned int enabled;
    unsigned int shape;
    unsigned int shapeSeed;
    unsigned int flags;
    float position[3];
    float density;
    float size[3];
    float scattering;
    float color[3];
    float absorption;
    float anisotropy;
    float coverage;
    float noiseScale;
    float noiseDetail;
    float shapeVariation;
    float rotationDegrees[3];
    float rotationXSinCos[2];
    float rotationYSinCos[2];
    float rotationZSinCos[2];
    float precomputedPadding[2];
    float seedOffset[3];
    float seedPadding;
    float openmwControl[4];
    float openmwTime[4];
};
static_assert(sizeof(GpuDustVolume) == 176,
              "precomputed dust volume GPU layout mismatch");
static_assert(offsetof(GpuDustVolume, rotationXSinCos) == 96,
              "authored dust prefix must remain byte-identical");

struct GpuDustLight
{
    float position[3];
    unsigned int type;
    float direction[3];
    float radius;
    float color[3];
    float intensity;
    float coneCos;
    float edgeCos;
    float padding[2];
};
static_assert(sizeof(GpuDustLight) == 64, "dust light GPU layout mismatch");

struct GpuDustWake
{
    float position[3];
    float radius;
    float direction[3];
    float length;
    float strength;
    float bowStrength;
    float recovery;
    float padding;
};
static_assert(sizeof(GpuDustWake) == 48, "dust wake GPU layout mismatch");
static_assert(sizeof(HWModernVolumetricDustVolume) == 96,
              "dust volume GPU layout mismatch");

// Developer-only serial comparison uses the identical command graph and settings.
const bool frameWorkersEnabled=[] { const char *value=std::getenv("HW_FRAME_WORKERS");return !value || std::strcmp(value,"0")!=0; }();
hwmodern::FrameWorker rayPreparationWorker(L"OpenHW ray/path tracing preparation",frameWorkersEnabled);
hwmodern::FrameWorker postProcessingWorker(L"OpenHW post processing",frameWorkersEnabled);
hwmodern::FrameWorker audioEventWorker(L"OpenHW audio events",frameWorkersEnabled);
struct AudioFrameWork { void (*function)()=nullptr; } audioFrameWork;

void logFailure(const char *operation,HRESULT result);

bool beginWorkerCommands(ID3D12Device *device,ComPtr<ID3D12CommandAllocator> &allocator,
    ComPtr<ID3D12GraphicsCommandList> &commands,const wchar_t *name)
{
    HRESULT result=S_OK;
    if (!allocator) result=device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator));
    if (SUCCEEDED(result) && !commands) {
        result=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),nullptr,IID_PPV_ARGS(&commands));
        if (SUCCEEDED(result)) {commands->SetName(name);result=commands->Close();}
    }
    if (SUCCEEDED(result)) result=allocator->Reset();
    if (SUCCEEDED(result)) result=commands->Reset(allocator.Get(),nullptr);
    if (FAILED(result)) logFailure("resetting a worker command list",result);
    return SUCCEEDED(result);
}

struct D3D12FrameContext
{
    ComPtr<ID3D12Resource> frameGenerationHudless;
    ComPtr<ID3D12Resource> shaftCapture[2];
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT shaftCaptureFootprint[2] = {};
    UINT64 shaftCaptureBytes[2] = {};
    bool shaftCapturePending = false;
    UINT64 shaftCaptureFrame = 0;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12CommandAllocator> rayAllocator,postAllocator;
    ComPtr<ID3D12GraphicsCommandList> rayCommands,postCommands;
    ComPtr<ID3D12Resource> upload;
    ComPtr<ID3D12Resource> worldUpload;
    ComPtr<ID3D12Resource> backgroundUpload;
    ComPtr<ID3D12Resource> occluderUpload;
    ComPtr<ID3D12Resource> dustVolumeUpload;
    ComPtr<ID3D12Resource> dustTileUpload;
    unsigned int *mappedDustTiles = nullptr;
    ComPtr<ID3D12Resource> dustLightUpload;
    ComPtr<ID3D12Resource> dustWakeUpload;
    ComPtr<ID3D12Resource> dustSunConstantsUpload;
    unsigned char *mappedDustSunConstants = nullptr;
    /* RTX-AAA: each swap-chain flight slot owns a tiny timestamp readback.
       Results are consumed only after that slot's existing fence completes,
       so profiling never introduces a new GPU/CPU synchronization point. */
    ComPtr<ID3D12Resource> timestampReadback;
    bool timestampPending = false;
    unsigned long long benchmarkCaptureId = 0;
    unsigned long long benchmarkFrameId = 0, submittedFrameId = 0;
    GpuDustVolume *mappedDustVolumes = nullptr;
    std::uint64_t particleGridFingerprint = 0;
    GpuDustLight *mappedDustLights = nullptr;
    GpuDustWake *mappedDustWakes = nullptr;
    unsigned char *mappedUpload = nullptr;
    unsigned char *mappedWorldUpload = nullptr;
    unsigned char *mappedBackgroundUpload = nullptr;
    unsigned char *mappedOccluderUpload = nullptr;
    UINT64 uploadSize = 0;
    UINT64 fenceValue = 0;
};

struct PresenterConstants
{
    unsigned int rayOverlayEnabled;
    unsigned int antiAliasingMode;
    unsigned int sceneCompositeEnabled;
    unsigned int frameIndex;
    float chromaticAberration;
    float motionBlur;
    float filmGrain;
    float godRays;
    float godRayCenter[2];
    unsigned int scenePrepass;
    unsigned int streamlineResolvedScene;
    float outputDither;
    float godRaySourceColor[3];
    unsigned int godRaySourceExternal;
    float bloom;
    float presenterPadding[2];
    unsigned int dustVolumeCount;
    unsigned int dustLightCount;
    unsigned int dustRenderEnabled;
    unsigned int dustWakeCount;
    float dustCameraPositionMaxDistance[4];
    float dustRayUv00[4];
    float dustRayUv10[4];
    float dustRayUv01SunStrength[4];
    float dustSunDirectionValid[4];
    float dustSunColor[4];
    float dustAmbientColorStrength[4];
    float dustGridMinWidth[4];
    float dustGridSizeHeight[4];
    unsigned int displayControls;
};

static_assert(sizeof(PresenterConstants) == 244,
              "presenter constant layout mismatch");
static_assert(sizeof(PresenterConstants) / 4 + 3 <= 64,
              "presenter root signature exceeds D3D12 DWORD budget");

struct D3D12Presenter
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Device5> raytracingDevice;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain4> swapchain;
    D3D12FrameContext frames[frameBufferCount];
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    ComPtr<ID3D12QueryHeap> timestampQueryHeap;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12RootSignature> dustFroxelRootSignature;
    ComPtr<ID3D12PipelineState> presenterPipeline;
    ComPtr<ID3D12PipelineState> scenePrepassPipeline;
    ComPtr<ID3D12PipelineState> dustFroxelPipeline;
    ComPtr<ID3D12PipelineState> dustSunVisibilityPipeline;
    ComPtr<ID3D12PipelineState> dustScreenPipeline;
    ComPtr<ID3DBlob> presenterVertexShader;
    ComPtr<ID3D12PipelineState> cloudFrameGenerationGuidePipeline;
    ComPtr<ID3D12DescriptorHeap> cloudFrameGenerationGuideRtvHeap;
    ComPtr<ID3D12Resource> cloudFrameGenerationGuide;
    ComPtr<ID3D12Resource> backBuffers[frameBufferCount];
    ComPtr<ID3D12Resource> sourceTexture;
    ComPtr<ID3D12Resource> worldTexture;
    ComPtr<ID3D12Resource> backgroundTexture;
    ComPtr<ID3D12Resource> occluderTexture;
    ComPtr<ID3D12Resource> sceneInputTexture;
    ComPtr<ID3D12Resource> cloudProbeTexture;
    ComPtr<ID3D12DescriptorHeap> cloudProbeRtvHeap;
    ComPtr<ID3D12Resource> dustFroxelTexture;
    ComPtr<ID3D12Resource> openmwNoiseTexture;
    ComPtr<ID3D12Resource> openmwNoiseUpload;
    ComPtr<ID3D12Resource> dustScreenTextures[2];
    ComPtr<ID3D12Fence> fence;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint = {};
    HANDLE fenceEvent = nullptr;
    HWND window = nullptr;
    UINT rtvDescriptorSize = 0;
    UINT srvDescriptorSize = 0;
    UINT width = 0;
    UINT height = 0;
    UINT sceneInputWidth = 0;
    UINT sceneInputHeight = 0;
    UINT dustFroxelWidth = 0;
    UINT dustFroxelHeight = 0;
    UINT dustFroxelDepth = 0;
    UINT dustScreenWidth = 0;
    UINT dustScreenHeight = 0;
    unsigned int lastDustVolumeCount = 0;
    unsigned int lastDustLightCount = 0;
    unsigned int lastDustWakeCount = 0;
    unsigned int lastDustEnabled = 0;
    unsigned int lastDustTileCandidates = 0;
    UINT dustScreenWriteIndex = 0;
    bool dustScreenShaderReadable[2] = {true, true};
    bool dustScreenHistoryValid = false;
    bool frameGenerationGuidesNeeded = false;
    bool cloudFrameGenerationGuideShaderReadable = true;
    bool cloudFrameGenerationGuideValidThisFrame = false;
    bool cloudFrameGenerationGuideFailed = false;
    UINT64 nextFenceValue = 1;
    UINT64 submittedFenceValue = 0;
    UINT64 timestampFrequency = 0;
    UINT activeFrameIndex = 0;
    double gpuRasterMilliseconds = 0.0;
    double gpuDxrMilliseconds = 0.0;
    double gpuDustPrepareMilliseconds = 0.0;
    double gpuDustScreenMilliseconds = 0.0;
    double gpuReconstructionMilliseconds = 0.0;
    double gpuCloudShadowMilliseconds = 0.0;
    double gpuCloudMarchMilliseconds = 0.0;
    double gpuSceneCompositeMilliseconds = 0.0;
    double gpuUpscalerMilliseconds = 0.0;
    double gpuPresenterMilliseconds = 0.0;
    double gpuFrameMilliseconds = 0.0;
    unsigned long long gpuTimestampFrames = 0;
    std::chrono::steady_clock::time_point cpuFrameStarted = {};
    double cpuFrameMilliseconds = 0.0;
    // Wall-clock attribution; driver calls may block and are not pure CPU work.
    double cpuFrameFenceWaitMilliseconds = 0.0;
    double cpuFrameDxrRecordMilliseconds = 0.0;
    double cpuFenceWaitMilliseconds = 0.0;
    double cpuRecordStageMilliseconds = 0.0;
    double cpuQueueExecuteMilliseconds = 0.0;
    double cpuPresentMilliseconds = 0.0;
    double cpuFenceSignalMilliseconds = 0.0;
    unsigned long long cpuFrameSamples = 0;
    bool cpuFrameTimerActive = false;
    unsigned long long frameCount = 0;
    UINT previousNativePresentCount = 0;
    bool nativePresentCountValid = false;
    float dustCameraPosition[3] = {0.0f, 0.0f, 0.0f};
    float godRayFocus[3] = {0.0f, 0.0f, 0.0f};
    bool godRayFocusValid = false;
    float dustRayUv00[3] = {-1.0f, -1.0f, -1.0f};
    float dustRayUv10[3] = { 1.0f, -1.0f, -1.0f};
    float dustRayUv01[3] = {-1.0f,  1.0f, -1.0f};
    float dustProjectionP10 = 0.0f;
    float dustProjectionP14 = 0.0f;
    bool dustProjectionValid = false;
    bool nativeRasterDepthSampleReady = false;
    bool dustCameraValid = false;
    bool dustCameraExplicitThisFrame = false;
    unsigned int missionSkyTextureName = 0;
    float missionSkyFade = 1.0f;
    bool missionSkyEnvironmentLogged = false;
    /* Streamline camera history is now reset only for real discontinuities.
       Sky misses carry dense camera-rotation motion from the DXR shader, so
       ordinary orbit/pan movement must preserve temporal reconstruction. */
    float streamlineCameraPosition[3] = {0.0f, 0.0f, 0.0f};
    float streamlineRayUv00[3] = {0.0f, 0.0f, 0.0f};
    float streamlineRayUv10[3] = {0.0f, 0.0f, 0.0f};
    float streamlineRayUv01[3] = {0.0f, 0.0f, 0.0f};
    bool streamlineCameraHistoryValid = false;
    float godRayCenterX = 0.5f;
    float godRayCenterY = 0.42f;
    float godRayWorldDirection[3] = {0.0f, 0.0f, -1.0f};
    int godRayMapIdentity = -2147483647;
    unsigned long long godRayLastBackgroundFrame = 0;
    bool godRaySourceLocked = false;
    bool godRaySourceVisible = false;
    float godRaySourceColor[3] = {0.0f, 0.0f, 0.0f};
    bool godRaySourceExternalThisFrame = false;
    bool godRaySourceExternalVisible = false;
    int vsyncMode = 1;
    bool allowTearing = false;
    bool active = false;
    bool visiblePresenter = false;
    bool frameOpen = false;
    bool frameCaptured = false;
    bool worldFrameCaptured = false;
    bool backgroundFrameCaptured = false;
    bool occluderFrameCaptured = false;
    bool sourceTextureShaderReadable = false;
    bool worldTextureShaderReadable = false;
    bool backgroundTextureShaderReadable = false;
    bool occluderTextureShaderReadable = false;
    bool sceneInputTextureShaderReadable = false;
    bool dustFroxelShaderReadable = false;
    bool dustFroxelValidThisFrame = false;
    double backgroundReadbackMilliseconds = 0.0;
    double worldReadbackMilliseconds = 0.0;
    double frameReadbackMilliseconds = 0.0;
    double dxrRecordMilliseconds = 0.0;
    unsigned long long backgroundReadbackCount = 0;
    unsigned long long worldReadbackCount = 0;
    unsigned long long frameReadbackCount = 0;
    unsigned long long dxrRecordCount = 0;
};

D3D12Presenter presenter;
bool raytracingRequested = true;
int requestedAntiAliasingMode = HW_MODERN_AA_DLAA;
int requestedFrameGenerationMode = HW_MODERN_FRAME_GENERATION_OFF;
// DLSS-G capability alone does not establish a working presentation path.
// The shared AMD proxy owns replacement buffers and an inner SL swapchain;
// the current integration has not produced extra NVIDIA presentations. Keep
// DLSS-G unavailable until that path is implemented and verified separately.
// DLSS Super Resolution, DLAA and Ray Reconstruction remain independent.
constexpr bool nvidiaFrameGenerationPresentationEnabled = false;
bool frameGenerationInputsReady = false;
bool frameGenerationRequestChanged = false;
int frameGenerationFrameProvider = HW_MODERN_FRAME_GENERATION_OFF;
const char *frameGenerationInputReason = "Waiting for a rendered game scene.";
float frameGenerationDeltaMilliseconds = 16.6667f;
std::chrono::steady_clock::time_point previousFrameGenerationTime;

int selectedFrameGenerationProvider()
{
    if (requestedFrameGenerationMode != HW_MODERN_FRAME_GENERATION_AUTO_2X)
        return requestedFrameGenerationMode;
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (nvidiaFrameGenerationPresentationEnabled &&
        hwmodern::dlaaFrameGenerationAvailable() && !hwmodern::dlaaFrameGenerationFailed())
        return HW_MODERN_FRAME_GENERATION_DLSS_G_2X;
    if (hwmodern::fsrFrameGenerationAvailable() && !hwmodern::fsrFrameGenerationFailed())
        return HW_MODERN_FRAME_GENERATION_FSR_2X;
    return nvidiaFrameGenerationPresentationEnabled && hwmodern::dlaaFrameGenerationAvailable() ? HW_MODERN_FRAME_GENERATION_DLSS_G_2X
        : HW_MODERN_FRAME_GENERATION_FSR_2X;
#else
    return HW_MODERN_FRAME_GENERATION_FSR_2X;
#endif
}
bool requestedRayReconstruction = true;
float requestedFxLightingStrength = 1.0f;
unsigned int requestedPathSamples = 1;
unsigned int requestedPathBounces = 3;
int observedModelTextureCap = -1;
float requestedNormalStrength = 1.0f;
float requestedEnvironmentStrength = 1.0f;
float requestedAuthoredLightStrength = 1.0f;
float requestedPrimarySunStrength = 1.0f;
float requestedSkyAmbientStrength = 1.0f;
float requestedSurfaceReflectivity = 1.25f;
float requestedSurfaceRoughness = 0.38f;
float requestedRasterGlowBrightness = 0.29f;
float requestedLightingExposure = 1.0f;
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
float requestedChromaticAberration = 0.0f;
float requestedMotionBlur = 0.0f;
float requestedFilmGrain = 0.0f;
float requestedDisplayBrightness = 1.0f;
float requestedDisplayGamma = 1.0f;
float requestedGodRays = 0.0f;
float missionGodRayStrength = 1.0f;
unsigned int missionGodRayTint = 0u;
float requestedBloom = 1.0f;
bool requestedLensFlares = true;
float requestedOutputDitherStrength = 1.0f;
std::vector<HWModernMapLightEmitter> mapLightEmitters;
std::vector<HWModernMissionLightEmitter> missionAuthoringLightEmitters;
bool missionAuthoringReplacesMapLightsForDust = false;
std::vector<HWModernDynamicLightEmitter> dynamicLightEmitters;
std::vector<HWModernVolumetricDustVolume> volumetricDustVolumes;
std::vector<HWModernVolumetricDustVolume> harvestableNebulaVolumes;
std::vector<HWModernVolumetricDustVolume> particleFieldVolumes;
std::vector<float> particleFieldDensity;
std::uint64_t particleDensityRevision = 1;
std::vector<HWModernVolumetricDustVolume> harvestableDustVolumes;
std::vector<HWModernHarvestableNebulaSegment> harvestableNebulaSegments;
HWModernHarvestableNebulaStyle harvestableNebulaStyle = {
    1u, 1.18f, 0.34f, 0.72f, 0.82f, 0.16f,
    {0.66f, 0.42f, 0.22f}, 0.00042f, 0.72f, 0.58f};
float volumetricDustFadeDistance = 65000.0f;
float volumetricDustFadeStrength = 1.0f;
std::vector<HWModernVolumetricDustVolume> volumetricDustScratch;
std::vector<GpuDustLight> volumetricDustLightScratch;
std::vector<HWModernVolumetricDustInteractor> volumetricDustInteractors;
std::vector<GpuDustWake> volumetricDustWakeScratch;

bool atmosphereCacheEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("HW_OPT_ATMOS_CACHE");
        return value == nullptr || std::strcmp(value,"0") != 0;
    }();
    return enabled;
}

#if HW_ENABLE_DIAGNOSTICS
#include "BenchmarkCapture.inc"
#else
struct BenchmarkCacheSnapshot {};
struct { static constexpr bool enabled=false, recording=false; static constexpr unsigned long long id=0; } benchmark;
void benchmarkFinish(bool) {}
void benchmarkPoll() {}
#endif

BenchmarkCacheSnapshot benchmarkCacheSnapshot()
{
#if HW_ENABLE_DIAGNOSTICS

    BenchmarkCacheSnapshot snapshot;
#if defined(HW_ENABLE_D3D12_BACKEND)
    const auto ray = hwmodern::raytracingCacheStats();
    snapshot.textureUploads = ray.uploads;
    snapshot.textureEvictions = ray.evictions;
    snapshot.textureTrimmedBytes = ray.trimmedBytes;
    snapshot.textureResidentBytes = ray.residentBytes;
#endif
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    const auto raster = hwlegacyd3d12::cacheStats();
    snapshot.rasterUploadedBytes = raster.uploadedBytes;
    snapshot.rasterCacheChargeBytes = raster.cacheChargeBytes;
#endif
    return snapshot;

#else
return {};
#endif
}

struct VolumetricDustWakeRecord
{
    unsigned long long identity = 0;
    float position[3] = {};
    float direction[3] = {0.0f, 0.0f, -1.0f};
    float radius = 1.0f;
    float length = 1.0f;
    float strength = 1.0f;
    std::chrono::steady_clock::time_point created;
};

struct VolumetricDustInteractorState
{
    float lastPosition[3] = {};
    float lastEmitPosition[3] = {};
    std::chrono::steady_clock::time_point lastSeen;
    std::chrono::steady_clock::time_point lastEmit;
    bool initialized = false;
};

std::vector<VolumetricDustWakeRecord> volumetricDustWakeHistory;
std::unordered_map<unsigned long long, VolumetricDustInteractorState>
    volumetricDustInteractorStates;
bool volumetricDustSimulationPaused = false;
std::chrono::steady_clock::time_point volumetricDustPauseStarted =
    std::chrono::steady_clock::now();
bool dustMissionSunValid = false;
bool dustMissionSkyAvailable = false;
float dustMissionSunDirection[3] = {0.0f, 0.0f, -1.0f};
float dustMissionSunColor[3] = {1.0f, 1.0f, 1.0f};
float dustMissionAmbientColor[3] = {0.08f, 0.08f, 0.10f};

using DynamicLightClock = std::chrono::steady_clock;
struct TimedDynamicLight
{
    HWModernDynamicLightEmitter emitter = {};
    DynamicLightClock::time_point started;
    float lifetimeSeconds = 0.0f;
};
std::vector<TimedDynamicLight> timedDynamicLightEmitters;

struct EmissiveMaterialRecord
{
    std::string meshName;
    std::string materialName;
    std::string textureName;
    unsigned int fullAmbientColors = 0;
    float color[3] = {};
    float intensity = 0.0f;
};

std::vector<EmissiveMaterialRecord> emissiveMaterials;

void logFailure(const char *operation, HRESULT result)
{
    HW_DIAG_FPRINTF(stderr, "[ModernGraphics] %s failed (HRESULT 0x%08lx).\n",
                 operation, static_cast<unsigned long>(result));
}

void refreshNativeRasterDepthSrv(ID3D12Resource *depthResource)
{
    if (!presenter.device || !presenter.srvHeap) return;
    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrv = {};
    depthSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
    depthSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(sceneDepthDescriptorIndex) *
                  presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(depthResource, &depthSrv, handle);
}

bool createVolumetricDustFrameResources()
{
    hwWindowsSplashTaskBegin("renderer.resources.frame", "Frame upload buffers");
    if (!presenter.device || !presenter.srvHeap) return false;
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC buffer = {};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (UINT slot = 0; slot < frameBufferCount; ++slot)
    {
        D3D12FrameContext &frame = presenter.frames[slot];
        buffer.Width = sizeof(GpuDustVolume) * maximumDustVolumeRecords;
        HRESULT result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.dustVolumeUpload));
        if (FAILED(result))
        {
            logFailure("CreateCommittedResource(volumetric dust buffer)", result);
            return false;
        }
        frame.dustVolumeUpload->SetName(L"Homeworld volumetric dust frame data");
        void *mappedVolumes = nullptr;
        if (FAILED(result = frame.dustVolumeUpload->Map(
            0, nullptr, &mappedVolumes)))
        {
            logFailure("Map(volumetric dust buffer)", result);
            return false;
        }
        frame.mappedDustVolumes = static_cast<GpuDustVolume *>(mappedVolumes);
        std::memset(frame.mappedDustVolumes, 0,
                    static_cast<size_t>(buffer.Width));
        frame.particleGridFingerprint=0;

        buffer.Width = sizeof(GpuDustLight) * maximumVolumetricDustLights;
        if (FAILED(result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.dustLightUpload))))
        {
            logFailure("CreateCommittedResource(volumetric dust light buffer)", result);
            return false;
        }
        frame.dustLightUpload->SetName(L"Homeworld volumetric dust local lights");
        void *mappedLights = nullptr;
        if (FAILED(result = frame.dustLightUpload->Map(0, nullptr, &mappedLights)))
        {
            logFailure("Map(volumetric dust light buffer)", result);
            return false;
        }
        frame.mappedDustLights = static_cast<GpuDustLight *>(mappedLights);
        std::memset(frame.mappedDustLights, 0, static_cast<size_t>(buffer.Width));

        D3D12_SHADER_RESOURCE_VIEW_DESC volumeSrv = {};
        volumeSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        volumeSrv.Format = DXGI_FORMAT_UNKNOWN;
        volumeSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        volumeSrv.Buffer.FirstElement = 0;
        volumeSrv.Buffer.NumElements = maximumDustVolumeRecords;
        volumeSrv.Buffer.StructureByteStride = sizeof(GpuDustVolume);
        D3D12_CPU_DESCRIPTOR_HANDLE volumeHandle =
            presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        volumeHandle.ptr += static_cast<SIZE_T>(
            dustDescriptorBase + slot * dustDescriptorsPerFrame) *
            presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            frame.dustVolumeUpload.Get(), &volumeSrv, volumeHandle);

        buffer.Width = dustTileColumns * dustTileRows * dustTileWords * sizeof(unsigned int);
        if (FAILED(presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.dustTileUpload)))) return false;
        void *mappedTiles = nullptr;
        if (FAILED(frame.dustTileUpload->Map(0, nullptr, &mappedTiles))) return false;
        frame.mappedDustTiles = static_cast<unsigned int *>(mappedTiles);
        D3D12_SHADER_RESOURCE_VIEW_DESC tileSrv = volumeSrv;
        tileSrv.Buffer.NumElements = dustTileColumns * dustTileRows * dustTileWords;
        tileSrv.Buffer.StructureByteStride = sizeof(unsigned int);
        auto tileHandle = volumeHandle;
        tileHandle.ptr += 8u * presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(frame.dustTileUpload.Get(), &tileSrv, tileHandle);

        D3D12_SHADER_RESOURCE_VIEW_DESC lightSrv = {};
        lightSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        lightSrv.Format = DXGI_FORMAT_UNKNOWN;
        lightSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        lightSrv.Buffer.FirstElement = 0;
        lightSrv.Buffer.NumElements = maximumVolumetricDustLights;
        lightSrv.Buffer.StructureByteStride = sizeof(GpuDustLight);
        D3D12_CPU_DESCRIPTOR_HANDLE lightHandle = volumeHandle;
        lightHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            frame.dustLightUpload.Get(), &lightSrv, lightHandle);

        buffer.Width = sizeof(GpuDustWake) * maximumVolumetricDustWakes;
        if (FAILED(result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.dustWakeUpload))))
        {
            logFailure("CreateCommittedResource(volumetric dust wake buffer)", result);
            return false;
        }
        frame.dustWakeUpload->SetName(L"Homeworld volumetric dust ship wakes");
        void *mappedWakes = nullptr;
        if (FAILED(result = frame.dustWakeUpload->Map(0, nullptr, &mappedWakes)))
        {
            logFailure("Map(volumetric dust wake buffer)", result);
            return false;
        }
        frame.mappedDustWakes = static_cast<GpuDustWake *>(mappedWakes);
        std::memset(frame.mappedDustWakes, 0, static_cast<size_t>(buffer.Width));

        D3D12_SHADER_RESOURCE_VIEW_DESC wakeSrv = {};
        wakeSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        wakeSrv.Format = DXGI_FORMAT_UNKNOWN;
        wakeSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        wakeSrv.Buffer.FirstElement = 0;
        wakeSrv.Buffer.NumElements = maximumVolumetricDustWakes;
        wakeSrv.Buffer.StructureByteStride = sizeof(GpuDustWake);
        D3D12_CPU_DESCRIPTOR_HANDLE wakeHandle = lightHandle;
        wakeHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            frame.dustWakeUpload.Get(), &wakeSrv, wakeHandle);

        D3D12_SHADER_RESOURCE_VIEW_DESC froxelSrv = {};
        froxelSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        froxelSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        froxelSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        froxelSrv.Texture3D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE froxelHandle = wakeHandle;
        froxelHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(nullptr, &froxelSrv, froxelHandle);

        /* t13 is a ray-space primary albedo guide used only to protect
           foreground emissive FX (engine trails/glows, beams) from being
           multiplied by opaque-surface DXR shadow lighting. Keep a valid null
           descriptor until the first ray dispatch supplies the real guide. */
        D3D12_SHADER_RESOURCE_VIEW_DESC albedoSrv = {};
        albedoSrv.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        albedoSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        albedoSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        albedoSrv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE albedoHandle = froxelHandle;
        albedoHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(nullptr, &albedoSrv,
                                                    albedoHandle);
        D3D12_CPU_DESCRIPTOR_HANDLE farFroxelHandle = albedoHandle;
        farFroxelHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(nullptr, &froxelSrv,
                                                    farFroxelHandle);
        D3D12_SHADER_RESOURCE_VIEW_DESC screenDustSrv = {};
        screenDustSrv.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        screenDustSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        screenDustSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        screenDustSrv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE screenDustHandle = farFroxelHandle;
        screenDustHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            nullptr, &screenDustSrv, screenDustHandle);
        screenDustHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            nullptr, &screenDustSrv, screenDustHandle);
        D3D12_SHADER_RESOURCE_VIEW_DESC trailSrv = {};
        trailSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        trailSrv.Format = DXGI_FORMAT_R32_FLOAT;
        trailSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        trailSrv.Texture2D.MipLevels = 1;
        auto trailHandle = volumeHandle;
        trailHandle.ptr += 9u * presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(nullptr, &trailSrv, trailHandle);
    }
    hwWindowsSplashTaskComplete("renderer.resources.frame", "Three flight slots initialized");
    return true;
}


float volumetricDustDegreesToCosine(float degrees)
{
    const float pi = 3.14159265358979323846f;
    return std::cos(std::max(0.0f, std::min(degrees, 179.0f)) * pi / 180.0f);
}

unsigned int volumetricDustHash(unsigned int value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

void volumetricDustPrecomputeVolume(
    const HWModernVolumetricDustVolume &source, GpuDustVolume &gpu)
{
    std::memset(&gpu, 0, sizeof(gpu));
    /* The authored layout is deliberately the first 96 bytes of the GPU
       record. Extra data below is runtime-only and never changes map/editor
       serialization. */
    std::memcpy(&gpu, &source, sizeof(HWModernVolumetricDustVolume));
    constexpr float pi = 3.14159265358979323846f;
    const float radians = -pi / 180.0f;
    const float angleX = source.rotationDegrees[0] * radians;
    const float angleY = source.rotationDegrees[1] * radians;
    const float angleZ = source.rotationDegrees[2] * radians;
    gpu.rotationXSinCos[0] = std::sin(angleX);
    gpu.rotationXSinCos[1] = std::cos(angleX);
    gpu.rotationYSinCos[0] = std::sin(angleY);
    gpu.rotationYSinCos[1] = std::cos(angleY);
    gpu.rotationZSinCos[0] = std::sin(angleZ);
    gpu.rotationZSinCos[1] = std::cos(angleZ);
    gpu.precomputedPadding[0] = volumetricDustFadeDistance;
    gpu.precomputedPadding[1] = volumetricDustFadeStrength;
    gpu.openmwControl[0] = cloudRenderSettings.samplingQuality;
    gpu.openmwControl[1] = static_cast<float>(cloudRenderSettings.detail);
    gpu.openmwControl[2] = cloudRenderSettings.motionEnabled ? cloudRenderSettings.motionSpeed : 0.0f;
    gpu.openmwControl[3] = cloudRenderSettings.billowStrength;
    gpu.openmwTime[0] = cloudSimulationTime;
    gpu.openmwTime[1] = cloudRenderSettings.fieldCoverage;
    gpu.seedOffset[0] = static_cast<float>(
        volumetricDustHash(source.shapeSeed ^ 0x9e3779b9u) & 1023u) / 37.0f;
    gpu.seedOffset[1] = static_cast<float>(
        volumetricDustHash(source.shapeSeed ^ 0xc8013ea4u) & 1023u) / 37.0f;
    gpu.seedOffset[2] = static_cast<float>(
        volumetricDustHash(source.shapeSeed ^ 0xad90777du) & 1023u) / 37.0f;
    if (source.shape == HW_MODERN_DUST_ROPE)
        gpu.seedPadding = hwdust::ropeSupportRadius(source.density,
            source.shapeVariation, std::min(source.size[0],source.size[1])*0.5f);
}

float volumetricDustLength3(const float value[3])
{
    return std::sqrt(value[0]*value[0] + value[1]*value[1] + value[2]*value[2]);
}

bool volumetricDustInteractorNearVolume(
    const HWModernVolumetricDustInteractor &interactor)
{
    for (const HWModernVolumetricDustVolume &volume : volumetricDustVolumes)
    {
        if (volume.enabled == 0 || volume.density <= 0.00001f) continue;
        float half[3] = {
            std::max(1.0f, volume.size[0] * 0.5f),
            std::max(1.0f, volume.size[1] * 0.5f),
            std::max(1.0f, volume.size[2] * 0.5f)};
        if (volume.shape == HW_MODERN_DUST_SPHERE)
        {
            const float radius = std::max(1.0f, (half[0] + half[1] + half[2]) / 3.0f);
            half[0] = half[1] = half[2] = radius;
        }
        const float margin = std::max(50.0f, interactor.radius * 2.5f);
        if (std::fabs(interactor.position[0] - volume.position[0]) <= half[0] + margin &&
            std::fabs(interactor.position[1] - volume.position[1]) <= half[1] + margin &&
            std::fabs(interactor.position[2] - volume.position[2]) <= half[2] + margin)
            return true;
    }
    return false;
}

void updateVolumetricDustShipWakes()
{
    /* Editor pause must freeze the medium.  RTX-0087 used wall-clock age for
       wake recovery, so even a paused authoring scene could visibly change
       while the user only rotated the camera. */
    const DynamicLightClock::time_point now = volumetricDustSimulationPaused
        ? volumetricDustPauseStarted : DynamicLightClock::now();
    constexpr float wakeLifetimeSeconds = 6.0f;

    size_t writeIndex = 0;
    for (size_t index = 0; index < volumetricDustWakeHistory.size(); ++index)
    {
        const float age = std::chrono::duration<float>(
            now - volumetricDustWakeHistory[index].created).count();
        if (age < wakeLifetimeSeconds)
            volumetricDustWakeHistory[writeIndex++] = volumetricDustWakeHistory[index];
    }
    volumetricDustWakeHistory.resize(writeIndex);

    for (const HWModernVolumetricDustInteractor &interactor : volumetricDustInteractors)
    {
        if (volumetricDustSimulationPaused) break;
        if (interactor.identity == 0 || interactor.radius <= 0.5f ||
            !volumetricDustInteractorNearVolume(interactor))
            continue;

        VolumetricDustInteractorState &state =
            volumetricDustInteractorStates[interactor.identity];
        if (!state.initialized)
        {
            std::memcpy(state.lastPosition, interactor.position, sizeof(state.lastPosition));
            std::memcpy(state.lastEmitPosition, interactor.position, sizeof(state.lastEmitPosition));
            state.lastSeen = state.lastEmit = now;
            state.initialized = true;
            continue;
        }

        float delta[3] = {
            interactor.position[0] - state.lastPosition[0],
            interactor.position[1] - state.lastPosition[1],
            interactor.position[2] - state.lastPosition[2]};
        const float frameDistance = volumetricDustLength3(delta);
        const float velocityLength = volumetricDustLength3(interactor.velocity);
        const float secondsSinceSeen = std::max(0.0001f,
            std::chrono::duration<float>(now - state.lastSeen).count());
        float direction[3] = {interactor.velocity[0], interactor.velocity[1], interactor.velocity[2]};
        float motionSpeed = velocityLength;
        if (frameDistance > 0.05f)
        {
            direction[0] = delta[0]; direction[1] = delta[1]; direction[2] = delta[2];
            motionSpeed = std::max(motionSpeed, frameDistance / secondsSinceSeen);
        }
        const float directionLength = volumetricDustLength3(direction);

        std::memcpy(state.lastPosition, interactor.position, sizeof(state.lastPosition));
        state.lastSeen = now;
        if (directionLength <= 0.05f || motionSpeed <= 0.5f)
            continue;

        /* Teleports and mission script relocations should not cut a kilometre-
           long tunnel through the medium in one frame. */
        if (frameDistance > std::max(2000.0f, interactor.radius * 12.0f))
        {
            std::memcpy(state.lastEmitPosition, interactor.position,
                        sizeof(state.lastEmitPosition));
            state.lastEmit = now;
            continue;
        }

        float sinceEmit[3] = {
            interactor.position[0] - state.lastEmitPosition[0],
            interactor.position[1] - state.lastEmitPosition[1],
            interactor.position[2] - state.lastEmitPosition[2]};
        const float emitDistance = volumetricDustLength3(sinceEmit);
        const float emitSpacing = std::max(22.0f, interactor.radius * 0.32f);
        const float emitSeconds = std::chrono::duration<float>(now - state.lastEmit).count();
        if (emitDistance < emitSpacing && emitSeconds < 0.12f)
            continue;

        VolumetricDustWakeRecord wake;
        wake.identity = interactor.identity;
        std::memcpy(wake.position, interactor.position, sizeof(wake.position));
        wake.direction[0] = direction[0] / directionLength;
        wake.direction[1] = direction[1] / directionLength;
        wake.direction[2] = direction[2] / directionLength;
        wake.radius = std::max(28.0f, interactor.radius * 0.72f);
        wake.length = std::max(wake.radius * 2.0f,
            std::min(wake.radius * 7.0f, motionSpeed * 0.70f + wake.radius * 1.5f));
        const float relativeSpeed = motionSpeed / std::max(80.0f, interactor.radius * 2.0f);
        wake.strength = std::max(0.22f, std::min(1.35f,
            interactor.strength * (0.50f + relativeSpeed * 0.28f)));
        wake.created = now;
        volumetricDustWakeHistory.push_back(wake);
        if (volumetricDustWakeHistory.size() > 128)
            volumetricDustWakeHistory.erase(volumetricDustWakeHistory.begin(),
                                            volumetricDustWakeHistory.begin() +
                                            (volumetricDustWakeHistory.size() - 128));
        std::memcpy(state.lastEmitPosition, interactor.position,
                    sizeof(state.lastEmitPosition));
        state.lastEmit = now;
    }

    for (auto state = volumetricDustInteractorStates.begin();
         state != volumetricDustInteractorStates.end(); )
    {
        const float unseen = std::chrono::duration<float>(now - state->second.lastSeen).count();
        if (unseen > 3.0f) state = volumetricDustInteractorStates.erase(state);
        else ++state;
    }

    volumetricDustWakeScratch.clear();
    volumetricDustWakeScratch.reserve(maximumVolumetricDustWakes);
    /* Newest records are most important near the ship; older records still
       remain in history and become visible again as newer ones recover. */
    for (auto record = volumetricDustWakeHistory.rbegin();
         record != volumetricDustWakeHistory.rend() &&
         volumetricDustWakeScratch.size() < maximumVolumetricDustWakes; ++record)
    {
        const float age = std::chrono::duration<float>(now - record->created).count();
        const float remaining = std::max(0.0f, 1.0f - age / wakeLifetimeSeconds);
        if (remaining <= 0.0f) continue;
        GpuDustWake gpu = {};
        std::memcpy(gpu.position, record->position, sizeof(gpu.position));
        std::memcpy(gpu.direction, record->direction, sizeof(gpu.direction));
        gpu.radius = record->radius;
        gpu.length = record->length;
        gpu.strength = record->strength * remaining * remaining;
        gpu.bowStrength = record->strength * remaining * remaining * remaining * remaining;
        gpu.recovery = remaining;
        volumetricDustWakeScratch.push_back(gpu);
    }
}

bool streamlineCameraChangedThisFrame()
{
    if (!presenter.dustCameraValid || !presenter.dustCameraExplicitThisFrame)
    {
        presenter.streamlineCameraHistoryValid = false;
        return true;
    }

    const bool firstFrame = !presenter.streamlineCameraHistoryValid;
    bool cameraCut = firstFrame;
    if (!firstFrame)
    {
        const float dx = presenter.dustCameraPosition[0] -
                         presenter.streamlineCameraPosition[0];
        const float dy = presenter.dustCameraPosition[1] -
                         presenter.streamlineCameraPosition[1];
        const float dz = presenter.dustCameraPosition[2] -
                         presenter.streamlineCameraPosition[2];
        const float translation = std::sqrt(dx * dx + dy * dy + dz * dz);

        auto centerDirection = [](const float uv00[3], const float uv10[3],
                                  const float uv01[3], float out[3])
        {
            for (unsigned int channel = 0; channel < 3; ++channel)
                out[channel] = uv00[channel] +
                    0.5f * (uv10[channel] - uv00[channel]) +
                    0.5f * (uv01[channel] - uv00[channel]);
            const float length = std::sqrt(out[0] * out[0] +
                                           out[1] * out[1] +
                                           out[2] * out[2]);
            if (length > 0.000001f)
                for (unsigned int channel = 0; channel < 3; ++channel)
                    out[channel] /= length;
        };
        float currentForward[3] = {};
        float previousForward[3] = {};
        centerDirection(presenter.dustRayUv00, presenter.dustRayUv10,
                        presenter.dustRayUv01, currentForward);
        centerDirection(presenter.streamlineRayUv00,
                        presenter.streamlineRayUv10,
                        presenter.streamlineRayUv01, previousForward);
        const float directionDot =
            currentForward[0] * previousForward[0] +
            currentForward[1] * previousForward[1] +
            currentForward[2] * previousForward[2];
        /* Preserve history during normal Homeworld camera orbit/pan. Only a
           teleport-scale translation or a >60 degree single-frame direction
           jump is treated as a true temporal reset/camera cut. */
        cameraCut = translation > 25000.0f || directionDot < 0.5f;
    }

    const float *currentVectors[4] = {
        presenter.dustCameraPosition, presenter.dustRayUv00,
        presenter.dustRayUv10, presenter.dustRayUv01};
    float *historyVectors[4] = {
        presenter.streamlineCameraPosition, presenter.streamlineRayUv00,
        presenter.streamlineRayUv10, presenter.streamlineRayUv01};
    for (unsigned int vectorIndex = 0; vectorIndex < 4; ++vectorIndex)
        for (unsigned int channel = 0; channel < 3; ++channel)
            historyVectors[vectorIndex][channel] =
                currentVectors[vectorIndex][channel];
    presenter.streamlineCameraHistoryValid = true;
    return cameraCut;
}

bool dustWorldCacheRequested()
{
    /* DustScreenPS deliberately uses the direct procedural field. Building a
       160^3 near cache and 80^3 far cache alongside it consumed GPU time but
       could not affect the visible dust image. Keep the cache opt-in for
       diagnostics until a screen path actually consumes it. */
    static int cachedMode = -1;
    if (cachedMode >= 0) return cachedMode != 0;

    char directOverride[8] = {};
    const DWORD directLength = GetEnvironmentVariableA(
        "HW_DUST_DIRECT_RAYMARCH", directOverride,
        static_cast<DWORD>(sizeof(directOverride)));
    if (directLength > 0 && directOverride[0] != '0')
    {
        cachedMode = 0;
        return false;
    }

    char cacheOverride[8] = {};
    const DWORD cacheLength = GetEnvironmentVariableA(
        "HW_DUST_WORLD_CACHE", cacheOverride,
        static_cast<DWORD>(sizeof(cacheOverride)));
    if (cacheLength > 0)
    {
        cachedMode = cacheOverride[0] != '0' ? 1 : 0;
        return cachedMode != 0;
    }

    cachedMode = 0;
    return false;
}

bool ensureOpenMWCloudNoise()
{
    if (presenter.openmwNoiseTexture) return true;
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = openmw_clouds::NoiseWidth;
    desc.Height = openmw_clouds::NoiseHeight;
    desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8_UNORM;
    desc.SampleDesc.Count = 1;
    HRESULT result = presenter.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&presenter.openmwNoiseTexture));
    if (FAILED(result)) { logFailure("Create OpenMW cloud noise", result); return false; }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64 total = 0;
    presenter.device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total);
    D3D12_RESOURCE_DESC upload = {};
    upload.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload.Width = total; upload.Height = upload.DepthOrArraySize = upload.MipLevels = 1;
    upload.SampleDesc.Count = 1; upload.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    result = presenter.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &upload,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&presenter.openmwNoiseUpload));
    if (FAILED(result)) { presenter.openmwNoiseTexture.Reset(); logFailure("Upload OpenMW cloud noise", result); return false; }
    void *mapped = nullptr;
    D3D12_RANGE noRead = {};
    result = presenter.openmwNoiseUpload->Map(0, &noRead, &mapped);
    if (FAILED(result)) { presenter.openmwNoiseTexture.Reset(); presenter.openmwNoiseUpload.Reset(); logFailure("Map OpenMW cloud noise", result); return false; }
    for (unsigned y=0; y<openmw_clouds::NoiseHeight; ++y)
        std::memcpy(static_cast<unsigned char *>(mapped)+footprint.Offset+y*footprint.Footprint.RowPitch,
            openmw_clouds::NoiseRG8+y*openmw_clouds::NoiseWidth*2u, openmw_clouds::NoiseWidth*2u);
    presenter.openmwNoiseUpload->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION from = {}, to = {};
    from.pResource = presenter.openmwNoiseUpload.Get();
    from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; from.PlacedFootprint = footprint;
    to.pResource = presenter.openmwNoiseTexture.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    presenter.commandList->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    D3D12_RESOURCE_BARRIER ready = {};
    ready.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    ready.Transition = {presenter.openmwNoiseTexture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
    presenter.commandList->ResourceBarrier(1, &ready);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R8G8_UNORM; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels = 1;
    auto handle = presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += 11ull*presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(presenter.openmwNoiseTexture.Get(), &srv, handle);
    presenter.openmwNoiseTexture->SetName(L"OpenMW cloud noise: original noise2/noise3 red channels");
    HW_DIAG_FPRINTF(stderr,"[CloudRenderer] OpenMW cloud kernel active; saved volume/grid bounds retained; motion %s; no swirling.\n",
        cloudRenderSettings.motionEnabled ? "enabled" : "OFF");
    return true;
}

void prepareVolumetricDustFrame(PresenterConstants &constants)
{
    constants.godRays = requestedGodRays > 0.0f ? missionGodRayStrength : 0.0f;
    constants.godRaySourceExternal = (constants.godRaySourceExternal & 3u) | missionGodRayTint;
    if (atmosphereCacheEnabled()) constants.godRaySourceExternal |= 16u;
    const bool atmosphereRequested = requestedGodRays > 0.0001f && dustMissionSunValid;
    constants.dustVolumeCount = 0;
    constants.dustLightCount = 0;
    constants.dustRenderEnabled = 0;
    constants.dustWakeCount = 0;
    constants.presenterPadding[0] = 0.0f;
    std::memset(constants.dustGridMinWidth, 0, sizeof(constants.dustGridMinWidth));
    std::memset(constants.dustGridSizeHeight, 0, sizeof(constants.dustGridSizeHeight));
    if (!presenter.dustCameraValid ||
(volumetricDustVolumes.empty() && harvestableNebulaVolumes.empty() && harvestableDustVolumes.empty() && particleFieldVolumes.empty() && !atmosphereRequested))
    {
        return;
    }

    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    if (frame.mappedDustVolumes == nullptr || frame.mappedDustLights == nullptr)
    {
        return;
    }
    if (!ensureOpenMWCloudNoise()) return;

    const float cameraX = presenter.dustCameraPosition[0];
    const float cameraY = presenter.dustCameraPosition[1];
    const float cameraZ = presenter.dustCameraPosition[2];

    constants.dustCameraPositionMaxDistance[0] = cameraX;
    constants.dustCameraPositionMaxDistance[1] = cameraY;
    constants.dustCameraPositionMaxDistance[2] = cameraZ;
    /* Filled below from the union of active clouds. This is only the far end
       of the shared camera froxel grid; it is never derived from a ray/shape
       intersection and is placed many radii beyond numerically visible tails. */
    constants.dustCameraPositionMaxDistance[3] = 3.0e30f;
    for (unsigned int channel = 0; channel < 3; ++channel)
    {
        constants.dustRayUv00[channel] = presenter.dustRayUv00[channel];
        constants.dustRayUv10[channel] = presenter.dustRayUv10[channel];
        constants.dustRayUv01SunStrength[channel] = presenter.dustRayUv01[channel];
    }
    /* Reuse unused vector .w channels for exact raster-depth reconstruction
       without growing the 60-DWORD presenter root-constant block. */
    constants.dustRayUv00[3] = presenter.dustProjectionP10;
    constants.dustRayUv10[3] = presenter.dustProjectionP14;
    constants.dustRayUv01SunStrength[3] =
        dustMissionSunValid ? 0.82f * requestedPrimarySunStrength : 0.0f;
    constants.dustSunDirectionValid[0] = dustMissionSunDirection[0];
    constants.dustSunDirectionValid[1] = dustMissionSunDirection[1];
    constants.dustSunDirectionValid[2] = dustMissionSunDirection[2];
    constants.dustSunDirectionValid[3] = dustMissionSunValid ? 1.0f : 0.0f;
    constants.dustSunColor[0] = dustMissionSunColor[0];
    constants.dustSunColor[1] = dustMissionSunColor[1];
    constants.dustSunColor[2] = dustMissionSunColor[2];
    constants.dustSunColor[3] =
        presenter.dustProjectionValid && presenter.nativeRasterDepthSampleReady ? 1.0f : 0.0f;

    float ambient[3] = {
        dustMissionSkyAvailable ? dustMissionAmbientColor[0] * requestedSkyAmbientStrength : 0.025f,
        dustMissionSkyAvailable ? dustMissionAmbientColor[1] * requestedSkyAmbientStrength : 0.025f,
        dustMissionSkyAvailable ? dustMissionAmbientColor[2] * requestedSkyAmbientStrength : 0.030f};

    volumetricDustLightScratch.clear();
    if (volumetricDustLightScratch.capacity() < maximumVolumetricDustLights)
        volumetricDustLightScratch.reserve(maximumVolumetricDustLights);

    const auto normalizeDustLightDirection = [](float direction[3])
    {
        const float lengthSquared = direction[0] * direction[0] +
            direction[1] * direction[1] + direction[2] * direction[2];
        if (lengthSquared > 0.00000001f)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            direction[0] *= inverseLength;
            direction[1] *= inverseLength;
            direction[2] *= inverseLength;
        }
        else
        {
            direction[0] = 0.0f;
            direction[1] = 0.0f;
            direction[2] = -1.0f;
        }
    };

    for (const HWModernMissionLightEmitter &source : missionAuthoringLightEmitters)
    {
        if (!source.enabled || source.intensity <= 0.0001f) continue;
        if (source.type == HW_MODERN_MISSION_LIGHT_AMBIENT)
        {
            const float strength = source.intensity * requestedAuthoredLightStrength * 0.55f;
            ambient[0] += source.color[0] * strength;
            ambient[1] += source.color[1] * strength;
            ambient[2] += source.color[2] * strength;
            continue;
        }
        if (volumetricDustLightScratch.size() >= maximumVolumetricDustLights) break;
        GpuDustLight light = {};
        light.type = source.type == HW_MODERN_MISSION_LIGHT_SPOT ? 3u : 2u;
        std::memcpy(light.position, source.position, sizeof(light.position));
        std::memcpy(light.direction, source.direction, sizeof(light.direction));
        normalizeDustLightDirection(light.direction);
        std::memcpy(light.color, source.color, sizeof(light.color));
        light.intensity = source.intensity * requestedAuthoredLightStrength;
        light.radius = std::max(1.0f, source.radius);
        light.coneCos = volumetricDustDegreesToCosine(source.coneAngle);
        light.edgeCos = volumetricDustDegreesToCosine(source.coneAngle + source.edgeAngle);
        volumetricDustLightScratch.push_back(light);
    }

    if (!missionAuthoringReplacesMapLightsForDust)
    {
        for (const HWModernMapLightEmitter &source : mapLightEmitters)
        {
            if (volumetricDustLightScratch.size() >= maximumVolumetricDustLights) break;
            if (source.intensity <= 0.0001f || source.type == 0 || source.type == 5)
                continue;
            GpuDustLight light = {};
            light.type = source.type == 2 ? 3u : 2u;
            std::memcpy(light.position, source.renderPosition, sizeof(light.position));
            std::memcpy(light.direction, source.renderDirection, sizeof(light.direction));
            normalizeDustLightDirection(light.direction);
            std::memcpy(light.color, source.color, sizeof(light.color));
            light.intensity = std::max(0.0f, source.intensity) * requestedAuthoredLightStrength;
            light.radius = 250000.0f;
            light.coneCos = volumetricDustDegreesToCosine(source.coneAngle);
            light.edgeCos = volumetricDustDegreesToCosine(source.coneAngle + source.edgeAngle);
            volumetricDustLightScratch.push_back(light);
        }
    }

    for (const HWModernDynamicLightEmitter &source : dynamicLightEmitters)
    {
        if (volumetricDustLightScratch.size() >= maximumVolumetricDustLights) break;
        if (source.intensity <= 0.0001f || source.radius <= 1.0f) continue;
        GpuDustLight light = {};
        light.type = 2u;
        if (source.shape == HW_MODERN_LIGHT_LINE)
        {
            light.position[0] = (source.position[0] + source.endPosition[0]) * 0.5f;
            light.position[1] = (source.position[1] + source.endPosition[1]) * 0.5f;
            light.position[2] = (source.position[2] + source.endPosition[2]) * 0.5f;
        }
        else
        {
            std::memcpy(light.position, source.position, sizeof(light.position));
        }
        std::memcpy(light.color, source.color, sizeof(light.color));
        const bool dedicatedWeaponEndpoint =
            source.source == HW_MODERN_LIGHT_MUZZLE_FLASH ||
            source.source == HW_MODERN_LIGHT_WEAPON_IMPACT;
        light.intensity = std::max(0.0f, source.intensity) *
            (source.source == HW_MODERN_LIGHT_NATIVE_FX ? 1.0f : 0.02f) *
            (dedicatedWeaponEndpoint ? 1.0f : requestedFxLightingStrength);
        light.radius = std::max(1.0f, source.radius);
        volumetricDustLightScratch.push_back(light);
    }

    constants.dustAmbientColorStrength[0] = std::max(0.0f, ambient[0]);
    constants.dustAmbientColorStrength[1] = std::max(0.0f, ambient[1]);
    constants.dustAmbientColorStrength[2] = std::max(0.0f, ambient[2]);
    constants.dustAmbientColorStrength[3] = 1.0f;

    /* Low-noise proof that live F12 authoring actually reaches the volumetric
       upload path. Emit only when the effective authored lighting changes. */
    {
        float localEnergy[3] = {0.0f, 0.0f, 0.0f};
        for (const GpuDustLight &light : volumetricDustLightScratch)
        {
            localEnergy[0] += std::max(0.0f, light.color[0]) * std::max(0.0f, light.intensity);
            localEnergy[1] += std::max(0.0f, light.color[1]) * std::max(0.0f, light.intensity);
            localEnergy[2] += std::max(0.0f, light.color[2]) * std::max(0.0f, light.intensity);
        }
        static int lastSunValid = -1;
        static unsigned int lastLightCount = ~0u;
        static float lastSun[3] = {-1000.0f, -1000.0f, -1000.0f};
        static float lastAmbient[3] = {-1000.0f, -1000.0f, -1000.0f};
        static float lastLocalEnergy[3] = {-1000.0f, -1000.0f, -1000.0f};
        bool changed = lastSunValid != (dustMissionSunValid ? 1 : 0) ||
                       lastLightCount != static_cast<unsigned int>(volumetricDustLightScratch.size());
        for (unsigned int channel = 0; channel < 3; ++channel)
        {
            changed = changed || std::fabs(lastSun[channel] - dustMissionSunColor[channel]) > 0.01f ||
                                std::fabs(lastAmbient[channel] - ambient[channel]) > 0.01f ||
                                std::fabs(lastLocalEnergy[channel] - localEnergy[channel]) > 0.01f;
        }
        if (changed)
        {
            HW_DIAG_FPRINTF(stderr,
                "[ModernGraphics] Dust authored lighting live: sunValid=%d "
                "sunRGB=(%.3f,%.3f,%.3f) sunStrength=%.2f "
                "ambientRGB=(%.3f,%.3f,%.3f) localLights=%zu "
                "localRGBxI=(%.3f,%.3f,%.3f).\n",
                dustMissionSunValid ? 1 : 0,
                dustMissionSunColor[0], dustMissionSunColor[1], dustMissionSunColor[2],
                dustMissionSunValid ? 0.82f * requestedPrimarySunStrength : 0.0f,
                ambient[0], ambient[1], ambient[2],
                volumetricDustLightScratch.size(),
                localEnergy[0], localEnergy[1], localEnergy[2]);
            lastSunValid = dustMissionSunValid ? 1 : 0;
            lastLightCount = static_cast<unsigned int>(volumetricDustLightScratch.size());
            for (unsigned int channel = 0; channel < 3; ++channel)
            {
                lastSun[channel] = dustMissionSunColor[channel];
                lastAmbient[channel] = ambient[channel];
                lastLocalEnergy[channel] = localEnergy[channel];
            }
        }
    }

    volumetricDustScratch.clear();
    if (volumetricDustScratch.capacity() < maximumVolumetricDustVolumes)
        volumetricDustScratch.reserve(maximumVolumetricDustVolumes);
    for (const HWModernVolumetricDustVolume &source : volumetricDustVolumes)
    {
        if (!source.enabled || source.density <= 0.00001f) continue;
        volumetricDustScratch.push_back(source);
        if (volumetricDustScratch.size() >= maximumVolumetricDustVolumes) break;
    }
    for (const HWModernVolumetricDustVolume &source : harvestableNebulaVolumes)
    {
        if (volumetricDustScratch.size() >= maximumVolumetricDustVolumes) break;
        if (!source.enabled || source.density <= 0.00001f) continue;
        volumetricDustScratch.push_back(source);
        if (volumetricDustScratch.size() >= maximumVolumetricDustVolumes) break;
    }
    for (const HWModernVolumetricDustVolume &source : harvestableDustVolumes)
    {
        if (volumetricDustScratch.size() >= maximumVolumetricDustVolumes) break;
        if (!source.enabled || source.density <= 0.00001f) continue;
        volumetricDustScratch.push_back(source);
        // Keep this distinction out of authored files and gameplay resources.
        volumetricDustScratch.back().enabled |= 2u;
    }
    const auto frontDistance = [cameraX, cameraY, cameraZ](
        const HWModernVolumetricDustVolume &volume)
    {
        float half[3] = {
            std::max(1.0f, volume.size[0] * 0.5f),
            std::max(1.0f, volume.size[1] * 0.5f),
            std::max(1.0f, volume.size[2] * 0.5f)};
        if (volume.shape == HW_MODERN_DUST_SPHERE)
        {
            const float radius = std::max(
                1.0f, (half[0] + half[1] + half[2]) / 3.0f);
            half[0] = half[1] = half[2] = radius;
        }
        const float radius = std::sqrt(
            half[0] * half[0] + half[1] * half[1] + half[2] * half[2]);
        const float dx = volume.position[0] - cameraX;
        const float dy = volume.position[1] - cameraY;
        const float dz = volume.position[2] - cameraZ;
        return std::max(0.0f, std::sqrt(dx * dx + dy * dy + dz * dz) -
                              radius);
    };
    /* Keep authored clouds and ropes first in the shared budget. Select nearby
       automatic fields before truncation so distant map-order entries cannot
       displace the cloud around the camera. */
    static std::vector<const HWModernVolumetricDustVolume *> fieldCandidates;
    fieldCandidates.clear();
    const auto fieldView = hwdust::makeViewFrustum(
        presenter.dustRayUv00, presenter.dustRayUv10, presenter.dustRayUv01,
        2.0f / std::max(1u, presenter.dustScreenWidth),
        2.0f / std::max(1u, presenter.dustScreenHeight));
    for (const auto &field : particleFieldVolumes)
        if (field.enabled && field.density > .00001f) {
            const float radius=.5f*std::sqrt(field.size[0]*field.size[0]+field.size[1]*field.size[1]+field.size[2]*field.size[2]);
            if (!hwdust::supportSphereIntersectsView(field.position, radius,
                presenter.dustCameraPosition, fieldView)) continue;
            fieldCandidates.push_back(&field);
        }
    std::stable_sort(fieldCandidates.begin(), fieldCandidates.end(),
        [&frontDistance](const auto *a, const auto *b){return frontDistance(*a)<frontDistance(*b);});
    unsigned int packedGridValues = 0;
    static std::vector<const HWModernVolumetricDustVolume *> packedFields;
    packedFields.clear();
    std::uint64_t gridFingerprint=particleDensityRevision;
    for (const auto *field : fieldCandidates) {
        if (volumetricDustScratch.size() >= maximumVolumetricDustVolumes) break;
        if (field->shape != HW_MODERN_DUST_PARTICLE_DENSITY ||
            static_cast<size_t>(field->shapeSeed) + particleGridValuesPerBrick > particleFieldDensity.size()) continue;
        volumetricDustScratch.push_back(*field);
        volumetricDustScratch.back().shapeSeed = packedGridValues;
        packedFields.push_back(field);
        gridFingerprint=(gridFingerprint ^ field->shapeSeed)*1099511628211ull;
        packedGridValues += particleGridValuesPerBrick;
    }
    if(frame.particleGridFingerprint!=gridFingerprint) {
      unsigned int base=0;
      for(const auto *field : packedFields) {
        unsigned char *destination=reinterpret_cast<unsigned char *>(frame.mappedDustVolumes+maximumVolumetricDustVolumes);
        std::memcpy(destination+static_cast<size_t>(base)*sizeof(float),
            particleFieldDensity.data()+field->shapeSeed,particleGridValuesPerBrick*sizeof(float));
        base+=particleGridValuesPerBrick;
      }
      frame.particleGridFingerprint=gridFingerprint;
    }
    /* Front-to-back volume order improves early transmittance termination when
       several huge clouds overlap. Center-distance sorting could process a
       farther giant volume first simply because its centre happened to be
       closer, multiplying work and producing a less physical composition. */
    std::stable_sort(volumetricDustScratch.begin(), volumetricDustScratch.end(),
        [&frontDistance](const HWModernVolumetricDustVolume &left,
                         const HWModernVolumetricDustVolume &right)
        {
            return frontDistance(left) < frontDistance(right);
        });

    /* Build transient wake data before uploading volumes so every volume can
       carry compact relevance masks. The high flag bits are runtime-only and
       never written back to the authored map/editor state. */
    /* Ship wakes were removed: authored density remains immutable. */
    volumetricDustWakeScratch.clear();

    const auto volumeInfluenceRadius = [](const HWModernVolumetricDustVolume &volume)
    {
        float half[3] = {
            std::max(1.0f, volume.size[0] * 0.5f),
            std::max(1.0f, volume.size[1] * 0.5f),
            std::max(1.0f, volume.size[2] * 0.5f)};
        if (volume.shape == HW_MODERN_DUST_SPHERE)
        {
            const float radius = std::max(
                1.0f, (half[0] + half[1] + half[2]) / 3.0f);
            half[0] = half[1] = half[2] = radius;
        }
        const float coreRadius = std::sqrt(
            half[0] * half[0] + half[1] * half[1] + half[2] * half[2]);
        const float largestHalf = std::max(half[0], std::max(half[1], half[2]));
        const float variation = std::max(0.0f, std::min(
            1.0f, volume.shapeVariation * 0.75f));
        /* Deliberately generous: masks may include an irrelevant source, but
           must never reject a source capable of visibly affecting the soft
           exterior tail. */
        return coreRadius * (1.0f + 0.60f * variation) +
               largestHalf * 0.85f;
    };

    const auto pointSegmentDistanceSquared = [](
        const float point[3], const float start[3], const float end[3])
    {
        const float sx = end[0] - start[0];
        const float sy = end[1] - start[1];
        const float sz = end[2] - start[2];
        const float px = point[0] - start[0];
        const float py = point[1] - start[1];
        const float pz = point[2] - start[2];
        const float segmentLengthSquared = sx * sx + sy * sy + sz * sz;
        const float t = segmentLengthSquared > 0.000001f
            ? std::max(0.0f, std::min(1.0f,
                (px * sx + py * sy + pz * sz) / segmentLengthSquared))
            : 0.0f;
        const float dx = point[0] - (start[0] + sx * t);
        const float dy = point[1] - (start[1] + sy * t);
        const float dz = point[2] - (start[2] + sz * t);
        return dx * dx + dy * dy + dz * dz;
    };

    for (HWModernVolumetricDustVolume &volume : volumetricDustScratch)
    {
        const float volumeRadius = volumeInfluenceRadius(volume);
        unsigned int wakeMask = 0u;
        for (unsigned int wakeIndex = 0u;
             wakeIndex < volumetricDustWakeScratch.size() && wakeIndex < 12u;
             ++wakeIndex)
        {
            const GpuDustWake &wake = volumetricDustWakeScratch[wakeIndex];
            float end[3] = {
                wake.position[0] - wake.direction[0] * wake.length,
                wake.position[1] - wake.direction[1] * wake.length,
                wake.position[2] - wake.direction[2] * wake.length};
            const float influence = volumeRadius + wake.radius * 2.4f;
            if (pointSegmentDistanceSquared(
                    volume.position, wake.position, end) <= influence * influence)
            {
                wakeMask |= 1u << wakeIndex;
            }
        }

        unsigned int lightMask = 0u;
        for (unsigned int lightIndex = 0u;
             lightIndex < volumetricDustLightScratch.size() && lightIndex < 12u;
             ++lightIndex)
        {
            const GpuDustLight &light = volumetricDustLightScratch[lightIndex];
            const float dx = light.position[0] - volume.position[0];
            const float dy = light.position[1] - volume.position[1];
            const float dz = light.position[2] - volume.position[2];
            const float influence = volumeRadius + std::max(1.0f, light.radius);
            if (dx * dx + dy * dy + dz * dz <= influence * influence)
            {
                lightMask |= 1u << lightIndex;
            }
        }

        volume.flags = (volume.flags & 0x000000ffu) |
                       ((wakeMask & 0x0fffu) << 8u) |
                       ((lightMask & 0x0fffu) << 20u);
    }

    const unsigned int volumeCount =
        static_cast<unsigned int>(volumetricDustScratch.size());
    const unsigned int lightCount =
        static_cast<unsigned int>(volumetricDustLightScratch.size());
    const unsigned int wakeCount =
        static_cast<unsigned int>(volumetricDustWakeScratch.size());
    /* Runtime GPU records are required by both renderers. The world-cache
       bounds below are not: when direct raymarch is active, skip all of that
       axis-aligned grid preparation and upload only the per-volume invariants. */
    const bool useWorldCache = dustWorldCacheRequested();
    float finestRequestedCellSize = 520.0f;
    for (unsigned int volumeIndex = 0; volumeIndex < volumeCount; ++volumeIndex)
    {
        const HWModernVolumetricDustVolume &volume =
            volumetricDustScratch[volumeIndex];
        volumetricDustPrecomputeVolume(volume, frame.mappedDustVolumes[volumeIndex]);
        if (!useWorldCache) continue;
        const float scale = std::max(volume.noiseScale, 0.000005f);
        const float featureSize = 1.0f / (scale * 1.65f);
        finestRequestedCellSize = std::min(finestRequestedCellSize,
            std::max(240.0f, std::min(520.0f, featureSize * 0.75f)));
    }

    // Conservative screen tile lists enclosing the exact rotated 2.6 support.
    // Long thin ropes no longer inflate into enormous all-screen spheres.
    // Near-plane intersections keep every tile; no camera-distance cutoff.
    std::memset(frame.mappedDustTiles, 0,
        dustTileColumns * dustTileRows * dustTileWords * sizeof(unsigned int));
    float basisX[3], basisY[3], forward[3];
    float xLength2 = 0, yLength2 = 0, forwardLength2 = 0;
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        basisX[axis] = presenter.dustRayUv10[axis] - presenter.dustRayUv00[axis];
        basisY[axis] = presenter.dustRayUv01[axis] - presenter.dustRayUv00[axis];
        forward[axis] = presenter.dustRayUv00[axis] +
            0.5f * (basisX[axis] + basisY[axis]);
        xLength2 += basisX[axis] * basisX[axis];
        yLength2 += basisY[axis] * basisY[axis];
        forwardLength2 += forward[axis] * forward[axis];
    }
    const float xLength = std::sqrt(std::max(xLength2, 1e-12f));
    const float yLength = std::sqrt(std::max(yLength2, 1e-12f));
    const float fLength = std::sqrt(std::max(forwardLength2, 1e-12f));
    for (unsigned int index = 0; index < volumeCount; ++index)
    {
        const auto &volume = volumetricDustScratch[index];
        float half[3];
        for (unsigned int axis = 0; axis < 3; ++axis)
        {
            half[axis] = std::max(1.0f, volume.size[axis] * 0.5f);
        }
        if (volume.shape == HW_MODERN_DUST_SPHERE)
        {
            const float radius = (half[0]+half[1]+half[2])/3.0f;
            half[0]=half[1]=half[2]=radius;
        }
        for (float &axis : half) axis *= volume.shape == HW_MODERN_DUST_PARTICLE_DENSITY ? 1.0f : 2.6f;
        if (volume.shape == HW_MODERN_DUST_ROPE)
            hwdust::ropeSupportHalfSize(volume.size, volume.density,
                volume.shapeVariation, half);
        const GpuDustVolume &gpu = frame.mappedDustVolumes[index];
        auto bounds = hwdust::projectSupport(volume.position, half,
            presenter.dustCameraPosition, gpu.rotationXSinCos, gpu.rotationYSinCos,
            gpu.rotationZSinCos, basisX, basisY, forward, xLength, yLength, fLength);
        if (bounds.behindCamera) continue;
        // Include filter footprints and temporal projection jitter at tile
        // boundaries; these margins never reduce the procedural support.
        const float marginX = 2.0f / std::max(1u, presenter.dustScreenWidth);
        const float marginY = 2.0f / std::max(1u, presenter.dustScreenHeight);
        bounds.lo[0] -= marginX; bounds.hi[0] += marginX;
        bounds.lo[1] -= marginY; bounds.hi[1] += marginY;
        if (bounds.hi[0] < 0 || bounds.lo[0] > 1 ||
            bounds.hi[1] < 0 || bounds.lo[1] > 1) continue;
        const float *lo = bounds.lo, *hi = bounds.hi;
        for (unsigned int ty = 0; ty < dustTileRows; ++ty)
        for (unsigned int tx = 0; tx < dustTileColumns; ++tx)
        {
            if ((tx + 1.0f) / dustTileColumns < lo[0] || (float)tx / dustTileColumns > hi[0] ||
                (ty + 1.0f) / dustTileRows < lo[1] || (float)ty / dustTileRows > hi[1]) continue;
            frame.mappedDustTiles[(ty * dustTileColumns + tx) * dustTileWords + index / 32u] |=
                1u << (index % 32u);
        }
    }

    presenter.lastDustTileCandidates = 0;
    for (unsigned int word = 0; word < dustTileColumns * dustTileRows * dustTileWords; ++word)
        for (unsigned int mask = frame.mappedDustTiles[word]; mask != 0; mask &= mask - 1u)
            ++presenter.lastDustTileCandidates;
    // Authored cloud RGB is unlit. Only active atmospheric shafts or the
    // explicit diagnostic density cache can consume these shadow cascades.
    if ((useWorldCache && volumeCount > 0) || atmosphereRequested)
    {
        /* A global union made six Mission 04 clouds spanning hundreds of
           thousands of units share one 128-ish grid; each 400-unit noise
           feature became a multi-thousand-unit blob. Use a world-snapped,
           camera-local clipmap instead. Overlapping cells retain identical
           world centres as the camera moves, so the field stays stable. */
        const bool visibilityOnlyGrid = !useWorldCache;
        float visibilityCellSize = 64.0f;
        if (presenter.godRayFocusValid)
        {
            float distanceSquared = 0.0f;
            for (unsigned axis=0;axis<3;++axis)
            {
                float delta=presenter.dustCameraPosition[axis]-presenter.godRayFocus[axis];
                distanceSquared+=delta*delta;
            }
            const float focusExtent = std::max(1024.0f,
                std::min(73728.0f,2.0f*std::sqrt(distanceSquared)));
            visibilityCellSize=focusExtent/static_cast<float>(dustWorldGridActiveDimension);
        }
        const float cellSize = visibilityOnlyGrid ? visibilityCellSize : finestRequestedCellSize;
        const UINT activeDimension = dustWorldGridActiveDimension;
        const float gridExtent = cellSize * static_cast<float>(activeDimension);
        float forward[3] = {
            presenter.dustRayUv00[0] + presenter.dustRayUv10[0] +
                presenter.dustRayUv01[0],
            presenter.dustRayUv00[1] + presenter.dustRayUv10[1] +
                presenter.dustRayUv01[1],
            presenter.dustRayUv00[2] + presenter.dustRayUv10[2] +
                presenter.dustRayUv01[2]};
        const float forwardLength = std::sqrt(
            forward[0] * forward[0] + forward[1] * forward[1] +
            forward[2] * forward[2]);
        if (forwardLength > 0.000001f)
            for (float &component : forward) component /= forwardLength;
        float gridMinimum[3] = {};
        for (unsigned int axis = 0; axis < 3; ++axis)
        {
            const float biasedCenter = visibilityOnlyGrid && presenter.godRayFocusValid
                ? presenter.godRayFocus[axis]
                : presenter.dustCameraPosition[axis] + forward[axis] * gridExtent * 0.18f;
            // Density-cache cells must remain world locked. The sun-aligned
            // visibility grid is rebuilt every frame and changes cell size
            // with zoom: world-axis snapping only adds discontinuous jumps.
            const float snappedCenter = visibilityOnlyGrid ? biasedCenter : std::floor(
                biasedCenter / cellSize + 0.5f) * cellSize;
            gridMinimum[axis] = snappedCenter - gridExtent * 0.5f;
        }

        constants.dustGridMinWidth[0] = gridMinimum[0];
        constants.dustGridMinWidth[1] = gridMinimum[1];
        constants.dustGridMinWidth[2] = gridMinimum[2];
        constants.dustGridMinWidth[3] = static_cast<float>(activeDimension);
        constants.dustGridSizeHeight[0] = gridExtent;
        constants.dustGridSizeHeight[1] = gridExtent;
        constants.dustGridSizeHeight[2] = gridExtent;
        constants.dustGridSizeHeight[3] = static_cast<float>(activeDimension);
        constants.presenterPadding[0] = static_cast<float>(activeDimension);

        static float lastCellSize = -1.0f;
        if (std::fabs(lastCellSize - cellSize) > 0.1f)
        {
            HW_DIAG_FPRINTF(stderr,
                "[ModernGraphics] World-snapped dust clipmap: %ux%ux%u, "
                "cubic voxel %.1f, extent %.1f world units.\n",
                activeDimension, activeDimension, activeDimension,
                cellSize, gridExtent);
            lastCellSize = cellSize;
        }
    }
    if (lightCount > 0)
        std::memcpy(frame.mappedDustLights, volumetricDustLightScratch.data(),
                    lightCount * sizeof(GpuDustLight));
    if (wakeCount > 0 && frame.mappedDustWakes != nullptr)
        std::memcpy(frame.mappedDustWakes, volumetricDustWakeScratch.data(),
                    wakeCount * sizeof(GpuDustWake));
    constants.dustVolumeCount = volumeCount;
    constants.dustLightCount = lightCount;
    constants.dustWakeCount = wakeCount;
    constants.dustRenderEnabled = volumeCount > 0 || atmosphereRequested ? 1u : 0u;
    presenter.lastDustVolumeCount = volumeCount;
    presenter.lastDustLightCount = lightCount;
    presenter.lastDustWakeCount = wakeCount;
    presenter.lastDustEnabled = constants.dustRenderEnabled;
}

std::wstring shaderCacheDirectory()
{
    wchar_t portableRoot[32768] = {};
    const DWORD portableLength = GetEnvironmentVariableW(L"HW_PORTABLE_ROOT", portableRoot, _countof(portableRoot));
    if (portableLength > 0 && portableLength < _countof(portableRoot))
    {
        std::wstring directory = std::wstring(portableRoot) +
            (GetEnvironmentVariableW(L"HW_LOOSE_DATA",nullptr,0)?L"\\Dev\\ShaderCache":L"\\Development\\ShaderCache");
        CreateDirectoryW(directory.c_str(), nullptr);
        return directory;
    }
    wchar_t localAppData[MAX_PATH] = {};
    DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData,
        static_cast<DWORD>(sizeof(localAppData) / sizeof(localAppData[0])));
    if (length == 0 || length >= sizeof(localAppData) / sizeof(localAppData[0]))
    {
        return std::wstring();
    }
    std::wstring directory(localAppData);
    directory += L"\\HomeworldModern";
    CreateDirectoryW(directory.c_str(), nullptr);
    directory += L"\\ShaderCache";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory;
}

std::wstring shaderCachePath(const wchar_t *name)
{
    std::wstring directory = shaderCacheDirectory();
    if (directory.empty()) return directory;
    directory += L"\\";
    directory += name;
    return directory;
}

bool readCacheFile(const std::wstring &path, std::vector<unsigned char> &bytes)
{
    if (path.empty()) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(file, &size) != FALSE && size.QuadPart > 0 &&
              size.QuadPart <= 64ll * 1024ll * 1024ll;
    if (ok)
    {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                      &read, nullptr) != FALSE && read == bytes.size();
    }
    CloseHandle(file);
    if (!ok) bytes.clear();
    return ok;
}

bool writeCacheFile(const std::wstring &path, const void *data, size_t size)
{
    if (path.empty() || data == nullptr || size == 0 || size > 0xffffffffu)
        return false;
    std::wstring temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, data, static_cast<DWORD>(size), &written,
                        nullptr) != FALSE && written == size;
    FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok)
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

std::uint64_t presenterShaderHash(const char *entryPoint, const char *target,
                                 const D3D_SHADER_MACRO *macros = nullptr,
                                 const char *sourceOverride = nullptr)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto append = [&hash](const void *source, size_t length)
    {
        const unsigned char *bytes = static_cast<const unsigned char *>(source);
        for (size_t index = 0; index < length; ++index)
        {
            hash ^= bytes[index];
            hash *= 1099511628211ull;
        }
    };
    if (sourceOverride) append(sourceOverride, std::strlen(sourceOverride));
    else append(presenterShader, sizeof(presenterShader) - 1);
    append(entryPoint, std::strlen(entryPoint));
    append(target, std::strlen(target));
    static const char cacheVersion[] = "HomeworldModern-0.91.2";
    append(cacheVersion, sizeof(cacheVersion) - 1);
    // Compile definitions are part of the bytecode identity. Preserve the
    // existing keys for the three shaders compiled without definitions.
    for (const D3D_SHADER_MACRO *macro = macros; macro && macro->Name; ++macro)
    {
        append(macro->Name, std::strlen(macro->Name) + 1);
        if (macro->Definition)
            append(macro->Definition, std::strlen(macro->Definition) + 1);
    }
    return hash;
}

void copyAdapterName(const wchar_t *source, char *destination,
                     size_t destinationSize)
{
    if (destinationSize == 0)
    {
        return;
    }
    destination[0] = '\0';
    if (source == nullptr)
    {
        return;
    }
    int written = WideCharToMultiByte(CP_UTF8, 0, source, -1, destination,
                                      static_cast<int>(destinationSize),
                                      nullptr, nullptr);
    if (written <= 0)
    {
        std::snprintf(destination, destinationSize, "Unknown adapter");
    }
    destination[destinationSize - 1] = '\0';
}

HRESULT selectHardwareAdapter(IDXGIFactory6 *factory,
                              ComPtr<IDXGIAdapter1> &selected)
{
    if (factory == nullptr)
    {
        return E_INVALIDARG;
    }
    for (UINT index = 0; ; ++index)
    {
        ComPtr<IDXGIAdapter1> candidate;
        HRESULT result = factory->EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&candidate));
        if (result == DXGI_ERROR_NOT_FOUND)
        {
            return result;
        }
        if (FAILED(result))
        {
            continue;
        }
        DXGI_ADAPTER_DESC1 description = {};
        if (FAILED(candidate->GetDesc1(&description)) ||
            (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(candidate.Get(),
                                        D3D_FEATURE_LEVEL_11_0,
                                        __uuidof(ID3D12Device), nullptr)))
        {
            selected = candidate;
            return S_OK;
        }
    }
}

unsigned int queryMaximumFeatureLevel(ID3D12Device *device)
{
    static const D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
    levels.NumFeatureLevels = static_cast<UINT>(
        sizeof(requestedLevels) / sizeof(requestedLevels[0]));
    levels.pFeatureLevelsRequested = requestedLevels;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,
                                           &levels, sizeof(levels))))
    {
        return 0;
    }
    return static_cast<unsigned int>(levels.MaxSupportedFeatureLevel);
}

const char *raytracingTierName(HWModernRaytracingTier tier)
{
    switch (tier)
    {
        case HW_MODERN_RAYTRACING_TIER_1_1: return "1.1";
        case HW_MODERN_RAYTRACING_TIER_1_0: return "1.0";
        default: return "unavailable";
    }
}

bool waitForFenceValue(UINT64 fenceValue)
{
    if (fenceValue == 0 || presenter.fence->GetCompletedValue() >= fenceValue)
    {
        return true;
    }
    HRESULT result = presenter.fence->SetEventOnCompletion(
        fenceValue, presenter.fenceEvent);
    if (FAILED(result))
    {
        logFailure("ID3D12Fence::SetEventOnCompletion", result);
        return false;
    }
    DWORD waited = WaitForSingleObject(presenter.fenceEvent, INFINITE);
    if (waited != WAIT_OBJECT_0)
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Waiting for D3D12 work failed "
                             "(Win32 error %lu).\n", GetLastError());
        return false;
    }
    return true;
}

bool waitForSubmittedWork()
{
    return waitForFenceValue(presenter.submittedFenceValue);
}

void transition(ID3D12GraphicsCommandList *commandList, ID3D12Resource *resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
// Diagnostic copies consume an already completed flight-slot fence, never
// stall a frame. Capture the real field and real screen scattering, not a
// surrogate test triangle. Temporary, sparse capture until shafts are verified.
void consumeShaftCapture(D3D12FrameContext &frame)
{
#if HW_ENABLE_DIAGNOSTICS

    if (!frame.shaftCapturePending) return;
    wchar_t directory[32768] = {};
    if (!GetEnvironmentVariableW(L"HW_CRASH_DIR", directory, 32768))
    { frame.shaftCapturePending = false; return; }
    const HRESULT comResult = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    for (unsigned kind = 0; kind < 2; ++kind)
    {
        if (!frame.shaftCapture[kind]) continue;
        void *mapped = nullptr;
        D3D12_RANGE range = {0, static_cast<SIZE_T>(frame.shaftCaptureBytes[kind])};
        if (FAILED(frame.shaftCapture[kind]->Map(0, &range, &mapped))) continue;
        auto &footprint = frame.shaftCaptureFootprint[kind].Footprint;
        const UINT imageWidth = kind == 0 ? std::min(footprint.Width,dustWorldGridActiveDimension) : footprint.Width;
        const UINT imageHeight = kind == 0 ? std::min(footprint.Height,dustWorldGridActiveDimension) : footprint.Height;
        const UINT imageDepth = kind == 0 ? std::min(footprint.Depth,dustWorldGridActiveDimension) : footprint.Depth;
        DirectX::ScratchImage image;
        image.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, imageWidth, imageHeight, 1, 1);
        const DirectX::Image *slice = image.GetImage(0, 0, 0);
        unsigned long long shadowCells = 0, totalCells = 0;
        // Sun image: maximum occlusion along Z (XY projection). Screen image:
        // exact scattering RGB and transmission from the production PS.
        for (UINT y=0; y<imageHeight; ++y)
        {
            auto destination = reinterpret_cast<uint16_t *>(slice->pixels+y*slice->rowPitch);
            for (UINT x=0; x<imageWidth; ++x)
            {
                const auto first = reinterpret_cast<const uint16_t *>(
                    static_cast<const unsigned char *>(mapped)+y*footprint.RowPitch)+x*4;
                if (kind == 1) std::memcpy(destination+x*4,first,8);
                else
                {
                    bool shadow = false;
                    for (UINT z=0; z<imageDepth; ++z)
                    {
                        const auto value = reinterpret_cast<const uint16_t *>(
                            static_cast<const unsigned char *>(mapped)+(z*footprint.Height+y)*footprint.RowPitch)+x*4;
                        // Positive half-float alpha: 0x3800 is 0.5.
                        shadow = shadow || value[3]<0x3800;
                        shadowCells += value[3]<0x3800; ++totalCells;
                    }
                    destination[x*4]=destination[x*4+1]=destination[x*4+2]=shadow ? 0 : 0x3c00;
                    destination[x*4+3]=0x3c00;
                }
            }
        }
        D3D12_RANGE noWrite = {0,0}; frame.shaftCapture[kind]->Unmap(0,&noWrite);
        // Scattering image alpha must be opaque for inspection, not its
        // physical transmittance (that would hide the diagnostic image).
        if (kind == 1)
            for (UINT y=0;y<footprint.Height;++y)
                for (UINT x=0;x<footprint.Width;++x)
                    reinterpret_cast<uint16_t *>(slice->pixels+y*slice->rowPitch)[x*4+3]=0x3c00;
        DirectX::ScratchImage display;
        HRESULT result = DirectX::Convert(*slice,DXGI_FORMAT_R8G8B8A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,0.5f,display);
        wchar_t path[32768];
        swprintf_s(path,L"%s\\shafts-%lu-%llu-%s.png",directory,GetCurrentProcessId(),
            frame.shaftCaptureFrame,kind == 0 ? L"sun-visibility" : L"screen-radiance");
        if (SUCCEEDED(result)) result = DirectX::SaveToWICFile(*display.GetImage(0,0,0),
            DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),path);
        HW_DIAG_FPRINTF(stderr,"[SunShaftCapture] kind=%u frame=%llu shadowCells=%llu/%llu saved=%d\n",
            kind,frame.shaftCaptureFrame,shadowCells,totalCells,SUCCEEDED(result));
    }
    frame.shaftCapturePending = false;
    if (SUCCEEDED(comResult)) CoUninitialize();

#else

#endif
}

void recordShaftCapture(ID3D12Resource *resource, unsigned kind)
{
#if HW_ENABLE_DIAGNOSTICS

    if (benchmark.enabled) return;
    // GPU readback and PNG encoding are diagnostic work, not normal gameplay.
    static const bool enabled = [] { const char *value = std::getenv("HW_CAPTURE_SUN_SHAFTS");
        return value && std::strcmp(value,"1")==0; }();
    if (!enabled) return;
    static UINT64 nextCaptureFrame = 0;
    if (!resource || presenter.frameCount < nextCaptureFrame) return;
    auto &frame = presenter.frames[presenter.activeFrameIndex];
    auto description = resource->GetDesc();
    UINT64 bytes=0;
    presenter.device->GetCopyableFootprints(&description,0,1,0,
        &frame.shaftCaptureFootprint[kind],nullptr,nullptr,&bytes);
    if (!frame.shaftCapture[kind] || frame.shaftCaptureBytes[kind]!=bytes)
    {
        frame.shaftCapture[kind].Reset();
        D3D12_HEAP_PROPERTIES heap = {}; heap.Type=D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer = {}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width=bytes; buffer.Height=1; buffer.DepthOrArraySize=1;
        buffer.MipLevels=1; buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(presenter.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
            &buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&frame.shaftCapture[kind])))) return;
        frame.shaftCaptureBytes[kind]=bytes;
    }
    transition(presenter.commandList.Get(),resource,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION from = {},to = {};
    from.pResource=resource; from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource=frame.shaftCapture[kind].Get(); to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint=frame.shaftCaptureFootprint[kind];
    presenter.commandList->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    transition(presenter.commandList.Get(),resource,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    frame.shaftCapturePending=true; frame.shaftCaptureFrame=presenter.frameCount;
    if (kind == 1) nextCaptureFrame=presenter.frameCount+600u;
    HW_DIAG_FPRINTF(stderr,"[SunShaftCapture] queued kind=%u frame=%llu bytes=%llu\n",
        kind,presenter.frameCount,bytes);

#else

#endif
}

void releaseUploadFrames()
{
    for (D3D12FrameContext &frame : presenter.frames)
    {
        if (frame.upload && frame.mappedUpload != nullptr)
        {
            frame.upload->Unmap(0, nullptr);
        }
        if (frame.worldUpload && frame.mappedWorldUpload != nullptr)
        {
            frame.worldUpload->Unmap(0, nullptr);
        }
        if (frame.backgroundUpload && frame.mappedBackgroundUpload != nullptr)
        {
            frame.backgroundUpload->Unmap(0, nullptr);
        }
        if (frame.occluderUpload && frame.mappedOccluderUpload != nullptr)
        {
            frame.occluderUpload->Unmap(0, nullptr);
        }
        frame.mappedUpload = nullptr;
        frame.mappedWorldUpload = nullptr;
        frame.mappedBackgroundUpload = nullptr;
        frame.mappedOccluderUpload = nullptr;
        frame.uploadSize = 0;
        frame.upload.Reset();
        frame.worldUpload.Reset();
        frame.backgroundUpload.Reset();
        frame.occluderUpload.Reset();
    }
}

bool stopFrameGenerationInputs()
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    // A proxy may still consume its tagged inputs on its presentation queue.
    // Disable and drain generation before releasing any referenced resource.
    hwmodern::fsrFrameGenerationSuspend();
    hwmodern::dlaaSetFrameGenerationEnabled(false);
    if (hwmodern::fsrFrameGenerationDisableFailed() || hwmodern::dlaaFrameGenerationDisableFailed())
        return false;
#endif
    frameGenerationInputsReady = false;
    previousFrameGenerationTime = {};
    return true;
}

bool releaseSizeDependentResources()
{
    if (!stopFrameGenerationInputs()) return false;
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::fgEffectGuidesReleaseOutputs();
    hwmodern::upscalerReleaseOutput();
    hwmodern::raytracingReleaseOutput();
#endif
    for (auto &frame : presenter.frames) {
        frame.frameGenerationHudless.Reset();
    }
    releaseUploadFrames();
    presenter.sourceTexture.Reset();
    presenter.worldTexture.Reset();
    presenter.backgroundTexture.Reset();
    presenter.occluderTexture.Reset();
    presenter.sceneInputTexture.Reset();
    presenter.cloudProbeTexture.Reset();
    presenter.cloudProbeRtvHeap.Reset();
    presenter.dustFroxelTexture.Reset();
    for (ComPtr<ID3D12Resource> &texture : presenter.dustScreenTextures)
        texture.Reset();
    presenter.cloudFrameGenerationGuide.Reset();
    presenter.cloudFrameGenerationGuideShaderReadable = true;
    presenter.cloudFrameGenerationGuideValidThisFrame = false;
    for (ComPtr<ID3D12Resource> &buffer : presenter.backBuffers)
    {
        buffer.Reset();
    }
    presenter.uploadFootprint = {};
    presenter.sourceTextureShaderReadable = false;
    presenter.worldTextureShaderReadable = false;
    presenter.backgroundTextureShaderReadable = false;
    presenter.occluderTextureShaderReadable = false;
    presenter.sceneInputTextureShaderReadable = false;
    presenter.sceneInputWidth = 0;
    presenter.sceneInputHeight = 0;
    presenter.dustFroxelWidth = 0;
    presenter.dustFroxelHeight = 0;
    presenter.dustFroxelDepth = 0;
    presenter.dustFroxelShaderReadable = false;
    presenter.dustFroxelValidThisFrame = false;
    presenter.dustScreenWidth = 0;
    presenter.dustScreenHeight = 0;
    presenter.dustScreenWriteIndex = 0;
    presenter.dustScreenShaderReadable[0] = true;
    presenter.dustScreenShaderReadable[1] = true;
    presenter.dustScreenHistoryValid = false;
    return true;
}

void releasePresenter()
{
    benchmarkFinish(false);
    presenter.active = false;
    presenter.visiblePresenter = false;
    presenter.frameOpen = false;
    presenter.frameCaptured = false;
    presenter.worldFrameCaptured = false;
    presenter.backgroundFrameCaptured = false;
    presenter.occluderFrameCaptured = false;
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::fgZoomCaptureFinish(presenter.fence.Get());
    hwmodern::fsrFrameGenerationRelease();
    hwmodern::fgEffectGuidesReleaseDevice();
    hwmodern::dlaaReleaseDevice();
#endif
    releaseSizeDependentResources();
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingShutdown();
    hwmodern::upscalerReleaseDevice();
#endif
    presenter.presenterPipeline.Reset();
    presenter.scenePrepassPipeline.Reset();
    presenter.dustFroxelPipeline.Reset();
    presenter.dustSunVisibilityPipeline.Reset();
    presenter.dustScreenPipeline.Reset();
    presenter.presenterVertexShader.Reset();
    presenter.cloudFrameGenerationGuidePipeline.Reset();
    presenter.cloudFrameGenerationGuideRtvHeap.Reset();
    presenter.frameGenerationGuidesNeeded = false;
    presenter.cloudFrameGenerationGuideFailed = false;
    presenter.dustFroxelRootSignature.Reset();
    presenter.openmwNoiseTexture.Reset();
    presenter.openmwNoiseUpload.Reset();
    presenter.rootSignature.Reset();
    presenter.timestampQueryHeap.Reset();
    presenter.srvHeap.Reset();
    presenter.rtvHeap.Reset();
    presenter.commandList.Reset();
    for (D3D12FrameContext &frame : presenter.frames)
    {
        if (frame.dustVolumeUpload && frame.mappedDustVolumes)
            frame.dustVolumeUpload->Unmap(0, nullptr);
        if (frame.dustLightUpload && frame.mappedDustLights)
            frame.dustLightUpload->Unmap(0, nullptr);
        if (frame.dustWakeUpload && frame.mappedDustWakes)
            frame.dustWakeUpload->Unmap(0, nullptr);
        frame.mappedDustVolumes = nullptr;
        frame.mappedDustLights = nullptr;
        frame.mappedDustWakes = nullptr;
        frame.dustVolumeUpload.Reset();
        if (frame.dustTileUpload && frame.mappedDustTiles)
            frame.dustTileUpload->Unmap(0, nullptr);
        frame.mappedDustTiles = nullptr;
        frame.dustTileUpload.Reset();
        frame.dustLightUpload.Reset();
        if (frame.dustSunConstantsUpload && frame.mappedDustSunConstants)
            frame.dustSunConstantsUpload->Unmap(0, nullptr);
        frame.mappedDustSunConstants = nullptr;
        frame.dustSunConstantsUpload.Reset();
        frame.dustWakeUpload.Reset();
        frame.timestampReadback.Reset();
        frame.timestampPending = false;
        frame.rayCommands.Reset(); frame.postCommands.Reset();
        frame.rayAllocator.Reset(); frame.postAllocator.Reset();
        frame.allocator.Reset();
        frame.fenceValue = 0;
    }
    if (presenter.swapchain)
    {
        presenter.swapchain->SetFullscreenState(FALSE, nullptr);
    }
    presenter.swapchain.Reset();
    presenter.queue.Reset();
    presenter.fence.Reset();
    presenter.raytracingDevice.Reset();
    presenter.device.Reset();
    presenter.adapter.Reset();
    presenter.factory.Reset();
    if (presenter.fenceEvent != nullptr)
    {
        CloseHandle(presenter.fenceEvent);
        presenter.fenceEvent = nullptr;
    }
    presenter.window = nullptr;
    presenter.rtvDescriptorSize = 0;
    presenter.srvDescriptorSize = 0;
    presenter.width = 0;
    presenter.height = 0;
    presenter.nextFenceValue = 1;
    presenter.submittedFenceValue = 0;
    presenter.activeFrameIndex = 0;
    presenter.frameCount = 0;
    presenter.nativePresentCountValid = false;
    presentedFrameRate.reset();
    presenter.dustCameraValid = false;
    presenter.backgroundReadbackMilliseconds = 0.0;
    presenter.worldReadbackMilliseconds = 0.0;
    presenter.frameReadbackMilliseconds = 0.0;
    presenter.dxrRecordMilliseconds = 0.0;
    presenter.backgroundReadbackCount = 0;
    presenter.worldReadbackCount = 0;
    presenter.frameReadbackCount = 0;
    presenter.dxrRecordCount = 0;
    presenter.allowTearing = false;
    presenter.vsyncMode = 1;
    dynamicLightEmitters.clear();
    timedDynamicLightEmitters.clear();
}

bool compileShader(const char *entryPoint, const char *target,
                   ComPtr<ID3DBlob> &bytecode, const char *taskId,
                   const char *label, const D3D_SHADER_MACRO *macros = nullptr,
                   const char *sourceName = "HomeworldModernPresenter.hlsl",
                   const char *sourceOverride = nullptr)
{
    hwWindowsSplashTaskBegin(taskId, label);
    hwWindowsSplashUpdate("Checking validated shader cache");
    const std::uint64_t hash = presenterShaderHash(entryPoint, target, macros, sourceOverride);
    wchar_t cacheName[160] = {};
    swprintf_s(cacheName, L"Presenter-%S-%S-%016llx.cso", entryPoint, target,
               static_cast<unsigned long long>(hash));
    const std::wstring cachePath = shaderCachePath(cacheName);
    std::vector<unsigned char> cached;
    bool cachedBytecodeValid = false;
    if (readCacheFile(cachePath, cached) && cached.size() >= 32 &&
        std::memcmp(cached.data(), "DXBC", 4) == 0)
    {
        std::uint32_t declaredSize = 0;
        std::memcpy(&declaredSize, cached.data() + 24,
                    sizeof(declaredSize));
        cachedBytecodeValid = declaredSize == cached.size();
    }
    if (cachedBytecodeValid &&
        SUCCEEDED(D3DCreateBlob(cached.size(), &bytecode)))
    {
        std::memcpy(bytecode->GetBufferPointer(), cached.data(), cached.size());
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Shader cache hit: %s/%s.\n",
                     entryPoint, target);
        hwWindowsSplashTaskComplete(taskId, "Loaded validated cached shader");
        return true;
    }
    if (!cached.empty())
    {
        DeleteFileW(cachePath.c_str());
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] Rejected invalid shader cache entry: %s/%s.\n",
            entryPoint, target);
    }

    ComPtr<ID3DBlob> errors;
    hwWindowsSplashUpdate("Compiling shader with D3DCompile; waiting for compiler");
    const char *shaderSource = sourceOverride ? sourceOverride : presenterShader;
    const size_t shaderBytes = sourceOverride ? std::strlen(sourceOverride) : sizeof(presenterShader) - 1;
    HRESULT result = D3DCompile(shaderSource, shaderBytes,
                                sourceName, macros,
                                nullptr, entryPoint, target,
                                D3DCOMPILE_ENABLE_STRICTNESS |
                                D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                0, &bytecode, &errors);
    if (FAILED(result))
    {
        if (errors && errors->GetBufferPointer())
        {
            HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Presenter shader error: %s\n",
                         static_cast<const char *>(errors->GetBufferPointer()));
        }
        logFailure("D3DCompile", result);
        return false;
    }
    hwWindowsSplashUpdate("Saving compiled shader cache");
    if (writeCacheFile(cachePath, bytecode->GetBufferPointer(),
                       bytecode->GetBufferSize()))
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Shader cache stored: %s/%s.\n",
                     entryPoint, target);
    }
    hwWindowsSplashTaskComplete(taskId, "Shader compilation complete");
    return true;
}

bool compileDustFroxelShader(ComPtr<ID3DBlob> &bytecode)
{
    D3D_SHADER_MACRO macros[] = {
        { "DUST_FROXEL_COMPUTE", "1" },
        { nullptr, nullptr }
    };
    return compileShader("BuildDustFroxelCS", "cs_5_0", bytecode,
        "renderer.shader.dust-froxel", "Cloud grid compute shader", macros,
        "HomeworldModernDustFroxel.hlsl");
}

void resetGodRaySource(int mapIdentity)
{
    presenter.godRayMapIdentity = mapIdentity;
    presenter.godRayCenterX = 0.5f;
    presenter.godRayCenterY = 0.42f;
    presenter.godRayWorldDirection[0] = 0.0f;
    presenter.godRayWorldDirection[1] = 0.0f;
    presenter.godRayWorldDirection[2] = -1.0f;
    presenter.godRaySourceLocked = false;
    presenter.godRaySourceVisible = false;
}
void projectLockedGodRaySource(float verticalFieldOfViewDegrees,
                               float aspectRatio,
                               const float viewMatrix[16])
{
    if (!presenter.godRaySourceLocked || viewMatrix == nullptr)
    {
        presenter.godRaySourceVisible = false;
        return;
    }
    const float wx = presenter.godRayWorldDirection[0];
    const float wy = presenter.godRayWorldDirection[1];
    const float wz = presenter.godRayWorldDirection[2];
    const float viewX = viewMatrix[0] * wx + viewMatrix[4] * wy + viewMatrix[8] * wz;
    const float viewY = viewMatrix[1] * wx + viewMatrix[5] * wy + viewMatrix[9] * wz;
    const float viewZ = viewMatrix[2] * wx + viewMatrix[6] * wy + viewMatrix[10] * wz;
    if (viewZ >= -0.0001f)
    {
        presenter.godRaySourceVisible = false;
        return;
    }

    constexpr float pi = 3.14159265358979323846f;
    const float safeAspect = aspectRatio > 0.01f ? aspectRatio : 1.0f;
    const float tanHalfFov = std::tan(
        std::max(1.0f, std::min(verticalFieldOfViewDegrees, 179.0f)) *
        (pi / 360.0f));
    const float ndcX = viewX / (-viewZ * tanHalfFov * safeAspect);
    const float ndcY = viewY / (-viewZ * tanHalfFov);
    presenter.godRayCenterX = ndcX * 0.5f + 0.5f;
    presenter.godRayCenterY = ndcY * 0.5f + 0.5f;
    presenter.godRaySourceVisible =
        presenter.godRayCenterX >= 0.0f && presenter.godRayCenterX <= 1.0f &&
        presenter.godRayCenterY >= 0.0f && presenter.godRayCenterY <= 1.0f;
}

bool updateDustCameraFromRenderView(float verticalFieldOfViewDegrees,
                                    float aspectRatio,
                                    const float viewMatrix[16])
{
    if (viewMatrix == nullptr) return false;
    for (unsigned int index = 0; index < 16; ++index)
    {
        if (!std::isfinite(viewMatrix[index])) return false;
    }

    constexpr float pi = 3.14159265358979323846f;
    const float safeAspect = aspectRatio > 0.01f ? aspectRatio : 1.0f;
    const float safeFov = std::max(1.0f,
        std::min(verticalFieldOfViewDegrees, 179.0f));
    const float tanHalfFov = std::tan(safeFov * (pi / 360.0f));
    if (!(tanHalfFov > 0.000001f) || !std::isfinite(tanHalfFov)) return false;

    /* OpenGL supplies a column-major rigid world->view matrix.  Recover the
       ACTUAL render-camera origin from -R^T*t instead of trusting a separate
       editor/camera callback. */
    const float tx = viewMatrix[12];
    const float ty = viewMatrix[13];
    const float tz = viewMatrix[14];
    const float eye[3] = {
        -(viewMatrix[0] * tx + viewMatrix[1] * ty + viewMatrix[2] * tz),
        -(viewMatrix[4] * tx + viewMatrix[5] * ty + viewMatrix[6] * tz),
        -(viewMatrix[8] * tx + viewMatrix[9] * ty + viewMatrix[10] * tz)
    };
    if (!std::isfinite(eye[0]) || !std::isfinite(eye[1]) ||
        !std::isfinite(eye[2])) return false;

    /* Store UNNORMALIZED camera-plane vectors for UV (0,0), (1,0), (0,1).
       HLSL linearly interpolates these and normalizes once per pixel.  This is
       exact perspective-ray reconstruction for the same view matrix used to
       draw the world and cannot become a camera-facing billboard basis. */
    const float ndcX[3] = {-1.0f, 1.0f, -1.0f};
    /* PresentVS supplies BOTTOM-UP UVs: camera-up pixels have uv.y == 1.
       The cloud RT is subsequently sampled top-down, exactly once. Build
       rays in the input UV convention, matching native compatibility depth;
       otherwise correcting the RT lookup exposes vertically mirrored rays. */
    const float ndcY[3] = {-1.0f, -1.0f, 1.0f};
    float rays[3][3] = {};
    for (unsigned int rayIndex = 0; rayIndex < 3; ++rayIndex)
    {
        const float viewX = ndcX[rayIndex] * safeAspect * tanHalfFov;
        const float viewY = ndcY[rayIndex] * tanHalfFov;
        const float viewZ = -1.0f;
        rays[rayIndex][0] = viewMatrix[0] * viewX +
                            viewMatrix[1] * viewY +
                            viewMatrix[2] * viewZ;
        rays[rayIndex][1] = viewMatrix[4] * viewX +
                            viewMatrix[5] * viewY +
                            viewMatrix[6] * viewZ;
        rays[rayIndex][2] = viewMatrix[8] * viewX +
                            viewMatrix[9] * viewY +
                            viewMatrix[10] * viewZ;
        const float lengthSquared =
            rays[rayIndex][0] * rays[rayIndex][0] +
            rays[rayIndex][1] * rays[rayIndex][1] +
            rays[rayIndex][2] * rays[rayIndex][2];
        if (!(lengthSquared > 1.0e-10f) || !std::isfinite(lengthSquared))
            return false;
    }

    for (unsigned int channel = 0; channel < 3; ++channel)
    {
        presenter.dustCameraPosition[channel] = eye[channel];
        presenter.dustRayUv00[channel] = rays[0][channel];
        presenter.dustRayUv10[channel] = rays[1][channel];
        presenter.dustRayUv01[channel] = rays[2][channel];
    }
    presenter.dustCameraValid = true;
    presenter.dustCameraExplicitThisFrame = true;

    static bool logged = false;
    if (!logged)
    {
        logged = true;
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] Dust camera hard-locked to the actual render "
            "view matrix; legacy corner-ray callback is no longer authoritative.\n");
    }
    return true;
}

void updateDustDepthProjection(const float projectionMatrix[16])
{
    presenter.dustProjectionValid = false;
    if (projectionMatrix == nullptr) return;
    for (unsigned int index = 0; index < 16; ++index)
        if (!std::isfinite(projectionMatrix[index])) return;
    /* Homeworld uses the conventional OpenGL perspective matrix, including
       its infinite-far variant: clip.w=-view.z, P[11]=-1, P[15]=0. */
    if (std::fabs(projectionMatrix[11] + 1.0f) > 0.01f ||
        std::fabs(projectionMatrix[15]) > 0.01f ||
        std::fabs(projectionMatrix[14]) <= 0.000001f)
        return;
    presenter.dustProjectionP10 = projectionMatrix[10];
    presenter.dustProjectionP14 = projectionMatrix[14];
    presenter.dustProjectionValid = true;
}

bool createPresenterPipeline()
{
    hwWindowsSplashTaskBegin("renderer.root.presenter", "Presentation root signature");
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 4;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE motionRange = {};
    motionRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    /* t4 motion, t5 background, t6 OpenMW noise lookup,
       t7 god-ray occluder, and t8 is the exact native-raster D32 depth sampled
       by volumetric dust. */
    motionRange.NumDescriptors = 5;
    motionRange.BaseShaderRegister = 4;
    motionRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE dustRange = {};
    dustRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    dustRange.NumDescriptors = dustDescriptorsPerFrame;
    dustRange.BaseShaderRegister = 9;
    dustRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &range;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &motionRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[2].Constants.ShaderRegister = 0;
    rootParameters[2].Constants.Num32BitValues =
        sizeof(PresenterConstants) / sizeof(unsigned int);
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &dustRange;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC samplers[2] = {sampler, sampler};
    samplers[1].ShaderRegister = 1;
    samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    D3D12_ROOT_SIGNATURE_DESC signatureDescription = {};
    signatureDescription.NumParameters = 4;
    signatureDescription.pParameters = rootParameters;
    signatureDescription.NumStaticSamplers = 2;
    signatureDescription.pStaticSamplers = samplers;
    signatureDescription.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> serializedSignature;
    ComPtr<ID3DBlob> signatureErrors;
    HRESULT result = D3D12SerializeRootSignature(
        &signatureDescription, D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedSignature, &signatureErrors);
    if (FAILED(result))
    {
        logFailure("D3D12SerializeRootSignature", result);
        return false;
    }
    result = presenter.device->CreateRootSignature(
        0, serializedSignature->GetBufferPointer(),
        serializedSignature->GetBufferSize(),
        IID_PPV_ARGS(&presenter.rootSignature));
    if (FAILED(result))
    {
        logFailure("ID3D12Device::CreateRootSignature", result);
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.root.presenter", "Presentation bindings ready");

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    if (!compileShader("PresentVS", "vs_5_0", vertexShader,
            "renderer.shader.present-vs", "Presentation vertex shader") ||
        !compileShader("PresentPS", "ps_5_0", pixelShader,
            "renderer.shader.present-ps", "Presentation pixel shader"))
    {
        return false;
    }
    presenter.presenterVertexShader = vertexShader;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = presenter.rootSignature.Get();
    pipeline.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    pipeline.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    pipeline.BlendState.AlphaToCoverageEnable = FALSE;
    pipeline.BlendState.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC defaultBlend = {};
    defaultBlend.BlendEnable = FALSE;
    defaultBlend.LogicOpEnable = FALSE;
    defaultBlend.SrcBlend = D3D12_BLEND_ONE;
    defaultBlend.DestBlend = D3D12_BLEND_ZERO;
    defaultBlend.BlendOp = D3D12_BLEND_OP_ADD;
    defaultBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    defaultBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
    defaultBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    defaultBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
    defaultBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    for (D3D12_RENDER_TARGET_BLEND_DESC &target : pipeline.BlendState.RenderTarget)
    {
        target = defaultBlend;
    }
    pipeline.SampleMask = 0xffffffffu;
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    pipeline.DepthStencilState.DepthEnable = FALSE;
    pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeline.DepthStencilState.StencilEnable = FALSE;
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = presenterFormat;
    pipeline.SampleDesc.Count = 1;
    /* Driver pipeline-library blobs are intentionally not persisted. A valid
       blob written by one run can make D3D12Core access invalid memory on a
       later run after the driver/runtime state changes. Presenter bytecode is
       still cached above; creating this single lightweight PSO is negligible
       and keeps startup deterministic. */
    hwWindowsSplashTaskBegin("renderer.pso.present", "Presentation pipeline");
    result = presenter.device->CreateGraphicsPipelineState(
        &pipeline, IID_PPV_ARGS(&presenter.presenterPipeline));
    if (FAILED(result))
    {
        logFailure("ID3D12Device::CreateGraphicsPipelineState", result);
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.pso.present", "Presentation pipeline ready");

    /* The Streamline input is a HUD-less full scene rendered at the selected
       internal DLSS/DLAA resolution. It uses FP16 so path lighting, dust and
       bloom stay intact until the reconstruction pass. */
    pipeline.RTVFormats[0] = sceneFormat;
    hwWindowsSplashTaskBegin("renderer.pso.scene", "Scene prepass pipeline");
    result = presenter.device->CreateGraphicsPipelineState(
        &pipeline, IID_PPV_ARGS(&presenter.scenePrepassPipeline));
    if (FAILED(result))
    {
        logFailure("ID3D12Device::CreateGraphicsPipelineState(scene prepass)", result);
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.pso.scene", "Scene prepass pipeline ready");

    ComPtr<ID3DBlob> dustScreenShader;
    if (!compileShader("DustScreenPS", "ps_5_0", dustScreenShader,
            "renderer.shader.dust-screen", "Cloud composite shader"))
        return false;
    pipeline.PS = { dustScreenShader->GetBufferPointer(),
                    dustScreenShader->GetBufferSize() };
    hwWindowsSplashTaskBegin("renderer.pso.dust-screen", "Cloud composite pipeline");
    result = presenter.device->CreateGraphicsPipelineState(
        &pipeline, IID_PPV_ARGS(&presenter.dustScreenPipeline));
    if (FAILED(result))
    {
        logFailure("CreateGraphicsPipelineState(dust screen)", result);
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.pso.dust-screen", "Cloud composite pipeline ready");
    return true;
}

bool createDustFroxelPipeline()
{
    hwWindowsSplashTaskBegin("renderer.root.dust", "Cloud root signature");
    HW_DIAG_FPRINTF(stderr,
        "[ModernGraphics] Dust sunlight visibility pipeline available; "
        "procedural cloud density remains authoritative.\n");

    D3D12_DESCRIPTOR_RANGE dustSrvRange = {};
    dustSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    dustSrvRange.NumDescriptors = 3;
    dustSrvRange.BaseShaderRegister = 9;
    dustSrvRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE froxelUavRange = {};
    froxelUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    froxelUavRange.NumDescriptors = 1;
    froxelUavRange.BaseShaderRegister = 0;
    froxelUavRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE noiseRange = {};
    noiseRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    noiseRange.NumDescriptors = 1;
    noiseRange.BaseShaderRegister = 6;
    noiseRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[6] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    parameters[0].DescriptorTable.pDescriptorRanges = &dustSrvRange;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable.NumDescriptorRanges = 1;
    parameters[2].DescriptorTable.pDescriptorRanges = &froxelUavRange;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[3].Descriptor.ShaderRegister = 18;
    parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[4].Descriptor.ShaderRegister = 1;
    parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[5].DescriptorTable.NumDescriptorRanges = 1;
    parameters[5].DescriptorTable.pDescriptorRanges = &noiseRange;
    parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC noiseSampler = {};
    noiseSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    noiseSampler.AddressU = noiseSampler.AddressV = noiseSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    noiseSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    noiseSampler.MaxLOD = D3D12_FLOAT32_MAX;
    noiseSampler.ShaderRegister = 1;
    noiseSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC signature = {};
    signature.NumParameters = 6;
    signature.pParameters = parameters;
    signature.NumStaticSamplers = 1;
    signature.pStaticSamplers = &noiseSampler;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(
        &signature, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(result))
    {
        logFailure("D3D12SerializeRootSignature(dust froxel)", result);
        return false;
    }
    result = presenter.device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&presenter.dustFroxelRootSignature));
    if (FAILED(result))
    {
        logFailure("CreateRootSignature(dust froxel)", result);
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.root.dust", "Cloud bindings ready");

    ComPtr<ID3DBlob> computeShader;
    if (!compileDustFroxelShader(computeShader)) return false;
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = presenter.dustFroxelRootSignature.Get();
    pso.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };
    hwWindowsSplashTaskBegin("renderer.pso.dust-froxel", "Cloud grid pipeline");
    result = presenter.device->CreateComputePipelineState(
        &pso, IID_PPV_ARGS(&presenter.dustFroxelPipeline));
    if (FAILED(result))
    {
        logFailure("CreateComputePipelineState(dust froxel)", result);
        return false;
    }
    hwWindowsSplashTaskComplete("renderer.pso.dust-froxel", "Cloud grid pipeline ready");
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 rayOptions = {};
    if (SUCCEEDED(presenter.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
        &rayOptions, sizeof(rayOptions))) &&
        rayOptions.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1)
    {
        pso.CS = {g_hwDustSunVisibilityShader, sizeof(g_hwDustSunVisibilityShader)};
        hwWindowsSplashTaskBegin("renderer.pso.dust-sun", "Cloud sunlight pipeline");
        result = presenter.device->CreateComputePipelineState(&pso,
            IID_PPV_ARGS(&presenter.dustSunVisibilityPipeline));
        if (FAILED(result))
        {
            logFailure("CreateComputePipelineState(dust sunlight)", result);
            hwWindowsSplashTaskSkip("renderer.pso.dust-sun", "Optional sunlight pipeline unavailable");
        }
        else hwWindowsSplashTaskComplete("renderer.pso.dust-sun", "Cloud sunlight pipeline ready");
    }
    else hwWindowsSplashTaskSkip("renderer.pso.dust-sun", "Raytracing tier 1.1 unavailable");
    return true;
}

bool createSwapchain(UINT width, UINT height)
{
    hwWindowsSplashTaskBegin("renderer.swapchain", "Presentation swapchain");
    BOOL tearing = FALSE;
    if (SUCCEEDED(presenter.factory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))))
    {
        presenter.allowTearing = tearing == TRUE;
    }

    DXGI_SWAP_CHAIN_DESC1 description = {};
    description.Width = width;
    description.Height = height;
    description.Format = presenterFormat;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = frameBufferCount;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    description.Flags = presenter.allowTearing ?
        DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    HRESULT result = S_OK;
#if defined(HW_ENABLE_D3D12_BACKEND)
    // Create the proxy directly. Replacing a live HWND swapchain while other
    // COM references retain it makes DXGI reject the replacement.
    IDXGISwapChain4 *generatedSwapchain = nullptr;
    if (hwmodern::fsrFrameGenerationCreateSwapchain(presenter.factory.Get(),
            presenter.queue.Get(), presenter.window, &description, &generatedSwapchain))
        presenter.swapchain.Attach(generatedSwapchain);
#endif
    if (!presenter.swapchain)
    {
        ComPtr<IDXGISwapChain1> swapchain1;
        result = presenter.factory->CreateSwapChainForHwnd(
            presenter.queue.Get(), presenter.window, &description,
            nullptr, nullptr, &swapchain1);
        if (FAILED(result))
        {
            logFailure("IDXGIFactory::CreateSwapChainForHwnd", result);
            return false;
        }
        result = swapchain1.As(&presenter.swapchain);
        if (FAILED(result))
        {
            logFailure("IDXGISwapChain4 query", result);
            return false;
        }
    }
    UINT colorSpaceSupport = 0;
    result = presenter.swapchain->CheckColorSpaceSupport(
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &colorSpaceSupport);
    if (FAILED(result) ||
        (colorSpaceSupport &
         DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == 0)
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] SDR sRGB presentation is unsupported by the "
            "current Windows display path.\n");
        return false;
    }
    result = presenter.swapchain->SetColorSpace1(
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    if (FAILED(result))
    {
        logFailure("IDXGISwapChain4::SetColorSpace1(sRGB)", result);
        return false;
    }
    HW_DIAG_FPRINTF(stderr,
        "[ModernGraphics] SDR sRGB presenter active; capture and display now "
        "receive the same display-referred color values.\n");
    presenter.factory->MakeWindowAssociation(presenter.window,
                                              DXGI_MWA_NO_ALT_ENTER);
    hwWindowsSplashTaskComplete("renderer.swapchain", "SDR swapchain ready");
    return true;
}

float raytracingScaleForAntiAliasingMode(int mode)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (!hwmodern::upscalerAvailableForMode(mode))
    {
        return 1.0f;
    }
#endif
    switch (mode)
    {
        case HW_MODERN_AA_DLSS_QUALITY: return 2.0f / 3.0f;
        case HW_MODERN_AA_DLSS_BALANCED: return 0.58f;
        case HW_MODERN_AA_DLSS_PERFORMANCE: return 0.50f;
        case HW_MODERN_AA_DLSS_ULTRA_PERFORMANCE: return 1.0f / 3.0f;
        default: return 1.0f;
    }
}

UINT scaledRayDimension(UINT displayDimension, float scale)
{
    UINT dimension = std::max(1u, static_cast<UINT>(
        std::lround(static_cast<float>(displayDimension) * scale)));
    /* Even extents keep the temporal jitter and bilinear reconstruction
       centered on the same half-pixel convention in both axes. */
    if (dimension > 1u)
    {
        dimension &= ~1u;
    }
    return std::max(1u, dimension);
}

void raytracingDimensionsForAntiAliasingMode(int mode,
                                             UINT displayWidth,
                                             UINT displayHeight,
                                             UINT &rayWidth,
                                             UINT &rayHeight)
{
    const float fallbackScale = raytracingScaleForAntiAliasingMode(mode);
    rayWidth = scaledRayDimension(displayWidth, fallbackScale);
    rayHeight = scaledRayDimension(displayHeight, fallbackScale);
#if defined(HW_ENABLE_D3D12_BACKEND)
    unsigned int optimalWidth = 0;
    unsigned int optimalHeight = 0;
    if (hwmodern::upscalerGetRenderSize(
            mode, displayWidth, displayHeight,
            &optimalWidth, &optimalHeight))
    {
        rayWidth = std::max(1u, static_cast<UINT>(optimalWidth));
        rayHeight = std::max(1u, static_cast<UINT>(optimalHeight));
    }
#endif
}

#if defined(HW_ENABLE_D3D12_BACKEND)
bool createSceneInputTexture(UINT width, UINT height)
{
    if (!presenter.device || !presenter.rtvHeap) return false;
    width = std::max(1u, width);
    height = std::max(1u, height);

    presenter.sceneInputTexture.Reset();
    presenter.sceneInputTextureShaderReadable = false;
    presenter.sceneInputWidth = 0;
    presenter.sceneInputHeight = 0;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = sceneFormat;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clear = {};
    clear.Format = sceneFormat;
    HRESULT result = presenter.device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
        IID_PPV_ARGS(&presenter.sceneInputTexture));
    if (FAILED(result))
    {
        logFailure("CreateCommittedResource(full-scene Streamline input)", result);
        return false;
    }
    presenter.sceneInputTexture->SetName(
        L"Homeworld HUD-less full-scene DLSS/DLAA input");
    presenter.sceneInputWidth = width;
    presenter.sceneInputHeight = height;
    presenter.sceneInputTextureShaderReadable = true;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(frameBufferCount) *
               presenter.rtvDescriptorSize;
    presenter.device->CreateRenderTargetView(
        presenter.sceneInputTexture.Get(), nullptr, rtv);
    return true;
}
#endif

bool createDustFroxelTexture(UINT sceneWidth, UINT sceneHeight)
{
    (void)sceneWidth;
    (void)sceneHeight;
    if (!presenter.device || !presenter.srvHeap) return false;
    presenter.dustFroxelTexture.Reset();
    presenter.dustFroxelShaderReadable = false;
    presenter.dustFroxelValidThisFrame = false;

    /* The world-locked shared cache is the performance default. Direct A/B mode
       keeps a valid t11/u0 descriptor bound but reduces dormant storage to one
       texel. */
    const bool useWorldCache = true; // Storage also holds sun-only visibility.
    const UINT width = useWorldCache ? dustWorldGridTextureDimension : 1u;
    const UINT height = useWorldCache ? dustWorldGridTextureDimension : 1u;
    const UINT depth = useWorldCache ? dustWorldGridTextureDimension : 1u;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = static_cast<UINT16>(depth);
    desc.MipLevels = useWorldCache ? 2 : 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    HRESULT result = presenter.device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
        IID_PPV_ARGS(&presenter.dustFroxelTexture));
    if (FAILED(result))
    {
        logFailure("CreateCommittedResource(shared dust froxel field)", result);
        return false;
    }
    presenter.dustFroxelTexture->SetName(
        L"Homeworld world-space shared volumetric dust cache");
    presenter.dustFroxelWidth = width;
    presenter.dustFroxelHeight = height;
    presenter.dustFroxelDepth = depth;
    presenter.dustFroxelShaderReadable = true;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srv.Texture3D.MostDetailedMip = 0;
    srv.Texture3D.MipLevels = 1;
    for (UINT slot = 0; slot < frameBufferCount; ++slot)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(
            dustDescriptorBase + slot * dustDescriptorsPerFrame + 3u) *
            presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            presenter.dustFroxelTexture.Get(), &srv, handle);

        if (useWorldCache)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC farSrv = srv;
            farSrv.Texture3D.MostDetailedMip = 1;
            D3D12_CPU_DESCRIPTOR_HANDLE farHandle = handle;
            farHandle.ptr += static_cast<SIZE_T>(2u) *
                             presenter.srvDescriptorSize;
            presenter.device->CreateShaderResourceView(
                presenter.dustFroxelTexture.Get(), &farSrv, farHandle);
        }
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    uav.Texture3D.MipSlice = 0;
    uav.Texture3D.WSize = depth;
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    uavHandle.ptr += static_cast<SIZE_T>(dustFroxelNearUavDescriptorIndex) *
                     presenter.srvDescriptorSize;
    presenter.device->CreateUnorderedAccessView(
        presenter.dustFroxelTexture.Get(), nullptr, &uav, uavHandle);
    if (useWorldCache)
    {
        uav.Texture3D.MipSlice = 1;
        uav.Texture3D.WSize = std::max(1u, depth / 2u);
        D3D12_CPU_DESCRIPTOR_HANDLE farUavHandle = uavHandle;
        farUavHandle.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateUnorderedAccessView(
            presenter.dustFroxelTexture.Get(), nullptr, &uav, farUavHandle);
    }

    if (useWorldCache)
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] World-locked dust cache storage: %ux%ux%u cells.\n",
            width, height, depth);
    }
    else
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] Dust cache compatibility resource: 1x1x1; direct "
            "procedural A/B mode is active.\n");
    }
    return true;
}

bool createDustScreenTextures(UINT sceneWidth, UINT sceneHeight)
{
    if (!presenter.device || !presenter.rtvHeap || !presenter.srvHeap)
        return false;
    const UINT width = std::max(1u, (sceneWidth + 3u) / 4u);
    const UINT height = std::max(1u, (sceneHeight + 3u) / 4u);
    // The caller already drained old size-dependent work. Recreate the
    // optional companion lazily, only if frame generation needs it again.
    presenter.cloudFrameGenerationGuide.Reset();
    presenter.cloudFrameGenerationGuideShaderReadable = true;
    presenter.cloudFrameGenerationGuideValidThisFrame = false;
    presenter.cloudFrameGenerationGuideFailed = false;
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = desc.Format;
    clear.Color[3] = 1.0f;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = desc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    for (UINT index = 0; index < 2u; ++index)
    {
        presenter.dustScreenTextures[index].Reset();
        HRESULT result = presenter.device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
            IID_PPV_ARGS(&presenter.dustScreenTextures[index]));
        if (FAILED(result))
        {
            logFailure("CreateCommittedResource(screen dust)", result);
            return false;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(frameBufferCount + 1u + index) *
                   presenter.rtvDescriptorSize;
        presenter.device->CreateRenderTargetView(
            presenter.dustScreenTextures[index].Get(), nullptr, rtv);
        presenter.dustScreenShaderReadable[index] = true;
    }
    presenter.dustScreenWidth = width;
    presenter.dustScreenHeight = height;
    presenter.dustScreenWriteIndex = 0;
    presenter.dustScreenHistoryValid = false;
    HW_DIAG_FPRINTF(stderr,
        "[ModernGraphics] Quarter-resolution FP16 screen-space dust with "
        "depth-aware reconstruction: %ux%u.\n",
        width, height);
    return true;
}

bool ensureCloudFrameGenerationGuidePipeline()
{
    if (presenter.cloudFrameGenerationGuideFailed)
        return false;
    const auto failed = [](const char *operation, HRESULT result)
    {
        logFailure(operation, result);
        presenter.cloudFrameGenerationGuideFailed = true;
        presenter.cloudFrameGenerationGuideValidThisFrame = false;
        hwWindowsSplashTaskSkip("renderer.shader.dust-guide",
            "Cloud FG guide unavailable; frame generation paused");
        hwWindowsSplashTaskSkip("renderer.pso.dust-guide",
            "Cloud FG guide unavailable; frame generation paused");
        return false;
    };
    if (!presenter.cloudFrameGenerationGuidePipeline)
    {
        std::string source;
        if (!makeCloudFrameGenerationShaderSource(source))
            return failed("Cloud FG shader source extension", E_FAIL);
        ComPtr<ID3DBlob> shader;
        if (!compileShader("DustScreenGuidedPS", "ps_5_0", shader,
                "renderer.shader.dust-guide", "Cloud frame-generation guide shader",
                nullptr, "HomeworldModernCloudGuide.hlsl", source.c_str()) ||
            !presenter.presenterVertexShader || !presenter.rootSignature)
            return failed("Compile cloud frame-generation guide shader", E_FAIL);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
        pipeline.pRootSignature = presenter.rootSignature.Get();
        pipeline.VS = {presenter.presenterVertexShader->GetBufferPointer(),
                       presenter.presenterVertexShader->GetBufferSize()};
        pipeline.PS = {shader->GetBufferPointer(), shader->GetBufferSize()};
        for (auto &target : pipeline.BlendState.RenderTarget)
        {
            target.SrcBlend = target.SrcBlendAlpha = D3D12_BLEND_ONE;
            target.DestBlend = target.DestBlendAlpha = D3D12_BLEND_ZERO;
            target.BlendOp = target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            target.LogicOp = D3D12_LOGIC_OP_NOOP;
            target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        pipeline.SampleMask = 0xffffffffu;
        pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pipeline.RasterizerState.DepthClipEnable = TRUE;
        pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipeline.NumRenderTargets = 2;
        pipeline.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        pipeline.RTVFormats[1] = DXGI_FORMAT_R32G32_FLOAT;
        pipeline.SampleDesc.Count = 1;
        hwWindowsSplashTaskBegin("renderer.pso.dust-guide", "Cloud frame-generation guide pipeline");
        HRESULT result = presenter.device->CreateGraphicsPipelineState(
            &pipeline, IID_PPV_ARGS(&presenter.cloudFrameGenerationGuidePipeline));
        if (FAILED(result)) return failed("CreateGraphicsPipelineState(cloud FG guide)", result);
        hwWindowsSplashTaskComplete("renderer.pso.dust-guide", "Cloud frame-generation guide pipeline ready");
    }
    return true;
}

bool ensureCloudFrameGenerationGuide()
{
    if (!presenter.frameGenerationGuidesNeeded || !ensureCloudFrameGenerationGuidePipeline())
        return false;
    const auto failed = [](const char *operation, HRESULT result)
    {
        logFailure(operation, result);
        presenter.cloudFrameGenerationGuideFailed = true;
        presenter.cloudFrameGenerationGuideValidThisFrame = false;
        return false;
    };
    if (!presenter.cloudFrameGenerationGuideRtvHeap)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap = {};
        heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap.NumDescriptors = 1;
        HRESULT result = presenter.device->CreateDescriptorHeap(
            &heap, IID_PPV_ARGS(&presenter.cloudFrameGenerationGuideRtvHeap));
        if (FAILED(result)) return failed("CreateDescriptorHeap(cloud FG guide)", result);
    }
    if (!presenter.cloudFrameGenerationGuide)
    {
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = presenter.dustScreenWidth;
        desc.Height = presenter.dustScreenHeight;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE clear = {};
        clear.Format = desc.Format;
        const auto shaderRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        HRESULT result = presenter.device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, shaderRead, &clear,
            IID_PPV_ARGS(&presenter.cloudFrameGenerationGuide));
        if (FAILED(result)) return failed("CreateCommittedResource(cloud FG guide)", result);
        presenter.device->CreateRenderTargetView(presenter.cloudFrameGenerationGuide.Get(),
            nullptr, presenter.cloudFrameGenerationGuideRtvHeap->GetCPUDescriptorHandleForHeapStart());
        presenter.cloudFrameGenerationGuide->SetName(L"Cloud FG extinction-weighted ray distance and coverage");
        presenter.cloudFrameGenerationGuideShaderReadable = true;
    }
    return true;
}

bool createSizeDependentResources(UINT width, UINT height)
{
    hwWindowsSplashTaskBegin("renderer.resources.size", "Scene render targets");
    width = width == 0 ? 1 : width;
    height = height == 0 ? 1 : height;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < frameBufferCount; ++index)
    {
        HRESULT result = presenter.swapchain->GetBuffer(
            index, IID_PPV_ARGS(&presenter.backBuffers[index]));
        if (FAILED(result))
        {
            logFailure("IDXGISwapChain::GetBuffer", result);
            return false;
        }
        presenter.device->CreateRenderTargetView(
            presenter.backBuffers[index].Get(), nullptr, rtv);
        rtv.ptr += presenter.rtvDescriptorSize;
    }

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC texture = {};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = width;
    texture.Height = height;
    texture.DepthOrArraySize = 1;
    texture.MipLevels = 1;
    texture.Format = sceneFormat;
    texture.SampleDesc.Count = 1;
    texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    texture.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    const D3D12_RESOURCE_STATES compatibilityInitialState =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
#else
    const D3D12_RESOURCE_STATES compatibilityInitialState =
        D3D12_RESOURCE_STATE_COPY_DEST;
#endif

    HRESULT result = presenter.device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &texture,
        compatibilityInitialState, nullptr,
        IID_PPV_ARGS(&presenter.sourceTexture));
    if (FAILED(result))
    {
        logFailure("CreateCommittedResource(presenter texture)", result);
        return false;
    }
    #ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    presenter.sourceTexture->SetName(L"Homeworld native D3D12 raster frame");
#else
    presenter.sourceTexture->SetName(L"Homeworld OpenGL compatibility frame");
#endif

    result = presenter.device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &texture,
        compatibilityInitialState, nullptr,
        IID_PPV_ARGS(&presenter.worldTexture));
    if (FAILED(result))
    {
        logFailure("CreateCommittedResource(world texture)", result);
        return false;
    }
    #ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    presenter.worldTexture->SetName(L"Homeworld native D3D12 world-only frame");
#else
    presenter.worldTexture->SetName(L"Homeworld OpenGL world-only frame");
#endif

    result = presenter.device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &texture,
        compatibilityInitialState, nullptr,
        IID_PPV_ARGS(&presenter.backgroundTexture));
    if (FAILED(result))
    {
        logFailure("CreateCommittedResource(background texture)", result);
        return false;
    }
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    presenter.backgroundTexture->SetName(L"Homeworld native D3D12 background-only frame");
#else
    presenter.backgroundTexture->SetName(L"Homeworld OpenGL background-only frame");
#endif

    result = presenter.device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &texture,
        compatibilityInitialState, nullptr,
        IID_PPV_ARGS(&presenter.occluderTexture));
    if (FAILED(result))
    {
        logFailure("CreateCommittedResource(occluder texture)", result);
        return false;
    }
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    presenter.occluderTexture->SetName(L"Homeworld native D3D12 pre-FX god-ray occluder frame");
#else
    presenter.occluderTexture->SetName(L"Homeworld OpenGL pre-FX god-ray occluder frame");
#endif

#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    texture.Flags = D3D12_RESOURCE_FLAG_NONE;
#endif

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = sceneFormat;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    presenter.device->CreateShaderResourceView(
        presenter.sourceTexture.Get(), &srv,
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_CPU_DESCRIPTOR_HANDLE worldSrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    worldSrv.ptr += static_cast<SIZE_T>(2) * presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.worldTexture.Get(), &srv, worldSrv);

    D3D12_CPU_DESCRIPTOR_HANDLE backgroundSrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    backgroundSrv.ptr += static_cast<SIZE_T>(10) *
                         presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.backgroundTexture.Get(), &srv, backgroundSrv);

    D3D12_CPU_DESCRIPTOR_HANDLE rayEnvironmentSrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    rayEnvironmentSrv.ptr +=
        static_cast<SIZE_T>(rayEnvironmentDescriptorIndex) *
        presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.backgroundTexture.Get(), &srv, rayEnvironmentSrv);

    /* The tiny, persistent OpenMW lookup reuses the retired history slot. */
    D3D12_CPU_DESCRIPTOR_HANDLE retiredHistorySrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    retiredHistorySrv.ptr += static_cast<SIZE_T>(11) *
                             presenter.srvDescriptorSize;
    D3D12_SHADER_RESOURCE_VIEW_DESC noiseSrv = srv;
    noiseSrv.Format = DXGI_FORMAT_R8G8_UNORM;
    presenter.device->CreateShaderResourceView(presenter.openmwNoiseTexture.Get(), &noiseSrv, retiredHistorySrv);

    D3D12_CPU_DESCRIPTOR_HANDLE occluderSrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    occluderSrv.ptr += static_cast<SIZE_T>(12) *
                       presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.occluderTexture.Get(), &srv, occluderSrv);

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrv = {};
    depthSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
    depthSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE sceneDepthSrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    sceneDepthSrv.ptr += static_cast<SIZE_T>(sceneDepthDescriptorIndex) *
                         presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(nullptr, &depthSrv, sceneDepthSrv);

    /* t3 falls back to the unfiltered world. A successfully created DLAA
       output replaces this descriptor without changing the presenter table. */
    D3D12_CPU_DESCRIPTOR_HANDLE filteredWorldSrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    filteredWorldSrv.ptr += static_cast<SIZE_T>(3) *
                            presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.worldTexture.Get(), &srv, filteredWorldSrv);

    /* The presenter shader always declares t1 even when its runtime branch is
       disabled. Keep that descriptor valid on non-DXR hardware and after a
       runtime disable; the real ray output overwrites this null descriptor. */
    D3D12_CPU_DESCRIPTOR_HANDLE raySrv =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    raySrv.ptr += presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(nullptr, &srv, raySrv);

    D3D12_SHADER_RESOURCE_VIEW_DESC motionSrv = {};
    motionSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    motionSrv.Format = DXGI_FORMAT_R16G16_FLOAT;
    motionSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    motionSrv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE motionHandle =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    motionHandle.ptr += static_cast<SIZE_T>(9) * presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(nullptr, &motionSrv,
                                                motionHandle);

#if defined(HW_ENABLE_D3D12_BACKEND)
    UINT sceneWidth = width;
    UINT sceneHeight = height;
    raytracingDimensionsForAntiAliasingMode(
        requestedAntiAliasingMode, width, height, sceneWidth, sceneHeight);
    if (hwmodern::raytracingInitialized())
    {
        hwWindowsSplashUpdate("Allocating raytracing outputs and histories");
        if (!hwmodern::raytracingCreateOutput(sceneWidth, sceneHeight))
        {
            HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR output unavailable; "
                                 "continuing with D3D12 raster presentation.\n");
        }
        else if (sceneWidth < width || sceneHeight < height)
        {
            HW_DIAG_FPRINTF(stderr,
                         "[ModernGraphics] Full-scene DLSS input: %ux%u "
                         "-> %ux%u display (DXR + dust + scene post).\n",
                         sceneWidth, sceneHeight, width, height);
        }
    }

    /* The old integration sent only the path-lighting buffer to DLSS while
       the real raster scene and dust remained native resolution. Build a
       complete HUD-less scene at the same dimensions as depth/motion instead. */
    hwWindowsSplashUpdate("Allocating scene input and cloud textures");
    if (!createSceneInputTexture(sceneWidth, sceneHeight))
    {
        return false;
    }
    if (!createDustFroxelTexture(sceneWidth, sceneHeight))
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] Shared dust froxel field unavailable; "
            "falling back to direct boundaryless raymarch.\n");
    }
    if (!createDustScreenTextures(sceneWidth, sceneHeight))
        return false;

    hwWindowsSplashUpdate("Initializing selected reconstruction output");
    hwmodern::upscalerCreateOutput(
        presenter.srvHeap.Get(), presenter.srvDescriptorSize, 3,
        width, height);
#endif

#ifndef HW_ENABLE_D3D12_NATIVE_RASTER
    UINT64 totalSize = 0;
    presenter.device->GetCopyableFootprints(
        &texture, 0, 1, 0, &presenter.uploadFootprint,
        nullptr, nullptr, &totalSize);

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC upload = {};
    upload.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload.Width = totalSize;
    upload.Height = 1;
    upload.DepthOrArraySize = 1;
    upload.MipLevels = 1;
    upload.Format = DXGI_FORMAT_UNKNOWN;
    upload.SampleDesc.Count = 1;
    upload.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (D3D12FrameContext &frame : presenter.frames)
    {
        result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &upload,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.upload));
        if (FAILED(result))
        {
            logFailure("CreateCommittedResource(frame upload)", result);
            return false;
        }
        void *mapped = nullptr;
        D3D12_RANGE noRead = { 0, 0 };
        result = frame.upload->Map(0, &noRead, &mapped);
        if (FAILED(result))
        {
            logFailure("ID3D12Resource::Map(frame upload)", result);
            return false;
        }
        frame.mappedUpload = static_cast<unsigned char *>(mapped);

        result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &upload,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.worldUpload));
        if (FAILED(result))
        {
            logFailure("CreateCommittedResource(world upload)", result);
            return false;
        }
        mapped = nullptr;
        result = frame.worldUpload->Map(0, &noRead, &mapped);
        if (FAILED(result))
        {
            logFailure("ID3D12Resource::Map(world upload)", result);
            return false;
        }
        frame.mappedWorldUpload = static_cast<unsigned char *>(mapped);

        result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &upload,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.backgroundUpload));
        if (FAILED(result))
        {
            logFailure("CreateCommittedResource(background upload)", result);
            return false;
        }
        mapped = nullptr;
        result = frame.backgroundUpload->Map(0, &noRead, &mapped);
        if (FAILED(result))
        {
            logFailure("ID3D12Resource::Map(background upload)", result);
            return false;
        }
        frame.mappedBackgroundUpload = static_cast<unsigned char *>(mapped);

        result = presenter.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &upload,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&frame.occluderUpload));
        if (FAILED(result))
        {
            logFailure("CreateCommittedResource(occluder upload)", result);
            return false;
        }
        mapped = nullptr;
        result = frame.occluderUpload->Map(0, &noRead, &mapped);
        if (FAILED(result))
        {
            logFailure("ID3D12Resource::Map(occluder upload)", result);
            return false;
        }
        frame.mappedOccluderUpload = static_cast<unsigned char *>(mapped);
        frame.uploadSize = totalSize;
    }
#else
    presenter.uploadFootprint = {};
#endif

    presenter.width = width;
    presenter.height = height;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    presenter.sourceTextureShaderReadable = true;
    presenter.worldTextureShaderReadable = true;
    presenter.backgroundTextureShaderReadable = true;
    presenter.occluderTextureShaderReadable = true;
#else
    presenter.sourceTextureShaderReadable = false;
    presenter.worldTextureShaderReadable = false;
    presenter.backgroundTextureShaderReadable = false;
    presenter.occluderTextureShaderReadable = false;
#endif
    hwWindowsSplashTaskComplete("renderer.resources.size", "Scene render targets ready");
    return true;
}

bool resizePresenter(UINT width, UINT height)
{
    if (!waitForSubmittedWork())
    {
        return false;
    }
    if (!releaseSizeDependentResources()) return false;
    const UINT flags = presenter.allowTearing ?
        DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT result = presenter.swapchain->ResizeBuffers(
        frameBufferCount, width, height, presenterFormat, flags);
    if (FAILED(result))
    {
        logFailure("IDXGISwapChain::ResizeBuffers", result);
        return false;
    }
    if (!createSizeDependentResources(width, height))
    {
        return false;
    }
    HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Resized D3D12 visible presenter to "
                         "%ux%u.\n", width, height);
    return true;
}

void transition(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    commands->ResourceBarrier(1, &barrier);
}
}

extern "C" int hwModernGraphicsQueryCapabilities(
    HWModernGraphicsCapabilities *capabilities)
{
    if (capabilities == nullptr)
    {
        return 0;
    }
    std::memset(capabilities, 0, sizeof(*capabilities));
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
    {
        return 0;
    }
    ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(selectHardwareAdapter(factory.Get(), adapter)))
    {
        return 0;
    }
    DXGI_ADAPTER_DESC1 description = {};
    ComPtr<ID3D12Device> device;
    if (FAILED(adapter->GetDesc1(&description)) ||
        FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&device))))
    {
        return 0;
    }

    capabilities->d3d12Available = 1;
    capabilities->hardwareAdapter = 1;
    capabilities->vendorId = description.VendorId;
    capabilities->deviceId = description.DeviceId;
    capabilities->dedicatedVideoMemoryBytes =
        static_cast<unsigned long long>(description.DedicatedVideoMemory);
    capabilities->maximumFeatureLevel = queryMaximumFeatureLevel(device.Get());
    copyAdapterName(description.Description, capabilities->adapterName,
                    sizeof(capabilities->adapterName));

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
                                              &options5, sizeof(options5))))
    {
        if (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1)
            capabilities->raytracingTier = HW_MODERN_RAYTRACING_TIER_1_1;
        else if (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0)
            capabilities->raytracingTier = HW_MODERN_RAYTRACING_TIER_1_0;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6,
                                              &options6, sizeof(options6))))
    {
        capabilities->variableRateShadingTier =
            static_cast<unsigned int>(options6.VariableShadingRateTier);
        capabilities->additionalShadingRatesSupported =
            options6.AdditionalShadingRatesSupported ? 1 : 0;
    }
    return 1;
}

extern "C" void hwModernGraphicsLogCapabilities(void)
{
    HWModernGraphicsCapabilities capabilities = {};
    if (!hwModernGraphicsQueryCapabilities(&capabilities))
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] No hardware D3D12 adapter is "
                             "available.\n");
        return;
    }
    const unsigned long long videoMemoryMiB =
        capabilities.dedicatedVideoMemoryBytes / (1024ull * 1024ull);
    const unsigned int featureMajor =
        (capabilities.maximumFeatureLevel >> 12u) & 0xfu;
    const unsigned int featureMinor =
        (capabilities.maximumFeatureLevel >> 8u) & 0xfu;
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] GPU: %s (vendor %04x, device %04x, %llu MiB VRAM)\n",
                 capabilities.adapterName, capabilities.vendorId,
                 capabilities.deviceId, videoMemoryMiB);
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] D3D12 feature level %u_%u; DXR tier %s; "
                 "VRS tier %u%s.\n",
                 featureMajor, featureMinor,
                 raytracingTierName(capabilities.raytracingTier),
                 capabilities.variableRateShadingTier,
                 capabilities.additionalShadingRatesSupported ?
                    " with additional shading rates" : "");
}

extern "C" int hwModernGraphicsInitialize(void *nativeWindow,
                                            unsigned int width,
                                            unsigned int height)
{
    hwmodern::dlaaEarlyInitialize();
    hwModernGraphicsShutdown();
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    int argumentCount=0;
    LPWSTR *arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);
    bool requestDX11=false;
    for(int i=1;arguments && i<argumentCount;++i)
        if(_wcsicmp(arguments[i],L"/dx11")==0 || _wcsicmp(arguments[i],L"-dx11")==0)
            requestDX11=true;
    if(arguments)LocalFree(arguments);
    if(requestDX11)return hwlegacyd3d11::initialize(nativeWindow,width,height)?1:0;
#endif
    presenter.window = static_cast<HWND>(nativeWindow);
    if (presenter.window == nullptr)
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Cannot create a visible D3D12 "
                             "presenter without a Win32 window.\n");
        return 0;
    }

    hwmodern::dlaaSetMode(requestedAntiAliasingMode);
    hwmodern::dlaaSetRayReconstructionEnabled(requestedRayReconstruction);

    HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&presenter.factory));
    if (FAILED(result) ||
        FAILED(result = selectHardwareAdapter(presenter.factory.Get(),
                                               presenter.adapter)) ||
        FAILED(result = D3D12CreateDevice(presenter.adapter.Get(),
                                          D3D_FEATURE_LEVEL_11_0,
                                          IID_PPV_ARGS(&presenter.device))))
    {
        logFailure("initializing D3D12", result);
        releasePresenter();
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
        return hwlegacyd3d11::initialize(nativeWindow,width,height)?1:0;
#else
        return 0;
#endif
    }
    hwmodern::dlaaSetDevice(presenter.device.Get(), presenter.adapter.Get());
    hwmodern::upscalerSetDevice(presenter.device.Get(), presenter.adapter.Get());
    hwmodern::upscalerSetRequestedMode(requestedAntiAliasingMode);

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 raytracingOptions = {};
    if (SUCCEEDED(presenter.device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, &raytracingOptions,
            sizeof(raytracingOptions))) &&
        raytracingOptions.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0)
    {
        presenter.device.As(&presenter.raytracingDevice);
    }

    D3D12_COMMAND_QUEUE_DESC queueDescription = {};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = presenter.device->CreateCommandQueue(
        &queueDescription, IID_PPV_ARGS(&presenter.queue));
    if (FAILED(result))
    {
        logFailure("ID3D12Device::CreateCommandQueue", result);
        releasePresenter();
        return 0;
    }

    /* Permanent low-overhead GPU pass timing. Ten timestamps per flight
       slot separate raster, DXR, cloud work, reconstruction, and presentation. */
    if (!hwDiagnosticsDisabled()) {
    if (FAILED(presenter.queue->GetTimestampFrequency(&presenter.timestampFrequency)))
        presenter.timestampFrequency = 0;
    D3D12_QUERY_HEAP_DESC timestampHeap = {};
    timestampHeap.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    timestampHeap.Count = frameBufferCount * gpuTimestampCount;
    result = presenter.device->CreateQueryHeap(
        &timestampHeap, IID_PPV_ARGS(&presenter.timestampQueryHeap));
    if (FAILED(result))
    {
        logFailure("CreateQueryHeap(GPU timestamps)", result);
        presenter.timestampQueryHeap.Reset();
        presenter.timestampFrequency = 0;
    }

    }
    const UINT safeWidth = width == 0 ? 1 : static_cast<UINT>(width);
    const UINT safeHeight = height == 0 ? 1 : static_cast<UINT>(height);
    if (!createSwapchain(safeWidth, safeHeight))
    {
        releasePresenter();
        return 0;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeap = {};
    rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeap.NumDescriptors = frameBufferCount + 3u + frameBufferCount;
    result = presenter.device->CreateDescriptorHeap(
        &rtvHeap, IID_PPV_ARGS(&presenter.rtvHeap));
    if (FAILED(result))
    {
        logFailure("CreateDescriptorHeap(RTV)", result);
        releasePresenter();
        return 0;
    }
    presenter.rtvDescriptorSize = presenter.device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeap = {};
    srvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    /* t0-t3 presenter color, u0-u4 DXR outputs/history, t4 motion,
       t5 background-only environment, t6 frame-generation history,
       t7 pre-FX god-ray occluders, t8 exact native raster depth, and
       per-frame t9/t10/t11 structured buffers for volumetric dust/lights/wakes. */
    srvHeap.NumDescriptors = presenterDescriptorCount;
    srvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = presenter.device->CreateDescriptorHeap(
        &srvHeap, IID_PPV_ARGS(&presenter.srvHeap));
    if (FAILED(result))
    {
        logFailure("CreateDescriptorHeap(SRV)", result);
        releasePresenter();
        return 0;
    }
    presenter.srvDescriptorSize =
        presenter.device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (!createVolumetricDustFrameResources())
    {
        releasePresenter();
        return 0;
    }

    for (D3D12FrameContext &frame : presenter.frames)
    {
        result = presenter.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator));
        if (FAILED(result))
        {
            logFailure("CreateCommandAllocator", result);
            releasePresenter();
            return 0;
        }

        if (presenter.timestampQueryHeap)
        {
            D3D12_HEAP_PROPERTIES heap = {};
            heap.Type = D3D12_HEAP_TYPE_READBACK;
            heap.CreationNodeMask = 1;
            heap.VisibleNodeMask = 1;
            D3D12_RESOURCE_DESC buffer = {};
            buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width = sizeof(UINT64) * gpuTimestampCount;
            buffer.Height = 1;
            buffer.DepthOrArraySize = 1;
            buffer.MipLevels = 1;
            buffer.SampleDesc.Count = 1;
            buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            result = presenter.device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &buffer,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&frame.timestampReadback));
            if (FAILED(result))
            {
                logFailure("CreateCommittedResource(GPU timestamp readback)", result);
                presenter.timestampQueryHeap.Reset();
                presenter.timestampFrequency = 0;
                for (D3D12FrameContext &other : presenter.frames)
                {
                    other.timestampReadback.Reset();
                    other.timestampPending = false;
                }
                continue;
            }
        }
    }
    result = presenter.device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        presenter.frames[0].allocator.Get(), nullptr,
        IID_PPV_ARGS(&presenter.commandList));
    if (FAILED(result) || FAILED(result = presenter.commandList->Close()))
    {
        logFailure("CreateCommandList", result);
        releasePresenter();
        return 0;
    }
    result = presenter.device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presenter.fence));
    if (FAILED(result))
    {
        logFailure("CreateFence", result);
        releasePresenter();
        return 0;
    }
    presenter.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (presenter.fenceEvent == nullptr)
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] CreateEventW failed "
                             "(Win32 error %lu).\n", GetLastError());
        releasePresenter();
        return 0;
    }

    if (!createPresenterPipeline())
    {
        releasePresenter();
        return 0;
    }
    if (!createDustFroxelPipeline())
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] Dust froxel compute pipeline unavailable; "
            "direct boundaryless raymarch remains available.\n");
        presenter.dustFroxelPipeline.Reset();
        presenter.dustFroxelRootSignature.Reset();
        hwWindowsSplashTaskSkip("renderer.root.dust", "Optional cloud grid unavailable");
        hwWindowsSplashTaskSkip("renderer.shader.dust-froxel", "Optional cloud grid unavailable");
        hwWindowsSplashTaskSkip("renderer.pso.dust-froxel", "Direct cloud rendering retained");
        hwWindowsSplashTaskSkip("renderer.pso.dust-sun", "Optional cloud grid unavailable");
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (raytracingRequested && presenter.raytracingDevice &&
        !hwmodern::raytracingInitialize(
            presenter.device.Get(), presenter.srvHeap.Get(),
            presenter.srvDescriptorSize, 1, 4,
            rayEnvironmentDescriptorIndex, rrGuideUavDescriptorBase,
            presenter.queue.Get()))
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR pipeline initialization "
                             "failed; D3D12 presentation remains active.\n");
    }
    if (!hwmodern::raytracingInitialized())
    {
        hwWindowsSplashTaskSkip("renderer.dxr.feedback", "Raytracing inactive");
        hwWindowsSplashTaskSkip("renderer.dxr.roots", "Raytracing inactive");
        hwWindowsSplashTaskSkip("renderer.dxr.pipeline", "Raytracing inactive");
        hwWindowsSplashTaskSkip("renderer.dxr.feedback-pipeline", "Raytracing inactive");
    }
    hwmodern::raytracingSetFxLightingStrength(
        requestedFxLightingStrength);
    hwmodern::raytracingSetPathSettings(
        requestedPathSamples, requestedPathBounces,
        requestedNormalStrength);
    hwmodern::raytracingSetRayReconstructionEnabled(
        requestedRayReconstruction &&
        requestedAntiAliasingMode >= HW_MODERN_AA_DLSS_QUALITY &&
        requestedAntiAliasingMode <= HW_MODERN_AA_DLSS_ULTRA_PERFORMANCE &&
        hwmodern::dlaaRayReconstructionAvailable());
    hwmodern::raytracingSetLightingTuning(
        requestedEnvironmentStrength, requestedAuthoredLightStrength,
        requestedSurfaceReflectivity, requestedSurfaceRoughness,
        requestedLightingExposure);
    hwmodern::raytracingSetSkyLightingStrengths(
        requestedPrimarySunStrength, requestedSkyAmbientStrength);
#endif
    if (
        !createSizeDependentResources(safeWidth, safeHeight))
    {
        releasePresenter();
        return 0;
    }
    // This is another full cloud shader. A cold compile belongs under the
    // responsive startup splash rather than the first mission's render frame.
    if (selectedFrameGenerationProvider() == HW_MODERN_FRAME_GENERATION_FSR_2X &&
        hwmodern::fsrFrameGenerationAvailable() && !hwmodern::fsrFrameGenerationFailed() &&
        raytracingRequested)
        ensureCloudFrameGenerationGuidePipeline();

    presenter.active = true;
    presenter.visiblePresenter = true;
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] D3D12 visible presenter active: %ux%u "
                 "DXGI flip-discard, triple-buffered%s%s.\n",
                 presenter.width, presenter.height,
                 presenter.allowTearing ? ", tearing available" : "",
                 presenter.raytracingDevice ? ", DXR command interface ready" : "");
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] Native D3D12 raster active: legacy fixed-function "
                 "draw vocabulary -> D3D12 command recording -> DXGI present; no "
                 "OpenGL context or framebuffer capture is used.\n");
#else
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] Compatibility capture: OpenGL scene -> "
                 "D3D12 upload -> DXGI present. This transfer is transitional; "
                 "native mesh migration remains in progress.\n");
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (hwmodern::raytracingActive())
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR path tracing enabled; "
                             "BLAS/TLAS construction begins with the first "
                             "gameplay frame.\n");
    }
#endif
    return 1;
}

#if HW_ENABLE_DIAGNOSTICS
extern "C" void hwModernBenchmarkRequest(void)
{
    if (benchmarkLaunchRequested()) benchmark.requested = true;
}
#endif

#if HW_ENABLE_DIAGNOSTICS
extern "C" void hwModernBenchmarkCpuBegin(unsigned int phase)
{
    if (!benchmark.recording) return;
    if (phase == 0) { benchmark.taskStarted = std::chrono::steady_clock::now(); benchmark.taskActive = true; }
    if (phase == 1) { benchmark.simulationStarted = std::chrono::steady_clock::now(); benchmark.simulationActive = true; }
}
#endif

#if HW_ENABLE_DIAGNOSTICS
extern "C" void hwModernBenchmarkCpuEnd(unsigned int phase)
{
    if (!benchmark.recording) return;
    if (phase == 0 && benchmark.taskActive) {
        benchmark.taskCpu.add(std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-benchmark.taskStarted).count());
        benchmark.taskActive = false;
    }
    if (phase == 1 && benchmark.simulationActive) {
        benchmark.simulationCpu.add(std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-benchmark.simulationStarted).count());
        benchmark.simulationActive = false;
    }
}
#endif


extern "C" void hwModernGraphicsBeginFrame(unsigned int width,
                                             unsigned int height)
{
    const auto cpuFrameStarted = performanceClockNow();
    benchmarkPoll();
    /* Render callbacks repopulate this from the same transforms used to draw
       the visible legacy frame.  Clearing here prevents deleted effects from
       leaving stale lights in the DXR scene. */
    dynamicLightEmitters.clear();
    volumetricDustInteractors.clear();
    presenter.dustCameraExplicitThisFrame = false;
    presenter.nativeRasterDepthSampleReady = false;
    presenter.dustFroxelValidThisFrame = false;
    presenter.godRaySourceExternalThisFrame = false;
    presenter.godRaySourceExternalVisible = false;
    presenter.missionSkyTextureName = 0;
    presenter.missionSkyFade = 1.0f;
    const DynamicLightClock::time_point now = DynamicLightClock::now();
    size_t retainedCount = 0;
    for (size_t index = 0; index < timedDynamicLightEmitters.size(); ++index)
    {
        TimedDynamicLight retained = timedDynamicLightEmitters[index];
        const float elapsedSeconds =
            std::chrono::duration<float>(now - retained.started).count();
        if (elapsedSeconds >= retained.lifetimeSeconds)
        {
            continue;
        }

        const float remaining = 1.0f -
            elapsedSeconds / retained.lifetimeSeconds;
        HWModernDynamicLightEmitter frameEmitter = retained.emitter;
        /* A squared envelope gives the flash a hard initial strike and a
           short photographic falloff without a single-frame discontinuity. */
        frameEmitter.intensity *= remaining * remaining;
        dynamicLightEmitters.push_back(frameEmitter);
        timedDynamicLightEmitters[retainedCount++] = retained;
    }
    timedDynamicLightEmitters.resize(retainedCount);
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){hwlegacyd3d11::beginFrame(width,height);return;}
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingBeginFrame();
#endif

    if (!presenter.active || presenter.frameOpen)
    {
        return;
    }
    const UINT requestedWidth = width == 0 ? 1 : static_cast<UINT>(width);
    const UINT requestedHeight = height == 0 ? 1 : static_cast<UINT>(height);
    if (requestedWidth != presenter.width || requestedHeight != presenter.height)
    {
        if (!resizePresenter(requestedWidth, requestedHeight))
        {
            presenter.active = false;
            presenter.visiblePresenter = false;
            return;
        }
    }

    presenter.activeFrameIndex = presenter.swapchain->GetCurrentBackBufferIndex();
    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    
#if HW_ENABLE_DIAGNOSTICS
if (benchmark.recording)
        benchmarkBeginFrame(frame.submittedFrameId, benchmarkCacheSnapshot());
#endif

    presenter.cpuFrameDxrRecordMilliseconds = 0.0;
    /* Every back-buffer slot now owns its DXR TLAS and upload resources. Wait
       only for this slot instead of draining the entire GPU every frame. */
    const auto cpuFenceWaitStarted = performanceClockNow();
    if (!waitForFenceValue(frame.fenceValue))
    {
        presenter.active = false;
        presenter.visiblePresenter = false;
        return;
    }
    const auto cpuFenceWaitFinished = performanceClockNow();
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::fgZoomCapturePoll(presenter.fence.Get());
#endif
    presenter.cpuFrameFenceWaitMilliseconds = hwDiagnosticsDisabled() ? 0.0 :
        std::chrono::duration<double, std::milli>(
            cpuFenceWaitFinished - cpuFenceWaitStarted).count();

    consumeShaftCapture(frame);
    /* Consume this slot's previous GPU timestamps after the fence we already
       had to wait for. Profiling therefore adds no frame drain or query wait. */
    if (HW_ENABLE_DIAGNOSTICS && frame.timestampPending && frame.timestampReadback &&
        presenter.timestampFrequency != 0)
    {
        UINT64 *ticks = nullptr;
        D3D12_RANGE readRange = {0, sizeof(UINT64) * gpuTimestampCount};
        if (SUCCEEDED(frame.timestampReadback->Map(
                0, &readRange, reinterpret_cast<void **>(&ticks))) && ticks)
        {
            const double millisecondsPerTick =
                1000.0 / static_cast<double>(presenter.timestampFrequency);
            bool timestampsOrdered = true;
            for (unsigned int i = 1; i < gpuTimestampCount; ++i)
                timestampsOrdered = timestampsOrdered && ticks[i] >= ticks[i - 1u];
            if (timestampsOrdered)
            {
                
#if HW_ENABLE_DIAGNOSTICS
if (benchmark.recording && frame.benchmarkCaptureId == benchmark.id)
                {
                    std::array<double,10> gpu;
                    for (unsigned int i = 0; i < 9; ++i)
                    {
                        gpu[i] = static_cast<double>(ticks[i+1]-ticks[i])*millisecondsPerTick;
                        benchmark.gpu[i].add(gpu[i]);
                    }
                    gpu[9] = static_cast<double>(ticks[9]-ticks[0])*millisecondsPerTick;
                    benchmark.gpu[9].add(gpu[9]);
                    benchmarkResolveGpu(frame.benchmarkCaptureId, frame.benchmarkFrameId, gpu);
                }
#endif

                presenter.gpuRasterMilliseconds +=
                    static_cast<double>(ticks[1] - ticks[0]) * millisecondsPerTick;
                presenter.gpuDxrMilliseconds +=
                    static_cast<double>(ticks[2] - ticks[1]) * millisecondsPerTick;
                presenter.gpuDustPrepareMilliseconds +=
                    static_cast<double>(ticks[4] - ticks[3]) * millisecondsPerTick;
                presenter.gpuDustScreenMilliseconds +=
                    static_cast<double>(ticks[6] - ticks[4]) * millisecondsPerTick;
                presenter.gpuReconstructionMilliseconds +=
                    static_cast<double>(ticks[8] - ticks[6]) * millisecondsPerTick;
                presenter.gpuCloudShadowMilliseconds +=
                    static_cast<double>(ticks[5] - ticks[4]) * millisecondsPerTick;
                presenter.gpuCloudMarchMilliseconds +=
                    static_cast<double>(ticks[6] - ticks[5]) * millisecondsPerTick;
                presenter.gpuSceneCompositeMilliseconds +=
                    static_cast<double>(ticks[7] - ticks[6]) * millisecondsPerTick;
                presenter.gpuUpscalerMilliseconds +=
                    static_cast<double>(ticks[8] - ticks[7]) * millisecondsPerTick;
                presenter.gpuPresenterMilliseconds +=
                    static_cast<double>(ticks[9] - ticks[8]) * millisecondsPerTick;
                presenter.gpuFrameMilliseconds +=
                    static_cast<double>(ticks[9] - ticks[0]) * millisecondsPerTick;
                ++presenter.gpuTimestampFrames;
            }
            D3D12_RANGE noWrite = {0, 0};
            frame.timestampReadback->Unmap(0, &noWrite);
        }
        frame.timestampPending = false;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetFrameSlot(presenter.activeFrameIndex);
#endif
    presenter.frameOpen = true;
    frameGenerationInputsReady = false;
    if (requestedFrameGenerationMode != HW_MODERN_FRAME_GENERATION_OFF) {
        const auto now = std::chrono::steady_clock::now();
        if (previousFrameGenerationTime != std::chrono::steady_clock::time_point{})
            frameGenerationDeltaMilliseconds = static_cast<float>(std::clamp(
                std::chrono::duration<double,std::milli>(now-previousFrameGenerationTime).count(), .1, 1000.0));
        previousFrameGenerationTime = now;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    const int fgProvider = selectedFrameGenerationProvider();
    frameGenerationFrameProvider = fgProvider;
    if (frameGenerationRequestChanged) {
        hwmodern::dlaaSetFrameGenerationEnabled(false);
        hwmodern::fsrFrameGenerationSetEnabled(false);
        frameGenerationRequestChanged = false;
    }
    // Never activate one provider while the other may still own presentation.
    if (fgProvider == HW_MODERN_FRAME_GENERATION_DLSS_G_2X && nvidiaFrameGenerationPresentationEnabled) {
        hwmodern::fsrFrameGenerationSetEnabled(false);
        hwmodern::dlaaSetFrameGenerationEnabled(!hwmodern::fsrFrameGenerationDisableFailed());
    } else {
        hwmodern::dlaaSetFrameGenerationEnabled(false);
        hwmodern::fsrFrameGenerationSetEnabled(
            fgProvider == HW_MODERN_FRAME_GENERATION_FSR_2X &&
            !hwmodern::dlaaFrameGenerationDisableFailed());
    }
    presenter.frameGenerationGuidesNeeded =
        fgProvider == HW_MODERN_FRAME_GENERATION_FSR_2X &&
        hwmodern::fsrFrameGenerationAvailable() && !hwmodern::fsrFrameGenerationFailed() &&
        !hwmodern::dlaaFrameGenerationDisableFailed() && raytracingRequested;
    presenter.cloudFrameGenerationGuideValidThisFrame = false;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    // The same native coverage guide also keeps exhaust self-lit over DXR
    // hull lighting when interpolation is disabled.
    hwlegacyd3d12::setFrameGenerationGuidesEnabled(presenter.frameGenerationGuidesNeeded || raytracingRequested);
#endif
    hwmodern::dlaaFrameGenerationBeginFrame(presenter.frameCount + 1);
#endif
    presenter.cpuFrameStarted = cpuFrameStarted;
    presenter.cpuFrameTimerActive = !hwDiagnosticsDisabled();
    presenter.frameCaptured = false;
    presenter.worldFrameCaptured = false;
    presenter.backgroundFrameCaptured = false;
    presenter.occluderFrameCaptured = false;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwglBeginFrame(presenter.width, presenter.height, presenter.activeFrameIndex);
#endif
}

extern "C" void hwModernGraphicsSetSensorViewActive(int active)
{
    hwmodern::raytracingSetSensorViewActive(active != 0);
}

extern "C" void hwModernGraphicsSetGodRayWorldSource(
    int mapIdentity, int sourceValid, const float direction[3])
{
    const bool mapChanged = presenter.godRayMapIdentity != mapIdentity;
    float normalized[3];
    float length;
    bool directionChanged;
    if (mapChanged)
    {
        resetGodRaySource(mapIdentity);
    }
    if (!sourceValid || direction == nullptr)
    {
        /* A transient missing backdrop sample must not unlock an emitter that
           already belongs to this mission. Only a real mapIdentity change
           resets the lock. */
        if (!presenter.godRaySourceLocked)
        {
            presenter.godRaySourceVisible = false;
        }
        return;
    }
    length = std::sqrt(direction[0] * direction[0] +
                       direction[1] * direction[1] +
                       direction[2] * direction[2]);
    if (length <= 0.000001f)
    {
        presenter.godRaySourceVisible = false;
        return;
    }
    normalized[0] = direction[0] / length;
    normalized[1] = direction[1] / length;
    normalized[2] = direction[2] / length;
    directionChanged = !presenter.godRaySourceLocked ||
        std::fabs(presenter.godRayWorldDirection[0] - normalized[0]) > 0.0001f ||
        std::fabs(presenter.godRayWorldDirection[1] - normalized[1]) > 0.0001f ||
        std::fabs(presenter.godRayWorldDirection[2] - normalized[2]) > 0.0001f;
    if (directionChanged)
    {
        presenter.godRayWorldDirection[0] = normalized[0];
        presenter.godRayWorldDirection[1] = normalized[1];
        presenter.godRayWorldDirection[2] = normalized[2];
        presenter.godRaySourceLocked = true;
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] God-ray source %s for map %d at direction "
            "(%.4f, %.4f, %.4f).\n",
            mapChanged ? "bound" : "updated",
            mapIdentity,
            presenter.godRayWorldDirection[0],
            presenter.godRayWorldDirection[1],
            presenter.godRayWorldDirection[2]);
    }
}

extern "C" void hwModernGraphicsSetMissionSkyLighting(
    int mapIdentity, int sourceValid, const float direction[3],
    const float sourceColor[3], const float ambientColor[3])
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d11::setSun(sourceValid!=0,direction,sourceColor,ambientColor,
        requestedPrimarySunStrength,requestedSkyAmbientStrength);
#endif
    /* Keep the visual shaft source, surface key and volumetric key tied to
       exactly the same mission-owned sky object. */
    hwModernGraphicsSetGodRayWorldSource(
        mapIdentity, sourceValid, direction);
    dustMissionSkyAvailable = ambientColor != nullptr;
    if (ambientColor != nullptr)
    {
        for (unsigned int channel = 0; channel < 3; ++channel)
            dustMissionAmbientColor[channel] =
                std::max(0.0f, std::min(ambientColor[channel], 16.0f));
    }
    dustMissionSunValid = sourceValid != 0 && direction != nullptr && sourceColor != nullptr;
    if (dustMissionSunValid)
    {
        const float length = std::sqrt(direction[0]*direction[0] +
                                       direction[1]*direction[1] +
                                       direction[2]*direction[2]);
        if (length > 0.000001f)
        {
            for (unsigned int channel = 0; channel < 3; ++channel)
            {
                dustMissionSunDirection[channel] = direction[channel] / length;
                dustMissionSunColor[channel] =
                    std::max(0.0f, std::min(sourceColor[channel], 16.0f));
            }
        }
        else
        {
            dustMissionSunValid = false;
        }
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetMissionSkyLighting(
        sourceValid, direction, sourceColor, ambientColor);
#else
    (void)sourceColor;
    (void)ambientColor;
#endif
}

extern "C" void hwModernGraphicsSetGodRayFocus(const float focus[3])
{
    if (focus == nullptr) return;
    for (unsigned int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(focus[axis])) return;
    std::memcpy(presenter.godRayFocus, focus, sizeof(presenter.godRayFocus));
    presenter.godRayFocusValid = true;
}

#if HW_ENABLE_DIAGNOSTICS
extern "C" void hwModernGraphicsNotifyCameraZoom(void)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::fgZoomCaptureNotifyZoom();
#endif
}
#endif


extern "C" void hwModernGraphicsSetVolumetricDustCamera(
    const float eye[3], const float rayUv00[3], const float rayUv10[3],
    const float rayUv01[3])
{
    if (eye == nullptr || rayUv00 == nullptr || rayUv10 == nullptr ||
        rayUv01 == nullptr) return;
    const float *rays[3] = {rayUv00, rayUv10, rayUv01};
    for (unsigned int rayIndex = 0; rayIndex < 3; ++rayIndex)
    {
        const float lengthSquared =
            rays[rayIndex][0] * rays[rayIndex][0] +
            rays[rayIndex][1] * rays[rayIndex][1] +
            rays[rayIndex][2] * rays[rayIndex][2];
        if (!(lengthSquared > 1.0e-10f) || !std::isfinite(lengthSquared))
            return;
    }
    for (unsigned int channel = 0; channel < 3; ++channel)
    {
        presenter.dustCameraPosition[channel] = eye[channel];
        presenter.dustRayUv00[channel] = rayUv00[channel];
        presenter.dustRayUv10[channel] = rayUv10[channel];
        presenter.dustRayUv01[channel] = rayUv01[channel];
    }
    presenter.dustCameraValid = true;
    presenter.dustCameraExplicitThisFrame = true;
}

extern "C" int hwModernGraphicsCaptureOpenGLBackground(
    int mapIdentity, float verticalFieldOfViewDegrees, float aspectRatio,
    const float viewMatrix[16], const float projectionMatrix[16])
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return 1;
#endif
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen)
    {
        return 0;
    }
    updateDustDepthProjection(projectionMatrix);
    /* The view matrix passed here is the matrix that actually rendered the
       world snapshot.  Make it authoritative for volumetrics every frame so
       editor/fleet camera callbacks cannot rotate the medium in screen space. */
    if (viewMatrix == nullptr ||
        !updateDustCameraFromRenderView(verticalFieldOfViewDegrees,
                                        aspectRatio, viewMatrix))
    {
        presenter.dustCameraValid = false;
        presenter.dustCameraExplicitThisFrame = false;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (!hwmodern::raytracingInitialized() &&
        !requestedLensFlares && requestedGodRays <= 0.0001f && requestedBloom <= 0.0001f &&
        (volumetricDustVolumes.empty() && harvestableNebulaVolumes.empty() && harvestableDustVolumes.empty() && particleFieldVolumes.empty()))
    {
        return 0;
    }
#else
    if (!requestedLensFlares && requestedGodRays <= 0.0001f && requestedBloom <= 0.0001f &&
        (volumetricDustVolumes.empty() && harvestableNebulaVolumes.empty() && harvestableDustVolumes.empty() && particleFieldVolumes.empty())) return 0;
#endif
    hwglMarkSnapshot(HW_LEGACY_RASTER_BACKGROUND);
    if (presenter.godRayMapIdentity != mapIdentity)
    {
        resetGodRaySource(mapIdentity);
    }
    presenter.godRayLastBackgroundFrame = presenter.frameCount;
    if (!presenter.godRaySourceExternalThisFrame)
    {
        if (presenter.godRaySourceLocked)
            projectLockedGodRaySource(verticalFieldOfViewDegrees, aspectRatio, viewMatrix);
        else
            presenter.godRaySourceVisible = false;
    }
    presenter.backgroundFrameCaptured = true;
    return 1;
#else
    if (!presenter.active || !presenter.visiblePresenter ||
        !presenter.frameOpen)
    {
        return 0;
    }
    updateDustDepthProjection(projectionMatrix);
    /* The view matrix passed here is the matrix that actually rendered the
       world snapshot.  Make it authoritative for volumetrics every frame so
       editor/fleet camera callbacks cannot rotate the medium in screen space. */
    if (viewMatrix == nullptr ||
        !updateDustCameraFromRenderView(verticalFieldOfViewDegrees,
                                        aspectRatio, viewMatrix))
    {
        presenter.dustCameraValid = false;
        presenter.dustCameraExplicitThisFrame = false;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    /* The environment frame is consumed by DXR, post-FX and the volumetric
       media pass.  Avoid the synchronous readback only when none needs it. */
    if (!hwmodern::raytracingInitialized() &&
        !requestedLensFlares && requestedGodRays <= 0.0001f && requestedBloom <= 0.0001f &&
        (volumetricDustVolumes.empty() && harvestableNebulaVolumes.empty() && harvestableDustVolumes.empty() && particleFieldVolumes.empty()))
    {
        return 0;
    }
#else
    if (!requestedLensFlares && requestedGodRays <= 0.0001f && requestedBloom <= 0.0001f &&
        (volumetricDustVolumes.empty() && harvestableNebulaVolumes.empty() && harvestableDustVolumes.empty() && particleFieldVolumes.empty()))
    {
        return 0;
    }
#endif
    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    if (frame.mappedBackgroundUpload == nullptr)
    {
        return 0;
    }

    const auto readbackStarted = performanceClockNow();
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH,
                  static_cast<GLint>(presenter.uploadFootprint.Footprint.RowPitch / 4));
    glReadPixels(0, 0, static_cast<GLsizei>(presenter.width),
                 static_cast<GLsizei>(presenter.height),
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 frame.mappedBackgroundUpload + presenter.uploadFootprint.Offset);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR)
    {
        return 0;
    }
    presenter.backgroundReadbackMilliseconds +=
        std::chrono::duration<double, std::milli>(
            performanceClockNow() - readbackStarted).count();
    ++presenter.backgroundReadbackCount;
    /* The mission backdrop owns source selection. Camera/focus changes must
       never trigger a new search or swap to another star. */
    if (presenter.godRayMapIdentity != mapIdentity)
    {
        resetGodRaySource(mapIdentity);
    }
    presenter.godRayLastBackgroundFrame = presenter.frameCount;
    if (presenter.godRaySourceExternalThisFrame)
    {
        /* ModernMissionBackdrop already projected the exact authored source
           using the live OpenGL matrices.  Keep that screen-space position,
           including modest off-screen coordinates, instead of overwriting it
           with the legacy in-viewport projection gate below. */
    }
    else if (presenter.godRaySourceLocked)
    {
        projectLockedGodRaySource(
            verticalFieldOfViewDegrees, aspectRatio, viewMatrix);
    }
    else
    {
        presenter.godRaySourceVisible = false;
    }
    presenter.backgroundFrameCaptured = true;
    return 1;
#endif
}

extern "C" int hwModernGraphicsCaptureOpenGLOccluders(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return 1;
#endif
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen ||
        requestedGodRays <= 0.0001f)
    {
        return 0;
    }
    hwglMarkSnapshot(HW_LEGACY_RASTER_OCCLUDERS);
    presenter.occluderFrameCaptured = true;
    return 1;
#else
    if (!presenter.active || !presenter.visiblePresenter ||
        !presenter.frameOpen || requestedGodRays <= 0.0001f)
    {
        return 0;
    }
    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    if (frame.mappedOccluderUpload == nullptr)
    {
        return 0;
    }

    /* Capture after planets/world geometry but before the render-list pass.
       This gives the shaft shader a raster occluder image that deliberately
       excludes ship engine trails, beams, particles and other transient FX.
       Opaque ships and ordinary derelicts are supplied by the DXR hit mask. */
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH,
                  static_cast<GLint>(presenter.uploadFootprint.Footprint.RowPitch / 4));
    glReadPixels(0, 0, static_cast<GLsizei>(presenter.width),
                 static_cast<GLsizei>(presenter.height),
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 frame.mappedOccluderUpload + presenter.uploadFootprint.Offset);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR)
    {
        return 0;
    }
    presenter.occluderFrameCaptured = true;
    return 1;
#endif
}

extern "C" int hwModernGraphicsCaptureOpenGLWorld(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return 1;
#endif
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen)
    {
        return 0;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    const bool dxrNeedsWorld = hwmodern::raytracingInitialized();
#else
    const bool dxrNeedsWorld = false;
#endif
    const bool postProcessingNeedsWorld =
        requestedAntiAliasingMode != HW_MODERN_AA_OFF ||
        requestedChromaticAberration > 0.0001f ||
        requestedMotionBlur > 0.0001f ||
        requestedFilmGrain > 0.0001f ||
        requestedGodRays > 0.0001f ||
        requestedBloom > 0.0001f || requestedLensFlares ||
        !volumetricDustVolumes.empty() || !harvestableNebulaVolumes.empty() ||
        !harvestableDustVolumes.empty() || !particleFieldVolumes.empty();
    if (!dxrNeedsWorld && !postProcessingNeedsWorld) return 0;
    hwglMarkSnapshot(HW_LEGACY_RASTER_WORLD);
    presenter.worldFrameCaptured = true;
    return 1;
#else
    if (!presenter.active || !presenter.visiblePresenter ||
        !presenter.frameOpen)
    {
        return 0;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    const bool dxrNeedsWorld = hwmodern::raytracingInitialized();
#else
    const bool dxrNeedsWorld = false;
#endif
    const bool postProcessingNeedsWorld =
        requestedAntiAliasingMode != HW_MODERN_AA_OFF ||
        requestedChromaticAberration > 0.0001f ||
        requestedMotionBlur > 0.0001f ||
        requestedFilmGrain > 0.0001f ||
        requestedGodRays > 0.0001f ||
        requestedBloom > 0.0001f || requestedLensFlares ||
        !volumetricDustVolumes.empty() || !harvestableNebulaVolumes.empty() ||
        !harvestableDustVolumes.empty() || !particleFieldVolumes.empty();
    /* With DXR, AA, volumetric clouds and post processing disabled, the final composite can
       be presented directly. This removes a second synchronous GL readback
       from the zero-effects path without changing image quality. */
    if (!dxrNeedsWorld && !postProcessingNeedsWorld)
    {
        return 0;
    }
    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    if (frame.mappedWorldUpload == nullptr)
    {
        return 0;
    }

    const auto readbackStarted = performanceClockNow();
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH,
                  static_cast<GLint>(presenter.uploadFootprint.Footprint.RowPitch / 4));
    glReadPixels(0, 0, static_cast<GLsizei>(presenter.width),
                 static_cast<GLsizei>(presenter.height),
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 frame.mappedWorldUpload + presenter.uploadFootprint.Offset);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR)
    {
        return 0;
    }
    presenter.worldReadbackMilliseconds +=
        std::chrono::duration<double, std::milli>(
            performanceClockNow() - readbackStarted).count();
    ++presenter.worldReadbackCount;
    presenter.worldFrameCaptured = true;
    return 1;
#endif
}

extern "C" int hwModernGraphicsCaptureOpenGLFrame(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return 1;
#endif
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen)
    {
        return 0;
    }
    hwglMarkSnapshot(HW_LEGACY_RASTER_FINAL);
    presenter.frameCaptured = true;
    return 1;
#else
    if (!presenter.active || !presenter.visiblePresenter ||
        !presenter.frameOpen)
    {
        return 0;
    }
    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    if (frame.mappedUpload == nullptr)
    {
        return 0;
    }

    const auto readbackStarted = performanceClockNow();
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH,
                  static_cast<GLint>(presenter.uploadFootprint.Footprint.RowPitch / 4));
    glReadPixels(0, 0, static_cast<GLsizei>(presenter.width),
                 static_cast<GLsizei>(presenter.height),
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 frame.mappedUpload + presenter.uploadFootprint.Offset);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] OpenGL compatibility capture "
                             "failed (GL error 0x%04x); reverting to SDL "
                             "presentation.\n", static_cast<unsigned int>(error));
        presenter.active = false;
        presenter.visiblePresenter = false;
        return 0;
    }
    presenter.frameReadbackMilliseconds +=
        std::chrono::duration<double, std::milli>(
            performanceClockNow() - readbackStarted).count();
    ++presenter.frameReadbackCount;
    presenter.frameCaptured = true;
    return 1;
#endif
}

extern "C" int hwModernGraphicsRenderMissionSky(
    unsigned int textureName, float fade)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return hwglRecordMissionSky(static_cast<GLuint>(textureName),fade);
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen)
        return 0;
    const int recorded =
        hwglRecordMissionSky(static_cast<GLuint>(textureName), fade);
    if (recorded)
    {
        presenter.missionSkyTextureName = textureName;
        presenter.missionSkyFade = std::max(0.0f, std::min(fade, 1.0f));
    }
    return recorded;
#else
    (void)textureName;
    (void)fade;
    return 0;
#endif
}

extern "C" unsigned int hwModernGraphicsCreateBackgroundProgram(const void *bytes, size_t size)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    return hwglCreateBackgroundProgram(bytes,size);
#else
    (void)bytes; (void)size; return 0;
#endif
}

extern "C" int hwModernGraphicsRenderBackgroundMesh(unsigned int program, const unsigned int *textures,
    const HWBackgroundVertex *vertices, unsigned int count, const float *parameters,
    const float *objectToWorld, unsigned int flags, float atmosphereExpansion)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return hwglRecordBackgroundMesh(program,textures,vertices,count,parameters,objectToWorld,flags,atmosphereExpansion);
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen) return 0;
    return hwglRecordBackgroundMesh(program,textures,vertices,count,parameters,objectToWorld,flags,atmosphereExpansion);
#else
    (void)program; (void)textures; (void)vertices; (void)count; (void)parameters;
    (void)objectToWorld; (void)flags; (void)atmosphereExpansion; return 0;
#endif
}

extern "C" int hwModernGraphicsRenderBackgroundStars(unsigned int texture,
    const float *stars, unsigned int count, float fade)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return hwglRecordBackgroundStars(texture,stars,count,fade);
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen) return 0;
    return hwglRecordBackgroundStars(texture, stars, count, fade);
#else
    (void)texture; (void)stars; (void)count; (void)fade;
    return 0;
#endif
}

extern "C" int hwModernGraphicsRenderBackgroundFlares(unsigned int texture,
    const float *records, unsigned int count, float fade)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return hwglRecordBackgroundFlares(texture,records,count,fade);
    if (!presenter.active || !presenter.visiblePresenter || !presenter.frameOpen) return 0;
    return hwglRecordBackgroundFlares(texture,records,count,fade);
#else
    (void)texture;(void)records;(void)count;(void)fade;return 0;
#endif
}

extern "C" int hwModernGraphicsFlushOpenGLFrame(unsigned int width,
                                                   unsigned int height)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){
        if(!hwlegacyd3d11::frameOpen()) {hwlegacyd3d11::beginFrame(width,height);hwlegacyd3d11::present();}
        return 1;
    }
#endif
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    bool standaloneFrame;
    if (!presenter.active || !presenter.visiblePresenter) return 0;
    standaloneFrame = !presenter.frameOpen;
    if (standaloneFrame)
    {
        hwModernGraphicsBeginFrame(width, height);
        if (!presenter.active || !presenter.visiblePresenter) return 0;
        if (!presenter.frameOpen) return 1;
    }
    hwglMarkSnapshot(HW_LEGACY_RASTER_FINAL);
    presenter.frameCaptured = true;
    if (standaloneFrame && presenter.frameOpen) hwModernGraphicsEndFrame();
    return presenter.active && presenter.visiblePresenter ? 1 : 0;
#else
    bool standaloneFrame;
    int captured;

    if (!presenter.active || !presenter.visiblePresenter)
    {
        return 0;
    }

    /* The ordinary gameplay/HorseRace paths already bracket rndFlush with a
       modern frame.  Old FE helpers such as rndClear/TradeMgr do not.  Never
       let those paths fall through to SDL_GL_SwapWindow while DXGI owns the
       visible HWND: mixing the two presentation models can leave the DXGI
       image black/stale until Windows forces an expose via Alt+Tab. */
    standaloneFrame = !presenter.frameOpen;
    if (standaloneFrame)
    {
        hwModernGraphicsBeginFrame(width, height);
        if (!presenter.active || !presenter.visiblePresenter)
        {
            return 0;
        }
        if (!presenter.frameOpen)
        {
            /* DXGI still owns presentation, so suppress a legacy window swap
               even if this bridge frame could not be opened. */
            return 1;
        }
    }

    captured = hwModernGraphicsCaptureOpenGLFrame();
    if (standaloneFrame && presenter.frameOpen)
    {
        hwModernGraphicsEndFrame();
    }

    /* Capture can explicitly disable the presenter after a GL/D3D failure.
       Only then is it safe for the caller to resume SDL presentation. */
    if (!presenter.active || !presenter.visiblePresenter)
    {
        return 0;
    }

    (void)captured;
    return 1;
#endif
}

void recordDustFroxelBuild(PresenterConstants &constants)
{
    const bool visibilityOnly = !dustWorldCacheRequested() || constants.godRays > 0.0001f;
    constants.presenterPadding[1] = 0.0f;
    if (constants.dustRenderEnabled == 0u ||
        constants.dustGridMinWidth[3] < 1.0f ||
        !presenter.dustFroxelTexture || !presenter.dustFroxelPipeline ||
        !presenter.dustFroxelRootSignature)
    {
        return;
    }
    if (presenter.dustFroxelValidThisFrame)
    {
        constants.presenterPadding[1] = visibilityOnly ? 3.0f : 1.0f;
        return;
    }

    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    if (!frame.dustSunConstantsUpload)
    {
        D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC buffer = {};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = 768; buffer.Height = 1; buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1; buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        HRESULT result = presenter.device->CreateCommittedResource(&heap,
            D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&frame.dustSunConstantsUpload));
        if (FAILED(result)) { logFailure("dust sun constant upload", result); return; }
        void *mapped = nullptr;
        result = frame.dustSunConstantsUpload->Map(0, nullptr, &mapped);
        if (FAILED(result)) { frame.dustSunConstantsUpload.Reset(); return; }
        frame.mappedDustSunConstants = static_cast<unsigned char *>(mapped);
    }
    static_assert(sizeof(PresenterConstants) <= 256, "dust constant slot overflow");
    float shadowConstants[16] = {};
    const auto shadowScene = constants.rayOverlayEnabled && constants.dustSunDirectionValid[3] > 0.5f
        ? hwmodern::raytracingSunVisibilityScene(shadowConstants) : 0ull;
    const bool geometricSun = shadowScene && presenter.dustSunVisibilityPipeline;
    if (constants.godRays > 0.0001f && presenter.frameCount % 600u == 0u)
    {
        float viewDirection[3] = {};
        float viewLength = 0.0f;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            viewDirection[axis] = 0.5f * (constants.dustRayUv10[axis] +
                constants.dustRayUv01SunStrength[axis]);
            viewLength += viewDirection[axis] * viewDirection[axis];
        }
        viewLength = std::sqrt(viewLength);
        float sunCosine = 0.0f;
        if (viewLength > 0.0f)
            for (unsigned axis = 0; axis < 3; ++axis)
                sunCosine += viewDirection[axis] / viewLength * constants.dustSunDirectionValid[axis];
        HW_DIAG_FPRINTF(stderr, "[SunShafts] enabled=%u TLAS=%d pipeline=%d sun=%g "
            "strength=%g RGB=(%g,%g,%g) viewSunCos=%g focusValid=%d "
            "eye=(%g,%g,%g) focus=(%g,%g,%g) grid=(%g,%g,%g)/%g volumes=%u\n",
            constants.dustRenderEnabled, shadowScene != 0, !!presenter.dustSunVisibilityPipeline,
            constants.dustSunDirectionValid[3], constants.dustRayUv01SunStrength[3],
            constants.dustSunColor[0], constants.dustSunColor[1], constants.dustSunColor[2],
            sunCosine, presenter.godRayFocusValid,
            constants.dustCameraPositionMaxDistance[0], constants.dustCameraPositionMaxDistance[1], constants.dustCameraPositionMaxDistance[2],
            presenter.godRayFocus[0], presenter.godRayFocus[1], presenter.godRayFocus[2],
            constants.dustGridMinWidth[0], constants.dustGridMinWidth[1], constants.dustGridMinWidth[2],
            constants.dustGridSizeHeight[0], constants.dustVolumeCount);
    }
    if (visibilityOnly && !geometricSun) return;
    shadowConstants[12] = geometricSun ? 1.0f : 0.0f;
    shadowConstants[13] = requestedShadowMaximumDistance;
    shadowConstants[14] = 0.5f;
    std::memcpy(frame.mappedDustSunConstants+512, shadowConstants, sizeof(shadowConstants));

    if (presenter.dustFroxelShaderReadable)
    {
        transition(presenter.commandList.Get(), presenter.dustFroxelTexture.Get(),
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        presenter.dustFroxelShaderReadable = false;
    }

    presenter.commandList->SetPipelineState(geometricSun
        ? presenter.dustSunVisibilityPipeline.Get() : presenter.dustFroxelPipeline.Get());
    presenter.commandList->SetComputeRootSignature(
        presenter.dustFroxelRootSignature.Get());
    ID3D12DescriptorHeap *heaps[] = { presenter.srvHeap.Get() };
    presenter.commandList->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE dustHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    dustHandle.ptr += static_cast<UINT64>(
        dustDescriptorBase + presenter.activeFrameIndex * dustDescriptorsPerFrame) *
        presenter.srvDescriptorSize;
    presenter.commandList->SetComputeRootDescriptorTable(0, dustHandle);
    auto noiseHandle = presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    noiseHandle.ptr += 11ull * presenter.srvDescriptorSize;
    presenter.commandList->SetComputeRootDescriptorTable(5, noiseHandle);
    if (geometricSun)
        presenter.commandList->SetComputeRootShaderResourceView(3, shadowScene);
    presenter.commandList->SetComputeRootConstantBufferView(4,
        frame.dustSunConstantsUpload->GetGPUVirtualAddress()+512);
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    uavHandle.ptr += static_cast<UINT64>(dustFroxelNearUavDescriptorIndex) *
                     presenter.srvDescriptorSize;
    constants.presenterPadding[1] = visibilityOnly ? 3.0f : 0.0f;
    std::memcpy(frame.mappedDustSunConstants, &constants, sizeof(constants));
    presenter.commandList->SetComputeRootConstantBufferView(1,
        frame.dustSunConstantsUpload->GetGPUVirtualAddress());
    presenter.commandList->SetComputeRootDescriptorTable(2, uavHandle);

    const UINT activeWidth = std::max(1u, static_cast<UINT>(
        std::lround(constants.dustGridMinWidth[3])));
    const UINT activeHeight = std::max(1u, static_cast<UINT>(
        std::lround(constants.dustGridSizeHeight[3])));
    const UINT activeDepth = std::max(1u, static_cast<UINT>(
        std::lround(constants.presenterPadding[0])));
    presenter.commandList->Dispatch(
        geometricSun ? (activeWidth+7u)/8u : (activeWidth+3u)/4u,
        geometricSun ? (activeHeight+7u)/8u : (activeHeight+3u)/4u,
        geometricSun ? 1u : (activeDepth+3u)/4u);

    /* Mip 1 is independent storage for an 80^3 far cascade spanning four
       times the near extent. Reusing the allocation adds only 12.5% memory. */
    const UINT farWidth = std::max(1u, presenter.dustFroxelWidth / 2u);
    const UINT farHeight = std::max(1u, presenter.dustFroxelHeight / 2u);
    const UINT farDepth = std::max(1u, presenter.dustFroxelDepth / 2u);
    constants.presenterPadding[1] = visibilityOnly ? 4.0f : 2.0f;
    std::memcpy(frame.mappedDustSunConstants+256, &constants, sizeof(constants));
    presenter.commandList->SetComputeRootConstantBufferView(1,
        frame.dustSunConstantsUpload->GetGPUVirtualAddress()+256);
    uavHandle.ptr += presenter.srvDescriptorSize;
    presenter.commandList->SetComputeRootDescriptorTable(2, uavHandle);
    presenter.commandList->Dispatch(
        geometricSun ? (farWidth+7u)/8u : (farWidth+3u)/4u,
        geometricSun ? (farHeight+7u)/8u : (farHeight+3u)/4u,
        geometricSun ? 1u : (farDepth+3u)/4u);
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = presenter.dustFroxelTexture.Get();
    presenter.commandList->ResourceBarrier(1, &uavBarrier);
    transition(presenter.commandList.Get(), presenter.dustFroxelTexture.Get(),
               D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    presenter.dustFroxelShaderReadable = true;
    presenter.dustFroxelValidThisFrame = true;
    constants.presenterPadding[1] = visibilityOnly ? 3.0f : 1.0f;
}

void recordDustScreenBuild(PresenterConstants &constants)
{
    presenter.cloudFrameGenerationGuideValidThisFrame = false;
    if (constants.dustRenderEnabled == 0u ||
        !presenter.dustScreenPipeline || !presenter.dustScreenTextures[0] ||
        !presenter.dustScreenTextures[1])
        return;

    const bool recordGuide = presenter.frameGenerationGuidesNeeded &&
        ensureCloudFrameGenerationGuide();
    const UINT writeIndex = presenter.dustScreenWriteIndex;
    const UINT historyIndex = writeIndex ^ 1u;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE dustDescriptors =
        presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
    dustDescriptors.ptr += static_cast<SIZE_T>(dustDescriptorBase +
        presenter.activeFrameIndex * dustDescriptorsPerFrame + 6u) *
        presenter.srvDescriptorSize;
    /* t15 is not read by DustScreenPS; keep it on history while the current
       target is writable. t16 is the actual reprojected prior frame. */
    presenter.device->CreateShaderResourceView(
        presenter.dustScreenTextures[historyIndex].Get(), &srv,
        dustDescriptors);
    dustDescriptors.ptr += presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.dustScreenTextures[historyIndex].Get(), &srv,
        dustDescriptors);

    if (presenter.dustScreenShaderReadable[writeIndex])
    {
        transition(presenter.commandList.Get(),
                   presenter.dustScreenTextures[writeIndex].Get(),
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        presenter.dustScreenShaderReadable[writeIndex] = false;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(frameBufferCount + 1u + writeIndex) *
               presenter.rtvDescriptorSize;
    const float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    presenter.commandList->ClearRenderTargetView(rtv, clear, 0, nullptr);
    if (recordGuide)
    {
        const auto shaderRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        transition(presenter.commandList.Get(), presenter.cloudFrameGenerationGuide.Get(),
                   shaderRead, D3D12_RESOURCE_STATE_RENDER_TARGET);
        presenter.cloudFrameGenerationGuideShaderReadable = false;
        const auto guideRtv = presenter.cloudFrameGenerationGuideRtvHeap->GetCPUDescriptorHandleForHeapStart();
        const float noCloud[4] = {};
        presenter.commandList->ClearRenderTargetView(guideRtv, noCloud, 0, nullptr);
        const D3D12_CPU_DESCRIPTOR_HANDLE targets[] = {rtv, guideRtv};
        presenter.commandList->OMSetRenderTargets(2, targets, FALSE, nullptr);
    }
    else presenter.commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(presenter.dustScreenWidth);
    viewport.Height = static_cast<float>(presenter.dustScreenHeight);
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor = {0, 0,
        static_cast<LONG>(presenter.dustScreenWidth),
        static_cast<LONG>(presenter.dustScreenHeight)};
    presenter.commandList->RSSetViewports(1, &viewport);
    presenter.commandList->RSSetScissorRects(1, &scissor);
    presenter.commandList->SetPipelineState(recordGuide
        ? presenter.cloudFrameGenerationGuidePipeline.Get() : presenter.dustScreenPipeline.Get());
    presenter.commandList->SetGraphicsRootSignature(presenter.rootSignature.Get());
    ID3D12DescriptorHeap *heaps[] = {presenter.srvHeap.Get()};
    presenter.commandList->SetDescriptorHeaps(1, heaps);
    presenter.commandList->SetGraphicsRootDescriptorTable(
        0, presenter.srvHeap->GetGPUDescriptorHandleForHeapStart());
    D3D12_GPU_DESCRIPTOR_HANDLE motionHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    motionHandle.ptr += static_cast<UINT64>(9u) * presenter.srvDescriptorSize;
    presenter.commandList->SetGraphicsRootDescriptorTable(1, motionHandle);
    /* recordDustFroxelBuild owns the field mode. In particular, mode 3 is
       visibility, NOT extinction. Replacing it with mode 1 makes visibility
       values (~1) become dense fog and blacks out the entire scene. */
    presenter.commandList->SetGraphicsRoot32BitConstants(
        2, sizeof(constants) / sizeof(unsigned int), &constants, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE dustHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    dustHandle.ptr += static_cast<UINT64>(dustDescriptorBase +
        presenter.activeFrameIndex * dustDescriptorsPerFrame) *
        presenter.srvDescriptorSize;
    presenter.commandList->SetGraphicsRootDescriptorTable(3, dustHandle);
    presenter.commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    presenter.commandList->DrawInstanced(3, 1, 0, 0);
    transition(presenter.commandList.Get(),
               presenter.dustScreenTextures[writeIndex].Get(),
               D3D12_RESOURCE_STATE_RENDER_TARGET,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    presenter.dustScreenShaderReadable[writeIndex] = true;
    if (recordGuide)
    {
        transition(presenter.commandList.Get(), presenter.cloudFrameGenerationGuide.Get(),
                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        presenter.cloudFrameGenerationGuideShaderReadable = true;
        presenter.cloudFrameGenerationGuideValidThisFrame = true;
    }
    if (constants.presenterPadding[1] == 3.0f)
    {
        recordShaftCapture(presenter.dustFroxelTexture.Get(),0);
        recordShaftCapture(presenter.dustScreenTextures[writeIndex].Get(),1);
    }

    /* Final presentation in this frame samples the freshly written field. */
    dustDescriptors.ptr -= presenter.srvDescriptorSize;
    presenter.device->CreateShaderResourceView(
        presenter.dustScreenTextures[writeIndex].Get(), &srv,
        dustDescriptors);
    presenter.dustScreenWriteIndex = historyIndex;
    presenter.dustScreenHistoryValid = true;
}

bool recordScenePrepass(PresenterConstants &constants, bool cloudProbe = false)
{
    if (!presenter.sceneInputTexture || !presenter.scenePrepassPipeline) return false;
    ID3D12Resource *target = presenter.sceneInputTexture.Get();
    if (cloudProbe)
    {
        // An independent target preserves all SDK-tagged resources through
        // Present. The probe is a counterfactual image, never a displayed one.
        const auto desc = presenter.sceneInputTexture->GetDesc();
        if (!presenter.cloudProbeTexture ||
            presenter.cloudProbeTexture->GetDesc().Width != desc.Width ||
            presenter.cloudProbeTexture->GetDesc().Height != desc.Height)
        {
            D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            ComPtr<ID3D12Resource> texture;
            if (FAILED(presenter.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
                &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture)))) return false;
            if (!presenter.cloudProbeRtvHeap)
            {
                D3D12_DESCRIPTOR_HEAP_DESC rtvHeap = {};
                rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; rtvHeap.NumDescriptors = 1;
                if (FAILED(presenter.device->CreateDescriptorHeap(&rtvHeap,
                    IID_PPV_ARGS(&presenter.cloudProbeRtvHeap)))) return false;
            }
            presenter.device->CreateRenderTargetView(texture.Get(), nullptr,
                presenter.cloudProbeRtvHeap->GetCPUDescriptorHandleForHeapStart());
            texture->SetName(L"Diagnostic only: exact scene input without volumetric composite");
            presenter.cloudProbeTexture = texture;
        }
        target = presenter.cloudProbeTexture.Get();
    }
    if (cloudProbe || presenter.sceneInputTextureShaderReadable)
    {
        transition(presenter.commandList.Get(), target,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (!cloudProbe) presenter.sceneInputTextureShaderReadable = false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(frameBufferCount) *
               presenter.rtvDescriptorSize;
    if (cloudProbe) rtv = presenter.cloudProbeRtvHeap->GetCPUDescriptorHandleForHeapStart();
    presenter.commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(presenter.sceneInputWidth);
    viewport.Height = static_cast<float>(presenter.sceneInputHeight);
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor = { 0, 0,
        static_cast<LONG>(presenter.sceneInputWidth),
        static_cast<LONG>(presenter.sceneInputHeight) };
    presenter.commandList->RSSetViewports(1, &viewport);
    presenter.commandList->RSSetScissorRects(1, &scissor);
    presenter.commandList->SetPipelineState(presenter.scenePrepassPipeline.Get());
    presenter.commandList->SetGraphicsRootSignature(presenter.rootSignature.Get());
    ID3D12DescriptorHeap *heaps[] = { presenter.srvHeap.Get() };
    presenter.commandList->SetDescriptorHeaps(1, heaps);
    presenter.commandList->SetGraphicsRootDescriptorTable(
        0, presenter.srvHeap->GetGPUDescriptorHandleForHeapStart());
    D3D12_GPU_DESCRIPTOR_HANDLE motionHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    motionHandle.ptr += static_cast<UINT64>(9) * presenter.srvDescriptorSize;
    presenter.commandList->SetGraphicsRootDescriptorTable(1, motionHandle);
    presenter.commandList->SetGraphicsRoot32BitConstants(
        2, sizeof(constants) / sizeof(unsigned int), &constants, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE dustHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    dustHandle.ptr += static_cast<UINT64>(
        dustDescriptorBase + presenter.activeFrameIndex * dustDescriptorsPerFrame) *
        presenter.srvDescriptorSize;
    presenter.commandList->SetGraphicsRootDescriptorTable(3, dustHandle);
    presenter.commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    presenter.commandList->DrawInstanced(3, 1, 0, 0);

    transition(presenter.commandList.Get(), target,
               D3D12_RESOURCE_STATE_RENDER_TARGET,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (!cloudProbe) presenter.sceneInputTextureShaderReadable = true;
    return true;
}

bool recordPresenterPass(UINT backBufferIndex,
                         PresenterConstants &constants, bool hudless = false)
{
    /* DustScreenPS intentionally uses the direct authored-volume marcher so
       cloud silhouettes, rotations and soft boundaries remain exact.  The
       old code still rebuilt the 144^3 near cache and 80^3 far cache here,
       even though no later pass sampled either texture.  That dead dispatch
       dominated dense-cloud frames without changing a single output pixel. */
    auto &frame = presenter.frames[presenter.activeFrameIndex];
    const auto shaderRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (hudless && !frame.frameGenerationHudless) {
        D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = presenter.width; desc.Height = presenter.height;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = presenterFormat; desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (FAILED(presenter.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
                &desc,shaderRead,nullptr,IID_PPV_ARGS(&frame.frameGenerationHudless))))
            return false;
        D3D12_CPU_DESCRIPTOR_HANDLE target = presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
        target.ptr += static_cast<SIZE_T>(frameBufferCount + 3u + presenter.activeFrameIndex) * presenter.rtvDescriptorSize;
        presenter.device->CreateRenderTargetView(frame.frameGenerationHudless.Get(),nullptr,target);
        frame.frameGenerationHudless->SetName(L"Frame generation display-size SDR HUD-less input");
    }
    ID3D12Resource *backBuffer = hudless ? frame.frameGenerationHudless.Get()
        : presenter.backBuffers[backBufferIndex].Get();
    transition(presenter.commandList.Get(), backBuffer,
               hudless ? shaderRead : D3D12_RESOURCE_STATE_PRESENT,
               D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(hudless ? frameBufferCount + 3u + presenter.activeFrameIndex : backBufferIndex) *
               presenter.rtvDescriptorSize;
    presenter.commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(presenter.width);
    viewport.Height = static_cast<float>(presenter.height);
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor = { 0, 0,
                           static_cast<LONG>(presenter.width),
                           static_cast<LONG>(presenter.height) };
    presenter.commandList->RSSetViewports(1, &viewport);
    presenter.commandList->RSSetScissorRects(1, &scissor);
    presenter.commandList->SetPipelineState(presenter.presenterPipeline.Get());
    presenter.commandList->SetGraphicsRootSignature(presenter.rootSignature.Get());
    ID3D12DescriptorHeap *heaps[] = { presenter.srvHeap.Get() };
    presenter.commandList->SetDescriptorHeaps(1, heaps);
    presenter.commandList->SetGraphicsRootDescriptorTable(
        0, presenter.srvHeap->GetGPUDescriptorHandleForHeapStart());
    D3D12_GPU_DESCRIPTOR_HANDLE motionHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    motionHandle.ptr += static_cast<UINT64>(9) * presenter.srvDescriptorSize;
    presenter.commandList->SetGraphicsRootDescriptorTable(1, motionHandle);
    presenter.commandList->SetGraphicsRoot32BitConstants(
        2, sizeof(constants) / sizeof(unsigned int), &constants, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE dustHandle =
        presenter.srvHeap->GetGPUDescriptorHandleForHeapStart();
    dustHandle.ptr += static_cast<UINT64>(
        dustDescriptorBase + presenter.activeFrameIndex * dustDescriptorsPerFrame) *
        presenter.srvDescriptorSize;
    presenter.commandList->SetGraphicsRootDescriptorTable(3, dustHandle);
    presenter.commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    presenter.commandList->DrawInstanced(3, 1, 0, 0);
    transition(presenter.commandList.Get(), backBuffer,
               D3D12_RESOURCE_STATE_RENDER_TARGET,
               hudless ? shaderRead : D3D12_RESOURCE_STATE_PRESENT);
    return true;
}

extern "C" void hwModernGraphicsWaitAudioWork(void)
{
    if (!audioEventWorker.wait()) HW_DIAG_FPRINTF(stderr,"[FrameWorkers] Audio event update failed.\n");
}
extern "C" void hwModernGraphicsBeginAudioWork(void (*function)(void))
{
    hwModernGraphicsWaitAudioWork();
    audioFrameWork.function=function;
    if (function) audioEventWorker.dispatch([](void *context) {
        static bool logged=false;
        if (!logged && !hwDiagnosticsDisabled()) {
            HW_DIAG_FPRINTF(stderr,"[FrameWorkers] audio events thread=%lu; mixer/streaming remain on their existing audio threads.\n",GetCurrentThreadId());
            logged=true;
        }
        static_cast<AudioFrameWork *>(context)->function();
    },&audioFrameWork);
}

extern "C" void hwModernGraphicsEndFrame(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){
        hwModernGraphicsWaitAudioWork();
        hwlegacyd3d11::setSun(dustMissionSunValid,dustMissionSunDirection,dustMissionSunColor,
            dustMissionAmbientColor,requestedPrimarySunStrength,requestedSkyAmbientStrength);
        hwlegacyd3d11::setFrameLighting(mapLightEmitters.data(),static_cast<unsigned>(mapLightEmitters.size()),
            missionAuthoringLightEmitters.data(),static_cast<unsigned>(missionAuthoringLightEmitters.size()),
            dynamicLightEmitters.data(),static_cast<unsigned>(dynamicLightEmitters.size()),missionAuthoringReplacesMapLightsForDust,
            requestedEnvironmentStrength,requestedAuthoredLightStrength,requestedSurfaceReflectivity,
            requestedSurfaceRoughness,requestedLightingExposure,requestedFxLightingStrength,requestedShadowStrength);
        hwlegacyd3d11::present();
        if (hwlegacyd3d11::active()) recordPresentedFrame();
        return;
    }
#endif
    if (!presenter.active || !presenter.frameOpen)
    {
        return;
    }
    presenter.frameOpen = false;
    if (!presenter.frameCaptured)
    {
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
        hwglEndFrame();
#endif
        return;
    }

    D3D12FrameContext &frame = presenter.frames[presenter.activeFrameIndex];
    HRESULT result = frame.allocator->Reset();
    if (SUCCEEDED(result))
    {
        result = presenter.commandList->Reset(frame.allocator.Get(),
                                               presenter.presenterPipeline.Get());
    }
    if (FAILED(result))
    {
        logFailure("resetting the D3D12 present command list", result);
        presenter.active = false;
        presenter.visiblePresenter = false;
        return;
    }

    const UINT gpuQueryBase = presenter.activeFrameIndex * gpuTimestampCount;
    const bool gpuTimingActive = HW_ENABLE_DIAGNOSTICS && presenter.timestampQueryHeap &&
        frame.timestampReadback && presenter.timestampFrequency != 0;
    auto writeGpuTimestamp = [&](UINT offset)
    {
        if (gpuTimingActive)
            presenter.commandList->EndQuery(
                presenter.timestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                gpuQueryBase + offset);
    };
    bool gpuDxrTimestampWritten = false;
    if (gpuTimingActive)
        presenter.commandList->EndQuery(
            presenter.timestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            gpuQueryBase + 0u);

    ID3D12Resource *rayEnvironmentResource = presenter.backgroundTexture.Get();
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    /* Bind DXR environment lighting to the same world-space BC6H mission sky
       used by the visible native backdrop. This removes the old camera-screen
       projection/folding that made secondary bounce energy change while the
       camera orbited a stationary ship. */
    bool rayEnvironmentIsDualParaboloid = false;
    if (presenter.missionSkyTextureName != 0 &&
        hwglPrepareMissionSkyForCompute(
            static_cast<GLuint>(presenter.missionSkyTextureName),
            presenter.commandList.Get()))
    {
        void *rawEnvironment = hwglGetMissionSkyD3D12Resource(
            static_cast<GLuint>(presenter.missionSkyTextureName));
        if (rawEnvironment != nullptr)
        {
            rayEnvironmentResource =
                static_cast<ID3D12Resource *>(rawEnvironment);
            const D3D12_RESOURCE_DESC environmentDesc =
                rayEnvironmentResource->GetDesc();
            D3D12_SHADER_RESOURCE_VIEW_DESC environmentSrv = {};
            environmentSrv.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            environmentSrv.Format = environmentDesc.Format;
            environmentSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            environmentSrv.Texture2D.MipLevels = environmentDesc.MipLevels;
            D3D12_CPU_DESCRIPTOR_HANDLE handle =
                presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(rayEnvironmentDescriptorIndex) *
                          presenter.srvDescriptorSize;
            presenter.device->CreateShaderResourceView(
                rayEnvironmentResource, &environmentSrv, handle);
            rayEnvironmentIsDualParaboloid = true;
            if (!presenter.missionSkyEnvironmentLogged)
            {
                HW_DIAG_FPRINTF(stderr,
                    "[ModernGraphics] DXR bounce environment: world-space "
                    "dual-paraboloid BC6H mission sky active.\n");
                presenter.missionSkyEnvironmentLogged = true;
            }
        }
    }
    if (!rayEnvironmentIsDualParaboloid)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC environmentSrv = {};
        environmentSrv.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        environmentSrv.Format = sceneFormat;
        environmentSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        environmentSrv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(rayEnvironmentDescriptorIndex) *
                      presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            presenter.backgroundTexture.Get(), &environmentSrv, handle);
    }
    hwmodern::raytracingSetEnvironmentMode(
        rayEnvironmentIsDualParaboloid, presenter.missionSkyFade);
#else
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetEnvironmentMode(false,1.0f);
#endif
#endif
    if (!beginWorkerCommands(presenter.device.Get(),frame.rayAllocator,frame.rayCommands,L"OpenHW ray tracing commands") ||
        !beginWorkerCommands(presenter.device.Get(),frame.postAllocator,frame.postCommands,L"OpenHW post processing commands")) {
        presenter.active=false;presenter.visiblePresenter=false;return;
    }
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    static std::vector<hwmodern::ResolvedSceneLight> rasterLights;
    const bool rasterLighting = !hwmodern::raytracingActive();
    hwlegacyd3d12::setEmissionSource(hwmodern::raytracingRasterEmissionSource,hwmodern::raytracingReadRasterEmission);
    hwlegacyd3d12::setEmissionBrightness(requestedRasterGlowBrightness);
    if (rasterLighting) hwmodern::resolveRasterSceneLights(hwlegacyd3d12::sceneView(),
        mapLightEmitters.data(),static_cast<unsigned>(mapLightEmitters.size()),
        dynamicLightEmitters.data(),static_cast<unsigned>(dynamicLightEmitters.size()),rasterLights);
    hwlegacyd3d12::setMissionLighting(rasterLighting,rasterLights,requestedLightingExposure,
        requestedShadowStrength,requestedShadowReceiverBias,requestedShadowNormalBias);
#endif
    bool rayOverlayActive=false;
    bool streamlineActiveThisFrame=false,streamlineCameraReset=false;
    float streamlineJitterX=0,streamlineJitterY=0;
    bool rayRecordSucceeded=false;
    bool rayReconstructionEligibleThisFrame=false;
    double dxrRecordMilliseconds=0;
    DWORD rayWorkerId=0,postWorkerId=0;
    // The scene, emitter arrays and descriptor ranges are immutable until join.
    // Frame-slot fences were already waited before these allocators are reset.
    hwmodern::FrameJob rayJob(rayPreparationWorker,[&] {
        rayWorkerId=GetCurrentThreadId();
#if defined(HW_ENABLE_D3D12_BACKEND)
        if (presenter.worldFrameCaptured) {
            const auto started=performanceClockNow();
            rayReconstructionEligibleThisFrame=requestedRayReconstruction &&
                requestedAntiAliasingMode>=HW_MODERN_AA_DLSS_QUALITY &&
                requestedAntiAliasingMode<=HW_MODERN_AA_DLSS_ULTRA_PERFORMANCE &&
                hwmodern::dlaaRayReconstructionAvailable();
            if (requestedAntiAliasingMode!=HW_MODERN_AA_OFF && requestedAntiAliasingMode!=HW_MODERN_AA_FXAA)
                temporalJitter(presenter.frameCount,streamlineJitterX,streamlineJitterY);
            hwmodern::raytracingSetJitter(streamlineJitterX,streamlineJitterY);
            hwmodern::raytracingSetRayReconstructionEnabled(rayReconstructionEligibleThisFrame);
            rayOverlayActive=hwmodern::raytracingRecord(frame.rayCommands.Get(),rayEnvironmentResource,
                mapLightEmitters.data(),static_cast<unsigned>(mapLightEmitters.size()),
                dynamicLightEmitters.data(),static_cast<unsigned>(dynamicLightEmitters.size()));
            dxrRecordMilliseconds=std::chrono::duration<double,std::milli>(performanceClockNow()-started).count();
            if (gpuTimingActive) frame.rayCommands->EndQuery(presenter.timestampQueryHeap.Get(),
                D3D12_QUERY_TYPE_TIMESTAMP,gpuQueryBase+2u);
        }
#endif
        rayRecordSucceeded=SUCCEEDED(frame.rayCommands->Close());
    });
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d12::SnapshotCopyTarget nativeCopies[3] = {};
    size_t nativeCopyCount = 0;
    if (presenter.worldFrameCaptured)
    {
        nativeCopies[nativeCopyCount++] = {
            presenter.worldTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            hwglSnapshotCommandCount(HW_LEGACY_RASTER_WORLD)
        };
    }
    if (presenter.backgroundFrameCaptured)
    {
        nativeCopies[nativeCopyCount++] = {
            presenter.backgroundTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            hwglSnapshotCommandCount(HW_LEGACY_RASTER_BACKGROUND)
        };
    }
    if (presenter.occluderFrameCaptured)
    {
        nativeCopies[nativeCopyCount++] = {
            presenter.occluderTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            hwglSnapshotCommandCount(HW_LEGACY_RASTER_OCCLUDERS)
        };
    }

    /* RTX-0061: replay the legacy command stream once.  The old native path
       replayed the same prefix up to four times to reconstruct final/world/
       background/occluder snapshots.  A real legacy framebuffer only rendered
       once and snapshots observed its state at those boundaries, so copy the
       single D3D12 raster target at the marked command counts instead. */
    if (!hwlegacyd3d12::renderFrameSnapshots(
            presenter.device.Get(), presenter.commandList.Get(),
            presenter.sourceTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            presenter.width, presenter.height,
            hwglSnapshotCommandCount(HW_LEGACY_RASTER_FINAL),
            nativeCopies, nativeCopyCount))
    {
        HW_DIAG_FPRINTF(stderr, "[NativeRaster] Single-pass D3D12 frame replay failed.\n");
        presenter.active = false;
        presenter.visiblePresenter = false;
        return;
    }
    presenter.sourceTextureShaderReadable = true;
    {
        // Refresh the waited flight slot even on allocation/guide failure;
        // a null descriptor prevents reusing the previous frame's core mask.
        D3D12_SHADER_RESOURCE_VIEW_DESC trailSrv = {};
        trailSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        trailSrv.Format = DXGI_FORMAT_R32_FLOAT;
        trailSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        trailSrv.Texture2D.MipLevels = 1;
        auto trailHandle = presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        trailHandle.ptr += static_cast<SIZE_T>(dustDescriptorBase +
            presenter.activeFrameIndex * dustDescriptorsPerFrame + 9u) * presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(hwlegacyd3d12::frameGenerationEffectDepthResource(),
            &trailSrv, trailHandle);
    }
    if (hwlegacyd3d12::prepareDepthForSampling(presenter.commandList.Get()))
    {
        refreshNativeRasterDepthSrv(hwlegacyd3d12::depthResource());
        presenter.nativeRasterDepthSampleReady = true;
    }
    if (presenter.worldFrameCaptured)
        presenter.worldTextureShaderReadable = true;
    if (presenter.backgroundFrameCaptured)
        presenter.backgroundTextureShaderReadable = true;
    if (presenter.occluderFrameCaptured)
        presenter.occluderTextureShaderReadable = true;


#else
    if (presenter.sourceTextureShaderReadable)
    {
        transition(presenter.commandList.Get(), presenter.sourceTexture.Get(),
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COPY_DEST);
    }

    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = presenter.sourceTexture.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource = frame.upload.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = presenter.uploadFootprint;
    presenter.commandList->CopyTextureRegion(&destination, 0, 0, 0,
                                              &source, nullptr);
    transition(presenter.commandList.Get(), presenter.sourceTexture.Get(),
               D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    presenter.sourceTextureShaderReadable = true;

    if (presenter.worldFrameCaptured)
    {
        if (presenter.worldTextureShaderReadable)
        {
            transition(presenter.commandList.Get(), presenter.worldTexture.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST);
        }
        D3D12_TEXTURE_COPY_LOCATION worldDestination = {};
        worldDestination.pResource = presenter.worldTexture.Get();
        worldDestination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION worldSource = {};
        worldSource.pResource = frame.worldUpload.Get();
        worldSource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        worldSource.PlacedFootprint = presenter.uploadFootprint;
        presenter.commandList->CopyTextureRegion(
            &worldDestination, 0, 0, 0, &worldSource, nullptr);
        transition(presenter.commandList.Get(), presenter.worldTexture.Get(),
                   D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        presenter.worldTextureShaderReadable = true;
    }
    else if (!presenter.worldTextureShaderReadable)
    {
        /* t2 remains part of the presenter table even when the runtime branch
           is disabled; keep its resource state valid for D3D12 validation. */
        transition(presenter.commandList.Get(), presenter.worldTexture.Get(),
                   D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        presenter.worldTextureShaderReadable = true;
    }


    if (presenter.backgroundFrameCaptured)
    {
        if (presenter.backgroundTextureShaderReadable)
        {
            transition(presenter.commandList.Get(),
                       presenter.backgroundTexture.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST);
        }
        D3D12_TEXTURE_COPY_LOCATION backgroundDestination = {};
        backgroundDestination.pResource = presenter.backgroundTexture.Get();
        backgroundDestination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION backgroundSource = {};
        backgroundSource.pResource = frame.backgroundUpload.Get();
        backgroundSource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        backgroundSource.PlacedFootprint = presenter.uploadFootprint;
        presenter.commandList->CopyTextureRegion(
            &backgroundDestination, 0, 0, 0, &backgroundSource, nullptr);
        transition(presenter.commandList.Get(), presenter.backgroundTexture.Get(),
                   D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        presenter.backgroundTextureShaderReadable = true;
    }
    else if (!presenter.backgroundTextureShaderReadable)
    {
        transition(presenter.commandList.Get(), presenter.backgroundTexture.Get(),
                   D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        presenter.backgroundTextureShaderReadable = true;
    }

    if (presenter.occluderFrameCaptured)
    {
        if (presenter.occluderTextureShaderReadable)
        {
            transition(presenter.commandList.Get(),
                       presenter.occluderTexture.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST);
        }
        D3D12_TEXTURE_COPY_LOCATION occluderDestination = {};
        occluderDestination.pResource = presenter.occluderTexture.Get();
        occluderDestination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION occluderSource = {};
        occluderSource.pResource = frame.occluderUpload.Get();
        occluderSource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        occluderSource.PlacedFootprint = presenter.uploadFootprint;
        presenter.commandList->CopyTextureRegion(
            &occluderDestination, 0, 0, 0, &occluderSource, nullptr);
        transition(presenter.commandList.Get(), presenter.occluderTexture.Get(),
                   D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        presenter.occluderTextureShaderReadable = true;
    }
    else if (!presenter.occluderTextureShaderReadable)
    {
        /* The shader only samples t7 while god rays are enabled, but keep the
           declared descriptor in a validation-safe state on all other frames. */
        transition(presenter.commandList.Get(), presenter.occluderTexture.Get(),
                   D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        presenter.occluderTextureShaderReadable = true;
    }

#endif

    if (gpuTimingActive)
        presenter.commandList->EndQuery(
            presenter.timestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            gpuQueryBase + 1u);

    // No worker may outlive this frame, including replay/allocation failures.
    if (!rayJob.wait() || !rayRecordSucceeded) {
        logFailure("recording ray tracing worker commands",E_FAIL);
        presenter.active=false;presenter.visiblePresenter=false;return;
    }
    result=presenter.commandList->Close();
    if (FAILED(result)) {logFailure("closing raster commands",result);presenter.active=false;presenter.visiblePresenter=false;return;}
    auto rasterCommandList=presenter.commandList;
    presenter.commandList=frame.postCommands;
    bool postSucceeded=false;
    PresenterConstants constants = {};
    hwmodern::FrameJob postJob(postProcessingWorker,[&] {
    postWorkerId=GetCurrentThreadId();
    auto recordPost=[&]() -> bool {
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (presenter.worldFrameCaptured) {
        presenter.dxrRecordMilliseconds+=dxrRecordMilliseconds;
        
#if HW_ENABLE_DIAGNOSTICS
if (benchmark.recording) presenter.cpuFrameDxrRecordMilliseconds+=dxrRecordMilliseconds;
#endif

        ++presenter.dxrRecordCount;
        gpuDxrTimestampWritten=true;
        if (rayOverlayActive)
        {
            ID3D12Resource *primaryAlbedo =
                hwmodern::raytracingDiffuseAlbedoResource();
            if (primaryAlbedo != nullptr)
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC albedoSrv = {};
                albedoSrv.Shader4ComponentMapping =
                    D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                albedoSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                albedoSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                albedoSrv.Texture2D.MipLevels = 1;
                D3D12_CPU_DESCRIPTOR_HANDLE albedoHandle =
                    presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
                albedoHandle.ptr += static_cast<SIZE_T>(
                    dustDescriptorBase +
                    presenter.activeFrameIndex * dustDescriptorsPerFrame + 4u) *
                    presenter.srvDescriptorSize;
                presenter.device->CreateShaderResourceView(
                    primaryAlbedo, &albedoSrv, albedoHandle);
                static bool emissiveFxProtectionLogged = false;
                if (!emissiveFxProtectionLogged)
                {
                    HW_DIAG_FPRINTF(stderr,
                        "[ModernGraphics] Foreground emissive FX shadow protection "
                        "active: primary-albedo guide separates trails/beams/glows "
                        "from opaque DXR shadow receivers.\n");
                    emissiveFxProtectionLogged = true;
                }
            }
        }
        if (rayOverlayActive && presenter.worldFrameCaptured)
            streamlineCameraReset = streamlineCameraChangedThisFrame();
        else
            presenter.streamlineCameraHistoryValid = false;
        // An exhausted NVIDIA viewport has already selected native fallback.
        // Avoid rebuilding the low-resolution cloud/scene prepass every frame
        // only to discard it. Other backends may activate on first evaluation.
        const bool temporalOutputUsable =
            hwmodern::upscalerResolvedBackend() != hwmodern::UpscalerBackend::NvidiaDlss ||
            hwmodern::dlaaActive();
        if (rayOverlayActive && temporalOutputUsable &&
            requestedAntiAliasingMode != HW_MODERN_AA_OFF &&
            requestedAntiAliasingMode != HW_MODERN_AA_FXAA &&
            presenter.sceneInputTexture)
        {
            /* Real full-scene SR/DLAA input: render the complete HUD-less 3D
               scene (native raster + DXR + dust + scene post) at the same
               internal resolution as the matching depth/motion buffers. UI is
               deliberately absent and is composited at native resolution by
               the final presenter after Streamline reconstruction. */
            PresenterConstants sceneConstants = {};
            sceneConstants.rayOverlayEnabled = 1u;
            sceneConstants.antiAliasingMode =
                static_cast<unsigned int>(HW_MODERN_AA_OFF);
            sceneConstants.sceneCompositeEnabled = 1u;
            sceneConstants.frameIndex = static_cast<unsigned int>(presenter.frameCount);
            sceneConstants.chromaticAberration = requestedChromaticAberration;
            sceneConstants.motionBlur = requestedMotionBlur;
            /* Keep display-only grain out of reconstruction. The native
               colour lattice is aligned below; DXR guides stay untouched. */
            sceneConstants.filmGrain = 0.0f;
            sceneConstants.godRays = requestedGodRays;
            sceneConstants.godRayCenter[0] = presenter.godRayCenterX;
            sceneConstants.godRayCenter[1] = presenter.godRayCenterY;
            sceneConstants.outputDither = 0.0f;
            sceneConstants.godRaySourceColor[0] = presenter.godRaySourceColor[0];
            sceneConstants.godRaySourceColor[1] = presenter.godRaySourceColor[1];
            sceneConstants.godRaySourceColor[2] = presenter.godRaySourceColor[2];
            sceneConstants.godRaySourceExternal =
                presenter.godRaySourceExternalThisFrame ? 1u : 0u;
            sceneConstants.bloom = requestedBloom;
            sceneConstants.scenePrepass = 1u;
            sceneConstants.streamlineResolvedScene = 0u;
            writeGpuTimestamp(3u);
            prepareVolumetricDustFrame(sceneConstants);
            writeGpuTimestamp(4u);
            recordDustFroxelBuild(sceneConstants);
            writeGpuTimestamp(5u);
            recordDustScreenBuild(sceneConstants);
            writeGpuTimestamp(6u);
            // Native colour is sampled on the same lattice as DXR depth and
            // motion. Raster UVs run bottom-up; DXR jitter runs top-down.
            // Reuse the flare centre during this flare-free prepass so the
            // existing 64-DWORD root-signature budget remains unchanged.
            sceneConstants.godRayCenter[0] = streamlineJitterX / std::max(1u, presenter.sceneInputWidth);
            sceneConstants.godRayCenter[1] = -streamlineJitterY / std::max(1u, presenter.sceneInputHeight);
            recordScenePrepass(sceneConstants);
            writeGpuTimestamp(7u);

            if (hwmodern::temporalCaptureEnabled())
            {
                hwmodern::FGEffectGuideCamera camera;
                float viewRows[12] = {};
                if (hwmodern::raytracingSunVisibilityScene(viewRows))
                {
                    std::copy_n(presenter.dustCameraPosition, 3, camera.position);
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        camera.right[axis] = viewRows[axis]; camera.up[axis] = viewRows[4 + axis];
                        camera.forward[axis] = -viewRows[8 + axis];
                    }
                    camera.verticalFovRadians = hwmodern::raytracingFieldOfViewDegrees() * .01745329252f;
                    camera.aspect = hwmodern::raytracingAspectRatio(); camera.frameId = presenter.frameCount + 1;
                    float distanceSquared = 0.0f;
                    if (presenter.godRayFocusValid) for (unsigned axis = 0; axis < 3; ++axis) {
                        const float d = camera.position[axis] - presenter.godRayFocus[axis]; distanceSquared += d * d;
                    }
                    hwmodern::temporalCaptureRecordInputs(presenter.commandList.Get(), presenter.device.Get(),
                        presenter.nextFenceValue, camera, std::sqrt(distanceSquared), streamlineJitterX, streamlineJitterY,
                        frameGenerationDeltaMilliseconds, hwmodern::raytracingHistoryReset() || streamlineCameraReset,
                        requestedAntiAliasingMode, frameGenerationFrameProvider, presenter.sceneInputTexture.Get(),
                        hwmodern::raytracingDepthResource(), hwmodern::raytracingMotionResource());
                }
            }

            if (rayReconstructionEligibleThisFrame)
            {
                streamlineActiveThisFrame =
                    hwmodern::dlaaEvaluateRayReconstruction(
                        presenter.commandList.Get(), presenter.sceneInputTexture.Get(),
                        presenter.sceneInputWidth, presenter.sceneInputHeight, sceneFormat,
                        hwmodern::raytracingDepthResource(),
                        hwmodern::raytracingMotionResource(),
                        hwmodern::raytracingDiffuseAlbedoResource(),
                        hwmodern::raytracingSpecularAlbedoResource(),
                        hwmodern::raytracingNormalRoughnessResource(),
                        hwmodern::raytracingFieldOfViewDegrees(),
                        hwmodern::raytracingAspectRatio(),
                        streamlineJitterX, streamlineJitterY,
                        hwmodern::raytracingHistoryReset() || streamlineCameraReset);
            }
            if (!streamlineActiveThisFrame)
            {
                /* RR failure never changes presentation/exposure math.  Fall
                   back to the existing DLSS SR path for this frame/run. */
                streamlineActiveThisFrame = hwmodern::upscalerEvaluate(
                    presenter.commandList.Get(), presenter.sceneInputTexture.Get(),
                    presenter.sceneInputWidth, presenter.sceneInputHeight, sceneFormat,
                    hwmodern::raytracingDepthResource(),
                    hwmodern::raytracingMotionResource(),
                    nullptr,
                    hwmodern::raytracingFieldOfViewDegrees(),
                    streamlineJitterX, streamlineJitterY,
                    16.6667f,
                    hwmodern::raytracingHistoryReset() || streamlineCameraReset);
            }
            writeGpuTimestamp(8u);
            if (streamlineActiveThisFrame)
                hwmodern::temporalCaptureRecordResolved(presenter.commandList.Get(), presenter.frameCount + 1,
                    hwmodern::upscalerOutputResource());
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
            if (sceneConstants.dustRenderEnabled != 0u &&
                hwmodern::temporalCaptureNeedsCloudProbe(presenter.frameCount + 1))
            {
                const auto motionState=hwmodern::raytracingMotionSnapshot();
                hwmodern::temporalCaptureRecordMotionState(presenter.frameCount + 1,motionState.c_str());
                PresenterConstants probe = sceneConstants;
                probe.dustRenderEnabled = 0u;
                const auto motionRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                transition(presenter.commandList.Get(), hwmodern::raytracingMotionResource(),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, motionRead);
                if (recordScenePrepass(probe, true))
                    hwmodern::temporalCaptureRecordCloudProbe(presenter.commandList.Get(), presenter.frameCount + 1,
                        presenter.cloudProbeTexture.Get(), hwlegacyd3d12::depthResource(),
                        presenter.dustScreenTextures[presenter.dustScreenWriteIndex ^ 1u].Get(),
                        presenter.dustProjectionP10, presenter.dustProjectionP14);
                transition(presenter.commandList.Get(), hwmodern::raytracingMotionResource(),
                    motionRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
#endif
        }
        if (rayOverlayActive)
        {
            // The final presenter and next DXR frame share this handoff state,
            // including frames that bypass a failed temporal reconstruction.
            transition(presenter.commandList.Get(),
                       hwmodern::raytracingMotionResource(),
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
    }
#endif

    if (!gpuDxrTimestampWritten)
        writeGpuTimestamp(2u);

    constants.rayOverlayEnabled = rayOverlayActive ? 1u : 0u;
    if (requestedAntiAliasingMode == HW_MODERN_AA_OFF ||
        requestedAntiAliasingMode == HW_MODERN_AA_FXAA)
    {
        constants.antiAliasingMode = static_cast<unsigned int>(
            requestedAntiAliasingMode);
    }
    else
    {
        /* DLSS/DLAA needs valid depth and motion. FXAA is the deterministic
           cross-vendor fallback for menus and unsupported adapters. */
        constants.antiAliasingMode = streamlineActiveThisFrame ?
            static_cast<unsigned int>(requestedAntiAliasingMode) :
            static_cast<unsigned int>(HW_MODERN_AA_FXAA);
    }
    constants.sceneCompositeEnabled = presenter.worldFrameCaptured ? 1u : 0u;
    constants.frameIndex = static_cast<unsigned int>(presenter.frameCount);
    constants.chromaticAberration = requestedChromaticAberration;
    constants.motionBlur = requestedMotionBlur;
    constants.filmGrain = requestedFilmGrain;
    constants.displayControls = static_cast<unsigned int>(std::lround(requestedDisplayBrightness * 1000.0f)) |
        (static_cast<unsigned int>(std::lround(requestedDisplayGamma * 1000.0f)) << 16u);
    // Sun visibility at individual world-space samples forms the shafts;
    // screen-space sun occlusion must not globally switch them off.
    constants.godRays = requestedGodRays;
    constants.godRayCenter[0] = presenter.godRayCenterX;
    constants.godRayCenter[1] = presenter.godRayCenterY;
    constants.outputDither = requestedOutputDitherStrength;
    constants.godRaySourceColor[0] = presenter.godRaySourceColor[0];
    constants.godRaySourceColor[1] = presenter.godRaySourceColor[1];
    constants.godRaySourceColor[2] = presenter.godRaySourceColor[2];
    constants.godRaySourceExternal = presenter.godRaySourceExternalThisFrame
        ? (presenter.godRaySourceExternalVisible ? 1u : 0u)
        : (presenter.godRaySourceLocked && presenter.godRaySourceVisible ? 1u : 0u);
    if (!presenter.godRaySourceExternalThisFrame && dustMissionSunValid)
        for (unsigned int channel=0; channel<3; ++channel)
            constants.godRaySourceColor[channel] = dustMissionSunColor[channel];
    constants.bloom = requestedBloom;
    if (requestedLensFlares) constants.godRaySourceExternal |= 2u;
    /* Upload/editor-pack dust only when the final presenter will actually
       raymarch it. With Streamline active the complete dust result already
       lives in the reconstructed HUD-less scene, so repeating CPU volume/light
       filtering here is pure overhead. */
    if (!streamlineActiveThisFrame)
    {
        writeGpuTimestamp(3u);
        prepareVolumetricDustFrame(constants);
        writeGpuTimestamp(4u);
        recordDustFroxelBuild(constants);
        writeGpuTimestamp(5u);
        recordDustScreenBuild(constants);
        writeGpuTimestamp(6u);
        writeGpuTimestamp(7u);
        writeGpuTimestamp(8u);
    }
    constants.scenePrepass = 0u;
    constants.streamlineResolvedScene = streamlineActiveThisFrame ? 1u : 0u;
#if defined(HW_ENABLE_D3D12_BACKEND)
    {
        const int provider = frameGenerationFrameProvider;
        const bool useFsr = provider == HW_MODERN_FRAME_GENERATION_FSR_2X &&
            hwmodern::fsrFrameGenerationAvailable() && !hwmodern::fsrFrameGenerationFailed() &&
            !hwmodern::dlaaFrameGenerationDisableFailed();
        const bool useDlss = provider == HW_MODERN_FRAME_GENERATION_DLSS_G_2X &&
            nvidiaFrameGenerationPresentationEnabled &&
            hwmodern::dlaaFrameGenerationAvailable() && !hwmodern::dlaaFrameGenerationFailed() &&
            !hwmodern::fsrFrameGenerationDisableFailed();
        auto *depth = rayOverlayActive ? hwmodern::raytracingDepthResource() : nullptr;
        auto *motion = rayOverlayActive ? hwmodern::raytracingMotionResource() : nullptr;
        frameGenerationInputReason = presenter.worldFrameCaptured
            ? "Frame generation requires valid scene depth and motion guides."
            : "Waiting for a rendered game scene.";
        if ((useFsr || useDlss) && presenter.worldFrameCaptured && depth && motion)
        {
            PresenterConstants hudlessConstants = constants;
            // Mode 2 removes native UI while retaining the exact full-size
            // display processing used by mode 0, including resolved SR color.
            hudlessConstants.scenePrepass = 2u;
            if (recordPresenterPass(presenter.activeFrameIndex,hudlessConstants,true)) {
                const auto depthDesc = depth->GetDesc();
                const auto shaderRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                frameGenerationInputsReady = true;
                frameGenerationInputReason = "Scene inputs ready; waiting for interpolated presentations.";
                if (useFsr) {
                    hwmodern::FsrFrameGenerationCamera camera;
                    camera.nearPlane = .05f; camera.farPlane = 500000.f;
                    std::copy_n(presenter.dustCameraPosition,3,camera.position);
                    float viewRows[12] = {};
                    const bool cameraValid = hwmodern::raytracingSunVisibilityScene(viewRows);
                    if (cameraValid) {
                        for (unsigned axis=0;axis<3;++axis) {
                            camera.right[axis]=viewRows[axis];
                            camera.up[axis]=viewRows[4+axis];
                            camera.forward[axis]=-viewRows[8+axis];
                        }
                    }
                    hwmodern::FGEffectGuideCamera guideCamera;
                    std::copy_n(camera.position,3,guideCamera.position);
                    std::copy_n(camera.right,3,guideCamera.right);
                    std::copy_n(camera.up,3,guideCamera.up);
                    std::copy_n(camera.forward,3,guideCamera.forward);
                    guideCamera.verticalFovRadians=hwmodern::raytracingFieldOfViewDegrees()*.01745329252f;
                    guideCamera.aspect=hwmodern::raytracingAspectRatio();
                    guideCamera.frameId=presenter.frameCount+1;
                    ID3D12Resource *nativeEffectDepth=nullptr;
                    bool nativeEffectDepthFailed=false;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
                    nativeEffectDepth=hwlegacyd3d12::frameGenerationEffectDepthResource();
                    nativeEffectDepthFailed=hwlegacyd3d12::frameGenerationEffectDepthFailed();
#endif
                    auto *cloudEffectDepth=presenter.cloudFrameGenerationGuideValidThisFrame
                        ? presenter.cloudFrameGenerationGuide.Get() : nullptr;
                    // A partially transparent cloud has no single surface
                    // velocity. Preserve the scene's opaque/sky motion; let
                    // FSR optical flow handle the OpenMW color contribution.
                    // The centroid target remains available to diagnostics.
                    const bool effectGuidesReady=cameraValid && !nativeEffectDepthFailed &&
                        hwmodern::fgEffectGuidesPrepare(presenter.commandList.Get(),presenter.device.Get(),
                            presenter.activeFrameIndex,depth,motion,nativeEffectDepth,nullptr,
                            guideCamera,hwmodern::raytracingHistoryReset() || streamlineCameraReset);
                    if (effectGuidesReady) {
                        float focusDistance = 0.0f;
                        if (presenter.godRayFocusValid) {
                            for (unsigned axis=0;axis<3;++axis) {
                                const float delta=camera.position[axis]-presenter.godRayFocus[axis];
                                focusDistance+=delta*delta;
                            }
                            focusDistance=std::sqrt(focusDistance);
                        }
                        hwmodern::fgZoomCaptureRecord(presenter.commandList.Get(),presenter.device.Get(),
                            presenter.nextFenceValue,guideCamera,focusDistance,
                            streamlineJitterX,streamlineJitterY,frameGenerationDeltaMilliseconds,
                            hwmodern::raytracingHistoryReset() || streamlineCameraReset,
                            frame.frameGenerationHudless.Get(),hwmodern::fgEffectGuidesDepth(),
                            hwmodern::fgEffectGuidesMotion(),depth,motion,nativeEffectDepth,cloudEffectDepth);
                        hwmodern::fsrFrameGenerationPrepare(presenter.commandList.Get(),presenter.swapchain.Get(),
                            frame.frameGenerationHudless.Get(),hwmodern::fgEffectGuidesDepth(),hwmodern::fgEffectGuidesMotion(),
                            static_cast<unsigned>(depthDesc.Width),depthDesc.Height,presenter.width,presenter.height,
                            streamlineJitterX,streamlineJitterY,guideCamera.verticalFovRadians,
                            frameGenerationDeltaMilliseconds,hwmodern::raytracingHistoryReset() || streamlineCameraReset,
                            presenter.frameCount+1,&camera);
                    } else {
                        frameGenerationInputsReady=false;
                        frameGenerationInputReason="Frame generation is waiting for valid effect motion guides.";
                    }
                } else {
                    hwmodern::dlaaFrameGenerationPrepare(presenter.commandList.Get(),frame.frameGenerationHudless.Get(),
                        depth,motion,static_cast<unsigned>(depthDesc.Width),depthDesc.Height,
                        presenter.width,presenter.height,hwmodern::raytracingFieldOfViewDegrees(),
                        streamlineJitterX,streamlineJitterY,hwmodern::raytracingHistoryReset() || streamlineCameraReset,
                        shaderRead,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,shaderRead);
                }
            } else {
                frameGenerationInputReason = "Cannot allocate the frame-generation scene input.";
            }
        }
        if (!useFsr || !frameGenerationInputsReady) hwmodern::fsrFrameGenerationSuspend();
    }
#endif
    recordPresenterPass(presenter.activeFrameIndex, constants);

    if (gpuTimingActive)
    {
        presenter.commandList->EndQuery(
            presenter.timestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            gpuQueryBase + 9u);
        presenter.commandList->ResolveQueryData(
            presenter.timestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            gpuQueryBase, gpuTimestampCount, frame.timestampReadback.Get(), 0);
        frame.timestampPending = true;
        frame.benchmarkCaptureId = benchmark.recording ? benchmark.id : 0;
        frame.benchmarkFrameId = presenter.frameCount + 1;
    }

    result = presenter.commandList->Close();
    if (FAILED(result))
    {
        logFailure("ID3D12GraphicsCommandList::Close", result);
        presenter.active = false;
        presenter.visiblePresenter = false;
        return false;
    }
    return true;
    };
    postSucceeded=recordPost();
    });
    // SDL/window presentation and simulation see completed audio handle updates.
    // While the game thread joins audio, the post worker can keep recording.
    hwModernGraphicsWaitAudioWork();
    const bool postJoined=postJob.wait();
    presenter.commandList=rasterCommandList;
    if (!postJoined || !postSucceeded) {
        logFailure("recording post processing worker commands",E_FAIL);
        presenter.active=false;presenter.visiblePresenter=false;return;
    }
    static bool workerOwnershipLogged=false;
    if (!workerOwnershipLogged && presenter.worldFrameCaptured && !hwDiagnosticsDisabled()) {
        HW_DIAG_FPRINTF(stderr,"[FrameWorkers] enabled=%u game=%lu ray/path=%lu post=%lu; independent command lists; GPU order raster/ray/post; audio joins before simulation.\n",
            frameWorkersEnabled?1u:0u,GetCurrentThreadId(),rayWorkerId,postWorkerId);
        workerOwnershipLogged=true;
    }
    ID3D12CommandList *lists[] = {rasterCommandList.Get(),frame.rayCommands.Get(),frame.postCommands.Get()};
    const auto cpuQueueExecuteStarted = performanceClockNow();
    presenter.queue->ExecuteCommandLists(3, lists);
    const auto cpuQueueExecuteFinished = performanceClockNow();

    const UINT syncInterval = presenter.vsyncMode == 0 ? 0u : 1u;
    const UINT presentFlags = syncInterval == 0 && presenter.allowTearing ?
        DXGI_PRESENT_ALLOW_TEARING : 0u;
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::dlaaFrameGenerationPresentBegin();
#endif
    result = presenter.swapchain->Present(syncInterval, presentFlags);
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::dlaaFrameGenerationPresentEnd(SUCCEEDED(result));
#endif
    const auto cpuPresentFinished = performanceClockNow();
    if (FAILED(result))
    {
        logFailure("IDXGISwapChain::Present", result);
        presenter.active = false;
        presenter.visiblePresenter = false;
        return;
    }
    unsigned int presentedCount = 1;
    UINT nativePresentCount = 0;
    if (requestedFrameGenerationMode != HW_MODERN_FRAME_GENERATION_OFF &&
        SUCCEEDED(presenter.swapchain->GetLastPresentCount(&nativePresentCount))) {
        if (presenter.nativePresentCountValid) {
            const UINT delta = nativePresentCount - presenter.previousNativePresentCount;
            // A reset/recreated presentation counter starts a new sample.
            if (delta < 1024u) presentedCount = delta;
        }
        presenter.previousNativePresentCount = nativePresentCount;
        presenter.nativePresentCountValid = true;
    } else {
        presenter.nativePresentCountValid = false;
    }
    recordPresentedFrame(presentedCount);
    const UINT64 fenceValue = presenter.nextFenceValue++;
    result = presenter.queue->Signal(presenter.fence.Get(), fenceValue);
    if (FAILED(result))
    {
        logFailure("ID3D12CommandQueue::Signal", result);
        presenter.active = false;
        presenter.visiblePresenter = false;
        return;
    }
    presenter.submittedFenceValue = fenceValue;
    frame.fenceValue = fenceValue;
    frame.submittedFrameId = presenter.frameCount + 1;
    if (HW_ENABLE_DIAGNOSTICS && presenter.cpuFrameTimerActive)
    {
        double benchmarkIntervalMilliseconds = -1.0;
        
#if HW_ENABLE_DIAGNOSTICS
if (benchmark.recording)
        {
            const auto now = std::chrono::steady_clock::now();
            benchmark.renderCpu.add(std::chrono::duration<double,std::milli>(now-presenter.cpuFrameStarted).count());
            if (benchmark.previousFrame != std::chrono::steady_clock::time_point{})
            {
                benchmarkIntervalMilliseconds = std::chrono::duration<double,std::milli>(now-benchmark.previousFrame).count();
                benchmark.frame.add(benchmarkIntervalMilliseconds);
            }
            benchmark.previousFrame = now;
        }
#endif

        const auto cpuFrameFinished = performanceClockNow();
        const auto milliseconds = [](auto elapsed) {
            return std::chrono::duration<double, std::milli>(elapsed).count();
        };
        presenter.cpuFrameMilliseconds += milliseconds(
            cpuFrameFinished - presenter.cpuFrameStarted);
        presenter.cpuFenceWaitMilliseconds += presenter.cpuFrameFenceWaitMilliseconds;
        presenter.cpuRecordStageMilliseconds += std::max(0.0, milliseconds(
            cpuQueueExecuteStarted - presenter.cpuFrameStarted) -
            presenter.cpuFrameFenceWaitMilliseconds);
        presenter.cpuQueueExecuteMilliseconds += milliseconds(
            cpuQueueExecuteFinished - cpuQueueExecuteStarted);
        presenter.cpuPresentMilliseconds += milliseconds(
            cpuPresentFinished - cpuQueueExecuteFinished);
        presenter.cpuFenceSignalMilliseconds += milliseconds(
            cpuFrameFinished - cpuPresentFinished);
        
#if HW_ENABLE_DIAGNOSTICS
if (benchmark.recording)
        {
            BenchmarkHitchFrame hitch;
            hitch.frameId = frame.submittedFrameId;
            hitch.intervalMs = benchmarkIntervalMilliseconds;
            hitch.rendererMs = milliseconds(cpuFrameFinished - presenter.cpuFrameStarted);
            hitch.fenceWaitMs = presenter.cpuFrameFenceWaitMilliseconds;
            hitch.recordMs = std::max(0.0, milliseconds(cpuQueueExecuteStarted - presenter.cpuFrameStarted) - hitch.fenceWaitMs);
            hitch.executeMs = milliseconds(cpuQueueExecuteFinished - cpuQueueExecuteStarted);
            hitch.presentMs = milliseconds(cpuPresentFinished - cpuQueueExecuteFinished);
            hitch.signalMs = milliseconds(cpuFrameFinished - cpuPresentFinished);
            hitch.dxrRecordMs = presenter.cpuFrameDxrRecordMilliseconds;
            hitch.qpcSeconds = benchmarkQpcSeconds();
            benchmarkRecordHitch(hitch, benchmarkCacheSnapshot());
        }
#endif

        ++presenter.cpuFrameSamples;
        presenter.cpuFrameTimerActive = false;
    }
    ++presenter.frameCount;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwglEndFrame();
#endif

    /* A marker beside the executable arms the user's normal launch. No special
       launcher, GPU captures, extra fence waits, or quality changes required. */
    const bool optimizationLogging = !hwDiagnosticsDisabled();
    const unsigned int telemetryInterval = optimizationLogging ? 120u : 600u;
    if (!hwDiagnosticsDisabled() && (!benchmark.enabled || optimizationLogging) && (presenter.frameCount % telemetryInterval) == 0u)
    {
        if (optimizationLogging)
            HW_DIAG_FPRINTF(stderr,
                "[Optimization] mission=%d frame=%llu output=%ux%u dustScreen=%ux%u "
                "volumes=%u lights=%u wakes=%u dustEnabled=%u godRays=%.3f "
                "camera=(%.1f,%.1f,%.1f) tileCandidates=%.1f\n",
                presenter.godRayMapIdentity, presenter.frameCount,
                presenter.width, presenter.height,
                presenter.dustScreenWidth, presenter.dustScreenHeight,
                presenter.lastDustVolumeCount, presenter.lastDustLightCount,
                presenter.lastDustWakeCount, presenter.lastDustEnabled,
                constants.godRays, presenter.dustCameraPosition[0],
                presenter.dustCameraPosition[1], presenter.dustCameraPosition[2],
                double(presenter.lastDustTileCandidates) / (dustTileColumns * dustTileRows));
        const auto average = [](double total, unsigned long long count)
        {
            return count == 0 ? 0.0 : total / static_cast<double>(count);
        };
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] CPU owning-frame telemetry: %.3f ms/frame "
            "across %llu completed frames (includes legacy recording, "
            "modern command staging and Present).\n",
            average(presenter.cpuFrameMilliseconds, presenter.cpuFrameSamples),
            presenter.cpuFrameSamples);
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] CPU frame phases (%llu completed frames; wall time): "
            "flight-slot wait %.3f ms, record/stage %.3f ms, queue execute %.3f ms, "
            "Present %.3f ms, fence signal/bookkeeping %.3f ms "
            "(driver calls can block); VSync=%d, FG requested=%d, DLSS-G active=%d, "
            "FSR-G active=%d, AA requested=%d, backend=%s.\n",
            presenter.cpuFrameSamples,
            average(presenter.cpuFenceWaitMilliseconds, presenter.cpuFrameSamples),
            average(presenter.cpuRecordStageMilliseconds, presenter.cpuFrameSamples),
            average(presenter.cpuQueueExecuteMilliseconds, presenter.cpuFrameSamples),
            average(presenter.cpuPresentMilliseconds, presenter.cpuFrameSamples),
            average(presenter.cpuFenceSignalMilliseconds, presenter.cpuFrameSamples),
            presenter.vsyncMode, requestedFrameGenerationMode,
            hwmodern::dlaaFrameGenerationActive() ? 1 : 0,
            hwmodern::fsrFrameGenerationActive() ? 1 : 0,
            requestedAntiAliasingMode, hwmodern::upscalerResolvedName());
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] CPU frame telemetry (600 frames): native D3D12 "
            "raster replay active (no OpenGL framebuffer readback); "
            "DXR command staging %.3f ms.\n",
            average(presenter.dxrRecordMilliseconds,
                    presenter.dxrRecordCount));
#else
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] CPU frame telemetry (600 frames): OpenGL "
            "readback background %.3f ms, world %.3f ms, composite %.3f ms; "
            "DXR command staging %.3f ms.\n",
            average(presenter.backgroundReadbackMilliseconds,
                    presenter.backgroundReadbackCount),
            average(presenter.worldReadbackMilliseconds,
                    presenter.worldReadbackCount),
            average(presenter.frameReadbackMilliseconds,
                    presenter.frameReadbackCount),
            average(presenter.dxrRecordMilliseconds,
                    presenter.dxrRecordCount));
#endif
        if (presenter.gpuTimestampFrames != 0)
        {
            const double gpuDivisor =
                static_cast<double>(presenter.gpuTimestampFrames);
            HW_DIAG_FPRINTF(stderr,
                "[ModernGraphics] GPU pass telemetry (%llu resolved frames): "
                "raster %.3f ms, DXR %.3f ms, dust prepare %.3f ms, "
                "dust screen %.3f ms, reconstruction %.3f ms, "
                "presenter %.3f ms, "
                "recorded frame %.3f ms.\n",
                presenter.gpuTimestampFrames,
                presenter.gpuRasterMilliseconds / gpuDivisor,
                presenter.gpuDxrMilliseconds / gpuDivisor,
                presenter.gpuDustPrepareMilliseconds / gpuDivisor,
                presenter.gpuDustScreenMilliseconds / gpuDivisor,
                presenter.gpuReconstructionMilliseconds / gpuDivisor,
                presenter.gpuPresenterMilliseconds / gpuDivisor,
                presenter.gpuFrameMilliseconds / gpuDivisor);
            if (optimizationLogging)
                HW_DIAG_FPRINTF(stderr,
                    "[OptimizationGPU] shadows=%.3f ms cloudMarch=%.3f ms "
                    "sceneComposite=%.3f ms upscaler=%.3f ms\n",
                    presenter.gpuCloudShadowMilliseconds / gpuDivisor,
                    presenter.gpuCloudMarchMilliseconds / gpuDivisor,
                    presenter.gpuSceneCompositeMilliseconds / gpuDivisor,
                    presenter.gpuUpscalerMilliseconds / gpuDivisor);
        }
        presenter.gpuRasterMilliseconds = 0.0;
        presenter.gpuDxrMilliseconds = 0.0;
        presenter.gpuDustPrepareMilliseconds = 0.0;
        presenter.gpuDustScreenMilliseconds = 0.0;
        presenter.gpuReconstructionMilliseconds = 0.0;
        presenter.gpuCloudShadowMilliseconds = 0.0;
        presenter.gpuCloudMarchMilliseconds = 0.0;
        presenter.gpuSceneCompositeMilliseconds = 0.0;
        presenter.gpuUpscalerMilliseconds = 0.0;
        presenter.gpuPresenterMilliseconds = 0.0;
        presenter.gpuFrameMilliseconds = 0.0;
        presenter.gpuTimestampFrames = 0;
        presenter.cpuFrameMilliseconds = 0.0;
        presenter.cpuFenceWaitMilliseconds = 0.0;
        presenter.cpuRecordStageMilliseconds = 0.0;
        presenter.cpuQueueExecuteMilliseconds = 0.0;
        presenter.cpuPresentMilliseconds = 0.0;
        presenter.cpuFenceSignalMilliseconds = 0.0;
        presenter.cpuFrameSamples = 0;
        presenter.backgroundReadbackMilliseconds = 0.0;
        presenter.worldReadbackMilliseconds = 0.0;
        presenter.frameReadbackMilliseconds = 0.0;
        presenter.dxrRecordMilliseconds = 0.0;
        presenter.backgroundReadbackCount = 0;
        presenter.worldReadbackCount = 0;
        presenter.frameReadbackCount = 0;
        presenter.dxrRecordCount = 0;
    }

    if ((presenter.frameCount % 600u) == 1u &&
        (!mapLightEmitters.empty() || !dynamicLightEmitters.empty()))
    {
        size_t navLightCount = 0;
        size_t weaponLightCount = 0;
        size_t engineLightCount = 0;
        for (const HWModernDynamicLightEmitter &emitter : dynamicLightEmitters)
        {
            if (emitter.source == HW_MODERN_LIGHT_NAV)
                ++navLightCount;
            else if (emitter.source == HW_MODERN_LIGHT_ENGINE)
                ++engineLightCount;
            else if (emitter.source == HW_MODERN_LIGHT_MUZZLE_FLASH ||
                     emitter.source == HW_MODERN_LIGHT_WEAPON ||
                     emitter.source == HW_MODERN_LIGHT_ION_BEAM ||
                     emitter.source == HW_MODERN_LIGHT_EXPLOSION)
                ++weaponLightCount;
        }
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] DXR path-traced frame: %zu map, "
                     "%zu dynamic (%zu engine, %zu weapon/beam/explosion, "
                     "%zu nav), %zu emissive material records "
                     "%s.\n",
                     mapLightEmitters.size(), dynamicLightEmitters.size(),
                     engineLightCount, weaponLightCount, navLightCount,
                     emissiveMaterials.size(),
                     rayOverlayActive ? "(DXR shadow rays active)" :
                                        "(awaiting DXR scene geometry)");
    }
}

extern "C" void hwModernGraphicsShutdown(void)
{
    hwModernGraphicsWaitAudioWork();
    rayPreparationWorker.stop();postProcessingWorker.stop();audioEventWorker.stop();
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d11::shutdown();
#endif
    logTextureLoadMetrics();
    presenter.frameOpen = false;
    presenter.frameCaptured = false;
    if (presenter.queue && presenter.fence && presenter.fenceEvent)
    {
        waitForSubmittedWork();
    }
    if (presenter.device && presenter.frameCount > 0)
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] D3D12 visible presenter stopped after "
                     "%llu frames.\n", presenter.frameCount);
    }
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d12::shutdown();
    hwglResetAll();
#endif
    releasePresenter();
}

extern "C" int hwModernGraphicsIsActive(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return 1;
#endif
    return presenter.active ? 1 : 0;
}

extern "C" int hwModernGraphicsIsD3D11(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    return hwlegacyd3d11::active()?1:0;
#else
    return 0;
#endif
}

extern "C" int hwModernGraphicsIsVisiblePresenterActive(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return 1;
#endif
    return presenter.active && presenter.visiblePresenter ? 1 : 0;
}

extern "C" void hwModernGraphicsSetVSync(int mode)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d11::setVSync(mode);
#endif
    presenter.vsyncMode = mode == 0 ? 0 : 1;
    if (mode < 0)
    {
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Adaptive VSync requested; "
                             "DXGI presenter is using ordinary VSync.\n");
    }
}

extern "C" void hwModernGraphicsSetRaytracingEnabled(int enabled)
{
    const bool requested = enabled != 0;
    if (raytracingRequested == requested)
    {
        return;
    }
    raytracingRequested = requested;
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (!presenter.active || !presenter.raytracingDevice)
    {
        return;
    }
    if (!waitForSubmittedWork())
    {
        return;
    }
    if (!raytracingRequested)
    {
        hwmodern::raytracingShutdown();
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv = {};
        nullSrv.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.Format = presenterFormat;
        nullSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nullSrv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE raySrv =
            presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        raySrv.ptr += presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(nullptr, &nullSrv, raySrv);
        D3D12_SHADER_RESOURCE_VIEW_DESC nullMotion = {};
        nullMotion.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullMotion.Format = DXGI_FORMAT_R16G16_FLOAT;
        nullMotion.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nullMotion.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE motionSrv =
            presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        motionSrv.ptr += static_cast<SIZE_T>(9) *
                         presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(nullptr, &nullMotion,
                                                    motionSrv);
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR path tracing disabled.\n");
        return;
    }
    if (hwmodern::raytracingInitialize(
            presenter.device.Get(), presenter.srvHeap.Get(),
            presenter.srvDescriptorSize, 1, 4,
            rayEnvironmentDescriptorIndex, rrGuideUavDescriptorBase,
            presenter.queue.Get()))
    {
        UINT rayWidth = presenter.width;
        UINT rayHeight = presenter.height;
        raytracingDimensionsForAntiAliasingMode(
            requestedAntiAliasingMode, presenter.width, presenter.height,
            rayWidth, rayHeight);
        if (!hwmodern::raytracingCreateOutput(rayWidth, rayHeight))
        {
            hwmodern::raytracingShutdown();
            HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR output allocation "
                                 "failed; D3D12 presentation remains active.\n");
            return;
        }
        hwmodern::raytracingSetFxLightingStrength(
            requestedFxLightingStrength);
        hwmodern::raytracingSetPathSettings(
            requestedPathSamples, requestedPathBounces,
            requestedNormalStrength);
        hwmodern::raytracingSetRayReconstructionEnabled(
            requestedRayReconstruction &&
            requestedAntiAliasingMode >= HW_MODERN_AA_DLSS_QUALITY &&
            requestedAntiAliasingMode <= HW_MODERN_AA_DLSS_ULTRA_PERFORMANCE &&
            hwmodern::dlaaRayReconstructionAvailable());
        hwmodern::raytracingSetLightingTuning(
            requestedEnvironmentStrength, requestedAuthoredLightStrength,
            requestedSurfaceReflectivity, requestedSurfaceRoughness,
            requestedLightingExposure);
        hwmodern::raytracingSetSkyLightingStrengths(
            requestedPrimarySunStrength, requestedSkyAmbientStrength);
        hwmodern::raytracingSetShadowSettings(
            requestedShadowStrength, requestedSunShadowAngularRadiusDegrees,
            requestedSunShadowSamples, requestedLocalShadowSamples,
            requestedLocalShadowAngularRadiusDegrees,
            requestedShadowReceiverBias, requestedShadowNormalBias,
            requestedContactShadowStrength, requestedContactShadowDistance,
            requestedShadowMaximumDistance);
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR path tracing enabled.\n");
    }
    else
    {
        hwmodern::raytracingShutdown();
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] DXR could not be enabled; "
                             "D3D12 presentation remains active.\n");
    }
#endif
}

extern "C" void hwModernGraphicsSetFxLightingStrength(float strength)
{
    if (strength < 0.0f)
    {
        strength = 0.0f;
    }
    else if (strength > 2.0f)
    {
        strength = 2.0f;
    }
    requestedFxLightingStrength = strength;
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetFxLightingStrength(strength);
#endif
}

extern "C" void hwModernGraphicsSetPathTracingSettings(
    unsigned int samplesPerPixel, unsigned int maximumBounces,
    float generatedNormalStrength)
{
    requestedPathSamples = std::max(1u, std::min(samplesPerPixel, 4u));
    requestedPathBounces = std::max(1u, std::min(maximumBounces, 6u));
    requestedNormalStrength = std::max(
        0.0f, std::min(generatedNormalStrength, 4.0f));
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetPathSettings(
        requestedPathSamples, requestedPathBounces,
        requestedNormalStrength);
#endif
}

extern "C" void hwModernGraphicsSetLightingEditorSettings(
    float environmentStrength, float authoredLightStrength,
    float surfaceReflectivity, float surfaceRoughness, float exposure)
{
    requestedEnvironmentStrength = std::max(
        0.0f, std::min(environmentStrength, 2.0f));
    requestedAuthoredLightStrength = std::max(
        0.0f, std::min(authoredLightStrength, 2.0f));
    requestedSurfaceReflectivity = std::max(
        0.0f, std::min(surfaceReflectivity, 2.0f));
    requestedSurfaceRoughness = std::max(
        0.0f, std::min(surfaceRoughness, 1.0f));
    requestedLightingExposure = std::max(
        0.5f, std::min(exposure, 2.0f));
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetLightingTuning(
        requestedEnvironmentStrength, requestedAuthoredLightStrength,
        requestedSurfaceReflectivity, requestedSurfaceRoughness,
        requestedLightingExposure);
#endif
}

extern "C" void hwModernGraphicsSetSkyLightingStrengths(
    float primarySunStrength, float skyAmbientStrength)
{
    requestedPrimarySunStrength = std::max(
        0.0f, std::min(primarySunStrength, 2.0f));
    requestedSkyAmbientStrength = std::max(
        0.0f, std::min(skyAmbientStrength, 2.0f));
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetSkyLightingStrengths(
        requestedPrimarySunStrength, requestedSkyAmbientStrength);
#endif
}

extern "C" void hwModernGraphicsSetShadowSettings(
    float shadowStrength, float sunAngularRadiusDegrees,
    unsigned int sunSamples, unsigned int localSamples,
    float localAngularRadiusDegrees, float receiverBias, float normalBias,
    float contactStrength, float contactDistance, float maximumDistance)
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
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetShadowSettings(
        requestedShadowStrength, requestedSunShadowAngularRadiusDegrees,
        requestedSunShadowSamples, requestedLocalShadowSamples,
        requestedLocalShadowAngularRadiusDegrees,
        requestedShadowReceiverBias, requestedShadowNormalBias,
        requestedContactShadowStrength, requestedContactShadowDistance,
        requestedShadowMaximumDistance);
#endif
}

extern "C" void hwModernGraphicsSetAntiAliasingMode(int mode)
{
    mode = std::max(static_cast<int>(HW_MODERN_AA_OFF),
                    std::min(mode,
                             static_cast<int>(HW_MODERN_AA_XESS_ULTRA_PERFORMANCE)));
    if (requestedAntiAliasingMode == mode)
    {
        return;
    }
#if defined(HW_ENABLE_D3D12_BACKEND)
    // Keep tagged inputs alive if the provider cannot confirm it has stopped.
    if (presenter.active && presenter.device && presenter.srvHeap &&
        (!waitForSubmittedWork() || !stopFrameGenerationInputs())) return;
#endif
    requestedAntiAliasingMode = mode;
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::dlaaSetMode(mode);
    hwmodern::upscalerSetRequestedMode(mode);
    hwmodern::dlaaSetRayReconstructionEnabled(requestedRayReconstruction);
    hwmodern::raytracingSetRayReconstructionEnabled(
        requestedRayReconstruction &&
        mode >= HW_MODERN_AA_DLSS_QUALITY &&
        mode <= HW_MODERN_AA_DLSS_ULTRA_PERFORMANCE &&
        hwmodern::dlaaRayReconstructionAvailable());
    if (!presenter.active || !presenter.device || !presenter.srvHeap)
    {
        return;
    }
    hwmodern::fgEffectGuidesReleaseOutputs();
    hwmodern::upscalerReleaseOutput();
    UINT sceneWidth = presenter.width;
    UINT sceneHeight = presenter.height;
    raytracingDimensionsForAntiAliasingMode(
        mode, presenter.width, presenter.height, sceneWidth, sceneHeight);
    if (hwmodern::raytracingInitialized())
    {
        hwmodern::raytracingReleaseOutput();
        if (!hwmodern::raytracingCreateOutput(sceneWidth, sceneHeight))
        {
            HW_DIAG_FPRINTF(stderr,
                         "[ModernGraphics] Could not resize the DXR target "
                         "for anti-aliasing mode %d.\n", mode);
        }
    }
    if (!createSceneInputTexture(sceneWidth, sceneHeight))
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] Could not resize the full-scene "
                     "Streamline input for anti-aliasing mode %d.\n", mode);
    }
    else
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] Full-scene AA mode %d: %ux%u internal "
                     "-> %ux%u display.\n",
                     mode, sceneWidth, sceneHeight,
                     presenter.width, presenter.height);
    }
    if (!createDustScreenTextures(sceneWidth, sceneHeight))
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] Could not resize screen-space dust target.\n");

    if (mode != HW_MODERN_AA_OFF && mode != HW_MODERN_AA_FXAA &&
        hwmodern::upscalerAvailableForMode(mode))
    {
        hwmodern::upscalerCreateOutput(
            presenter.srvHeap.Get(), presenter.srvDescriptorSize, 3,
            presenter.width, presenter.height);
    }
    else
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = sceneFormat;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            presenter.srvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(3) * presenter.srvDescriptorSize;
        presenter.device->CreateShaderResourceView(
            presenter.worldTexture.Get(), &srv, handle);
    }
#endif
}
extern "C" void hwModernGraphicsSetFrameGenerationMode(int mode)
{
    mode = std::max(static_cast<int>(HW_MODERN_FRAME_GENERATION_OFF),
                    std::min(mode, static_cast<int>(HW_MODERN_FRAME_GENERATION_XESS_2X)));
    if (requestedFrameGenerationMode != mode) {
        requestedFrameGenerationMode = mode;
        frameGenerationRequestChanged = true;
        previousFrameGenerationTime = {};
    }
    // Provider options are applied at the next frame boundary, after the
    // previous frame's inputs and presentation have completed.
}

extern "C" int hwModernGraphicsGetFrameGenerationStatus(void)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (hwmodern::dlaaFrameGenerationDisableFailed() || hwmodern::fsrFrameGenerationDisableFailed()) return 4;
    if (requestedFrameGenerationMode == HW_MODERN_FRAME_GENERATION_OFF) return 0;
    const int provider=selectedFrameGenerationProvider();
    if (provider==HW_MODERN_FRAME_GENERATION_XESS_2X) return 3;
    if (provider==HW_MODERN_FRAME_GENERATION_DLSS_G_2X && !nvidiaFrameGenerationPresentationEnabled) return 3;
    const bool nvidia=provider==HW_MODERN_FRAME_GENERATION_DLSS_G_2X;
    if (nvidia ? hwmodern::dlaaFrameGenerationFailed() : hwmodern::fsrFrameGenerationFailed()) return 4;
    if (!(nvidia ? hwmodern::dlaaFrameGenerationAvailable() : hwmodern::fsrFrameGenerationAvailable())) return 3;
    if (nvidia ? hwmodern::dlaaFrameGenerationActive() : hwmodern::fsrFrameGenerationActive()) return 2;
    return 1;
#else
    return requestedFrameGenerationMode == HW_MODERN_FRAME_GENERATION_OFF ? 0 : 3;
#endif
}

extern "C" const char *hwModernGraphicsGetFrameGenerationReason(void)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    if (hwmodern::dlaaFrameGenerationDisableFailed()) return hwmodern::dlaaFrameGenerationReason();
    if (hwmodern::fsrFrameGenerationDisableFailed()) return hwmodern::fsrFrameGenerationReason();
    if (requestedFrameGenerationMode == HW_MODERN_FRAME_GENERATION_OFF) return "Frame generation is off.";
    const int provider=selectedFrameGenerationProvider();
    if (provider==HW_MODERN_FRAME_GENERATION_XESS_2X) return "Intel XeSS frame generation is not implemented in this build.";
    if (provider==HW_MODERN_FRAME_GENERATION_DLSS_G_2X && !nvidiaFrameGenerationPresentationEnabled)
        return "NVIDIA FG is unavailable in this presenter; use Auto or AMD FSR 2X.";
    const int status=hwModernGraphicsGetFrameGenerationStatus();
    if (status==1 && !frameGenerationInputsReady) return frameGenerationInputReason;
    return provider==HW_MODERN_FRAME_GENERATION_DLSS_G_2X
        ? hwmodern::dlaaFrameGenerationReason() : hwmodern::fsrFrameGenerationReason();
#else
    return "Frame generation requires the D3D12 renderer.";
#endif
}
extern "C" int hwModernGraphicsIsDlaaActive(void)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    return hwmodern::upscalerActive() && hwmodern::raytracingActive() ? 1 : 0;
#else
    return 0;
#endif
}

extern "C" void hwModernGraphicsSetLensFlaresEnabled(int enabled)
{
    requestedLensFlares = enabled != 0;
}

extern "C" void hwModernGraphicsSetPostProcessingSettings(
    float chromaticAberration, float motionBlur, float filmGrain,
    float godRays, float bloom)
{
    requestedChromaticAberration = std::max(
        0.0f, std::min(chromaticAberration, 1.0f));
    requestedMotionBlur = std::max(0.0f, std::min(motionBlur, 1.0f));
    requestedFilmGrain = std::max(0.0f, std::min(filmGrain, 1.0f));
    requestedGodRays = godRays > 0.0f ? 1.0f : 0.0f;
    requestedBloom = std::max(0.0f, std::min(bloom, 4.0f));
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] Scene post FX: chromatic %.0f%%, motion "
                 "blur %.0f%%, film grain %.0f%%, god rays %.0f%%, "
                 "bloom %.0f%%.\n",
                 requestedChromaticAberration * 100.0f,
                 requestedMotionBlur * 100.0f,
                 requestedFilmGrain * 100.0f,
                 requestedGodRays * 100.0f,
                 requestedBloom * 100.0f);
}

extern "C" void hwModernGraphicsSetDisplayControls(float brightness, float gamma)
{
    requestedDisplayBrightness = std::max(0.25f, std::min(brightness, 2.0f));
    requestedDisplayGamma = std::max(0.60f, std::min(gamma, 1.60f));
}

extern "C" void hwModernGraphicsSetMissionGodRayStyle(float strength, const float color[3], int colorOverride)
{
    missionGodRayStrength = std::isfinite(strength) ? std::clamp(strength,0.0f,4.0f) : 1.0f;
    missionGodRayTint = 0u;
    if (colorOverride && color)
    {
        missionGodRayTint = 4u;
        for (unsigned int channel=0;channel<3;++channel)
        {
            float value = std::isfinite(color[channel]) ? std::clamp(color[channel],0.0f,1.0f) : 1.0f;
            missionGodRayTint |= static_cast<unsigned int>(std::lround(value*255.0f)) << (8u+8u*channel);
        }
    }
}

extern "C" void hwModernGraphicsSetGodRaySource(
    float screenX, float screenY, float red, float green, float blue,
    int visible)
{
    /* This is a per-frame projection override only.  Mission identity,
       world-space source locking, sky ambient, and primary-sun lighting stay
       on the existing map-owned path. */
    presenter.godRaySourceExternalThisFrame = true;
    presenter.godRaySourceExternalVisible = visible != 0;
    presenter.godRayCenterX = std::max(-4.0f, std::min(screenX, 5.0f));
    presenter.godRayCenterY = std::max(-4.0f, std::min(screenY, 5.0f));
    if (presenter.godRaySourceExternalVisible)
    {
        presenter.godRaySourceColor[0] = std::max(0.0f, std::min(red, 1.0f));
        presenter.godRaySourceColor[1] = std::max(0.0f, std::min(green, 1.0f));
        presenter.godRaySourceColor[2] = std::max(0.0f, std::min(blue, 1.0f));
    }
    else
    {
        presenter.godRaySourceColor[0] = 0.0f;
        presenter.godRaySourceColor[1] = 0.0f;
        presenter.godRaySourceColor[2] = 0.0f;
    }
}

extern "C" void hwModernGraphicsSetRasterGlowBrightness(float strength)
{
    requestedRasterGlowBrightness = std::isfinite(strength) ? std::max(0.0f, std::min(strength, 2.0f)) : 0.29f;
}

extern "C" void hwModernGraphicsSetOutputDitherStrength(float strength)
{
    requestedOutputDitherStrength = std::max(0.0f, std::min(strength, 2.0f));
    HW_DIAG_FPRINTF(stderr,
                 "[ModernGraphics] Output de-banding dither: %.0f%%.\n",
                 requestedOutputDitherStrength * 100.0f);
}

extern "C" void hwModernGraphicsEarlyInitialize(void)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::dlaaEarlyInitialize();
#endif
}

extern "C" void hwModernGraphicsBeginMapLightUpdate(void)
{
    mapLightEmitters.clear();
}

extern "C" void hwModernGraphicsSubmitMapLight(
    const HWModernMapLightEmitter *emitter)
{
    if (emitter != nullptr)
    {
        mapLightEmitters.push_back(*emitter);
    }
}

extern "C" void hwModernGraphicsEndMapLightUpdate(void)
{
    HW_DIAG_FPRINTF(stderr, "[ModernGraphics] Preserved %zu authored map light "
                         "emitters for the DXR scene.\n",
                 mapLightEmitters.size());
}

extern "C" unsigned int hwModernGraphicsGetMapLightCount(void)
{
    return static_cast<unsigned int>(mapLightEmitters.size());
}

extern "C" int hwModernGraphicsGetMapLight(
    unsigned int index, HWModernMapLightEmitter *outEmitter)
{
    if (outEmitter == nullptr || index >= mapLightEmitters.size())
    {
        return 0;
    }
    *outEmitter = mapLightEmitters[index];
    return 1;
}

extern "C" void hwModernGraphicsSetMissionAuthoringLights(
    const HWModernMissionLightEmitter *emitters, unsigned int count)
{
    missionAuthoringLightEmitters.clear();
    if (emitters != nullptr && count > 0)
        missionAuthoringLightEmitters.assign(emitters, emitters + count);
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetMissionAuthoringLights(emitters, count);
#endif
}

extern "C" void hwModernGraphicsSetMissionAuthoringReplacesMapLights(int replace)
{
    missionAuthoringReplacesMapLightsForDust = replace != 0;
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSetMissionAuthoringReplacesMapLights(replace != 0);
#endif
}

extern "C" void hwModernGraphicsSetVolumetricDustVolumes(
    const HWModernVolumetricDustVolume *volumes, unsigned int count)
{
    volumetricDustVolumes.clear();
    if (volumes == nullptr || count == 0)
    {
        /* A mission/editor reset must not leak recovering wakes into the next
           authored map. */
        volumetricDustWakeHistory.clear();
        volumetricDustInteractorStates.clear();
        return;
    }

    /* Pack renderable volumes BEFORE applying the GPU capacity limit.  The old
       setter truncated the authored array first and filtered disabled/zero-density
       entries later in prepareVolumetricDustFrame().  Dead editor entries could
       therefore consume slots and push valid clouds past the 32-volume cutoff. */
    volumetricDustVolumes.reserve(maximumVolumetricDustVolumes);
    for (unsigned int i = 0; i < count; ++i)
    {
        const HWModernVolumetricDustVolume &volume = volumes[i];
        if (!volume.enabled || volume.density <= 0.00001f) continue;
        volumetricDustVolumes.push_back(volume);
        if (volumetricDustVolumes.size() >= maximumVolumetricDustVolumes) break;
    }

    {
        static unsigned int lastIncomingCount = 0xffffffffu;
        static unsigned int lastPackedCount = 0xffffffffu;
        const unsigned int packedCount =
            static_cast<unsigned int>(volumetricDustVolumes.size());
        if (lastIncomingCount != count || lastPackedCount != packedCount)
        {
            HW_DIAG_FPRINTF(stderr,
                "[ModernGraphics] RTX dust volume upload: incoming=%u renderable=%u cap=%u.\n",
                count, packedCount, maximumVolumetricDustVolumes);
            lastIncomingCount = count;
            lastPackedCount = packedCount;
        }
    }
}

extern "C" void hwModernGraphicsSetParticleFieldVolumes(
    const HWModernVolumetricDustVolume *volumes, unsigned int count)
{
    particleFieldVolumes.clear();
    if (volumes && count) particleFieldVolumes.assign(volumes,volumes+std::min(count,16384u));
}

extern "C" void hwModernGraphicsSetParticleFieldDensity(const float *values, unsigned int count)
{
    ++particleDensityRevision;
    particleFieldDensity.clear();
    if(values && count) particleFieldDensity.assign(values,values+std::min(count,16384u*4913u));
}

extern "C" void hwModernGraphicsSetHarvestableDustVolumes(
    const HWModernVolumetricDustVolume *volumes, unsigned int count)
{
    harvestableDustVolumes.clear();
    if (volumes != nullptr && count > 0)
        harvestableDustVolumes.assign(volumes, volumes + std::min(count, maximumVolumetricDustVolumes));
}

extern "C" unsigned int hwModernGraphicsAsteroidShadowMask(const float position[3],unsigned int mask)
{
    unsigned int result=mask;
    for(const auto *volumes : {&volumetricDustVolumes,&harvestableDustVolumes,&harvestableNebulaVolumes})
        for(const auto &volume : *volumes)
            if(hwmodern::dustContainsAsteroid(volume,position))
                result=mask | hwmodern::excludeAtmosphericShafts;
    // Log each stationary resource once rather than flooding every frame.
    // This checks the live submission path, not only synthetic GPU masks.
    struct PolicySample { float p[3]; unsigned int result; size_t count; };
    static std::vector<PolicySample> samples;
    const size_t count=volumetricDustVolumes.size()+harvestableDustVolumes.size()+harvestableNebulaVolumes.size();
    bool known=false;
    for(const auto &sample:samples)
        if(sample.p[0]==position[0] && sample.p[1]==position[1] && sample.p[2]==position[2] &&
           (sample.result&hwmodern::excludeAtmosphericShafts)==(result&hwmodern::excludeAtmosphericShafts) && sample.count==count)
        { known=true; break; }
    if(!known && samples.size()<4096)
    {
        samples.push_back({{position[0],position[1],position[2]},result,count});
        HW_DIAG_FPRINTF(stderr,"[AsteroidDustPolicy] position=(%g,%g,%g) volumes=%zu exempt=%u TLASmask=%u.\n",
            position[0],position[1],position[2],count,
            (result&hwmodern::excludeAtmosphericShafts)!=0u,hwmodern::solidShadowMask(result));
        if(samples.size()==1 || samples[samples.size()-2].count!=count)
            for(const auto *volumes : {&volumetricDustVolumes,&harvestableDustVolumes,&harvestableNebulaVolumes})
                for(const auto &v:*volumes)
                    HW_DIAG_FPRINTF(stderr,"[AsteroidDustPolicy] cloud shape=%u enabled=%u density=%g coverage=%g position=(%g,%g,%g) size=(%g,%g,%g) rotation=(%g,%g,%g).\n",
                        v.shape,v.enabled,v.density,v.coverage,v.position[0],v.position[1],v.position[2],
                        v.size[0],v.size[1],v.size[2],v.rotationDegrees[0],v.rotationDegrees[1],v.rotationDegrees[2]);
    }
    return result;
}

static void rebuildHarvestableNebulaVolumes()
{
    harvestableNebulaVolumes.clear();
    if (!harvestableNebulaStyle.enabled) return;
    harvestableNebulaVolumes.reserve(harvestableNebulaSegments.size());
    constexpr float radiansToDegrees = 57.29577951308232f;
    for (const HWModernHarvestableNebulaSegment &segment : harvestableNebulaSegments)
    {
        const float dx = segment.end[0] - segment.start[0];
        const float dy = segment.end[1] - segment.start[1];
        const float dz = segment.end[2] - segment.start[2];
        const float horizontal = std::sqrt(dx * dx + dy * dy);
        const float length = std::sqrt(horizontal * horizontal + dz * dz);
        if (length <= 1.0f) continue;
        const float radius = std::max(32.0f,
            segment.radius * std::max(0.25f, harvestableNebulaStyle.radiusScale));
        HWModernVolumetricDustVolume volume = {};
        volume.enabled = 1u;
        volume.shape = HW_MODERN_DUST_ROPE;
        volume.shapeSeed = segment.seed ? segment.seed : 1u;
        volume.flags = HW_MODERN_DUST_RECEIVE_MISSION_KEY |
                       HW_MODERN_DUST_RECEIVE_LOCAL_LIGHTS;
        for (unsigned int axis = 0; axis < 3; ++axis)
            volume.position[axis] = (segment.start[axis] + segment.end[axis]) * 0.5f;
        volume.density = std::max(0.0f, harvestableNebulaStyle.density);
        volume.size[0] = volume.size[1] = radius * 2.0f;
        volume.size[2] = length + radius * 2.0f;
        volume.scattering = std::max(0.0f, harvestableNebulaStyle.scattering);
        volume.absorption = std::max(0.0f, harvestableNebulaStyle.absorption);
        volume.anisotropy = 0.18f;
        volume.coverage = std::max(0.0f, std::min(2.0f,
            0.55f + harvestableNebulaStyle.edgeFeather * 0.65f));
        for (unsigned int channel = 0; channel < 3; ++channel)
            volume.color[channel] = std::max(0.0f, harvestableNebulaStyle.color[channel]);
        volume.noiseScale = std::max(0.000005f, harvestableNebulaStyle.noiseScale);
        volume.noiseDetail = std::max(0.0f, harvestableNebulaStyle.noiseDetail);
        volume.shapeVariation = std::max(0.0f, harvestableNebulaStyle.shapeVariation);
        /* DustWorldToLocalVectorFast applies these inverse rotations. */
        /* volumetricDustPrecomputeVolume negates authored Euler angles before
           DustWorldToLocalVectorFast consumes them.  Store the positive world
           yaw/pitch here so local Z lands exactly on start->end. */
        volume.rotationDegrees[1] = std::atan2(horizontal, dz) * radiansToDegrees;
        volume.rotationDegrees[2] = std::atan2(dy, dx) * radiansToDegrees;
        harvestableNebulaVolumes.push_back(volume);
        if (harvestableNebulaVolumes.size() >= maximumVolumetricDustVolumes) break;
    }
}

extern "C" void hwModernGraphicsSetHarvestableNebulaStyle(
    const HWModernHarvestableNebulaStyle *style)
{
    if (style == nullptr) return;
    harvestableNebulaStyle = *style;
    rebuildHarvestableNebulaVolumes();
}

extern "C" void hwModernGraphicsGetHarvestableNebulaStyle(
    HWModernHarvestableNebulaStyle *style)
{
    if (style != nullptr) *style = harvestableNebulaStyle;
}

extern "C" void hwModernGraphicsSetHarvestableNebulaSegments(
    const HWModernHarvestableNebulaSegment *segments, unsigned int count)
{
    harvestableNebulaSegments.clear();
    if (segments != nullptr && count > 0)
        harvestableNebulaSegments.assign(segments, segments + count);
    rebuildHarvestableNebulaVolumes();
}

extern "C" void hwModernGraphicsSetVolumetricDustFade(
    float distance, float strength)
{
    if (!std::isfinite(distance) || !std::isfinite(strength)) return;
    volumetricDustFadeDistance = std::max(1000.0f,
        std::min(distance, 500000.0f));
    volumetricDustFadeStrength = std::max(0.0f,
        std::min(strength, 8.0f));
}

extern "C" void hwModernGraphicsSetVolumetricDustSimulationPaused(int paused)
{
    const bool nextPaused = paused != 0;
    if (nextPaused == volumetricDustSimulationPaused) return;

    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    if (nextPaused)
    {
        volumetricDustPauseStarted = now;
        volumetricDustSimulationPaused = true;
        return;
    }

    /* Move all wake/interactor timestamps forward by the editor pause span so
       their apparent age is unchanged when simulation resumes. */
    const std::chrono::steady_clock::duration pausedFor =
        now - volumetricDustPauseStarted;
    for (VolumetricDustWakeRecord &wake : volumetricDustWakeHistory)
        wake.created += pausedFor;
    for (auto &state : volumetricDustInteractorStates)
    {
        state.second.lastSeen += pausedFor;
        state.second.lastEmit += pausedFor;
    }
    volumetricDustSimulationPaused = false;
}

extern "C" void hwModernGraphicsSetVolumetricDustInteractors(
    const HWModernVolumetricDustInteractor *interactors, unsigned int count)
{
    (void)interactors;
    (void)count;
    volumetricDustInteractors.clear();
    volumetricDustWakeHistory.clear();
    volumetricDustInteractorStates.clear();
}

extern "C" void hwModernGraphicsSubmitDynamicLight(
    const HWModernDynamicLightEmitter *emitter)
{
    if (emitter != nullptr && emitter->intensity > 0.0f &&
        emitter->radius > 0.0f)
    {
        dynamicLightEmitters.push_back(*emitter);
    }
}

extern "C" void hwModernGraphicsTriggerDynamicLight(
    const HWModernDynamicLightEmitter *emitter, float lifetimeSeconds)
{
    static bool loggedFirstTimedLight = false;
    if (!hwModernGraphicsIsActive() || emitter == nullptr || emitter->intensity <= 0.0f ||
        emitter->radius <= 0.0f || lifetimeSeconds <= 0.0f)
    {
        return;
    }

    TimedDynamicLight retained;
    retained.emitter = *emitter;
    retained.started = DynamicLightClock::now();
    retained.lifetimeSeconds = lifetimeSeconds;
    if (timedDynamicLightEmitters.size() >= 512)
    {
        timedDynamicLightEmitters.erase(timedDynamicLightEmitters.begin());
    }
    timedDynamicLightEmitters.push_back(retained);

    /* If gameplay fired after BeginFrame, include the pulse immediately as
       well as retaining it for subsequent frames. */
    if (presenter.frameOpen)
    {
        dynamicLightEmitters.push_back(*emitter);
    }
    if (!loggedFirstTimedLight)
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] Gun-event timed muzzle lighting active.\n");
        loggedFirstTimedLight = true;
    }
}

extern "C" void hwModernGraphicsRegisterEmissiveMaterial(
    const char *meshName, const char *materialName, const char *textureName,
    unsigned int fullAmbientColors, const float color[3], float intensity)
{
    EmissiveMaterialRecord record;
    record.meshName = meshName != nullptr ? meshName : "";
    record.materialName = materialName != nullptr ? materialName : "";
    record.textureName = textureName != nullptr ? textureName : "";
    record.fullAmbientColors = fullAmbientColors;
    if (color != nullptr)
    {
        record.color[0] = color[0];
        record.color[1] = color[1];
        record.color[2] = color[2];
    }
    record.intensity = intensity;

    for (const EmissiveMaterialRecord &existing : emissiveMaterials)
    {
        if (existing.meshName == record.meshName &&
            existing.materialName == record.materialName &&
            existing.textureName == record.textureName)
        {
            return;
        }
    }
    emissiveMaterials.push_back(record);
}

extern "C" void hwModernGraphicsRegisterSurfaceTexture(
    const void *materialIdentity, unsigned int width, unsigned int height,
    const unsigned int *surfaceRgba, const unsigned int *emissiveRgba,
    const unsigned int *normalRgba, const unsigned int *ormRgba)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){hwlegacyd3d11::registerMaterial(materialIdentity,nullptr,width,height,surfaceRgba,emissiveRgba,normalRgba,ormRgba);return;}
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingRegisterSurfaceTexture(
        materialIdentity, width, height, surfaceRgba, emissiveRgba, normalRgba,
        ormRgba, nullptr);
#else
    (void)materialIdentity;
    (void)width;
    (void)height;
    (void)surfaceRgba;
    (void)emissiveRgba;
    (void)normalRgba;
    (void)ormRgba;
#endif
}

extern "C" void hwModernGraphicsRegisterSurfaceTextureNamed(
    const void *materialIdentity, const char *textureKey,
    unsigned int width, unsigned int height,
    const unsigned int *surfaceRgba, const unsigned int *emissiveRgba,
    const unsigned int *normalRgba, const unsigned int *ormRgba)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){hwlegacyd3d11::registerMaterial(materialIdentity,textureKey,width,height,surfaceRgba,emissiveRgba,normalRgba,ormRgba);return;}
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingRegisterSurfaceTexture(
        materialIdentity, width, height, surfaceRgba, emissiveRgba, normalRgba,
        ormRgba, textureKey);
#else
    (void)materialIdentity; (void)textureKey; (void)width; (void)height;
    (void)surfaceRgba; (void)emissiveRgba; (void)normalRgba; (void)ormRgba;
#endif
}

extern "C" int hwModernGraphicsTryAliasSurfaceTexture(
    const void *materialIdentity, const char *textureKey)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return hwlegacyd3d11::aliasMaterial(materialIdentity,textureKey)?1:0;
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    return hwmodern::raytracingTryAliasSurfaceTexture(materialIdentity,
                                                       textureKey) ? 1 : 0;
#else
    (void)materialIdentity; (void)textureKey; return 0;
#endif
}

namespace
{
struct TextureLoadMetrics
{
    unsigned long long decodeCalls = 0;
    unsigned long long decodeBytes = 0;
    double decodeMilliseconds = 0.0;
    unsigned long long sharedHits = 0;
    unsigned long long sharedMisses = 0;
    unsigned long long uploadCalls = 0;
    unsigned long long uploadBytes = 0;
    double uploadMilliseconds = 0.0;
};

TextureLoadMetrics textureLoadMetrics;

void logTextureLoadMetrics()
{
    if(hwDiagnosticsDisabled()) return;
    HW_DIAG_FPRINTF(stderr,
        "[TexturePerf] DDS decode=%llu calls %.1f MiB %.1f ms; shared=%llu hit/%llu miss; compressed upload=%llu calls %.1f MiB %.1f ms\n",
        textureLoadMetrics.decodeCalls,
        static_cast<double>(textureLoadMetrics.decodeBytes) / (1024.0 * 1024.0),
        textureLoadMetrics.decodeMilliseconds,
        textureLoadMetrics.sharedHits, textureLoadMetrics.sharedMisses,
        textureLoadMetrics.uploadCalls,
        static_cast<double>(textureLoadMetrics.uploadBytes) / (1024.0 * 1024.0),
        textureLoadMetrics.uploadMilliseconds);
}

struct TextureMetricTimer
{
    double *destination;
    std::chrono::steady_clock::time_point started;
    explicit TextureMetricTimer(double *value)
        : destination(value), started(performanceClockNow()) {}
    ~TextureMetricTimer()
    {
        if(hwDiagnosticsDisabled()) return;
        *destination += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    }
};
}

extern "C" int hwModernGraphicsDecodeDds(
    const void *data, unsigned int size, unsigned int *width,
    unsigned int *height, unsigned char **rgba)
{
    TextureMetricTimer metricTimer(&textureLoadMetrics.decodeMilliseconds);
    if(!hwDiagnosticsDisabled()) { ++textureLoadMetrics.decodeCalls; textureLoadMetrics.decodeBytes += size; }
    if (data == nullptr || size == 0 || width == nullptr || height == nullptr ||
        rgba == nullptr)
        return 0;
    *rgba = nullptr;
    DirectX::TexMetadata metadata{};
    DirectX::ScratchImage encoded;
    HRESULT result = DirectX::LoadFromDDSMemory(
        static_cast<const std::uint8_t *>(data), size,
        DirectX::DDS_FLAGS_NONE, &metadata, encoded);
    if (FAILED(result) || metadata.width == 0 || metadata.height == 0 ||
        metadata.width > 16384 || metadata.height > 16384)
        return 0;
    const DirectX::Image *source = encoded.GetImage(0, 0, 0);
    if (source == nullptr) return 0;
    DirectX::ScratchImage decoded;
    if (DirectX::IsCompressed(source->format))
        result = DirectX::Decompress(*source, DXGI_FORMAT_R8G8B8A8_UNORM, decoded);
    else if (source->format == DXGI_FORMAT_R8G8B8A8_UNORM)
        result = S_OK; // DirectXTex rejects Convert when source and target match.
    else
        result = DirectX::Convert(*source, DXGI_FORMAT_R8G8B8A8_UNORM,
                                  DirectX::TEX_FILTER_DEFAULT, 0.0f, decoded);
    if (FAILED(result)) return 0;
    const DirectX::Image *image = source->format == DXGI_FORMAT_R8G8B8A8_UNORM
        ? source : decoded.GetImage(0, 0, 0);
    if (image == nullptr) return 0;
    const size_t rowBytes = image->width * 4u;
    const size_t byteCount = rowBytes * image->height;
    unsigned char *pixels = static_cast<unsigned char *>(std::malloc(byteCount));
    if (pixels == nullptr) return 0;
    for (size_t y = 0; y < image->height; ++y)
        std::memcpy(pixels + y * rowBytes, image->pixels + y * image->rowPitch,
                    rowBytes);
    *width = static_cast<unsigned int>(image->width);
    *height = static_cast<unsigned int>(image->height);
    *rgba = pixels;
    return 1;
}

namespace
{
struct SharedDdsEntry
{
    unsigned char *pixels = nullptr;
    unsigned int width = 0;
    unsigned int height = 0;
    size_t bytes = 0;
    unsigned long long age = 0;
};
struct SharedDdsIssued
{
    std::string key;
    bool parkOnFree = false;
    unsigned int width = 0;
    unsigned int height = 0;
};
std::unordered_map<std::string, SharedDdsEntry> sharedDdsCache;
std::unordered_map<void *, SharedDdsIssued> sharedDdsIssued;
size_t sharedDdsCacheBytes = 0;
unsigned long long sharedDdsAge = 0;
constexpr size_t sharedDdsCacheLimit = 128u * 1024u * 1024u;

std::string normalizedDdsKey(const char *value)
{
    std::string key = value != nullptr ? value : "";
    for (char &character : key)
    {
        if (character == '\\') character = '/';
        else if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return key;
}

void trimSharedDdsCache()
{
    while (sharedDdsCacheBytes > sharedDdsCacheLimit &&
           !sharedDdsCache.empty())
    {
        auto oldest = sharedDdsCache.begin();
        for (auto it = sharedDdsCache.begin(); it != sharedDdsCache.end(); ++it)
            if (it->second.age < oldest->second.age) oldest = it;
        sharedDdsCacheBytes -= oldest->second.bytes;
        std::free(oldest->second.pixels);
        sharedDdsCache.erase(oldest);
    }
}
}

extern "C" int hwModernGraphicsDecodeDdsShared(
    const void *data, unsigned int size, const char *cacheKey,
    unsigned int *width, unsigned int *height, unsigned char **rgba)
{
    if (width == nullptr || height == nullptr || rgba == nullptr) return 0;
    const std::string key = normalizedDdsKey(cacheKey);
    auto found = sharedDdsCache.find(key);
    if (!key.empty() && found != sharedDdsCache.end())
    {
        if(!hwDiagnosticsDisabled()) ++textureLoadMetrics.sharedHits;
        SharedDdsEntry entry = found->second;
        sharedDdsCacheBytes -= entry.bytes;
        sharedDdsCache.erase(found);
        *width = entry.width;
        *height = entry.height;
        *rgba = entry.pixels;
        sharedDdsIssued[entry.pixels] = {key, false, entry.width, entry.height};
        return 1;
    }
    if(!hwDiagnosticsDisabled()) ++textureLoadMetrics.sharedMisses;
    if (!hwModernGraphicsDecodeDds(data, size, width, height, rgba)) return 0;
    if (!key.empty())
        sharedDdsIssued[*rgba] = {key, true, *width, *height};
    return 1;
}

extern "C" void hwModernGraphicsFreeDecodedDds(void *rgba)
{
    auto issued = sharedDdsIssued.find(rgba);
    if (issued != sharedDdsIssued.end())
    {
        SharedDdsIssued value = issued->second;
        sharedDdsIssued.erase(issued);
        if (value.parkOnFree)
        {
            const size_t bytes = static_cast<size_t>(value.width) *
                                 value.height * 4u;
            auto previous = sharedDdsCache.find(value.key);
            if (previous != sharedDdsCache.end())
            {
                sharedDdsCacheBytes -= previous->second.bytes;
                std::free(previous->second.pixels);
                sharedDdsCache.erase(previous);
            }
            sharedDdsCache[value.key] = {
                static_cast<unsigned char *>(rgba), value.width, value.height,
                bytes, ++sharedDdsAge};
            sharedDdsCacheBytes += bytes;
            trimSharedDdsCache();
            return;
        }
    }
    std::free(rgba);
}

extern "C" unsigned int hwModernGraphicsModelTextureCap(int quality)
{
    static const unsigned int cap = hwTextureResolutionCap(quality);
    observedModelTextureCap = static_cast<int>(cap);
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        HW_DIAG_FPRINTF(stderr, "[ModernTexture] Model resolution long-edge cap=%u (0=full source); restart required for changes.\n", cap);
    }
    return cap;
}

extern "C" int hwModernGraphicsDecodeModelDds(
    const void *data, unsigned int size, const char *key, int quality,
    unsigned int *width, unsigned int *height, unsigned char **rgba)
{
    const unsigned int cap = hwModernGraphicsModelTextureCap(quality);
    if (!hwModernGraphicsDecodeDdsShared(data, size, key, width, height, rgba)) return 0;
    unsigned int w = *width, h = *height;
    hwTextureResolutionDimensions(&w, &h, cap);
    if (w == *width && h == *height) return 1;
    DirectX::Image source{*width, *height, DXGI_FORMAT_R8G8B8A8_UNORM,
        static_cast<size_t>(*width) * 4u, static_cast<size_t>(*width) * *height * 4u, *rgba};
    DirectX::ScratchImage resized;
    if (FAILED(DirectX::Resize(source, w, h, DirectX::TEX_FILTER_FANT, resized)))
    {
        hwModernGraphicsFreeDecodedDds(*rgba); *rgba = nullptr; return 0;
    }
    const DirectX::Image *image = resized.GetImage(0, 0, 0);
    unsigned char *pixels = static_cast<unsigned char *>(std::malloc(static_cast<size_t>(w) * h * 4u));
    if (!pixels) { hwModernGraphicsFreeDecodedDds(*rgba); *rgba = nullptr; return 0; }
    for (unsigned int y = 0; y < h; ++y)
        std::memcpy(pixels + static_cast<size_t>(y) * w * 4u,
            image->pixels + y * image->rowPitch, static_cast<size_t>(w) * 4u);
    hwModernGraphicsFreeDecodedDds(*rgba);
    *rgba = pixels; *width = w; *height = h;
    return 1;
}

extern "C" int hwModernGraphicsUploadBoundDds(
    const void *data, unsigned int size)
{
#if defined(HW_ENABLE_D3D12_NATIVE_RASTER)
    TextureMetricTimer metricTimer(&textureLoadMetrics.uploadMilliseconds);
    if(!hwDiagnosticsDisabled()) { ++textureLoadMetrics.uploadCalls; textureLoadMetrics.uploadBytes += size; }
    if (data == nullptr || size == 0) return 0;
    DirectX::TexMetadata metadata{};
    DirectX::ScratchImage encoded;
    HRESULT result = DirectX::LoadFromDDSMemory(
        static_cast<const std::uint8_t *>(data), size,
        DirectX::DDS_FLAGS_NONE, &metadata, encoded);
    if (FAILED(result) || metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
        metadata.arraySize != 1 ||
        (metadata.format != DXGI_FORMAT_BC7_UNORM &&
         metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM) ||
        metadata.width == 0 || metadata.height == 0)
        return 0;
    for (size_t level = 0; level < metadata.mipLevels; ++level)
    {
        const DirectX::Image *image = encoded.GetImage(level, 0, 0);
        if (image == nullptr || image->slicePitch > 0x7fffffffu) return 0;
        if (metadata.format == DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            std::vector<unsigned char> packed(image->width * image->height * 4u);
            for (size_t y = 0; y < image->height; ++y)
                std::memcpy(packed.data() + y * image->width * 4u,
                    image->pixels + y * image->rowPitch, image->width * 4u);
            hwglTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), GL_RGBA,
                static_cast<GLsizei>(image->width), static_cast<GLsizei>(image->height),
                0, GL_RGBA, GL_UNSIGNED_BYTE, packed.data());
        }
        else hwglCompressedTexImage2D(
            GL_TEXTURE_2D, static_cast<GLint>(level), 0x8E8Cu,
            static_cast<GLsizei>(image->width),
            static_cast<GLsizei>(image->height), 0,
            static_cast<GLsizei>(image->slicePitch), image->pixels);
    }
    return 1;
#else
    (void)data;
    (void)size;
    return 0;
#endif
}

extern "C" void hwModernGraphicsUnregisterSurfaceTexture(
    const void *materialIdentity)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){hwlegacyd3d11::unregisterMaterial(materialIdentity);return;}
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingUnregisterSurfaceTexture(materialIdentity);
#else
    (void)materialIdentity;
#endif
}

extern "C" void hwModernGraphicsBeginRaytracingScene(
    float verticalFieldOfViewDegrees, float aspectRatio,
    const float viewMatrix[16])
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){hwlegacyd3d11::beginScene(viewMatrix);return;}
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d12::beginScene(viewMatrix);
#endif
    hwmodern::raytracingBeginScene(verticalFieldOfViewDegrees, aspectRatio,
                                   viewMatrix);
#else
    (void)verticalFieldOfViewDegrees;
    (void)aspectRatio;
    (void)viewMatrix;
#endif
}

extern "C" void hwModernGraphicsEndRaytracingScene(void)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active()){hwlegacyd3d11::endScene();return;}
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    hwlegacyd3d12::endScene();
#endif
    hwmodern::raytracingEndScene();
#endif
}

extern "C" int hwModernGraphicsIsRaytracingSceneOpen(void)
{
#if defined(HW_ENABLE_D3D12_BACKEND)
    return hwmodern::raytracingSceneOpen() ? 1 : 0;
#else
    return 0;
#endif
}

extern "C" void hwModernGraphicsSubmitTriangleGeometryMaskedIdentified(
    const void *geometryIdentity,
    const float *vertexPositions, unsigned int vertexCount,
    unsigned int vertexStrideBytes,
    const unsigned short *triangleIndices, unsigned int triangleCount,
    unsigned int triangleStrideBytes,
    unsigned int surfaceVariant,
    const HWModernTriangleSurface *triangleSurfaces,
    unsigned int triangleSurfaceStrideBytes,
    const float modelViewMatrix[16],
    const float baseColor[3], unsigned int instanceMask,
    unsigned long long motionOwner, unsigned long long motionPart)
{
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    if(hwlegacyd3d11::active())return;
#endif
#if defined(HW_ENABLE_D3D12_BACKEND)
    hwmodern::raytracingSubmitGeometry(
        geometryIdentity, vertexPositions, vertexCount, vertexStrideBytes,
        triangleIndices, triangleCount, triangleStrideBytes, surfaceVariant,
        triangleSurfaces, triangleSurfaceStrideBytes, modelViewMatrix,
        baseColor, instanceMask, motionOwner, motionPart);
#else
    (void)geometryIdentity; (void)vertexPositions; (void)vertexCount;
    (void)vertexStrideBytes; (void)triangleIndices; (void)triangleCount;
    (void)triangleStrideBytes; (void)surfaceVariant; (void)triangleSurfaces;
    (void)triangleSurfaceStrideBytes; (void)modelViewMatrix; (void)baseColor;
    (void)instanceMask;
    (void)motionOwner; (void)motionPart;
#endif
}

extern "C" void hwModernGraphicsSubmitTriangleGeometryMasked(
    const void *geometryIdentity,
    const float *vertexPositions, unsigned int vertexCount,
    unsigned int vertexStrideBytes,
    const unsigned short *triangleIndices, unsigned int triangleCount,
    unsigned int triangleStrideBytes,
    unsigned int surfaceVariant,
    const HWModernTriangleSurface *triangleSurfaces,
    unsigned int triangleSurfaceStrideBytes,
    const float modelViewMatrix[16],
    const float baseColor[3], unsigned int instanceMask)
{
    hwModernGraphicsSubmitTriangleGeometryMaskedIdentified(
        geometryIdentity, vertexPositions, vertexCount, vertexStrideBytes,
        triangleIndices, triangleCount, triangleStrideBytes, surfaceVariant,
        triangleSurfaces, triangleSurfaceStrideBytes, modelViewMatrix,
        baseColor, instanceMask, 0, 0);
}

extern "C" void hwModernGraphicsSubmitTriangleGeometry(
    const void *geometryIdentity,
    const float *vertexPositions, unsigned int vertexCount,
    unsigned int vertexStrideBytes,
    const unsigned short *triangleIndices, unsigned int triangleCount,
    unsigned int triangleStrideBytes,
    unsigned int surfaceVariant,
    const HWModernTriangleSurface *triangleSurfaces,
    unsigned int triangleSurfaceStrideBytes,
    const float modelViewMatrix[16],
    const float baseColor[3])
{
    hwModernGraphicsSubmitTriangleGeometryMasked(
        geometryIdentity, vertexPositions, vertexCount, vertexStrideBytes,
        triangleIndices, triangleCount, triangleStrideBytes, surfaceVariant,
        triangleSurfaces, triangleSurfaceStrideBytes, modelViewMatrix,
        baseColor, 0x03u);
}
#else

extern "C" int hwModernGraphicsQueryCapabilities(HWModernGraphicsCapabilities *capabilities)
{
    if (capabilities != nullptr)
        std::memset(capabilities, 0, sizeof(*capabilities));
    return 0;
}
extern "C" void hwModernGraphicsLogCapabilities(void)
{
    HW_DIAG_FPRINTF(stderr, "[ModernGraphics] D3D12 support is Windows-only.\n");
}
extern "C" int hwModernGraphicsInitialize(void *, unsigned int, unsigned int)
{
    return 0;
}
extern "C" void hwModernGraphicsBeginFrame(unsigned int, unsigned int) {}
extern "C" void hwModernGraphicsSetGodRayWorldSource(
    int, int, const float[3]) {}
extern "C" void hwModernGraphicsSetMissionSkyLighting(
    int, int, const float[3], const float[3], const float[3]) {}
extern "C" int hwModernGraphicsRenderMissionSky(unsigned int, float) { return 0; }
extern "C" int hwModernGraphicsRenderBackgroundStars(unsigned int, const float *, unsigned int, float) { return 0; }
extern "C" int hwModernGraphicsCaptureOpenGLBackground(
    int, float, float, const float[16], const float[16]) { return 0; }
extern "C" int hwModernGraphicsCaptureOpenGLOccluders(void) { return 0; }
extern "C" int hwModernGraphicsCaptureOpenGLWorld(void) { return 0; }
extern "C" int hwModernGraphicsCaptureOpenGLFrame(void) { return 0; }
extern "C" int hwModernGraphicsFlushOpenGLFrame(unsigned int, unsigned int)
{
    return 0;
}
extern "C" void hwModernGraphicsBeginAudioWork(void (*function)(void)) { if(function) function(); }
extern "C" void hwModernGraphicsWaitAudioWork(void) {}
extern "C" void hwModernGraphicsEndFrame(void) {}
extern "C" void hwModernGraphicsShutdown(void) {}
extern "C" void hwModernBenchmarkRequest(void) {}
extern "C" void hwModernBenchmarkCpuBegin(unsigned int) {}
extern "C" void hwModernBenchmarkCpuEnd(unsigned int) {}
extern "C" int hwModernGraphicsIsActive(void) { return 0; }
extern "C" int hwModernGraphicsIsD3D11(void) { return 0; }
extern "C" int hwModernGraphicsIsVisiblePresenterActive(void) { return 0; }
extern "C" void hwModernGraphicsSetVSync(int) {}
extern "C" void hwModernGraphicsSetRaytracingEnabled(int) {}
extern "C" void hwModernGraphicsSetSensorViewActive(int) {}
extern "C" void hwModernGraphicsSetFxLightingStrength(float) {}
extern "C" void hwModernGraphicsSetPathTracingSettings(
    unsigned int, unsigned int, float) {}
extern "C" void hwModernGraphicsSetLightingEditorSettings(
    float, float, float, float, float) {}
extern "C" void hwModernGraphicsSetSkyLightingStrengths(float, float) {}
extern "C" void hwModernGraphicsSetShadowSettings(
    float, float, unsigned int, unsigned int, float, float, float,
    float, float, float) {}
extern "C" void hwModernGraphicsSetAntiAliasingMode(int) {}
extern "C" void hwModernGraphicsSetFrameGenerationMode(int) {}
extern "C" int hwModernGraphicsGetFrameGenerationStatus(void) { return 3; }
extern "C" const char *hwModernGraphicsGetFrameGenerationReason(void) { return "Frame generation requires Windows D3D12."; }
extern "C" int hwModernGraphicsIsDlaaActive(void) { return 0; }
extern "C" void hwModernGraphicsSetLensFlaresEnabled(int) {}
extern "C" void hwModernGraphicsSetPostProcessingSettings(
    float, float, float, float, float) {}
extern "C" void hwModernGraphicsSetGodRaySource(
    float, float, float, float, float, int) {}
extern "C" void hwModernGraphicsSetRasterGlowBrightness(float) {}
extern "C" void hwModernGraphicsSetOutputDitherStrength(float) {}
extern "C" void hwModernGraphicsEarlyInitialize(void) {}
extern "C" void hwModernGraphicsBeginMapLightUpdate(void) {}
extern "C" void hwModernGraphicsSubmitMapLight(const HWModernMapLightEmitter *) {}
extern "C" void hwModernGraphicsEndMapLightUpdate(void) {}
extern "C" void hwModernGraphicsSetMissionAuthoringLights(
    const HWModernMissionLightEmitter *, unsigned int) {}
extern "C" void hwModernGraphicsSetMissionAuthoringReplacesMapLights(int) {}
extern "C" void hwModernGraphicsSetVolumetricDustVolumes(
    const HWModernVolumetricDustVolume *, unsigned int) {}
extern "C" void hwModernGraphicsSetParticleFieldVolumes(
    const HWModernVolumetricDustVolume *, unsigned int) {}
extern "C" void hwModernGraphicsSetParticleFieldDensity(const float *, unsigned int) {}
extern "C" void hwModernGraphicsSetHarvestableNebulaStyle(
    const HWModernHarvestableNebulaStyle *) {}
extern "C" void hwModernGraphicsSetHarvestableNebulaSegments(
    const HWModernHarvestableNebulaSegment *, unsigned int) {}
extern "C" void hwModernGraphicsSetHarvestableDustVolumes(
    const HWModernVolumetricDustVolume *, unsigned int) {}
extern "C" unsigned int hwModernGraphicsAsteroidShadowMask(const float *,unsigned int mask) { return mask; }
extern "C" void hwModernGraphicsSetVolumetricDustFade(float, float) {}
extern "C" void hwModernGraphicsSetVolumetricDustCamera(
    const float[3], const float[3], const float[3], const float[3]) {}
extern "C" void hwModernGraphicsSetGodRayFocus(const float[3]) {}
extern "C" void hwModernGraphicsNotifyCameraZoom(void) {}
extern "C" void hwModernGraphicsSetMissionGodRayStyle(float,const float[3],int) {}
extern "C" void hwModernGraphicsSetVolumetricDustSimulationPaused(int) {}
extern "C" void hwModernGraphicsSetVolumetricDustInteractors(
    const HWModernVolumetricDustInteractor *, unsigned int) {}
extern "C" void hwModernGraphicsSubmitDynamicLight(
    const HWModernDynamicLightEmitter *) {}
extern "C" void hwModernGraphicsTriggerDynamicLight(
    const HWModernDynamicLightEmitter *, float) {}
extern "C" void hwModernGraphicsRegisterEmissiveMaterial(
    const char *, const char *, const char *, unsigned int, const float[3],
    float) {}
extern "C" void hwModernGraphicsRegisterSurfaceTexture(
    const void *, unsigned int, unsigned int, const unsigned int *,
    const unsigned int *, const unsigned int *, const unsigned int *) {}
extern "C" void hwModernGraphicsUnregisterSurfaceTexture(const void *) {}
extern "C" void hwModernGraphicsRegisterSurfaceTextureNamed(
    const void *, const char *, unsigned int, unsigned int,
    const unsigned int *, const unsigned int *, const unsigned int *,
    const unsigned int *) {}
extern "C" int hwModernGraphicsTryAliasSurfaceTexture(const void *, const char *)
{ return 0; }
extern "C" void hwModernGraphicsBeginRaytracingScene(
    float, float, const float[16]) {}
extern "C" void hwModernGraphicsEndRaytracingScene(void) {}
extern "C" int hwModernGraphicsIsRaytracingSceneOpen(void) { return 0; }
extern "C" void hwModernGraphicsSubmitTriangleGeometryMaskedIdentified(
    const void *, const float *, unsigned int, unsigned int,
    const unsigned short *, unsigned int, unsigned int, unsigned int,
    const HWModernTriangleSurface *, unsigned int, const float[16],
    const float[3], unsigned int, unsigned long long, unsigned long long) {}
extern "C" void hwModernGraphicsSubmitTriangleGeometryMasked(
    const void *, const float *, unsigned int, unsigned int,
    const unsigned short *, unsigned int, unsigned int, unsigned int,
    const HWModernTriangleSurface *, unsigned int, const float[16],
    const float[3], unsigned int) {}
extern "C" void hwModernGraphicsSubmitTriangleGeometry(
    const void *, const float *, unsigned int, unsigned int,
    const unsigned short *, unsigned int, unsigned int, unsigned int,
    const HWModernTriangleSurface *, unsigned int, const float[16],
    const float[3]) {}
#endif
