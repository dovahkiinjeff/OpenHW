#include "../src/Modern/StartupProgressModel.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <string>
#include <thread>

int main() {
    hwmodern::StartupProgressModel model;
    model.reset(10); model.plan("setup","Setup"); model.plan("shader","Shader"); model.plan("first","First frame");
    assert(model.snapshot().total==3 && model.snapshot().percentHundredths==0);
    model.plan("shader","Duplicate"); assert(model.snapshot().total==3);
    assert(!model.begin("unknown","Unknown",10)); assert(model.snapshot().total==3);
    model.begin("setup","Setup",11); model.begin("shader","Compiling display shader",12);
    for(int i=0;i<500;++i) {
        model.detail("Waiting for the compiler");
        assert(model.snapshot().completed==0 && model.snapshot().percentHundredths==0);
        assert(model.snapshot().taskStartedSeconds==12);
    }
    assert(model.finish("shader","Loaded validated cache",false,40));
    assert(model.snapshot().completed==1 && model.snapshot().percentHundredths==3333);
    assert(std::string(model.snapshot().task)=="Setup" && model.snapshot().taskStartedSeconds==11);
    assert(!model.finish("shader","Repeated completion",false,41));
    model.finish("setup",nullptr,false,42); assert(model.snapshot().percentHundredths==6666);
    model.begin("first",nullptr,43); model.fail("First frame failed");
    assert(!model.finish("first",nullptr,false,44)); assert(model.snapshot().completed==2 && model.snapshot().failed);
    model.reset(50); model.plan("optional.a","Optional A"); model.plan("optional.b","Optional B"); model.plan("frame","Frame");
    model.begin("optional.a",nullptr,51); model.skipPending("optional.","Unused optional path",52);
    assert(model.snapshot().completed==1 && model.snapshot().skipped==1 && model.running("optional.a"));
    model.finish("optional.a","Using supported fallback",true,53); model.begin("frame",nullptr,54);
    assert(model.snapshot().completed==2 && model.snapshot().percentHundredths==6666);
    model.finish("frame",nullptr,false,55); assert(model.snapshot().percentHundredths==10000);
    model.stop(); assert(!model.begin("frame",nullptr,56) && !model.snapshot().active);

    hwWindowsSplashProgressReset(); HWWindowsSplashSnapshot snapshot{};
    hwWindowsSplashGetSnapshot(&snapshot); const unsigned total=snapshot.total;
    assert(total>70 && snapshot.completed==0 && snapshot.percentHundredths==0);
    // A reader on another thread may poll while a real task is blocked.
    hwWindowsSplashTaskBegin("main.install","Checking data");
    std::thread reader([] { for(int i=0;i<1000;++i) {
        HWWindowsSplashSnapshot view{}; hwWindowsSplashGetSnapshot(&view);
        assert(view.active && view.total>70 && view.percentHundredths<=10000);
    }});
    hwWindowsSplashTaskComplete("main.install","Data checked"); reader.join();
    assert(!hwWindowsSplashTaskIsRunning("main.firstframe"));
    // Resolve the complete declared plan, keeping the first frame genuinely pending.
#define HW_STARTUP_TASK(id,label) if(std::string(id)!="main.firstframe") hwWindowsSplashTaskComplete(id,label);
#include "../src/Modern/WindowsStartupTasks.inc"
#undef HW_STARTUP_TASK
    hwWindowsSplashGetSnapshot(&snapshot); assert(snapshot.completed==total-1 && snapshot.percentHundredths<10000);
    hwWindowsSplashTaskBegin("main.firstframe","First menu frame"); assert(hwWindowsSplashTaskIsRunning("main.firstframe"));
    hwWindowsSplashTaskComplete("main.firstframe","Ready"); hwWindowsSplashGetSnapshot(&snapshot);
    assert(snapshot.completed==total && snapshot.percentHundredths==10000);
    hwWindowsSplashProgressStop(); const auto revision=snapshot.revision+1;
    hwWindowsSplashTaskBegin("main.install","No work after startup"); hwWindowsSplashUpdate("No work after startup");
    hwWindowsSplashGetSnapshot(&snapshot); assert(!snapshot.active && snapshot.revision==revision);
    std::puts("PASS: actual checklist percentages, nested tasks/cache/skip/failure, blocked-task stability, thread-safe snapshots and first-frame completion");
}
