#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include "BenchmarkStats.h"
#include "WindowsDiagnostics.h"
#undef NDEBUG
#include <cassert>
using Microsoft::WRL::ComPtr;
/* Exercise production capture/report code without a game or a window. */
struct {
    ComPtr<IDXGIAdapter1> adapter;
    HWND window = nullptr;
    int godRayMapIdentity = 7, vsyncMode = 0;
    unsigned int width = 1920, height = 1080, lastDustVolumeCount = 12,
        lastDustLightCount = 3, lastDustWakeCount = 0, dustScreenWidth = 1920, dustScreenHeight = 1080;
    float dustCameraPosition[3] = {1,2,3}, dustRayUv00[3] = {-1,-1,-1},
        dustRayUv10[3] = {1,-1,-1}, dustRayUv01[3] = {-1,1,-1};
} presenter;
bool raytracingRequested = true, requestedRayReconstruction = false, requestedLensFlares = true;
int requestedAntiAliasingMode = 0, requestedFrameGenerationMode = 0;
int observedModelTextureCap = 0;
unsigned int requestedPathSamples = 1, requestedPathBounces = 3,
    requestedSunShadowSamples = 6, requestedLocalShadowSamples = 2, missionGodRayTint = 0;
float requestedGodRays=1,requestedPrimarySunStrength=1,requestedSkyAmbientStrength=1,
    requestedNormalStrength=1,requestedEnvironmentStrength=1,requestedAuthoredLightStrength=1,
    requestedSurfaceReflectivity=1,requestedSurfaceRoughness=1,requestedLightingExposure=1,
    requestedShadowStrength=1,requestedSunShadowAngularRadiusDegrees=1,
    requestedLocalShadowAngularRadiusDegrees=1,requestedShadowReceiverBias=1,
    requestedShadowNormalBias=1,requestedContactShadowStrength=1,requestedContactShadowDistance=1,
    requestedShadowMaximumDistance=1,requestedFxLightingStrength=1,requestedChromaticAberration=0,
    requestedMotionBlur=0,requestedFilmGrain=0,requestedBloom=1,requestedOutputDitherStrength=1,
    missionGodRayStrength=1;
static bool atmosphereCacheEnabled(){return true;}
#include "BenchmarkCapture.inc"

int main()
{
    if (hwDiagnosticsDisabled())
    {
        assert(!benchmarkLaunchRequested());
        return 0;
    }
    assert(benchmarkLaunchRequested());
    wchar_t temporary[MAX_PATH], folder[MAX_PATH];
    assert(GetTempPathW(MAX_PATH,temporary));
    swprintf_s(folder,L"%shw-benchmark-test-%lu",temporary,GetCurrentProcessId());
    assert(CreateDirectoryW(folder,nullptr));
    assert(SetEnvironmentVariableW(L"HW_CRASH_DIR",folder));
    // The sidecar can arm a capture without focus changes or injected keys.
    const std::wstring requestPath = std::wstring(folder) + L"\\capture.request";
    HANDLE request = CreateFileW(requestPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(request != INVALID_HANDLE_VALUE); CloseHandle(request);
    benchmarkPoll();
    assert(benchmark.warming && !benchmark.recording);
    assert(GetFileAttributesW(requestPath.c_str()) == INVALID_FILE_ATTRIBUTES);
    benchmark.started -= std::chrono::seconds(6);
    benchmarkPoll();
    assert(benchmark.recording && !benchmark.warming);
    assert(benchmark.qpcFrequency > 0 && benchmark.utcAnchorFileTime > 116444736000000000ull);
    assert(benchmark.qpcAnchorSeconds > 0 && benchmarkQpcSeconds() >= benchmark.qpcAnchorSeconds);
    assert(benchmark.mission==7 && benchmark.aa==0 && benchmark.fg==0);
    assert(benchmark.peakWorking>0 && benchmark.unavailableMemory==1); // no GPU adapter in this test
    benchmark.frame.add(10); benchmark.frame.add(30);
    benchmark.renderCpu.add(5); benchmark.simulationCpu.add(2); benchmark.gpu[1].add(7);
    BenchmarkCacheSnapshot cache;
    cache.textureUploads = 100; cache.textureEvictions = 20; cache.textureTrimmedBytes = 4096;
    cache.textureResidentBytes = 8192; cache.rasterUploadedBytes = 128; cache.rasterCacheChargeBytes = 512;
    auto sample = [](unsigned long long id, double interval) {
        BenchmarkHitchFrame frame;
        frame.frameId = id; frame.intervalMs = interval;
        frame.rendererMs = 5; frame.fenceWaitMs = 2; frame.recordMs = 2;
        frame.executeMs = .1; frame.presentMs = .8; frame.signalMs = .1; frame.dxrRecordMs = 1;
        frame.qpcSeconds = 123.5;
        return frame;
    };
    for (unsigned long long id = 1; id <= 300; ++id)
    {
        benchmarkBeginFrame(id > 3 ? id - 3 : 0, cache);
        ++cache.textureUploads;
        benchmarkRecordHitch(sample(id, double(id)), cache);
    }
    assert(benchmark.hitchCount == BenchmarkCapture::hitchCapacity);
    assert(benchmark.hitches[0].frameId == 300 && benchmark.hitches[127].frameId == 173);
    assert(benchmark.hitches[0].cache.textureUploads == 1 && !benchmark.hitches[0].cacheCountersReset);
    assert(benchmark.hitches[0].cache.textureResidentBytes == 8192);
    assert(benchmark.hitches[0].cache.rasterUploadedBytes == 128); // Already per-frame, not a delta.
    assert(benchmark.hitches[0].waitedForFrameId == 297 && benchmark.hitches[0].gpu[9] == -1);
    std::array<double,10> gpuA, gpuB;
    gpuA.fill(7); gpuB.fill(11);
    benchmarkResolveGpu(benchmark.id + 1, 300, gpuA);
    assert(benchmark.hitches[0].gpu[9] == -1); // An earlier/later capture cannot rewrite this one.
    benchmarkResolveGpu(benchmark.id, 300, gpuA);
    benchmarkResolveGpu(benchmark.id, 297, gpuB); // Out-of-order completion matches actual IDs.
    assert(benchmark.hitches[0].gpu[9] == 7 && benchmark.hitches[0].waitedGpu[9] == 11);
    assert(benchmark.hitches[3].frameId == 297 && benchmark.hitches[3].gpu[9] == 11);
    assert(benchmark.hitches[1].gpu[9] == -1);
    cache.textureUploads = 2; cache.textureEvictions = 0; cache.textureTrimmedBytes = 0;
    benchmarkBeginFrame(298, cache);
    benchmarkRecordHitch(sample(301, 1000), cache);
    assert(benchmark.hitches[0].cacheCountersReset && benchmark.hitches[0].cache.textureUploads == 2);
    assert(benchmark.hitches[0].cache.textureEvictions == 0 && benchmark.hitches[0].cache.textureTrimmedBytes == 0);
    benchmarkBeginFrame(300, cache);
    benchmarkResolveGpu(benchmark.id, 300, gpuA); // The waited frame can resolve before this CPU record exists.
    benchmarkRecordHitch(sample(500, 2000), cache);
    assert(benchmark.hitches[0].waitedForFrameId == 300 && benchmark.hitches[0].waitedGpu[9] == 7);
    benchmarkRecordHitch(sample(499, 2000), cache); // Equal intervals prefer the earlier submitted ID.
    assert(benchmark.hitches[0].frameId == 499 && benchmark.hitches[1].frameId == 500);
    benchmarkRecordHitch(sample(499, 3000), cache); // Duplicate IDs never consume a second slot.
    benchmarkRecordHitch(sample(501, std::nan("")), cache);
    benchmarkRecordHitch(sample(502, -1), cache);
    assert(benchmark.hitchCount == 128 && benchmark.hitches[0].frameId == 499);
    presenter.dustCameraPosition[0] += 1;
    requestedPathBounces = 6;
    benchmarkPoll();
    assert(benchmark.cameraChanged && benchmark.settingsChanged);
    benchmarkFinish(false);
    assert(!benchmark.recording);
    benchmarkRecordHitch(sample(999, 99999), cache);
    benchmarkResolveGpu(benchmark.id, 499, gpuA);
    assert(benchmark.hitches[0].frameId == 499 && benchmark.hitches[0].gpu[9] == -1);
    std::wstring directory=std::wstring(folder)+L"\\Benchmarks";
    WIN32_FIND_DATAW found;
    HANDLE search=FindFirstFileW((directory+L"\\*.csv").c_str(),&found);
    assert(search!=INVALID_HANDLE_VALUE); FindClose(search);
    std::wstring path=directory+L"\\"+found.cFileName;
    FILE *file=nullptr; assert(_wfopen_s(&file,path.c_str(),L"rb")==0);
    std::string report; char buffer[1024]; size_t count;
    while((count=fread(buffer,1,sizeof(buffer),file))>0) report.append(buffer,count);
    fclose(file);
    assert(report.find("status,partial")!=std::string::npos);
    assert(report.find("camera_changed,1")!=std::string::npos);
    assert(report.find("settings_changed,1")!=std::string::npos);
    assert(report.find("base_fps,50.000")!=std::string::npos);
    assert(report.find("gpu_dxr,1,7.000000")!=std::string::npos);
    assert(report.find("cpu_simulation_update,1,2.000000")!=std::string::npos);
    assert(report.find("hitch_frame_capacity,128")!=std::string::npos);
    assert(report.find("hitch_frames_retained,128")!=std::string::npos);
    assert(report.find("record_type,frame_id,waited_for_frame_id")!=std::string::npos);
    assert(report.find("hitch_frame,499,300,2000.000000")!=std::string::npos);
    assert(report.find("waited_gpu_recorded_total_ms")!=std::string::npos);
    assert(report.find("qpc_anchor_seconds,")!=std::string::npos);
    assert(report.find(",qpc_seconds\n")!=std::string::npos || report.find(",qpc_seconds\r\n")!=std::string::npos);
    assert(report.find(",123.500000000")!=std::string::npos);
    assert(report.find(",-1.000000")!=std::string::npos);
    benchmarkResetHitches();
    assert(benchmark.hitchCount == 0 && !benchmark.previousCacheValid && benchmark.waitedForFrameId == 0);
    assert(benchmark.waitedGpu[9] == -1);
    assert(DeleteFileW(path.c_str()));
    assert(RemoveDirectoryW(directory.c_str()));
    assert(RemoveDirectoryW(folder));
}
