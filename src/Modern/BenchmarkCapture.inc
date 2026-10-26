#include "DiagnosticPolicy.h"
/* Included after presenter/configuration state. No per-frame file writes,
   GPU waits, or quality changes. Resolved query slots carry a capture ID.
   Opt-in recordings also sample QPC for external trace correlation. */
struct BenchmarkCacheSnapshot {
    unsigned long long textureUploads = 0, textureEvictions = 0, textureTrimmedBytes = 0;
    unsigned long long textureResidentBytes = 0, rasterUploadedBytes = 0, rasterCacheChargeBytes = 0;
};

struct BenchmarkHitchFrame {
    unsigned long long frameId = 0, waitedForFrameId = 0;
    double intervalMs = -1, rendererMs = 0, fenceWaitMs = 0, recordMs = 0;
    double executeMs = 0, presentMs = 0, signalMs = 0, dxrRecordMs = 0;
    double qpcSeconds = -1;
    BenchmarkCacheSnapshot cache;
    bool cacheCountersReset = false;
    std::array<double,10> gpu, waitedGpu;
    BenchmarkHitchFrame() { gpu.fill(-1); waitedGpu.fill(-1); }
};

struct BenchmarkCapture {
    bool enabled = false, requested = false, warming = false, recording = false;
    unsigned long long id = 0;
    std::chrono::steady_clock::time_point started, previousFrame, memorySample;
    hwmodern::BenchmarkMetric frame, renderCpu, taskCpu, simulationCpu, gpu[10];
    unsigned long long peakWorking = 0, peakPrivate = 0, peakVram = 0, vramBudget = 0;
    unsigned long long unavailableMemory = 0;
    int mission = 0, aa = 0, fg = 0;
    unsigned int width = 0, height = 0, samples = 0, bounces = 0;
    float camera[3] = {}, ray00[3] = {}, ray10[3] = {}, ray01[3] = {};
    float godRays = 0, sun = 0, ambient = 0;
    bool cameraChanged = false, settingsChanged = false, focusLost = false;
    std::chrono::steady_clock::time_point taskStarted;
    bool taskActive = false;
    std::chrono::steady_clock::time_point simulationStarted;
    bool simulationActive = false;
    FILETIME processKernel = {}, processUser = {};
    std::array<float,25> tuning = {};
    unsigned int initialVolumes = 0, initialLights = 0, initialWakes = 0;
    unsigned int initialDustWidth = 0, initialDustHeight = 0;
    bool rayTracing = false, rayReconstruction = false;
    int vsync = 0;
    int textureCap = -1;
    // Fixed storage, populated only during an opt-in recording. No per-frame
    // allocations, output, extra clock reads, GPU queries or fence waits.
    static constexpr size_t hitchCapacity = 128;
    std::array<BenchmarkHitchFrame,hitchCapacity> hitches;
    size_t hitchCount = 0;
    unsigned long long waitedForFrameId = 0;
    std::array<double,10> waitedGpu;
    BenchmarkCacheSnapshot previousCache;
    bool previousCacheValid = false;
    std::chrono::steady_clock::time_point nextRequestPoll;
    unsigned long long qpcFrequency = 0, utcAnchorFileTime = 0;
    double qpcAnchorSeconds = -1, qpcAnchorSkewSeconds = 0;
};
BenchmarkCapture benchmark;

double benchmarkQpcSeconds()
{
    LARGE_INTEGER counter = {};
    return benchmark.qpcFrequency && QueryPerformanceCounter(&counter)
        ? static_cast<double>(counter.QuadPart) / benchmark.qpcFrequency : -1;
}

void benchmarkClockAnchor()
{
    LARGE_INTEGER frequency = {};
    benchmark.qpcFrequency = QueryPerformanceFrequency(&frequency) ? frequency.QuadPart : 0;
    const double before = benchmarkQpcSeconds();
    FILETIME utc = {};
    GetSystemTimePreciseAsFileTime(&utc);
    benchmark.utcAnchorFileTime = (static_cast<unsigned long long>(utc.dwHighDateTime) << 32) | utc.dwLowDateTime;
    const double after = benchmarkQpcSeconds();
    benchmark.qpcAnchorSeconds = before >= 0 && after >= before ? (before + after) * .5 : -1;
    benchmark.qpcAnchorSkewSeconds = before >= 0 && after >= before ? (after - before) * .5 : -1;
}

bool benchmarkConsumeFileRequest(std::chrono::steady_clock::time_point now)
{
    if (!benchmark.enabled || benchmark.warming || benchmark.recording || now < benchmark.nextRequestPoll)
        return false;
    benchmark.nextRequestPoll = now + std::chrono::milliseconds(500);
    wchar_t directory[32768] = {}, request[32768] = {};
    const DWORD length = GetEnvironmentVariableW(L"HW_CRASH_DIR", directory, 32768);
    if (!length || length > 32700) return false;
    swprintf_s(request, L"%s\\capture.request", directory);
    // Only this named marker is consumed; its contents are never executed/read.
    return DeleteFileW(request) != FALSE;
}

void benchmarkResetHitches()
{
    benchmark.hitchCount = 0;
    benchmark.waitedForFrameId = 0;
    benchmark.waitedGpu.fill(-1);
    benchmark.previousCache = {};
    benchmark.previousCacheValid = false;
}

void benchmarkBeginFrame(unsigned long long waitedForFrameId,
                         const BenchmarkCacheSnapshot &cache)
{
    if (!benchmark.recording) return;
    benchmark.waitedForFrameId = waitedForFrameId;
    benchmark.waitedGpu.fill(-1);
    if (!benchmark.previousCacheValid)
    {
        benchmark.previousCache = cache;
        benchmark.previousCacheValid = true;
    }
}

void benchmarkResolveGpu(unsigned long long captureId, unsigned long long frameId,
                         const std::array<double,10> &gpu)
{
    if (!benchmark.recording || captureId != benchmark.id || !frameId) return;
    if (benchmark.waitedForFrameId == frameId) benchmark.waitedGpu = gpu;
    for (size_t index = 0; index < benchmark.hitchCount; ++index)
    {
        auto &frame = benchmark.hitches[index];
        if (frame.frameId == frameId) frame.gpu = gpu;
        if (frame.waitedForFrameId == frameId) frame.waitedGpu = gpu;
    }
}

void benchmarkRecordHitch(BenchmarkHitchFrame frame,
                          const BenchmarkCacheSnapshot &cache)
{
    if (!benchmark.recording) return;
    frame.waitedForFrameId = benchmark.waitedForFrameId;
    frame.waitedGpu = benchmark.waitedGpu;
    frame.cache = cache;
    const auto &previous = benchmark.previousCache;
    frame.cacheCountersReset = benchmark.previousCacheValid &&
        (cache.textureUploads < previous.textureUploads ||
         cache.textureEvictions < previous.textureEvictions ||
         cache.textureTrimmedBytes < previous.textureTrimmedBytes);
    if (benchmark.previousCacheValid && !frame.cacheCountersReset)
    {
        frame.cache.textureUploads -= previous.textureUploads;
        frame.cache.textureEvictions -= previous.textureEvictions;
        frame.cache.textureTrimmedBytes -= previous.textureTrimmedBytes;
    }
    else if (!benchmark.previousCacheValid)
    {
        frame.cache.textureUploads = frame.cache.textureEvictions = frame.cache.textureTrimmedBytes = 0;
    }
    benchmark.previousCache = cache;
    benchmark.previousCacheValid = true;
    // The first frame has no preceding interval and cannot be ranked. Counter
    // baselines still advance, so the following row remains a per-frame delta.
    if (!frame.frameId || !std::isfinite(frame.intervalMs) || frame.intervalMs < 0) return;
    const auto before = [](const BenchmarkHitchFrame &a, const BenchmarkHitchFrame &b) {
        return a.intervalMs != b.intervalMs ? a.intervalMs > b.intervalMs : a.frameId < b.frameId;
    };
    for (size_t index = 0; index < benchmark.hitchCount; ++index)
        if (benchmark.hitches[index].frameId == frame.frameId) return;
    size_t position = 0;
    while (position < benchmark.hitchCount && !before(frame, benchmark.hitches[position]))
    {
        ++position;
    }
    if (position == BenchmarkCapture::hitchCapacity) return;
    const size_t count = std::min(benchmark.hitchCount + 1, BenchmarkCapture::hitchCapacity);
    for (size_t index = count - 1; index > position; --index)
        benchmark.hitches[index] = benchmark.hitches[index - 1];
    benchmark.hitches[position] = frame;
    benchmark.hitchCount = count;
}

std::array<float,25> benchmarkTuning()
{
    return {requestedNormalStrength,requestedEnvironmentStrength,requestedAuthoredLightStrength,
        requestedSurfaceReflectivity,requestedSurfaceRoughness,requestedLightingExposure,
        requestedShadowStrength,requestedSunShadowAngularRadiusDegrees,float(requestedSunShadowSamples),
        float(requestedLocalShadowSamples),requestedLocalShadowAngularRadiusDegrees,
        requestedShadowReceiverBias,requestedShadowNormalBias,requestedContactShadowStrength,
        requestedContactShadowDistance,requestedShadowMaximumDistance,requestedFxLightingStrength,
        requestedChromaticAberration,requestedMotionBlur,requestedFilmGrain,requestedBloom,
        float(requestedLensFlares),requestedOutputDitherStrength,missionGodRayStrength,float(missionGodRayTint)};
}

bool benchmarkLaunchRequested() { return !hwDiagnosticsDisabled(); }


unsigned long long benchmarkFileTime(FILETIME value)
{
    return (static_cast<unsigned long long>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

void benchmarkSampleMemory()
{
    PROCESS_MEMORY_COUNTERS_EX process = {};
    process.cb = sizeof(process);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&process), sizeof(process))) {
        benchmark.peakWorking = std::max(benchmark.peakWorking,
            static_cast<unsigned long long>(process.WorkingSetSize));
        benchmark.peakPrivate = std::max(benchmark.peakPrivate,
            static_cast<unsigned long long>(process.PrivateUsage));
    } else ++benchmark.unavailableMemory;
    ComPtr<IDXGIAdapter3> adapter;
    DXGI_QUERY_VIDEO_MEMORY_INFO memory = {};
    if (presenter.adapter && SUCCEEDED(presenter.adapter.As(&adapter)) &&
        SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory))) {
        benchmark.peakVram = std::max(benchmark.peakVram, memory.CurrentUsage);
        benchmark.vramBudget = memory.Budget;
    } else ++benchmark.unavailableMemory;
}

void benchmarkFinish(bool complete)
{
    if (!benchmark.recording) { benchmark.warming = false; return; }
    benchmark.recording = false;
    benchmark.taskActive = false;
    benchmark.simulationActive = false;
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - benchmark.started).count();
    wchar_t directory[32768] = {};
    DWORD length = GetEnvironmentVariableW(L"HW_CRASH_DIR", directory, 32768);
    if (!length || length >= 32768) {
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, directory))) return;
        wcscat_s(directory, L"\\My Games\\Homeworld Modern\\Logs");
    }
    wcscat_s(directory, L"\\Benchmarks");
    SHCreateDirectoryExW(nullptr, directory, nullptr);
    SYSTEMTIME now; GetLocalTime(&now);
    wchar_t path[32768];
    swprintf_s(path, L"%s\\benchmark-%04u%02u%02u-%02u%02u%02u-%lu-%llu.csv",
        directory, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        GetCurrentProcessId(), benchmark.id);
    FILE *report = nullptr;
    if (_wfopen_s(&report, path, L"wt") || !report) {HW_DIAG_FPRINTF(stderr,"[Benchmark] Report file could not be opened.\n");return;}
    HW_DIAG_FPRINTF(stderr,"[Benchmark] Writing %s report %llu to the session Benchmarks folder.\n",complete?"complete":"partial",benchmark.id);
    std::fprintf(report, "status,%s\nelapsed_seconds,%.3f\nmission,%d\noutput,%u x %u\naa_mode,%d\nframe_generation_mode,%d\npath_samples,%u\npath_bounces,%u\n",
        complete ? "complete" : "partial", elapsed, benchmark.mission,
        benchmark.width, benchmark.height, benchmark.aa, benchmark.fg, benchmark.samples, benchmark.bounces);
    std::fprintf(report, "camera,%.6f / %.6f / %.6f\ncamera_changed,%d\nsettings_changed,%d\ngod_rays,%.3f\nprimary_sun,%.3f\nambient,%.3f\n",
        benchmark.camera[0],benchmark.camera[1],benchmark.camera[2],benchmark.cameraChanged,
        benchmark.settingsChanged,benchmark.godRays,benchmark.sun,benchmark.ambient);
    std::fprintf(report,"focus_lost,%d\ncamera_ray00,%.6f / %.6f / %.6f\ncamera_ray10,%.6f / %.6f / %.6f\ncamera_ray01,%.6f / %.6f / %.6f\n",
        benchmark.focusLost,benchmark.ray00[0],benchmark.ray00[1],benchmark.ray00[2],
        benchmark.ray10[0],benchmark.ray10[1],benchmark.ray10[2],benchmark.ray01[0],benchmark.ray01[1],benchmark.ray01[2]);
    const char *tuningNames[25] = {"normal_strength","environment_strength","authored_light_strength",
        "reflectivity","roughness","lighting_exposure","shadow_strength","sun_shadow_radius",
        "sun_shadow_samples","local_shadow_samples","local_shadow_radius","shadow_receiver_bias",
        "shadow_normal_bias","contact_shadow_strength","contact_shadow_distance","shadow_max_distance",
        "fx_lighting_strength","chromatic_aberration","motion_blur","film_grain","bloom",
        "lens_flares","output_dither","mission_god_ray_strength","mission_god_ray_tint"};
    for (size_t i=0;i<benchmark.tuning.size();++i)
        std::fprintf(report,"%s,%.6f\n",tuningNames[i],benchmark.tuning[i]);
    std::fprintf(report,"ray_tracing_requested,%d\nray_reconstruction_requested,%d\nvsync_start,%d\ndust_volumes_start,%u\ndust_lights_start,%u\ndust_wakes_start,%u\ndust_screen_start,%u x %u\n",
        benchmark.rayTracing,benchmark.rayReconstruction,benchmark.vsync,
        benchmark.initialVolumes,benchmark.initialLights,benchmark.initialWakes,benchmark.initialDustWidth,benchmark.initialDustHeight);
    std::fprintf(report,"model_texture_long_edge_cap_0_full_source_minus1_unknown,%d\n",benchmark.textureCap);
    std::fprintf(report,"clear_atmosphere_cache,%d\n",atmosphereCacheEnabled() ? 1 : 0);
    std::fprintf(report,"dust_volumes_end,%u\ndust_lights_end,%u\ndust_wakes_end,%u\ndust_screen_end,%u x %u\nvsync_end,%d\n",
        presenter.lastDustVolumeCount,presenter.lastDustLightCount,presenter.lastDustWakeCount,
        presenter.dustScreenWidth,presenter.dustScreenHeight,presenter.vsyncMode);
    DXGI_ADAPTER_DESC1 description = {};
    if (presenter.adapter && SUCCEEDED(presenter.adapter->GetDesc1(&description))) {
        char name[256] = {};
        WideCharToMultiByte(CP_UTF8,0,description.Description,-1,name,sizeof(name),nullptr,nullptr);
        std::fprintf(report,"adapter,\"%s\"\n",name);
    }
    std::fprintf(report, "peak_working_set_bytes,%llu\npeak_private_bytes,%llu\npeak_local_vram_bytes,%llu\nlocal_vram_budget_bytes,%llu\nmemory_query_failures,%llu\n",
        benchmark.peakWorking,benchmark.peakPrivate,benchmark.peakVram,benchmark.vramBudget,benchmark.unavailableMemory);
    FILETIME created, exited, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
        std::fprintf(report,"process_cpu_core_equivalents,%.3f\n",
            ((benchmarkFileTime(kernel)-benchmarkFileTime(benchmark.processKernel)) +
             (benchmarkFileTime(user)-benchmarkFileTime(benchmark.processUser))) / 10000000.0 / std::max(elapsed,0.001));
    std::fprintf(report, "frames_over_16.667ms,%zu\nframes_over_33.333ms,%zu\nframes_over_50ms,%zu\n",
        benchmark.frame.over(16.667),benchmark.frame.over(33.333),benchmark.frame.over(50.0));
    std::fprintf(report,"base_fps,%.3f\n",benchmark.frame.mean()>0 ? 1000.0/benchmark.frame.mean() : 0.0);
    std::fprintf(report,"qpc_frequency,%llu\nqpc_anchor_seconds,%.9f\nutc_anchor_filetime_100ns,%llu\nqpc_anchor_skew_seconds,%.9f\n",
        benchmark.qpcFrequency,benchmark.qpcAnchorSeconds,benchmark.utcAnchorFileTime,benchmark.qpcAnchorSkewSeconds);
    std::fprintf(report,"metric,count,mean_ms,p50_ms,p95_ms,p99_ms,max_ms\n");
    const auto write = [report](const char *name,const hwmodern::BenchmarkMetric &metric) {
        std::fprintf(report,"%s,%zu,%.6f,%.6f,%.6f,%.6f,%.6f\n",name,metric.samples.size(),
            metric.mean(),metric.percentile(.50),metric.percentile(.95),metric.percentile(.99),metric.percentile(1));
    };
    write("base_frame_interval_includes_waits",benchmark.frame);
    write("cpu_renderer_includes_present_wait",benchmark.renderCpu);
    write("cpu_task_dispatch_includes_simulation_and_render",benchmark.taskCpu);
    write("cpu_simulation_update",benchmark.simulationCpu);
    const char *names[10] = {"gpu_raster","gpu_dxr","gpu_gap_after_dxr","gpu_dust_prepare",
        "gpu_cloud_shadows","gpu_cloud_march","gpu_scene_composite","gpu_upscaler","gpu_presenter","gpu_recorded_total"};
    for (int i=0;i<10;++i) write(names[i],benchmark.gpu[i]);
    std::fprintf(report,"notes,Base frames only; generated frames excluded. GPU passes overlap CPU work; do not add CPU and GPU times. GPU capture tail may be unresolved. Memory sampled once per second.\n");
    std::fprintf(report,"hitch_frame_capacity,%zu\nhitch_frames_retained,%zu\n",
        BenchmarkCapture::hitchCapacity, benchmark.hitchCount);
    std::fprintf(report,"record_type,frame_id,waited_for_frame_id,frame_interval_ms,cpu_renderer_ms,cpu_flight_wait_ms,cpu_record_stage_ms,cpu_execute_ms,cpu_present_ms,cpu_signal_ms,cpu_dxr_record_ms,texture_uploads_delta,texture_evictions_delta,texture_trimmed_bytes_delta,texture_resident_bytes,raster_uploaded_bytes,raster_cache_charge_bytes,cache_counters_reset");
    for (const char *prefix : {"", "waited_"})
        for (const char *name : names) std::fprintf(report,",%s%s_ms",prefix,name);
    std::fprintf(report,",qpc_seconds\n");
    for (size_t index = 0; index < benchmark.hitchCount; ++index)
    {
        const auto &frame = benchmark.hitches[index];
        std::fprintf(report,"hitch_frame,%llu,%llu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%d",
            frame.frameId,frame.waitedForFrameId,frame.intervalMs,frame.rendererMs,frame.fenceWaitMs,
            frame.recordMs,frame.executeMs,frame.presentMs,frame.signalMs,frame.dxrRecordMs,
            frame.cache.textureUploads,frame.cache.textureEvictions,frame.cache.textureTrimmedBytes,
            frame.cache.textureResidentBytes,frame.cache.rasterUploadedBytes,frame.cache.rasterCacheChargeBytes,
            frame.cacheCountersReset ? 1 : 0);
        for (double value : frame.gpu) std::fprintf(report,",%.6f",value);
        for (double value : frame.waitedGpu) std::fprintf(report,",%.6f",value);
        std::fprintf(report,",%.9f\n",frame.qpcSeconds);
    }
    std::fprintf(report,"hitch_notes,Up to 128 largest completed base-frame intervals; ties keep earlier frame IDs. CPU scopes overlap. GPU columns match submitted frame IDs; waited_gpu columns describe the older slot submission whose normal fence was checked. -1 means unavailable. Texture counters are frame deltas; reset rows use new-lifetime counts. Raster upload bytes are per-frame; cache/resident bytes are instantaneous.\n");
    fclose(report);
    SetWindowTextW(presenter.window,L"Homeworld Modern - benchmark saved");
}

void benchmarkPoll()
{
    static const bool enabled = benchmarkLaunchRequested();
    benchmark.enabled = enabled;
    if (!enabled) return;
    const auto now = std::chrono::steady_clock::now();
    const bool key = benchmark.requested || benchmarkConsumeFileRequest(now);
    benchmark.requested = false;
    if (key && !benchmark.warming && !benchmark.recording) {
        benchmark.warming = true; benchmark.started = now;
        HW_DIAG_FPRINTF(stderr,"[Benchmark] Request accepted: 5 seconds warmup, then 30 seconds capture.\n");
        ++benchmark.id;
        benchmarkResetHitches();
        benchmark.frame.reset(); benchmark.renderCpu.reset(); benchmark.taskCpu.reset();
        benchmark.simulationCpu.reset();
        for (auto &metric: benchmark.gpu) metric.reset();
        SetWindowTextW(presenter.window,L"Homeworld Modern - benchmark warming up (5 seconds)");
    }
    if (benchmark.warming && std::chrono::duration<double>(now-benchmark.started).count() >= 5.0) {
        benchmark.warming = false; benchmark.recording = true; benchmark.started = now;
        benchmarkClockAnchor();
        HW_DIAG_FPRINTF(stderr,"[Benchmark] Recording started; keep camera and settings unchanged.\n");
        benchmark.previousFrame = {}; benchmark.memorySample = now;
        benchmark.peakWorking = benchmark.peakPrivate = benchmark.peakVram = benchmark.vramBudget = benchmark.unavailableMemory = 0;
        benchmark.mission = presenter.godRayMapIdentity;
        benchmark.width = presenter.width; benchmark.height = presenter.height;
        benchmark.aa = requestedAntiAliasingMode; benchmark.fg = requestedFrameGenerationMode;
        benchmark.samples = requestedPathSamples; benchmark.bounces = requestedPathBounces;
        benchmark.godRays = requestedGodRays; benchmark.sun = requestedPrimarySunStrength; benchmark.ambient = requestedSkyAmbientStrength;
        benchmark.tuning = benchmarkTuning();
        benchmark.rayTracing = raytracingRequested; benchmark.rayReconstruction = requestedRayReconstruction;
        benchmark.vsync = presenter.vsyncMode;
        benchmark.textureCap = observedModelTextureCap;
        benchmark.initialVolumes = presenter.lastDustVolumeCount; benchmark.initialLights = presenter.lastDustLightCount;
        benchmark.initialWakes = presenter.lastDustWakeCount;
        benchmark.initialDustWidth = presenter.dustScreenWidth; benchmark.initialDustHeight = presenter.dustScreenHeight;
        std::copy_n(presenter.dustCameraPosition,3,benchmark.camera);
        std::copy_n(presenter.dustRayUv00,3,benchmark.ray00);
        std::copy_n(presenter.dustRayUv10,3,benchmark.ray10);
        std::copy_n(presenter.dustRayUv01,3,benchmark.ray01);
        benchmark.cameraChanged = benchmark.settingsChanged = benchmark.focusLost = false;
        FILETIME created, exited;
        GetProcessTimes(GetCurrentProcess(),&created,&exited,&benchmark.processKernel,&benchmark.processUser);
        benchmarkSampleMemory();
        SetWindowTextW(presenter.window,L"Homeworld Modern - benchmark recording (30 seconds)");
    }
    if (!benchmark.recording) return;
    if (GetForegroundWindow() != presenter.window) benchmark.focusLost = true;
    if (std::chrono::duration<double>(now-benchmark.started).count() >= 30.0) {
        benchmarkFinish(true); return;
    }
    for(int i=0;i<3;++i)
        if (std::abs(presenter.dustCameraPosition[i]-benchmark.camera[i])>.1f ||
            std::abs(presenter.dustRayUv00[i]-benchmark.ray00[i])>.0001f ||
            std::abs(presenter.dustRayUv10[i]-benchmark.ray10[i])>.0001f ||
            std::abs(presenter.dustRayUv01[i]-benchmark.ray01[i])>.0001f) benchmark.cameraChanged = true;
    benchmark.settingsChanged |= benchmark.mission != presenter.godRayMapIdentity ||
        benchmark.width != presenter.width || benchmark.height != presenter.height ||
        benchmark.aa != requestedAntiAliasingMode || benchmark.fg != requestedFrameGenerationMode ||
        benchmark.samples != requestedPathSamples || benchmark.bounces != requestedPathBounces ||
        benchmark.godRays != requestedGodRays || benchmark.sun != requestedPrimarySunStrength ||
        benchmark.ambient != requestedSkyAmbientStrength;
    benchmark.settingsChanged |= benchmark.tuning != benchmarkTuning() ||
        benchmark.rayTracing != raytracingRequested || benchmark.rayReconstruction != requestedRayReconstruction ||
        benchmark.vsync != presenter.vsyncMode;
    benchmark.settingsChanged |= benchmark.textureCap != observedModelTextureCap;
    if (std::chrono::duration<double>(now-benchmark.memorySample).count() >= 1.0) {
        benchmarkSampleMemory(); benchmark.memorySample = now;
    }
}
