#include "FrameWorker.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <vector>

static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main() try {
    hwmodern::FrameWorker ray(L"Test ray preparation"),post(L"Test post processing"),audio(L"Test audio");
    std::thread::id ids[3];
    std::mutex mutex; std::condition_variable ready;
    unsigned entered=0; bool release=false;
    auto overlap=[&](unsigned index){
        ids[index]=std::this_thread::get_id();
        std::unique_lock<std::mutex> lock(mutex);++entered;ready.notify_all();
        require(ready.wait_for(lock,std::chrono::seconds(5),[&]{return release;}),"worker overlap timed out");
    };
    hwmodern::FrameJob rayJob(ray,[&]{overlap(0);});
    hwmodern::FrameJob postJob(post,[&]{overlap(1);});
    hwmodern::FrameJob audioJob(audio,[&]{overlap(2);});
    {std::unique_lock<std::mutex> lock(mutex);
     const bool all=ready.wait_for(lock,std::chrono::seconds(5),[&]{return entered==3;});
     release=true;ready.notify_all();require(all,"independent workers failed to overlap");}
    require(rayJob.wait()&&postJob.wait()&&audioJob.wait(),"worker failed");
    require(ids[0]!=ids[1]&&ids[0]!=ids[2]&&ids[1]!=ids[2],"stages share a CPU thread");
    for(auto id:ids)require(id!=std::this_thread::get_id(),"stage ran on calling thread");
    unsigned generation=0;
    const auto caller=std::this_thread::get_id();
    hwmodern::FrameWorker serial(L"Serial reference",false);
    {hwmodern::FrameJob fallback(serial,[&]{require(std::this_thread::get_id()==caller,"serial fallback");generation=3;});
     require(fallback.wait()&&generation==3,"serial comparison failed");}
    generation=0;
    for(unsigned i=0;i<4000;++i){
        hwmodern::FrameJob frame(ray,[&]{require(std::this_thread::get_id()==ids[0],"per-frame thread recreated");++generation;});
        require(frame.wait()&&generation==i+1,"frame order/completion corrupted");
    }
    {hwmodern::FrameJob failed(post,[]{throw std::runtime_error("expected job failure");});
     require(!failed.wait(),"exception escaped or was lost");}
    {hwmodern::FrameJob recovery(post,[&]{generation=7;});require(recovery.wait()&&generation==7,"worker did not recover");}
    auto earlyReturn=[&]{hwmodern::FrameJob borrowed(audio,[&]{generation=9;});};
    earlyReturn();require(generation==9,"borrowed input outlived its scope");
    ray.stop();
    {hwmodern::FrameJob restart(ray,[&]{generation=10;});require(restart.wait()&&generation==10,"worker restart failed");}
    std::puts("PASS: three concurrent persistent stage threads, 4000 ordered frames, scoped joins, exception isolation and restart");
    return 0;
} catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
