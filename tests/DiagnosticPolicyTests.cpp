#include "DiagnosticPolicy.h"
#include "WindowsDiagnostics.h"
#include "FGZoomCapture.h"
#include "ModernGraphics.h"
#include <windows.h>
#include <cstdio>
#include <string>
int main() {
    wchar_t temporary[MAX_PATH]={};GetTempPathW(MAX_PATH,temporary);
    const std::wstring directory=std::wstring(temporary)+L"OpenHW-diagnostic-policy-"+std::to_wstring(GetCurrentProcessId());
    SetEnvironmentVariableW(L"HW_CRASH_DIR",directory.c_str());
    hwWindowsDiagnosticsInstall();
    int evaluated=0;
    HW_DIAG_FPRINTF(stderr,"diagnostic-policy-test %d\n",++evaluated);
#if HW_ENABLE_DIAGNOSTICS
    if(evaluated!=1 || GetFileAttributesW(directory.c_str())==INVALID_FILE_ATTRIBUTES)return 1;
    RemoveDirectoryW(directory.c_str());
#else
    if(evaluated || !hwDiagnosticsDisabled() || GetFileAttributesW(directory.c_str())!=INVALID_FILE_ATTRIBUTES)return 2;
    SetEnvironmentVariableA("HW_TEMPORAL_CAPTURE","1");
    SetEnvironmentVariableA("HW_FG_ZOOM_CAPTURE","1");
    if(hwmodern::temporalCaptureEnabled() || hwmodern::temporalCaptureNeedsCloudProbe(1))return 3;
    hwmodern::fgZoomCaptureNotifyZoom();
    hwModernBenchmarkRequest();hwModernBenchmarkCpuBegin(0);hwModernBenchmarkCpuEnd(0);
#endif
    std::printf("PASS: diagnostic build policy %s, argument evaluation and file creation verified\n",HW_ENABLE_DIAGNOSTICS?"ON":"OFF");
    return 0;
}
