#include "DiagnosticPolicy.h"
#include "ModernFrameGeneration.h"
#include "FGZoomCapture.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <ffx_api.hpp>
#include <dx12/ffx_api_dx12.hpp>
#include <ffx_framegeneration.h>
#include <dx12/ffx_api_framegeneration_dx12.h>

namespace hwmodern
{
namespace
{
ffx::Context swapchainContext = nullptr;
ffx::Context frameContext = nullptr;
IDXGISwapChain4 *proxy = nullptr; // Presenter owns the public reference.
DXGI_FORMAT backBufferFormat = DXGI_FORMAT_UNKNOWN;
DXGI_FORMAT configuredHudlessFormat = DXGI_FORMAT_UNKNOWN;
unsigned int configuredWidth = 0, configuredHeight = 0;
bool runtimeAvailable = false, requested = false, active = false;
bool prepared = false, configuredEnabled = false, failed = false, needsReset = true;
bool disableFailed = false;
FsrFrameGenerationStats counters;

void failure(const char *stage, ffx::ReturnCode result)
{
    ++counters.dispatchFailures;
    counters.lastError = static_cast<unsigned int>(result);
    counters.lastFailureStage = stage;
    if (!failed)
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] AMD FSR frame generation failed at %s (result %u); "
            "frame generation disabled; native presentation retained.\n",
            stage, counters.lastError);
    failed = true;
    prepared = active = false;
}

bool disableConfiguration(bool finalDrain = false)
{
    prepared = active = false;
    needsReset = true;
    if (!frameContext || !configuredEnabled) return true;
    if (disableFailed && !finalDrain) return false;
    ffxConfigureDescFrameGeneration config{};
    config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
    config.swapChain = proxy;
    // Removing callbacks and disabling generation flushes SDK-owned work
    // before the application changes inputs or destroys the FG context.
    config.frameGenerationEnabled = false;
    const auto result = ffx::Configure(frameContext, config);
    if (result != ffx::ReturnCode::Ok)
    {
        disableFailed = true;
        failure("disable configuration", result);
        return false;
    }
    configuredEnabled = false;
    disableFailed = false;
    return true;
}

bool destroyFrameContext(bool finalDrain = false)
{
    if (!disableConfiguration(finalDrain)) return false;
    if (frameContext)
    {
        const auto result = ffx::DestroyContext(frameContext);
        if (result != ffx::ReturnCode::Ok)
        {
            failure("destroy context", result);
            return false;
        }
    }
    frameContext = nullptr;
    configuredWidth = configuredHeight = 0;
    configuredHudlessFormat = DXGI_FORMAT_UNKNOWN;
    return true;
}

ffxReturnCode_t generateFrame(ffxDispatchDescFrameGeneration *params, void *context)
{
    ++counters.generationCallbacks;
    if (!params || !context || !frameContext || !prepared || failed)
        return FFX_API_RETURN_ERROR_PARAMETER;
    // Present supplies the correctly synchronized command list, color,
    // output and sRGB transfer function. This callback runs on its caller's
    // render thread; do not recursively configure FG inside it.
    const auto result = ffxDispatch(static_cast<ffxContext *>(context), &params->header);
    if (result != FFX_API_RETURN_OK)
    {
        failure("generation dispatch", static_cast<ffx::ReturnCode>(result));
        return result;
    }
    // With allowAsyncWorkloads=false the SDK executes this list on gameQueue
    // inside Present before it returns. The presenter's subsequent queue fence
    // therefore covers these opt-in readbacks as well as the scene inputs.
    if(params->numGeneratedFrames>0)
        fgZoomCaptureRecordGenerated(static_cast<ID3D12GraphicsCommandList*>(params->commandList),
            params->frameID,static_cast<ID3D12Resource*>(params->presentColor.resource),
            static_cast<ID3D12Resource*>(params->outputs[0].resource),params->reset,params->numGeneratedFrames);
    ++counters.generationDispatches;
    counters.generatedFrameRequests += params->numGeneratedFrames;
    active = true;
    if (counters.generationDispatches == 1)
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] AMD FSR frame generation dispatching through "
            "the presentation swapchain (frame %llu, %u generated-frame request).\n",
            static_cast<unsigned long long>(params->frameID), params->numGeneratedFrames);
    return FFX_API_RETURN_OK;
}

void runtimeMessage(uint32_t type, const wchar_t *message)
{
    if (message)
        HW_DIAG_FPRINTF(stderr, "[ModernGraphics] AMD FSR FG runtime %s: %ls\n",
            type == FFX_API_MESSAGE_TYPE_ERROR ? "error" : "warning", message);
}

void configureDiagnostics()
{
#if HW_ENABLE_DIAGNOSTICS

    ffx::ConfigureDescGlobalDebug debug{};
    debug.effectId = FFX_API_EFFECT_ID_FRAMEGENERATION;
    debug.fpMessage = runtimeMessage;
    debug.debugLevel = FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_ERRORS |
                       FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_WARNINGS;
    const auto result = ffx::Configure(frameContext, debug);
    if (result != ffx::ReturnCode::Ok)
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] AMD FSR FG runtime diagnostic callback unavailable (result %u).\n",
            static_cast<unsigned int>(result));

#else

#endif
}
}

bool fsrFrameGenerationCreateSwapchain(IDXGIFactory *factory,
                                      ID3D12CommandQueue *queue, HWND window,
                                      const DXGI_SWAP_CHAIN_DESC1 *description,
                                      IDXGISwapChain4 **swapchain)
{
    runtimeAvailable = GetModuleHandleW(L"amd_fidelityfx_framegeneration_dx12.dll") != nullptr ||
        LoadLibraryW(L"amd_fidelityfx_framegeneration_dx12.dll") != nullptr;
    if (!runtimeAvailable || !factory || !queue || !window || !description ||
        !swapchain || *swapchain || swapchainContext)
    {
        failure("swapchain runtime/arguments", ffx::ReturnCode::ErrorParameter);
        return false;
    }
    auto mutableDescription = *description;
    ffxCreateContextDescFrameGenerationSwapChainForHwndDX12 create{};
    create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12;
    create.swapchain = swapchain;
    create.hwnd = window;
    create.desc = &mutableDescription;
    create.dxgiFactory = factory;
    create.gameQueue = queue;
    ffxCreateContextDescFrameGenerationSwapChainVersionDX12 version{};
    version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_DX12;
    version.version = FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION;
    const auto result = ffx::CreateContext(swapchainContext, nullptr, create, version);
    runtimeAvailable = result == ffx::ReturnCode::Ok;
    HW_DIAG_FPRINTF(stderr, "[ModernGraphics] AMD FSR frame-generation swapchain %s (result %u).\n",
        runtimeAvailable ? "ready" : "unavailable", static_cast<unsigned int>(result));
    if (!runtimeAvailable)
    {
        failure("create presentation swapchain", result);
        // ForHwnd starts without any original swapchain, so any returned
        // partial proxy is owned by this attempt and can be released safely.
        if (swapchainContext) ffx::DestroyContext(swapchainContext);
        swapchainContext = nullptr;
        if (*swapchain) { (*swapchain)->Release(); *swapchain = nullptr; }
        return false;
    }
    proxy = *swapchain;
    backBufferFormat = description->Format;
    disableFailed = false;
    failed = false;
    counters.lastError = 0;
    counters.lastFailureStage = "none";
    return true;
}

void fsrFrameGenerationRelease(void)
{
    // Caller keeps the proxy and all input resources alive until this returns.
    if (!destroyFrameContext(true))
    {
        ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12 wait{};
        wait.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12;
        if (swapchainContext) ffx::Dispatch(swapchainContext, wait);
        if (frameContext) ffx::DestroyContext(frameContext);
        frameContext = nullptr;
    }
    if (swapchainContext) ffx::DestroyContext(swapchainContext);
    swapchainContext = nullptr;
    proxy = nullptr;
    backBufferFormat = configuredHudlessFormat = DXGI_FORMAT_UNKNOWN;
    configuredWidth = configuredHeight = 0;
    configuredEnabled = runtimeAvailable = prepared = active = false;
    disableFailed = false;
    needsReset = true;
}

void fsrFrameGenerationSetEnabled(bool enabled)
{
    const bool changed = enabled != requested;
    // Consume the setting edge even if its one disable retry fails. Repeated
    // per-frame requests must not become repeated SDK calls.
    requested = enabled;
    if (changed && disableFailed)
    {
        disableFailed = false;
        if (!disableConfiguration()) return;
    }
    // A setting toggle can retry effect initialization on a healthy proxy. It
    // cannot recreate a presentation swapchain that failed during startup.
    if (enabled && changed && runtimeAvailable && swapchainContext)
    {
        failed = false;
        counters.lastError = 0;
        counters.lastFailureStage = "none";
        needsReset = true;
    }
    if (!enabled || failed) disableConfiguration();
}

void fsrFrameGenerationSuspend(void) { disableConfiguration(); }

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
                               const FsrFrameGenerationCamera *camera)
{
    prepared = active = false;
    if (!requested) return false;
    if (failed) { disableConfiguration(); return false; }
    if (!runtimeAvailable || !swapchainContext || !commands || !swapchain ||
        swapchain != proxy || !hudlessColor || !depth || !motion ||
        !renderWidth || !renderHeight || !displayWidth || !displayHeight)
    {
        failure("prepare inputs/runtime", ffx::ReturnCode::ErrorParameter);
        disableConfiguration();
        return false;
    }
    const auto color = hudlessColor->GetDesc();
    const auto depthDesc = depth->GetDesc();
    const auto motionDesc = motion->GetDesc();
    if (color.Width != displayWidth || color.Height != displayHeight ||
        depthDesc.Width < renderWidth || depthDesc.Height < renderHeight ||
        motionDesc.Width < renderWidth || motionDesc.Height < renderHeight)
    {
        failure("prepare resource dimensions", ffx::ReturnCode::ErrorParameter);
        disableConfiguration();
        return false;
    }
    if (!frameContext || configuredWidth != displayWidth || configuredHeight != displayHeight ||
        configuredHudlessFormat != color.Format)
    {
        if (!destroyFrameContext()) return false;
        ffxCreateContextDescFrameGeneration create{};
        create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
        // Display-referred SDR and jitter-free motion: neither HDR nor
        // MOTION_VECTORS_JITTER_CANCELLATION describes these inputs.
        create.flags = 0;
#if HW_ENABLE_DIAGNOSTICS
        wchar_t diagnostics[2] = {};
        if (GetEnvironmentVariableW(L"HW_FSR_FG_DEBUG", diagnostics, 2) && diagnostics[0] != L'0')
            create.flags |= FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING;
#endif
        create.displaySize = {displayWidth, displayHeight};
        create.maxRenderSize = {displayWidth, displayHeight};
        create.backBufferFormat = ffxApiGetSurfaceFormatDX12(backBufferFormat);
        ffxCreateContextDescFrameGenerationVersion version{};
        version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION;
        version.version = FFX_FRAMEGENERATION_VERSION;
        ffxCreateContextDescFrameGenerationHudless hudless{};
        hudless.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_HUDLESS;
        hudless.hudlessBackBufferFormat = ffxApiGetSurfaceFormatDX12(color.Format);
        ffx::CreateBackendDX12Desc backend{};
        ID3D12Device *device = nullptr;
        if (FAILED(commands->GetDevice(IID_PPV_ARGS(&device))))
        {
            failure("prepare device", ffx::ReturnCode::ErrorRuntimeError);
            return false;
        }
        backend.device = device;
        const auto result = ffx::CreateContext(frameContext, nullptr, create, backend, version, hudless);
        device->Release();
        if (result != ffx::ReturnCode::Ok)
        {
            failure("create frame-generation context", result);
            return false;
        }
        configuredWidth = displayWidth;
        configuredHeight = displayHeight;
        configuredHudlessFormat = color.Format;
        configureDiagnostics();
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] AMD FSR FG context ready: display %ux%u, render %ux%u, "
            "SDR backbuffer format %u, HUD-less format %u, jitter-free pixel motion.\n",
            displayWidth, displayHeight, renderWidth, renderHeight,
            static_cast<unsigned int>(backBufferFormat), static_cast<unsigned int>(color.Format));
    }
    ffxConfigureDescFrameGeneration config{};
    config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
    config.swapChain = swapchain;
    config.frameGenerationEnabled = true;
    config.frameGenerationCallback = generateFrame;
    config.frameGenerationCallbackUserContext = &frameContext;
    config.allowAsyncWorkloads = false;
    config.HUDLessColor = ffxApiGetResourceDX12(hudlessColor, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    config.generationRect = {0, 0, static_cast<int32_t>(displayWidth),
                            static_cast<int32_t>(displayHeight)};
    config.frameID = frameId;
    // A provider can update internal/partial swapchain state before reporting
    // an error, so conservatively disable/flush even a failed first Configure.
    configuredEnabled = true;
    const auto configured = ffx::Configure(frameContext, config);
    if (configured != ffx::ReturnCode::Ok)
    {
        failure("configure frame generation", configured);
        disableConfiguration();
        return false;
    }
    ++counters.configuredFrames;

    ffxDispatchDescFrameGenerationPrepareV2 prepare{};
    prepare.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2;
    prepare.commandList = commands;
    prepare.frameID = frameId;
    prepare.renderSize = {renderWidth, renderHeight};
    prepare.jitterOffset = {-jitterX, -jitterY};
    prepare.motionVectorScale = {1.0f, 1.0f};
    prepare.frameTimeDelta = std::isfinite(deltaMilliseconds) && deltaMilliseconds > 0.0f
        ? deltaMilliseconds : 16.6667f;
    prepare.reset = reset || needsReset;
    const FsrFrameGenerationCamera defaultCamera{};
    const auto &view = camera ? *camera : defaultCamera;
    prepare.cameraNear = view.nearPlane;
    prepare.cameraFar = view.farPlane;
    prepare.cameraFovAngleVertical = fovRadians;
    prepare.viewSpaceToMetersFactor = 1.0f;
    prepare.depth = ffxApiGetResourceDX12(depth, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    prepare.motionVectors = ffxApiGetResourceDX12(motion, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    std::memcpy(prepare.cameraPosition, view.position, sizeof(prepare.cameraPosition));
    std::memcpy(prepare.cameraUp, view.up, sizeof(prepare.cameraUp));
    std::memcpy(prepare.cameraRight, view.right, sizeof(prepare.cameraRight));
    std::memcpy(prepare.cameraForward, view.forward, sizeof(prepare.cameraForward));
    const auto dispatched = ffx::Dispatch(frameContext, prepare);
    if (dispatched != ffx::ReturnCode::Ok)
    {
        failure("prepare dispatch", dispatched);
        disableConfiguration();
        return false;
    }
    prepared = true;
    needsReset = false;
    ++counters.preparedFrames;
    return true;
}

bool fsrFrameGenerationAvailable(void) { return runtimeAvailable && swapchainContext; }
bool fsrFrameGenerationActive(void) { return active; }
bool fsrFrameGenerationFailed(void) { return failed; }
bool fsrFrameGenerationDisableFailed(void) { return disableFailed; }
const char *fsrFrameGenerationReason(void)
{
    if (failed) return counters.lastFailureStage;
    if (!fsrFrameGenerationAvailable()) return "frame-generation swapchain unavailable";
    if (!requested) return "off";
    if (!prepared) return "waiting for valid scene inputs";
    return active ? "generating frames" : "waiting for generation Present";
}
FsrFrameGenerationStats fsrFrameGenerationStats(void)
{
    auto result = counters;
    result.wrapped = fsrFrameGenerationAvailable();
    result.requested = requested;
    result.prepared = prepared;
    result.active = active;
    result.failed = failed;
    result.disableFailed = disableFailed;
    return result;
}
}
