#pragma once

#include <d3d12.h>

namespace hwmodern
{
struct FGEffectGuideCamera
{
    float position[3] = {};
    float right[3] = {1.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    float forward[3] = {0.0f, 0.0f, -1.0f};
    float verticalFovRadians = 1.0471975512f;
    float aspect = 1.0f;
    // Consecutive actual application frame IDs; a gap resets reprojection.
    unsigned long long frameId = 0;
};

// Render-thread only; call solely when AMD FG will consume these outputs.
// Caller has already waited flightSlot's existing fence (three slots).
// Inputs: opaque R32_FLOAT normal depth + R16G16_FLOAT pixel motion at render
// resolution; optional R32_FLOAT positive view-Z at full output resolution,
// bottom-up (FLT_MAX empty; covering engine cores encode -1000000/viewZ);
// optional R32G32_FLOAT ray-distance/opacity cloud
// guide, top-down ((0,0) empty). Every input must include NPSR state.
// Opaque depth/motion is preserved exactly except where a covering engine
// core is in front. Ordinary translucent native FX exclude the opaque 3x3
// neighborhood; clear-sky trails retain their finite motion.
// Cloud guides retain sky depth; they never become solid foreground surfaces.
// Input/color/SR resources are never written. The only work is one CS dispatch.
// This changes compute root bindings and the shader-visible descriptor heap.
bool fgEffectGuidesPrepare(ID3D12GraphicsCommandList *commands,
    ID3D12Device *device, unsigned int flightSlot,
    ID3D12Resource *opaqueDepth, ID3D12Resource *opaquePixelMotion,
    ID3D12Resource *nativeFxLinearDepth, ID3D12Resource *cloudDistanceOpacity,
    const FGEffectGuideCamera &camera, bool reset);

// Borrowed, valid through this slot's fence: depth NPSR, motion PSR|NPSR.
ID3D12Resource *fgEffectGuidesDepth(void);
ID3D12Resource *fgEffectGuidesMotion(void);

// Stop interpolation and drain the presenter before either release call.
void fgEffectGuidesReleaseOutputs(void);
void fgEffectGuidesReleaseDevice(void);
}
