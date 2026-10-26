#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

namespace hwmodern
{
struct FsrFrameGenerationCamera
{
    float position[3] = {};
    float up[3] = {0.0f, 1.0f, 0.0f};
    float right[3] = {1.0f, 0.0f, 0.0f};
    float forward[3] = {0.0f, 0.0f, -1.0f};
    float nearPlane = 1.0f;
    float farPlane = 1000000.0f;
};

struct FsrFrameGenerationStats
{
    bool wrapped = false;
    bool requested = false;
    bool prepared = false;
    bool active = false;
    bool failed = false;
    bool disableFailed = false;
    unsigned long long configuredFrames = 0;
    unsigned long long preparedFrames = 0;
    unsigned long long generationCallbacks = 0;
    unsigned long long generationDispatches = 0;
    // Successful callback requests; native present counts prove delivery.
    unsigned long long generatedFrameRequests = 0;
    unsigned long long dispatchFailures = 0;
    unsigned int lastError = 0;
    const char *lastFailureStage = "none";
};

// Render-thread API, including Present on the returned proxy. Disable/Release
// before destroying input resources or releasing the proxy swapchain.
// Create the proxy directly; wrapping an already multiply-owned DXGI chain
// cannot safely satisfy the AMD replacement API's consuming ownership rule.
bool fsrFrameGenerationCreateSwapchain(IDXGIFactory *factory,
                                      ID3D12CommandQueue *queue, HWND window,
                                      const DXGI_SWAP_CHAIN_DESC1 *description,
                                      IDXGISwapChain4 **swapchain);
void fsrFrameGenerationRelease(void);
void fsrFrameGenerationSetEnabled(bool enabled);
// Stop interpolation for menus/invalid inputs without changing the request.
void fsrFrameGenerationSuspend(void);
// Prepare consumes display-referred SDR color at display resolution, unjittered
// pixel motion, and normal [0,1] depth. HUD-less color and motion are in combined
// PIXEL|NON_PIXEL_SHADER_RESOURCE; depth is NON_PIXEL_SHADER_RESOURCE.
bool fsrFrameGenerationPrepare(ID3D12GraphicsCommandList *commands,
                               IDXGISwapChain4 *swapchain,
                               ID3D12Resource *hudlessColor,
                               ID3D12Resource *depth,
                               ID3D12Resource *motion,
                               unsigned int renderWidth,
                               unsigned int renderHeight,
                               unsigned int displayWidth,
                               unsigned int displayHeight,
                               float jitterX, float jitterY,
                               float fovRadians, float deltaMilliseconds,
                               bool reset, unsigned long long frameId,
                               const FsrFrameGenerationCamera *camera = nullptr);
bool fsrFrameGenerationAvailable(void);
bool fsrFrameGenerationActive(void);
bool fsrFrameGenerationFailed(void);
// An unsuccessful SDK Off leaves provider ownership uncertain. Do not enable
// another interpolation provider until an explicit retry or final drain.
bool fsrFrameGenerationDisableFailed(void);
const char *fsrFrameGenerationReason(void);
FsrFrameGenerationStats fsrFrameGenerationStats(void);
}
