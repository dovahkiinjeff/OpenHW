#include "DiagnosticPolicy.h"
#include "ModernDlaa.h"

#if defined(_WIN32)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>

#include <windows.h>
#include <wrl/client.h>

#include <sl.h>
#include <sl_dlss.h>
#include <sl_dlss_d.h>
#include <sl_dlss_g.h>
#include <sl_pcl.h>
#include <sl_reflex.h>

using Microsoft::WRL::ComPtr;

namespace hwmodern
{
namespace
{
constexpr DXGI_FORMAT dlaaOutputFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr float cameraNearPlane = 0.05f;
constexpr float cameraFarPlane = 500000.0f;

struct DlaaState
{
    bool initialized = false;
    int mode = 1;
    bool deviceReady = false;
    bool supported = false;
    bool outputReadable = false;
    bool loggedEvaluateFailure = false;
    bool evaluateFailed = false;
    bool dlssResourcesUsed = false;
    bool rrPluginRequested = false;
    bool rrRequested = true;
    bool rrSupported = false;
    bool loggedRrEvaluateFailure = false;
    bool rrEvaluateFailed = false;
    bool rrResourcesUsed = false;
    bool loggedRrActive = false;
    bool frameGenerationPluginRequested = false;
    bool frameGenerationSupported = false;
    bool frameGenerationRequested = false;
    bool frameGenerationActive = false;
    bool frameGenerationConfigured = false;
    bool frameGenerationOptionsKnown = false;
    bool frameGenerationFailed = false;
    bool frameGenerationDisableFailed = false;
    bool reflexConfigured = false;
    bool frameOpen = false;
    bool frameGenerationPrepared = false;
    bool renderSubmitMarked = false;
    bool presentMarked = false;
    sl::FrameToken *frameToken = nullptr;
    unsigned int presentationFrameIndex = 0;
    unsigned int fgInputWidth = 0, fgInputHeight = 0;
    unsigned int fgOutputWidth = 0, fgOutputHeight = 0;
    unsigned long long fgPresentedFrames = 0, fgApplicationFrames = 0;
    unsigned long long fgWindowPresentedFrames = 0, fgWindowApplicationFrames = 0;
    char frameGenerationReason[192] = "NVIDIA frame generation is off";
    bool loggedFrameGenerationFailure = false;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int frameIndex = 0;
    bool forceHistoryReset = true;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> output;
};

DlaaState state;

bool evaluationFailed(sl::Result result, bool rayReconstruction,
                      const char *stage)
{
    bool &failed = rayReconstruction ? state.rrEvaluateFailed : state.evaluateFailed;
    bool &logged = rayReconstruction ? state.loggedRrEvaluateFailure : state.loggedEvaluateFailure;
    failed = true;
    state.outputReadable = false;
    // FG has its own full-resolution color/depth/motion preparation and failure
    // latch. A failed SR/RR evaluation must not invalidate those independent
    // inputs or force another reconstruction allocation attempt.
    if (!logged)
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] NVIDIA %s failed at %s (result %d); %s "
            "Retry waits for an explicit output/configuration reset.\n",
            rayReconstruction ? "DLSS Ray Reconstruction" : "DLSS/DLAA full-scene evaluation",
            stage, static_cast<int>(result),
            rayReconstruction ? "Falling back to normal DLSS SR." : "Native scene presentation retained.");
        logged = true;
    }
    // In particular, eWarnOutOfVRAM must not re-enter tagging/evaluation on
    // every frame. Volatile tags can otherwise allocate another clone before
    // reporting the same warning. Existing in-flight resources stay alive;
    // release them only at the drained output lifecycle boundary below.
    return false;
}

void streamlineLog(sl::LogType type, const char *message)
{
    if (message)
    {
        HW_DIAG_FPRINTF(stderr, "[Streamline] %s", message != nullptr ? message : "");
    }
}

void setIdentity(sl::float4x4 &matrix)
{
    matrix.row[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
    matrix.row[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
    matrix.row[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
    matrix.row[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

void setPerspective(sl::float4x4 &projection, sl::float4x4 &inverse,
                    float fovRadians, float aspect)
{
    const float y = 1.0f / std::tan(fovRadians * 0.5f);
    const float x = y / std::max(aspect, 0.01f);
    const float c = cameraFarPlane / (cameraFarPlane - cameraNearPlane);
    const float d = -cameraNearPlane * cameraFarPlane /
                    (cameraFarPlane - cameraNearPlane);
    projection.row[0] = { x, 0.0f, 0.0f, 0.0f };
    projection.row[1] = { 0.0f, y, 0.0f, 0.0f };
    projection.row[2] = { 0.0f, 0.0f, c, 1.0f };
    projection.row[3] = { 0.0f, 0.0f, d, 0.0f };

    inverse.row[0] = { 1.0f / x, 0.0f, 0.0f, 0.0f };
    inverse.row[1] = { 0.0f, 1.0f / y, 0.0f, 0.0f };
    inverse.row[2] = { 0.0f, 0.0f, 0.0f, 1.0f / d };
    inverse.row[3] = { 0.0f, 0.0f, 1.0f, -c / d };
}

sl::Result frameTokenForEvaluation(sl::FrameToken *&token)
{
    if (state.frameOpen && state.frameToken)
    {
        token = state.frameToken;
        return sl::Result::eOk;
    }
    const unsigned int index = state.frameOpen ? state.presentationFrameIndex : state.frameIndex++;
    const sl::Result result = slGetNewFrameToken(token, &index);
    if (result == sl::Result::eOk && token && state.frameOpen) state.frameToken = token;
    return result;
}

bool frameGenerationOff()
{
    if (!state.frameGenerationConfigured && state.frameGenerationOptionsKnown) return true;
    if (!state.initialized || !state.deviceReady || !state.frameGenerationSupported) return true;
    if (state.frameGenerationDisableFailed) return false;
    sl::DLSSGOptions options = {};
    options.mode = sl::DLSSGMode::eOff;
    options.numFramesToGenerate = 1;
    options.numBackBuffers = 3;
    options.enableUserInterfaceRecomposition = sl::Boolean::eFalse;
    const sl::Result result = slDLSSGSetOptions(sl::ViewportHandle(0u), options);
    state.frameGenerationActive = false;
    if (result != sl::Result::eOk)
    {
        state.frameGenerationConfigured = true; // SDK state is uncertain until Off succeeds.
        state.frameGenerationFailed = state.frameGenerationDisableFailed = true;
        std::snprintf(state.frameGenerationReason,sizeof(state.frameGenerationReason),
            "NVIDIA FG could not be disabled (result %d); renderer restart required",static_cast<int>(result));
        HW_DIAG_FPRINTF(stderr,"[ModernGraphics] %s.\n",state.frameGenerationReason);
        return false;
    }
    state.frameGenerationConfigured = false;
    state.frameGenerationOptionsKnown = true;
    return true;
}

bool frameGenerationFailure(const char *stage, sl::Result result, unsigned int status = 0)
{
    state.frameGenerationFailed = true;
    state.frameGenerationActive = false;
    std::snprintf(state.frameGenerationReason, sizeof(state.frameGenerationReason),
        "NVIDIA FG failed at %s (result %d, status 0x%x); change FG mode to retry",
        stage, static_cast<int>(result), status);
    if (!state.loggedFrameGenerationFailure)
    {
        HW_DIAG_FPRINTF(stderr,"[ModernGraphics] %s.\n",state.frameGenerationReason);
        state.loggedFrameGenerationFailure = true;
    }
    // Do not free tagged resources here: the present queue may still use them.
    frameGenerationOff();
    return false;
}

void transition(ID3D12GraphicsCommandList *commands,
                ID3D12Resource *resource,
                D3D12_RESOURCE_STATES before,
                D3D12_RESOURCE_STATES after)
{
    if (resource == nullptr || before == after)
    {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    commands->ResourceBarrier(1, &barrier);
}

bool runtimeFileBesideExecutable(const wchar_t *fileName)
{
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return false;
    }
    wchar_t *slash = std::wcsrchr(path, L'\\');
    if (slash == nullptr)
    {
        return false;
    }
    slash[1] = L'\0';
    const size_t directoryLength = std::wcslen(path);
    const size_t fileLength = std::wcslen(fileName);
    if (directoryLength + fileLength + 1 >= MAX_PATH)
    {
        return false;
    }
    std::wmemcpy(path + directoryLength, fileName, fileLength + 1);
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

sl::DLSSMode dlssModeFromSetting(int mode)
{
    switch (mode)
    {
        case 3: return sl::DLSSMode::eMaxQuality;
        case 4: return sl::DLSSMode::eBalanced;
        case 5: return sl::DLSSMode::eMaxPerformance;
        case 6: return sl::DLSSMode::eUltraPerformance;
        default: return sl::DLSSMode::eDLAA;
    }
}
}

void dlaaEarlyInitialize(void)
{
    if (state.initialized)
    {
        return;
    }
    state.rrPluginRequested =
        runtimeFileBesideExecutable(L"sl.dlss_d.dll") &&
        runtimeFileBesideExecutable(L"nvngx_dlssd.dll");
    state.frameGenerationPluginRequested =
        runtimeFileBesideExecutable(L"sl.dlss_g.dll") &&
        runtimeFileBesideExecutable(L"nvngx_dlssg.dll") &&
        runtimeFileBesideExecutable(L"sl.reflex.dll") &&
        runtimeFileBesideExecutable(L"sl.pcl.dll");
    static const sl::Feature dlssFeatures[] = {
        sl::kFeatureDLSS, sl::kFeatureDLSS_RR,
        sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeaturePCL
    };
    sl::Preferences preferences = {};
    preferences.showConsole = false;
    preferences.logLevel = HW_ENABLE_DIAGNOSTICS ? sl::LogLevel::eVerbose : sl::LogLevel::eOff;
    preferences.logMessageCallback = HW_ENABLE_DIAGNOSTICS ? streamlineLog : nullptr;
    preferences.flags = sl::PreferenceFlags::eDisableCLStateTracking |
                        sl::PreferenceFlags::eDisableDebugText |
                        sl::PreferenceFlags::eUseDXGIFactoryProxy |
                        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.featuresToLoad = dlssFeatures;
    preferences.numFeaturesToLoad = state.frameGenerationPluginRequested ? 5u :
        (state.rrPluginRequested ? 2u : 1u);
    preferences.engine = sl::EngineType::eCustom;
    preferences.engineVersion = "HomeworldModern-0.91.2";
    preferences.projectId = "1f070a4e-e397-49ee-86fc-e9f429326b11";
    preferences.renderAPI = sl::RenderAPI::eD3D12;
    const sl::Result result = slInit(preferences);
    state.initialized = result == sl::Result::eOk;
    if (!state.initialized)
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] NVIDIA Streamline initialization "
                     "failed (result %d); DLSS/DLAA will remain off.\n",
                     static_cast<int>(result));
    }
    else if (!state.rrPluginRequested)
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] NVIDIA DLSS Ray Reconstruction runtime is not "
            "beside the executable; DLSS/DLAA remains available and RR will "
            "fall back until sl.dlss_d.dll + nvngx_dlssd.dll are installed.\n");
    }
}

void dlaaSetMode(int mode)
{
    const int clampedMode = std::max(0, std::min(mode, 6));
    if (state.mode != clampedMode)
    {
        state.forceHistoryReset = true;
        state.evaluateFailed = state.rrEvaluateFailed = false;
        state.loggedEvaluateFailure = state.loggedRrEvaluateFailure = false;
    }
    state.mode = clampedMode;
    if (state.mode == 0 || state.mode == 2)
    {
        state.outputReadable = false;
    }
    if (state.mode < 3)
    {
    }
}

void dlaaSetRayReconstructionEnabled(bool enabled)
{
    if (state.rrRequested != enabled)
    {
        state.forceHistoryReset = true;
        state.evaluateFailed = state.rrEvaluateFailed = false;
        state.loggedEvaluateFailure = state.loggedRrEvaluateFailure = false;
    }
    state.rrRequested = enabled;
    if (!enabled)
    {
    }
}

void dlaaSetFrameGenerationEnabled(bool enabled)
{
    const bool changed = state.frameGenerationRequested != enabled;
    state.frameGenerationRequested = enabled;
    if (changed)
    {
        // An explicit request change permits one further disable attempt.
        // Never enable another NVIDIA configuration on top of uncertain SDK
        // state left by a failed Off operation.
        if (state.frameGenerationDisableFailed)
        {
            state.frameGenerationDisableFailed = false;
            if (!frameGenerationOff()) return;
        }
        if (enabled)
        {
            // Auto disables a failed provider while selecting its fallback.
            // Preserve that failure on the false edge or Auto would select
            // NVIDIA again at the very next provider query.
            state.frameGenerationFailed = false;
            state.loggedFrameGenerationFailure = false;
            state.fgWindowPresentedFrames = state.fgWindowApplicationFrames = 0;
            state.frameGenerationActive = false;
        }
    }
    if (!enabled)
    {
        if (!frameGenerationOff()) return;
        if (state.reflexConfigured && state.initialized && state.deviceReady)
        {
            sl::ReflexOptions reflex = {};
            reflex.mode = sl::ReflexMode::eOff;
            slReflexSetOptions(reflex);
        }
        state.reflexConfigured = false;
        std::snprintf(state.frameGenerationReason, sizeof(state.frameGenerationReason),
            "NVIDIA frame generation is off");
        return;
    }
    if (!state.initialized || !state.deviceReady ||
        !state.frameGenerationSupported)
    {
        state.frameGenerationActive = false;
        std::snprintf(state.frameGenerationReason, sizeof(state.frameGenerationReason),
            "NVIDIA DLSS-G / Reflex is unavailable on this adapter or runtime");
        return;
    }
    if (state.frameGenerationFailed || state.reflexConfigured) return;
    sl::ReflexOptions reflex = {};
    reflex.mode = sl::ReflexMode::eLowLatency;
    const sl::Result result = slReflexSetOptions(reflex);
    if (result != sl::Result::eOk) { frameGenerationFailure("Reflex options",result); return; }
    state.reflexConfigured = true;
    std::snprintf(state.frameGenerationReason, sizeof(state.frameGenerationReason),
        "NVIDIA frame generation is waiting for a gameplay frame");
}

bool dlaaFrameGenerationAvailable(void)
{
    return state.frameGenerationSupported;
}

bool dlaaFrameGenerationActive(void)
{
    return state.frameGenerationActive;
}

bool dlaaFrameGenerationFailed(void) { return state.frameGenerationFailed; }
bool dlaaFrameGenerationDisableFailed(void) { return state.frameGenerationDisableFailed; }
const char *dlaaFrameGenerationReason(void) { return state.frameGenerationReason; }
unsigned long long dlaaFrameGenerationPresentedFrames(void) { return state.fgPresentedFrames; }
unsigned long long dlaaFrameGenerationApplicationFrames(void) { return state.fgApplicationFrames; }

void dlaaFrameGenerationBeginFrame(unsigned long long frameId)
{
    state.frameOpen = true;
    state.frameToken = nullptr;
    state.presentationFrameIndex = static_cast<unsigned int>(frameId);
    state.frameGenerationPrepared = state.renderSubmitMarked = state.presentMarked = false;
    if (!state.frameGenerationRequested || !state.frameGenerationSupported ||
        !state.initialized || !state.deviceReady || state.frameGenerationFailed) return;
    dlaaSetFrameGenerationEnabled(true);
    if (state.frameGenerationFailed) return;
    sl::FrameToken *token = nullptr;
    sl::Result result = frameTokenForEvaluation(token);
    if (result != sl::Result::eOk || !token)
    { frameGenerationFailure("frame token",result == sl::Result::eOk ? sl::Result::eErrorInvalidState : result); return; }
    result = slReflexSleep(*token);
    if (result == sl::Result::eOk)
        result = slPCLSetMarker(sl::PCLMarker::eRenderSubmitStart,*token);
    if (result != sl::Result::eOk) { frameGenerationFailure("Reflex frame start",result); return; }
    state.renderSubmitMarked = true;
}

bool dlaaFrameGenerationPrepare(ID3D12GraphicsCommandList *commands,
    ID3D12Resource *hudless, ID3D12Resource *depth, ID3D12Resource *motion,
    unsigned int inputWidth, unsigned int inputHeight,
    unsigned int outputWidth, unsigned int outputHeight,
    float verticalFieldOfViewDegrees, float jitterX, float jitterY,
    bool resetHistory, D3D12_RESOURCE_STATES hudlessState,
    D3D12_RESOURCE_STATES depthState, D3D12_RESOURCE_STATES motionState)
{
    if (!state.frameGenerationRequested || !state.frameGenerationSupported ||
        !state.initialized || !state.deviceReady || state.frameGenerationFailed) return false;
    if (!state.frameOpen || !commands || !hudless || !depth || !motion ||
        !inputWidth || !inputHeight || !outputWidth || !outputHeight)
    {
        state.frameGenerationActive = false;
        std::snprintf(state.frameGenerationReason,sizeof(state.frameGenerationReason),
            "NVIDIA frame generation is waiting for color, depth and motion");
        return false;
    }
    sl::FrameToken *token = nullptr;
    sl::Result result = frameTokenForEvaluation(token);
    if (result != sl::Result::eOk || !token)
        return frameGenerationFailure("frame token",result == sl::Result::eOk ? sl::Result::eErrorInvalidState : result);
    if (!state.frameGenerationConfigured || state.fgInputWidth != inputWidth ||
        state.fgInputHeight != inputHeight || state.fgOutputWidth != outputWidth ||
        state.fgOutputHeight != outputHeight)
    {
        sl::DLSSGOptions options = {};
        options.mode = sl::DLSSGMode::eOn;
        options.numFramesToGenerate = 1;
        options.numBackBuffers = 3;
        options.mvecDepthWidth = inputWidth; options.mvecDepthHeight = inputHeight;
        options.colorWidth = outputWidth; options.colorHeight = outputHeight;
        options.colorBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        options.hudLessBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        options.depthBufferFormat = DXGI_FORMAT_R32_FLOAT;
        options.mvecBufferFormat = DXGI_FORMAT_R16G16_FLOAT;
        options.enableUserInterfaceRecomposition = sl::Boolean::eFalse;
        result = slDLSSGSetOptions(sl::ViewportHandle(0u),options);
        // A failed On call may have partially changed the SDK. Own that
        // uncertain configuration so failure makes exactly one Off attempt.
        state.frameGenerationConfigured = true;
        state.frameGenerationOptionsKnown = true;
        if (result != sl::Result::eOk) return frameGenerationFailure("DLSS-G options",result);
        state.fgInputWidth = inputWidth; state.fgInputHeight = inputHeight;
        state.fgOutputWidth = outputWidth; state.fgOutputHeight = outputHeight;
        // Discard presentation counts from earlier off/menu frames. GetState
        // without options/VRAM-estimate flags does not create another output.
        sl::DLSSGState previous = {};
        result = slDLSSGGetState(sl::ViewportHandle(0u),previous,nullptr);
        if (result != sl::Result::eOk) return frameGenerationFailure("initial runtime state",result);
        state.fgWindowPresentedFrames = state.fgWindowApplicationFrames = 0;
        state.frameGenerationActive = false;
    }
    sl::Constants constants = {};
    constexpr float degreesToRadians = 0.01745329251994329577f;
    const float fov = std::max(verticalFieldOfViewDegrees,1.0f) * degreesToRadians;
    setPerspective(constants.cameraViewToClip,constants.clipToCameraView,fov,
        static_cast<float>(outputWidth)/outputHeight);
    setIdentity(constants.clipToLensClip); setIdentity(constants.clipToPrevClip); setIdentity(constants.prevClipToClip);
    constants.jitterOffset = {jitterX,jitterY};
    constants.mvecScale = {1.0f/inputWidth,1.0f/inputHeight};
    constants.cameraPinholeOffset = {0,0}; constants.cameraPos = {0,0,0};
    constants.cameraUp = {0,1,0}; constants.cameraRight = {1,0,0}; constants.cameraFwd = {0,0,1};
    constants.cameraNear = cameraNearPlane; constants.cameraFar = cameraFarPlane;
    constants.cameraFOV = fov; constants.cameraAspectRatio = static_cast<float>(outputWidth)/outputHeight;
    constants.depthInverted = sl::Boolean::eFalse; constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    constants.reset = resetHistory ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    constants.orthographicProjection = sl::Boolean::eFalse;
    constants.motionVectorsDilated = sl::Boolean::eFalse; constants.motionVectorsJittered = sl::Boolean::eFalse;
    result = slSetConstants(constants,*token,sl::ViewportHandle(0u));
    if (result != sl::Result::eOk) return frameGenerationFailure("FG constants",result);
    sl::Extent inputExtent = {0,0,inputWidth,inputHeight}, outputExtent = {0,0,outputWidth,outputHeight};
    sl::Resource color(sl::ResourceType::eTex2d,hudless,static_cast<unsigned int>(hudlessState));
    sl::Resource depthResource(sl::ResourceType::eTex2d,depth,static_cast<unsigned int>(depthState));
    sl::Resource motionResource(sl::ResourceType::eTex2d,motion,static_cast<unsigned int>(motionState));
    color.width = outputWidth; color.height = outputHeight; color.nativeFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    depthResource.width = motionResource.width = inputWidth;
    depthResource.height = motionResource.height = inputHeight;
    depthResource.nativeFormat = DXGI_FORMAT_R32_FLOAT; motionResource.nativeFormat = DXGI_FORMAT_R16G16_FLOAT;
    sl::ResourceTag tags[] = {
        {&depthResource,sl::kBufferTypeDepth,sl::ResourceLifecycle::eValidUntilPresent,&inputExtent},
        {&motionResource,sl::kBufferTypeMotionVectors,sl::ResourceLifecycle::eValidUntilPresent,&inputExtent},
        {&color,sl::kBufferTypeHUDLessColor,sl::ResourceLifecycle::eValidUntilPresent,&outputExtent},
    };
    result = slSetTagForFrame(*token,sl::ViewportHandle(0u),tags,static_cast<unsigned int>(_countof(tags)),
        reinterpret_cast<sl::CommandBuffer*>(commands));
    if (result != sl::Result::eOk) return frameGenerationFailure("FG resource tags",result);
    state.frameGenerationPrepared = true;
    return true;
}

void dlaaFrameGenerationPresentBegin(void)
{
    if (!state.frameGenerationPrepared || state.frameGenerationFailed)
    {
        frameGenerationOff();
        if (!state.frameGenerationFailed && state.frameGenerationRequested)
            std::snprintf(state.frameGenerationReason,sizeof(state.frameGenerationReason),
                "NVIDIA frame generation is waiting for a gameplay frame");
    }
    if (state.frameGenerationFailed || !state.frameToken || !state.renderSubmitMarked) return;
    sl::Result result = sl::Result::eOk;
    if (state.renderSubmitMarked)
        result = slPCLSetMarker(sl::PCLMarker::eRenderSubmitEnd,*state.frameToken);
    if (result == sl::Result::eOk)
        result = slPCLSetMarker(sl::PCLMarker::ePresentStart,*state.frameToken);
    if (result != sl::Result::eOk) { frameGenerationFailure("Reflex present start",result); return; }
    state.presentMarked = true;
}

void dlaaFrameGenerationPresentEnd(bool presentSucceeded)
{
    if (state.presentMarked && state.frameToken)
    {
        const sl::Result marker = slPCLSetMarker(sl::PCLMarker::ePresentEnd,*state.frameToken);
        if (marker != sl::Result::eOk) frameGenerationFailure("Reflex present end",marker);
    }
    if (presentSucceeded && state.frameGenerationPrepared && state.frameGenerationConfigured && !state.frameGenerationFailed)
    {
        sl::DLSSGState current = {};
        const sl::Result result = slDLSSGGetState(sl::ViewportHandle(0u),current,nullptr);
        if (result != sl::Result::eOk || current.status != sl::DLSSGStatus::eOk)
            frameGenerationFailure("runtime status",result,static_cast<unsigned int>(current.status));
        else
        {
            ++state.fgApplicationFrames;
            state.fgPresentedFrames += current.numFramesActuallyPresented;
            ++state.fgWindowApplicationFrames;
            state.fgWindowPresentedFrames += current.numFramesActuallyPresented;
            const bool observed = state.fgWindowPresentedFrames > state.fgWindowApplicationFrames;
            if (observed && !state.frameGenerationActive)
                HW_DIAG_FPRINTF(stderr,"[ModernGraphics] NVIDIA DLSS-G ACTIVE: runtime status OK; extra presentations observed.\n");
            state.frameGenerationActive = observed;
            std::snprintf(state.frameGenerationReason,sizeof(state.frameGenerationReason),
                observed ? "NVIDIA DLSS-G is generating additional displayed frames" :
                           "NVIDIA DLSS-G inputs accepted; waiting for generated presentations");
        }
    }
    state.frameOpen = false; state.frameToken = nullptr;
    state.frameGenerationPrepared = state.renderSubmitMarked = state.presentMarked = false;
}

ID3D12Resource *dlaaOutputResource(void) { return state.output.Get(); }

bool dlaaSetDevice(ID3D12Device *device, IDXGIAdapter1 *adapter)
{
    state.deviceReady = false;
    state.supported = false;
    state.rrSupported = false;
    state.frameGenerationSupported = false;
    state.device = device;
    if (!state.initialized || device == nullptr || adapter == nullptr)
    {
        return false;
    }
    const sl::Result deviceResult = slSetD3DDevice(device);
    if (deviceResult != sl::Result::eOk)
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] Streamline rejected the D3D12 device "
                     "(result %d).\n", static_cast<int>(deviceResult));
        return false;
    }
    state.deviceReady = true;

    DXGI_ADAPTER_DESC1 description = {};
    if (FAILED(adapter->GetDesc1(&description)))
    {
        return false;
    }
    sl::AdapterInfo adapterInfo = {};
    adapterInfo.deviceLUID = reinterpret_cast<unsigned char *>(
        &description.AdapterLuid);
    adapterInfo.deviceLUIDSizeInBytes = sizeof(description.AdapterLuid);
    const sl::Result support = slIsFeatureSupported(
        sl::kFeatureDLSS, adapterInfo);
    state.supported = support == sl::Result::eOk;
    if (state.supported)
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] NVIDIA DLSS Super Resolution and "
                     "DLAA are available.\n");
    }
    else
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] NVIDIA DLSS/DLAA unavailable on this adapter "
                     "(result %d).\n", static_cast<int>(support));
    }
    if (state.supported && state.rrPluginRequested)
    {
        const sl::Result rrSupport = slIsFeatureSupported(
            sl::kFeatureDLSS_RR, adapterInfo);
        state.rrSupported = rrSupport == sl::Result::eOk;
        if (state.rrSupported)
        {
            HW_DIAG_FPRINTF(stderr,
                "[ModernGraphics] NVIDIA DLSS Ray Reconstruction is available.\n");
        }
        else
        {
            HW_DIAG_FPRINTF(stderr,
                "[ModernGraphics] NVIDIA DLSS Ray Reconstruction unavailable "
                "on this adapter/runtime (result %d); DLSS SR fallback remains live.\n",
                static_cast<int>(rrSupport));
        }
    }
    if (state.frameGenerationPluginRequested)
    {
        const sl::Result fgSupport = slIsFeatureSupported(
            sl::kFeatureDLSS_G, adapterInfo);
        const sl::Result reflexSupport = slIsFeatureSupported(
            sl::kFeatureReflex, adapterInfo);
        state.frameGenerationSupported =
            fgSupport == sl::Result::eOk && reflexSupport == sl::Result::eOk;
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] NVIDIA DLSS-G/Reflex %s (FG result %d, Reflex result %d).\n",
            state.frameGenerationSupported ? "available" : "unavailable",
            static_cast<int>(fgSupport), static_cast<int>(reflexSupport));
        // Initialize Off once even when FG was never selected. The intercepted
        // swapchain may Present menus before any scene/FG preparation occurs.
        dlaaSetFrameGenerationEnabled(state.frameGenerationRequested);
    }
    return state.supported;
}

void dlaaReleaseDevice(void)
{
    // This boundary is drained by the presenter; permit one last cleanup
    // attempt if an earlier in-frame disable failed.
    state.frameGenerationDisableFailed = false;
    frameGenerationOff();
    dlaaReleaseOutput();
    state.device.Reset();
    state.deviceReady = false;
    state.supported = false;
    state.rrSupported = false;
    state.frameGenerationSupported = false;
    state.frameGenerationActive = false;
    state.frameGenerationConfigured = state.reflexConfigured = false;
    state.frameGenerationOptionsKnown = false;
    state.frameGenerationFailed = false;
    state.frameGenerationDisableFailed = false;
    state.frameOpen = false; state.frameToken = nullptr;
    state.frameGenerationPrepared = state.renderSubmitMarked = state.presentMarked = false;
    state.fgInputWidth = state.fgInputHeight = state.fgOutputWidth = state.fgOutputHeight = 0;
    state.fgPresentedFrames = state.fgApplicationFrames = 0;
    state.fgWindowPresentedFrames = state.fgWindowApplicationFrames = 0;
}

bool dlaaCreateOutput(ID3D12DescriptorHeap *shaderVisibleHeap,
                      unsigned int descriptorSize,
                      unsigned int outputSrvIndex,
                      unsigned int width, unsigned int height)
{
    dlaaReleaseOutput();
    if ((state.mode == 0 || state.mode == 2) || !state.supported ||
        !state.deviceReady ||
        shaderVisibleHeap == nullptr || descriptorSize == 0)
    {
        return false;
    }
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC texture = {};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = width;
    texture.Height = height;
    texture.DepthOrArraySize = 1;
    texture.MipLevels = 1;
    texture.Format = dlaaOutputFormat;
    texture.SampleDesc.Count = 1;
    texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const HRESULT result = state.device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &texture,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
        IID_PPV_ARGS(&state.output));
    if (FAILED(result))
    {
        HW_DIAG_FPRINTF(stderr,
                     "[ModernGraphics] DLAA output allocation failed "
                     "(HRESULT 0x%08lx).\n",
                     static_cast<unsigned long>(result));
        return false;
    }
    state.output->SetName(L"Homeworld NVIDIA DLSS/DLAA full-scene output");
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = dlaaOutputFormat;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        shaderVisibleHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(outputSrvIndex) * descriptorSize;
    state.device->CreateShaderResourceView(state.output.Get(), &srv, handle);
    state.width = width;
    state.height = height;
    state.outputReadable = false;
    state.loggedEvaluateFailure = false;
    state.loggedRrEvaluateFailure = false;
    state.evaluateFailed = state.rrEvaluateFailed = false;
    state.forceHistoryReset = true;
    // Output recreation is an explicit, drained configuration boundary. A
    // generation request paused by native fallback may now be retried once.
    if (state.frameGenerationRequested && !state.frameGenerationActive)
        dlaaSetFrameGenerationEnabled(true);
    return true;
}

void dlaaReleaseOutput(void)
{
    // Callers already drain submitted GPU work before replacing/releasing
    // output resources. slFreeResources is unsafe inside failed Evaluate:
    // its command list may still contain work referencing these allocations.
    if (state.initialized && state.deviceReady)
    {
        const sl::ViewportHandle viewport(0u);
        if (state.rrResourcesUsed) slFreeResources(sl::kFeatureDLSS_RR, viewport);
        if (state.dlssResourcesUsed) slFreeResources(sl::kFeatureDLSS, viewport);
    }
    state.rrResourcesUsed = state.dlssResourcesUsed = false;
    state.output.Reset();
    state.width = 0;
    state.height = 0;
    state.outputReadable = false;
    state.evaluateFailed = state.rrEvaluateFailed = false;
    state.loggedEvaluateFailure = state.loggedRrEvaluateFailure = false;
}

bool dlaaGetOptimalRenderSize(int mode,
                              unsigned int outputWidth,
                              unsigned int outputHeight,
                              unsigned int *renderWidth,
                              unsigned int *renderHeight)
{
    if (!state.supported || mode < 3 || mode > 6 || outputWidth == 0 ||
        outputHeight == 0 || renderWidth == nullptr || renderHeight == nullptr)
    {
        return false;
    }
    if (state.rrRequested && state.rrSupported)
    {
        sl::DLSSDOptions rrOptions = {};
        rrOptions.mode = dlssModeFromSetting(mode);
        rrOptions.outputWidth = outputWidth;
        rrOptions.outputHeight = outputHeight;
        rrOptions.colorBuffersHDR = sl::Boolean::eTrue;
        rrOptions.normalRoughnessMode = sl::DLSSDNormalRoughnessMode::ePacked;
        rrOptions.dlaaPreset = sl::DLSSDPreset::ePresetE;
        rrOptions.qualityPreset = sl::DLSSDPreset::ePresetE;
        rrOptions.balancedPreset = sl::DLSSDPreset::ePresetE;
        rrOptions.performancePreset = sl::DLSSDPreset::ePresetE;
        rrOptions.ultraPerformancePreset = sl::DLSSDPreset::ePresetE;
        sl::DLSSDOptimalSettings rrSettings = {};
        if (slDLSSDGetOptimalSettings(rrOptions, rrSettings) == sl::Result::eOk &&
            rrSettings.optimalRenderWidth != 0 &&
            rrSettings.optimalRenderHeight != 0)
        {
            *renderWidth = rrSettings.optimalRenderWidth;
            *renderHeight = rrSettings.optimalRenderHeight;
            return true;
        }
    }
    sl::DLSSOptions options = {};
    options.mode = dlssModeFromSetting(mode);
    options.outputWidth = outputWidth;
    options.outputHeight = outputHeight;
    options.qualityPreset = sl::DLSSPreset::ePresetK;
    options.balancedPreset = sl::DLSSPreset::ePresetK;
    options.performancePreset = sl::DLSSPreset::ePresetM;
    options.ultraPerformancePreset = sl::DLSSPreset::ePresetL;
    sl::DLSSOptimalSettings settings = {};
    if (slDLSSGetOptimalSettings(options, settings) != sl::Result::eOk ||
        settings.optimalRenderWidth == 0 || settings.optimalRenderHeight == 0)
    {
        return false;
    }
    *renderWidth = settings.optimalRenderWidth;
    *renderHeight = settings.optimalRenderHeight;
    return true;
}

bool dlaaEvaluate(ID3D12GraphicsCommandList *commands,
                  ID3D12Resource *inputColor,
                  unsigned int inputWidth,
                  unsigned int inputHeight,
                  DXGI_FORMAT inputFormat,
                  ID3D12Resource *depth,
                  ID3D12Resource *motion,
                  float verticalFieldOfViewDegrees,
                  float aspectRatio,
                  float jitterX,
                  float jitterY,
                  bool resetHistory)
{
    if (state.evaluateFailed || (state.mode == 0 || state.mode == 2) || !state.supported ||
        !state.output || commands == nullptr || inputColor == nullptr ||
        depth == nullptr || motion == nullptr || inputWidth == 0 ||
        inputHeight == 0)
    {
        state.outputReadable = false;
        return false;
    }
    if (state.outputReadable)
    {
        transition(commands, state.output.Get(),
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        state.outputReadable = false;
    }

    sl::FrameToken *frame = nullptr;
    sl::Result result = frameTokenForEvaluation(frame);
    if (result != sl::Result::eOk || frame == nullptr)
    {
        return evaluationFailed(result == sl::Result::eOk ? sl::Result::eErrorInvalidState : result,
                                false, "frame token");
    }
    const sl::ViewportHandle viewport(0u);
    constexpr float pi = 3.14159265358979323846f;
    const float fovRadians = std::max(verticalFieldOfViewDegrees, 1.0f) *
                             (pi / 180.0f);
    sl::Constants constants = {};
    setPerspective(constants.cameraViewToClip, constants.clipToCameraView,
                   fovRadians, aspectRatio);
    setIdentity(constants.clipToLensClip);
    setIdentity(constants.clipToPrevClip);
    setIdentity(constants.prevClipToClip);
    constants.jitterOffset = { jitterX, jitterY };
    constants.mvecScale = { 1.0f / static_cast<float>(inputWidth),
                            1.0f / static_cast<float>(inputHeight) };
    constants.cameraPinholeOffset = { 0.0f, 0.0f };
    constants.cameraPos = { 0.0f, 0.0f, 0.0f };
    constants.cameraUp = { 0.0f, 1.0f, 0.0f };
    constants.cameraRight = { 1.0f, 0.0f, 0.0f };
    constants.cameraFwd = { 0.0f, 0.0f, 1.0f };
    constants.cameraNear = cameraNearPlane;
    constants.cameraFar = cameraFarPlane;
    constants.cameraFOV = fovRadians;
    constants.cameraAspectRatio = aspectRatio;
    constants.depthInverted = sl::Boolean::eFalse;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    const bool resetTemporalHistory = resetHistory || state.forceHistoryReset;
    constants.reset = resetTemporalHistory ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    constants.orthographicProjection = sl::Boolean::eFalse;
    constants.motionVectorsDilated = sl::Boolean::eFalse;
    constants.motionVectorsJittered = sl::Boolean::eFalse;
    result = slSetConstants(constants, *frame, viewport);
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, false, "constants");
    }

    sl::DLSSOptions options = {};
    options.mode = dlssModeFromSetting(state.mode);
    options.outputWidth = state.width;
    options.outputHeight = state.height;
    /* The Streamline input is now the complete HUD-less presenter scene. It is
       stored in FP16 for precision, but it is already display-referred by the
       hybrid raster/DXR compositor; do not let DLSS invent exposure from a
       partial signal and do not treat alpha as a temporal content channel. */
    options.colorBuffersHDR = sl::Boolean::eFalse;
    options.useAutoExposure = sl::Boolean::eFalse;
    options.alphaUpscalingEnabled = sl::Boolean::eFalse;
    options.dlaaPreset = sl::DLSSPreset::ePresetK;
    options.qualityPreset = sl::DLSSPreset::ePresetK;
    options.balancedPreset = sl::DLSSPreset::ePresetK;
    options.performancePreset = sl::DLSSPreset::ePresetM;
    options.ultraPerformancePreset = sl::DLSSPreset::ePresetL;
    state.dlssResourcesUsed = true;
    result = slDLSSSetOptions(viewport, options);
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, false, "DLSS options");
    }

    sl::Extent inputExtent = { 0, 0, inputWidth, inputHeight };
    sl::Extent outputExtent = { 0, 0, state.width, state.height };
    sl::Resource colorIn(sl::ResourceType::eTex2d, inputColor,
                         static_cast<unsigned int>(
                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    sl::Resource colorOut(sl::ResourceType::eTex2d, state.output.Get(),
                          static_cast<unsigned int>(
                              D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    sl::Resource depthIn(sl::ResourceType::eTex2d, depth,
                         static_cast<unsigned int>(
                             D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    sl::Resource motionIn(sl::ResourceType::eTex2d, motion,
                          static_cast<unsigned int>(
                              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    colorIn.width = depthIn.width = motionIn.width = inputWidth;
    colorIn.height = depthIn.height = motionIn.height = inputHeight;
    colorOut.width = state.width;
    colorOut.height = state.height;
    colorIn.nativeFormat = inputFormat;
    colorOut.nativeFormat = dlaaOutputFormat;
    depthIn.nativeFormat = DXGI_FORMAT_R32_FLOAT;
    motionIn.nativeFormat = DXGI_FORMAT_R16G16_FLOAT;

    // Inputs/guides remain intact through Present; later passes only read
    // them or transition their states. Output has the shorter Evaluate life
    // because a failed RR attempt can run SR into that same output this frame.
    // eOnlyValidNow unnecessarily asks SL to clone the inputs.
    sl::ResourceTag tags[] = {
        { &colorIn, sl::kBufferTypeScalingInputColor,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &colorOut, sl::kBufferTypeScalingOutputColor,
          sl::ResourceLifecycle::eValidUntilEvaluate, &outputExtent },
        { &depthIn, sl::kBufferTypeDepth,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &motionIn, sl::kBufferTypeMotionVectors,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent }
    };
    result = slSetTagForFrame(*frame, viewport, tags,
                              static_cast<unsigned int>(_countof(tags)),
                              reinterpret_cast<sl::CommandBuffer *>(commands));
    if (result == sl::Result::eOk)
    {
        const sl::BaseStructure *inputs[] = { &viewport };
        result = slEvaluateFeature(
            sl::kFeatureDLSS, *frame, inputs,
            static_cast<unsigned int>(_countof(inputs)),
            reinterpret_cast<sl::CommandBuffer *>(commands));
    }
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, false, "resource tagging/evaluation");
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = state.output.Get();
    commands->ResourceBarrier(1, &barrier);
    transition(commands, state.output.Get(),
               D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    state.outputReadable = true;
    state.forceHistoryReset = false;
    return true;
}

bool dlaaEvaluateRayReconstruction(
                  ID3D12GraphicsCommandList *commands,
                  ID3D12Resource *inputColor,
                  unsigned int inputWidth,
                  unsigned int inputHeight,
                  DXGI_FORMAT inputFormat,
                  ID3D12Resource *depth,
                  ID3D12Resource *motion,
                  ID3D12Resource *diffuseAlbedo,
                  ID3D12Resource *specularAlbedo,
                  ID3D12Resource *normalRoughness,
                  float verticalFieldOfViewDegrees,
                  float aspectRatio,
                  float jitterX,
                  float jitterY,
                  bool resetHistory)
{
    if (state.evaluateFailed || state.rrEvaluateFailed || !state.rrRequested || !state.rrSupported || state.mode < 3 ||
        state.mode > 6 || !state.output || commands == nullptr ||
        inputColor == nullptr || depth == nullptr || motion == nullptr ||
        diffuseAlbedo == nullptr || specularAlbedo == nullptr ||
        normalRoughness == nullptr || inputWidth == 0 || inputHeight == 0 ||
        inputFormat != DXGI_FORMAT_R16G16B16A16_FLOAT ||
        (inputWidth >= state.width && inputHeight >= state.height))
    {
        return false;
    }
    if (state.outputReadable)
    {
        transition(commands, state.output.Get(),
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        state.outputReadable = false;
    }

    sl::FrameToken *frame = nullptr;
    sl::Result result = frameTokenForEvaluation(frame);
    if (result != sl::Result::eOk || frame == nullptr)
    {
        return evaluationFailed(result == sl::Result::eOk ? sl::Result::eErrorInvalidState : result,
                                true, "frame token");
    }
    const sl::ViewportHandle viewport(0u);
    constexpr float pi = 3.14159265358979323846f;
    const float fovRadians = std::max(verticalFieldOfViewDegrees, 1.0f) *
                             (pi / 180.0f);
    sl::Constants constants = {};
    setPerspective(constants.cameraViewToClip, constants.clipToCameraView,
                   fovRadians, aspectRatio);
    setIdentity(constants.clipToLensClip);
    setIdentity(constants.clipToPrevClip);
    setIdentity(constants.prevClipToClip);
    constants.jitterOffset = { jitterX, jitterY };
    constants.mvecScale = { 1.0f / static_cast<float>(inputWidth),
                            1.0f / static_cast<float>(inputHeight) };
    constants.cameraPinholeOffset = { 0.0f, 0.0f };
    constants.cameraPos = { 0.0f, 0.0f, 0.0f };
    constants.cameraUp = { 0.0f, 1.0f, 0.0f };
    constants.cameraRight = { 1.0f, 0.0f, 0.0f };
    constants.cameraFwd = { 0.0f, 0.0f, 1.0f };
    constants.cameraNear = cameraNearPlane;
    constants.cameraFar = cameraFarPlane;
    constants.cameraFOV = fovRadians;
    constants.cameraAspectRatio = aspectRatio;
    constants.depthInverted = sl::Boolean::eFalse;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    const bool resetTemporalHistory = resetHistory || state.forceHistoryReset;
    constants.reset = resetTemporalHistory ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    constants.orthographicProjection = sl::Boolean::eFalse;
    constants.motionVectorsDilated = sl::Boolean::eFalse;
    constants.motionVectorsJittered = sl::Boolean::eFalse;
    result = slSetConstants(constants, *frame, viewport);
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, true, "constants");
    }

    /* NVIDIA's DLSS-RR contract requires compatible DLSS Super Resolution
       options to be supplied alongside the DLSS-D options for the same
       viewport. Keep these synchronized with the normal full-scene path. */
    sl::DLSSOptions dlssOptions = {};
    dlssOptions.mode = dlssModeFromSetting(state.mode);
    dlssOptions.outputWidth = state.width;
    dlssOptions.outputHeight = state.height;
    /* DLSS Ray Reconstruction only supports HDR color input.  The v20
       integration accidentally advertised the FP16 HUD-less scene as LDR,
       which can cause DLSS-D option/evaluation rejection even when the RR DLLs
       are present. */
    dlssOptions.colorBuffersHDR = sl::Boolean::eTrue;
    dlssOptions.useAutoExposure = sl::Boolean::eFalse;
    dlssOptions.alphaUpscalingEnabled = sl::Boolean::eFalse;
    dlssOptions.dlaaPreset = sl::DLSSPreset::ePresetK;
    dlssOptions.qualityPreset = sl::DLSSPreset::ePresetK;
    dlssOptions.balancedPreset = sl::DLSSPreset::ePresetK;
    dlssOptions.performancePreset = sl::DLSSPreset::ePresetM;
    dlssOptions.ultraPerformancePreset = sl::DLSSPreset::ePresetL;
    state.dlssResourcesUsed = true;
    result = slDLSSSetOptions(viewport, dlssOptions);
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, true, "DLSS options");
    }

    sl::DLSSDOptions options = {};
    options.mode = dlssModeFromSetting(state.mode);
    options.outputWidth = state.width;
    options.outputHeight = state.height;
    options.preExposure = 1.0f;
    options.exposureScale = 1.0f;
    options.colorBuffersHDR = sl::Boolean::eTrue;
    options.normalRoughnessMode = sl::DLSSDNormalRoughnessMode::ePacked;
    setIdentity(options.worldToCameraView);
    setIdentity(options.cameraViewToWorld);
    options.alphaUpscalingEnabled = sl::Boolean::eFalse;
    options.dlaaPreset = sl::DLSSDPreset::ePresetE;
    options.qualityPreset = sl::DLSSDPreset::ePresetE;
    options.balancedPreset = sl::DLSSDPreset::ePresetE;
    options.performancePreset = sl::DLSSDPreset::ePresetE;
    options.ultraPerformancePreset = sl::DLSSDPreset::ePresetE;
    state.rrResourcesUsed = true;
    result = slDLSSDSetOptions(viewport, options);
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, true, "ray reconstruction options");
    }

    sl::Extent inputExtent = { 0, 0, inputWidth, inputHeight };
    sl::Extent outputExtent = { 0, 0, state.width, state.height };
    sl::Resource colorIn(sl::ResourceType::eTex2d, inputColor,
                         static_cast<unsigned int>(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    sl::Resource colorOut(sl::ResourceType::eTex2d, state.output.Get(),
                          static_cast<unsigned int>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    sl::Resource depthIn(sl::ResourceType::eTex2d, depth,
                         static_cast<unsigned int>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    sl::Resource motionIn(sl::ResourceType::eTex2d, motion,
                          static_cast<unsigned int>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    sl::Resource diffuseIn(sl::ResourceType::eTex2d, diffuseAlbedo,
                           static_cast<unsigned int>(
                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    sl::Resource specularIn(sl::ResourceType::eTex2d, specularAlbedo,
                            static_cast<unsigned int>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    sl::Resource normalRoughnessIn(sl::ResourceType::eTex2d, normalRoughness,
                                   static_cast<unsigned int>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    colorIn.width = depthIn.width = motionIn.width = diffuseIn.width =
        specularIn.width = normalRoughnessIn.width = inputWidth;
    colorIn.height = depthIn.height = motionIn.height = diffuseIn.height =
        specularIn.height = normalRoughnessIn.height = inputHeight;
    colorOut.width = state.width;
    colorOut.height = state.height;
    colorIn.nativeFormat = inputFormat;
    colorOut.nativeFormat = dlaaOutputFormat;
    depthIn.nativeFormat = DXGI_FORMAT_R32_FLOAT;
    motionIn.nativeFormat = DXGI_FORMAT_R16G16_FLOAT;
    diffuseIn.nativeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    specularIn.nativeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    normalRoughnessIn.nativeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    sl::ResourceTag tags[] = {
        { &colorIn, sl::kBufferTypeScalingInputColor,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &colorOut, sl::kBufferTypeScalingOutputColor,
          sl::ResourceLifecycle::eValidUntilEvaluate, &outputExtent },
        { &depthIn, sl::kBufferTypeDepth,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &motionIn, sl::kBufferTypeMotionVectors,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &diffuseIn, sl::kBufferTypeAlbedo,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &specularIn, sl::kBufferTypeSpecularAlbedo,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &normalRoughnessIn, sl::kBufferTypeNormalRoughness,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent },
        { &motionIn, sl::kBufferTypeSpecularMotionVectors,
          sl::ResourceLifecycle::eValidUntilPresent, &inputExtent }
    };
    result = slSetTagForFrame(*frame, viewport, tags,
                              static_cast<unsigned int>(_countof(tags)),
                              reinterpret_cast<sl::CommandBuffer *>(commands));
    if (result == sl::Result::eOk)
    {
        const sl::BaseStructure *inputs[] = { &viewport };
        result = slEvaluateFeature(
            sl::kFeatureDLSS_RR, *frame, inputs,
            static_cast<unsigned int>(_countof(inputs)),
            reinterpret_cast<sl::CommandBuffer *>(commands));
    }
    if (result != sl::Result::eOk)
    {
        return evaluationFailed(result, true, "resource tagging/evaluation");
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = state.output.Get();
    commands->ResourceBarrier(1, &barrier);
    transition(commands, state.output.Get(),
               D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    state.outputReadable = true;
    state.forceHistoryReset = false;
    if (!state.loggedRrActive)
    {
        HW_DIAG_FPRINTF(stderr,
            "[ModernGraphics] NVIDIA DLSS Ray Reconstruction ACTIVE: "
            "FP16 HDR low-resolution scene + depth/motion/diffuse/specular/normal-roughness guides.\n");
        state.loggedRrActive = true;
    }
    return true;
}

bool dlaaAvailable(void)
{
    return state.initialized && state.supported;
}

bool dlaaActive(void)
{
    return state.mode != 0 && state.mode != 2 && state.supported && !state.evaluateFailed &&
           state.output.Get() != nullptr;
}

bool dlaaRayReconstructionAvailable(void)
{
    return state.initialized && state.supported && state.rrPluginRequested &&
           state.rrSupported && !state.evaluateFailed && !state.rrEvaluateFailed;
}

}

#endif
