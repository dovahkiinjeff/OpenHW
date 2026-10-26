#pragma once
#include "DiagnosticPolicy.h"
#include "FGEffectGuides.h"
#include <d3d12.h>
namespace hwmodern {
#if HW_ENABLE_DIAGNOSTICS
// Re-armable reconstruction capture. The user/launcher supplies a new marker
// label per burst; frame generation is optional. Disabled in normal launches.
bool temporalCaptureEnabled();
// Probe only frames already accepted into a burst. It never changes displayed
// settings and does not allocate or render anything on normal game launches.
bool temporalCaptureNeedsCloudProbe(unsigned long long frameId);
void temporalCaptureRecordMotionState(unsigned long long frameId, const char* json);
void temporalCaptureRecordCloudProbe(ID3D12GraphicsCommandList*, unsigned long long frameId,
    ID3D12Resource* cloudlessInput, ID3D12Resource* nativeDepth,
    ID3D12Resource* cloudField, float projectionP10, float projectionP14);
void temporalCaptureRecordInputs(ID3D12GraphicsCommandList*, ID3D12Device*,
    unsigned long long fenceValue, const FGEffectGuideCamera&, float focusDistance,
    float jitterX, float jitterY, float deltaMilliseconds, bool reset,
    int antiAliasingMode, int frameGenerationMode,
    ID3D12Resource* input, ID3D12Resource* depth, ID3D12Resource* motion);
void temporalCaptureRecordResolved(ID3D12GraphicsCommandList*, unsigned long long frameId,
    ID3D12Resource* resolved);
// Opt-in HW_FG_ZOOM_CAPTURE=1 only. Twelve samples after a user zoom command;
// no GPU wait, no changes to the captured resources, no work when disabled.
void fgZoomCaptureNotifyZoom();
// Called after the SDK generation dispatch, on its synchronized game-queue
// command list. Pairs actual generated/presented color with the input frame ID.
void fgZoomCaptureRecordGenerated(ID3D12GraphicsCommandList*, unsigned long long frameId,
    ID3D12Resource* presentedColor, ID3D12Resource* generatedColor,
    bool generationReset, unsigned generatedFrameCount);
void fgZoomCaptureRecord(ID3D12GraphicsCommandList*, ID3D12Device*,
    unsigned long long fenceValue, const FGEffectGuideCamera&, float focusDistance,
    float jitterX, float jitterY, float deltaMilliseconds, bool reset,
    ID3D12Resource* hudless, ID3D12Resource* mergedDepth, ID3D12Resource* mergedMotion,
    ID3D12Resource* opaqueDepth, ID3D12Resource* opaqueMotion,
    ID3D12Resource* nativeEffectDepth = nullptr, ID3D12Resource* cloudDistanceOpacity = nullptr);
// Only completed copies may be mapped. Normal frame/resize/shutdown fence
// ownership remains with the presenter. pending readbacks retain their memory.
void fgZoomCapturePoll(ID3D12Fence*);
void fgZoomCaptureFinish(ID3D12Fence*);
#else
inline bool temporalCaptureEnabled() {return false;}
inline bool temporalCaptureNeedsCloudProbe(unsigned long long frameId) {return false;}
inline void temporalCaptureRecordMotionState(unsigned long long frameId, const char* json) {}
inline void temporalCaptureRecordCloudProbe(ID3D12GraphicsCommandList*, unsigned long long frameId,
    ID3D12Resource* cloudlessInput, ID3D12Resource* nativeDepth,
    ID3D12Resource* cloudField, float projectionP10, float projectionP14) {}
inline void temporalCaptureRecordInputs(ID3D12GraphicsCommandList*, ID3D12Device*,
    unsigned long long fenceValue, const FGEffectGuideCamera&, float focusDistance,
    float jitterX, float jitterY, float deltaMilliseconds, bool reset,
    int antiAliasingMode, int frameGenerationMode,
    ID3D12Resource* input, ID3D12Resource* depth, ID3D12Resource* motion) {}
inline void temporalCaptureRecordResolved(ID3D12GraphicsCommandList*, unsigned long long frameId,
    ID3D12Resource* resolved) {}
inline void fgZoomCaptureNotifyZoom() {}
inline void fgZoomCaptureRecordGenerated(ID3D12GraphicsCommandList*, unsigned long long frameId,
    ID3D12Resource* presentedColor, ID3D12Resource* generatedColor,
    bool generationReset, unsigned generatedFrameCount) {}
inline void fgZoomCaptureRecord(ID3D12GraphicsCommandList*, ID3D12Device*,
    unsigned long long fenceValue, const FGEffectGuideCamera&, float focusDistance,
    float jitterX, float jitterY, float deltaMilliseconds, bool reset,
    ID3D12Resource* hudless, ID3D12Resource* mergedDepth, ID3D12Resource* mergedMotion,
    ID3D12Resource* opaqueDepth, ID3D12Resource* opaqueMotion,
    ID3D12Resource* nativeEffectDepth = nullptr, ID3D12Resource* cloudDistanceOpacity = nullptr) {}
inline void fgZoomCapturePoll(ID3D12Fence*) {}
inline void fgZoomCaptureFinish(ID3D12Fence*) {}
#endif
}
