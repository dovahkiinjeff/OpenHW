#include "DiagnosticPolicy.h"
#include "StartupProgressModel.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace {
std::mutex progressMutex;
std::atomic<bool> progressActive{false};
hwmodern::StartupProgressModel progress;
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}
extern "C" int hwWindowsSplashIsActive(void) { return progressActive.load(std::memory_order_relaxed)?1:0; }
extern "C" int hwWindowsSplashTaskIsRunning(const char *id) {
    if(!hwWindowsSplashIsActive()) return 0;
    std::lock_guard<std::mutex> lock(progressMutex); return progress.running(id)?1:0;
}
extern "C" void hwWindowsSplashProgressReset(void) {
    std::lock_guard<std::mutex> lock(progressMutex);
    progress.reset(seconds());
#define HW_STARTUP_TASK(id,label) progress.plan(id,label);
#include "WindowsStartupTasks.inc"
#undef HW_STARTUP_TASK
    progressActive.store(true,std::memory_order_relaxed);
}
extern "C" void hwWindowsSplashProgressStop(void) {
    progressActive.store(false,std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(progressMutex); progress.stop();
}
extern "C" void hwWindowsSplashTaskBegin(const char *id,const char *label) {
    if(!hwWindowsSplashIsActive()) return;
    std::lock_guard<std::mutex> lock(progressMutex);
    if(progress.begin(id,label,seconds())) HW_DIAG_FPRINTF(stderr,"[StartupWork] begin %s: %s\n",id,label?label:id);
}
extern "C" void hwWindowsSplashTaskComplete(const char *id,const char *detail) {
    if(!hwWindowsSplashIsActive()) return;
    std::lock_guard<std::mutex> lock(progressMutex);
    if(progress.finish(id,detail,false,seconds())) HW_DIAG_FPRINTF(stderr,"[StartupWork] complete %s%s%s\n",id,detail?": ":"",detail?detail:"");
}
extern "C" void hwWindowsSplashTaskSkip(const char *id,const char *reason) {
    if(!hwWindowsSplashIsActive()) return;
    std::lock_guard<std::mutex> lock(progressMutex);
    if(progress.finish(id,reason,true,seconds())) HW_DIAG_FPRINTF(stderr,"[StartupWork] skip %s: %s\n",id,reason?reason:"not required");
}
extern "C" void hwWindowsSplashSkipPending(const char *prefix,const char *reason) {
    if(!hwWindowsSplashIsActive()) return;
    std::lock_guard<std::mutex> lock(progressMutex); progress.skipPending(prefix,reason,seconds());
}
extern "C" void hwWindowsSplashProgressDetail(const char *detail) {
    if(!hwWindowsSplashIsActive()) return;
    std::lock_guard<std::mutex> lock(progressMutex); progress.detail(detail);
}
extern "C" void hwWindowsSplashUpdate(const char *detail) {
    hwWindowsSplashProgressDetail(detail);
}
extern "C" void hwWindowsSplashFail(const char *reason) {
    if(!hwWindowsSplashIsActive()) return;
    std::lock_guard<std::mutex> lock(progressMutex); progress.fail(reason);
}
extern "C" void hwWindowsSplashGetSnapshot(HWWindowsSplashSnapshot *snapshot) {
    if(!snapshot) return;
    std::lock_guard<std::mutex> lock(progressMutex); *snapshot=progress.snapshot();
}
