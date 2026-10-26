#pragma once
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace hwmodern {
// One persistent owner, one borrowed job at a time. No per-frame queue or
// thread allocation. The caller must join before changing borrowed inputs.
class FrameWorker {
public:
    using Function = void (*)(void *);
    explicit FrameWorker(const wchar_t *name,bool threaded=true) : name_(name),threaded_(threaded) {}
    FrameWorker(const FrameWorker &) = delete;
    FrameWorker &operator=(const FrameWorker &) = delete;
    ~FrameWorker() { stop(); }

    void dispatch(Function function, void *context) {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_.wait(lock,[this]{return !busy_;});
        failure_ = nullptr;
        if (!threaded_) {
            lock.unlock();
            try { function(context); }
            catch (...) { lock.lock(); failure_=std::current_exception(); }
            return;
        }
        if (!thread_.joinable()) {
            try { thread_ = std::thread([this]{ run(); }); }
            catch (...) {
                // Resource exhaustion must preserve a valid serial frame.
                lock.unlock();
                try { function(context); }
                catch (...) { lock.lock(); failure_=std::current_exception(); }
                return;
            }
        }
        function_=function; context_=context; busy_=true;
        ready_.notify_one();
    }
    bool wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_.wait(lock,[this]{return !busy_;});
        return !failure_;
    }
    void stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_.wait(lock,[this]{return !busy_;});
        if (!thread_.joinable()) return;
        stopping_=true; ready_.notify_one();
        lock.unlock(); thread_.join(); lock.lock(); stopping_=false;
    }
private:
    void run() {
#if defined(_WIN32)
        using SetName = HRESULT (WINAPI *)(HANDLE,PCWSTR);
        auto setName=reinterpret_cast<SetName>(GetProcAddress(GetModuleHandleW(L"Kernel32.dll"),"SetThreadDescription"));
        if (setName) setName(GetCurrentThread(),name_);
#endif
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            ready_.wait(lock,[this]{return busy_ || stopping_;});
            if (stopping_) return;
            auto function=function_; auto context=context_;
            lock.unlock();
            std::exception_ptr failure;
            try { function(context); } catch (...) { failure=std::current_exception(); }
            lock.lock();
            failure_=failure; function_=nullptr; context_=nullptr; busy_=false;
            finished_.notify_all();
        }
    }
    const wchar_t *name_;
    bool threaded_;
    std::mutex mutex_;
    std::condition_variable ready_,finished_;
    std::thread thread_;
    Function function_=nullptr;
    void *context_=nullptr;
    std::exception_ptr failure_;
    bool busy_=false,stopping_=false;
};

template<class Function> class FrameJob {
public:
    FrameJob(FrameWorker &worker,Function function) : worker_(worker),function_(std::move(function)) {
        worker_.dispatch([](void *context){static_cast<FrameJob *>(context)->function_();},this);
    }
    FrameJob(const FrameJob &)=delete;
    FrameJob &operator=(const FrameJob &)=delete;
    ~FrameJob(){ wait(); }
    bool wait(){ if (!joined_) { success_=worker_.wait(); joined_=true; } return success_; }
private:
    FrameWorker &worker_;
    Function function_;
    bool joined_=false,success_=true;
};
template<class Function> FrameJob(FrameWorker &,Function)->FrameJob<Function>;
}
