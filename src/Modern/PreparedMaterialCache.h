#pragma once
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <windows.h>
namespace hwmodern {
// Exact immutable mip bytes, backed by a disposable file whose OS cache can be
// reclaimed. A bounded writer owns normal CPU ScratchImage storage; it never
// reads write-combined GPU upload memory or adds a write wait to a game frame.
class PreparedMaterialCache {
public:
    struct Range { std::uint64_t offset,bytes,destination; };
    struct Stats { std::uint64_t hits=0,stores=0,readBytes=0,writeBytes=0,failures=0; };
    explicit PreparedMaterialCache(std::uint64_t capacity=8ull<<30,std::uint64_t pending=512ull<<20)
        : capacity_(capacity),pendingLimit_(pending) {}
    ~PreparedMaterialCache(){clear();}
    PreparedMaterialCache(const PreparedMaterialCache&)=delete;
    PreparedMaterialCache& operator=(const PreparedMaterialCache&)=delete;
    bool read(std::size_t index,std::uint64_t key,void* target,std::uint64_t targetBytes,const std::vector<Range>& ranges){
        Entry entry;
        {std::lock_guard<std::mutex> guard(lock_);
         if(!target||index>=entries_.size()||entries_[index].key!=key||!entries_[index].bytes)return false;
         entry=entries_[index];}
        for(const auto&r:ranges)if(r.offset>entry.bytes||r.bytes>entry.bytes-r.offset||r.destination>targetBytes||r.bytes>targetBytes-r.destination)return false;
        std::uint64_t total=0;
        // Coalesce tight file rows before scattering into padded GPU rows.
        // Non-power-of-two textures must not issue one file read per row.
        std::vector<unsigned char> scratch;
        for(std::size_t first=0;first<ranges.size();){
            std::size_t end=first+1;
            while(end<ranges.size()&&ranges[end-1].offset+ranges[end-1].bytes==ranges[end].offset)++end;
            const auto groupBytes=ranges[end-1].offset+ranges[end-1].bytes-ranges[first].offset;
            bool ok=true;
            if(end==first+1){const auto&r=ranges[first];
                ok=transfer(false,entry.offset+r.offset,static_cast<unsigned char*>(target)+r.destination,r.bytes);
            }else{
                scratch.resize(static_cast<std::size_t>(std::min<std::uint64_t>(groupBytes,1ull<<20)));
                std::size_t rangeIndex=first;std::uint64_t consumed=0;
                for(std::uint64_t offset=0;offset<groupBytes&&ok;){
                    const auto chunk=std::min<std::uint64_t>(groupBytes-offset,scratch.size());
                    ok=transfer(false,entry.offset+ranges[first].offset+offset,scratch.data(),chunk);
                    if(!ok)break;
                    std::uint64_t position=0;
                    while(position<chunk){const auto&r=ranges[rangeIndex];
                        const auto copy=std::min<std::uint64_t>(r.bytes-consumed,chunk-position);
                        std::memcpy(static_cast<unsigned char*>(target)+r.destination+consumed,scratch.data()+position,static_cast<std::size_t>(copy));
                        position+=copy;consumed+=copy;
                        if(consumed==r.bytes){++rangeIndex;consumed=0;}
                    }
                    offset+=chunk;
                }
            }
            if(!ok){std::lock_guard<std::mutex> guard(lock_);entries_[index].bytes=0;++stats_.failures;return false;}
            total+=groupBytes;first=end;
        }
        std::lock_guard<std::mutex> guard(lock_);++stats_.hits;stats_.readBytes+=total;return true;
    }
    bool queue(std::size_t index,std::uint64_t key,std::shared_ptr<const void> owner,const void* data,std::uint64_t bytes){
        if(!owner||!data||!bytes)return false;
        std::lock_guard<std::mutex> guard(lock_);
        if(index<entries_.size()&&entries_[index].key==key&&entries_[index].bytes==bytes)return true;
        if(stopping_||bytes>capacity_||used_>capacity_-bytes||bytes>pendingLimit_||pendingBytes_>pendingLimit_-bytes)return false;
        if(activeIndex_==index&&activeKey_==key)return true;
        for(const auto&j:jobs_)if(j.index==index&&j.key==key)return true;
        try{if(!worker_.joinable())worker_=std::thread([this]{run();});jobs_.push_back({index,key,std::move(owner),data,bytes});}
        catch(...){++stats_.failures;return false;}
        pendingBytes_+=bytes;changed_.notify_one();return true;
    }
    void waitPending(){std::unique_lock<std::mutex> guard(lock_);changed_.wait(guard,[this]{return pendingBytes_==0;});} // CPU tests only
    Stats stats() const{std::lock_guard<std::mutex> guard(lock_);return stats_;}
    std::uint64_t storedBytes()const{std::lock_guard<std::mutex> guard(lock_);return used_;}
    void clear()noexcept{
        {std::lock_guard<std::mutex> guard(lock_);stopping_=true;changed_.notify_all();}
        if(worker_.joinable())worker_.join();
        if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);
        std::lock_guard<std::mutex> guard(lock_);file_=INVALID_HANDLE_VALUE;jobs_.clear();entries_.clear();
        used_=pendingBytes_=0;attempted_=stopping_=false;activeIndex_=~std::size_t(0);stats_={};
    }
private:
    struct Entry{std::uint64_t offset=0,bytes=0,key=0;};
    struct Job{std::size_t index;std::uint64_t key;std::shared_ptr<const void> owner;const void* data;std::uint64_t bytes;};
    bool open(){ // lock held, handle stable until worker joined at clear()
        if(file_!=INVALID_HANDLE_VALUE)return true;
        if(attempted_)return false;attempted_=true;
        wchar_t directory[MAX_PATH+1]={},name[MAX_PATH+1]={};DWORD count=GetTempPathW(MAX_PATH,directory);
        if(!count||count>=MAX_PATH||!GetTempFileNameW(directory,L"HWM",0,name))return false;
        file_=CreateFileW(name,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
            FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_RANDOM_ACCESS|FILE_FLAG_OVERLAPPED,nullptr);
        if(file_==INVALID_HANDLE_VALUE){DeleteFileW(name);return false;}return true;
    }
    bool transfer(bool write,std::uint64_t offset,void* data,std::uint64_t bytes){
        HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event)return false;
        bool success=true;auto* cursor=static_cast<unsigned char*>(data);
        while(bytes){OVERLAPPED io={};io.Offset=static_cast<DWORD>(offset);io.OffsetHigh=static_cast<DWORD>(offset>>32);io.hEvent=event;ResetEvent(event);
            DWORD count=static_cast<DWORD>(std::min<std::uint64_t>(bytes,16ull<<20)),completed=0;
            BOOL ok=write?WriteFile(file_,cursor,count,&completed,&io):ReadFile(file_,cursor,count,&completed,&io);
            if(!ok&&GetLastError()==ERROR_IO_PENDING)ok=GetOverlappedResult(file_,&io,&completed,TRUE);
            if(!ok||completed!=count){success=false;break;}offset+=completed;cursor+=completed;bytes-=completed;
        }CloseHandle(event);return success;
    }
    bool store(const Job&job){
        std::uint64_t offset;
        {std::lock_guard<std::mutex> guard(lock_);if(job.bytes>capacity_||used_>capacity_-job.bytes||!open())return false;
         offset=used_;used_+=job.bytes;if(entries_.size()<=job.index)entries_.resize(job.index+1);}
        if(!transfer(true,offset,const_cast<void*>(job.data),job.bytes))return false;
        std::lock_guard<std::mutex> guard(lock_);entries_[job.index]={offset,job.bytes,job.key};++stats_.stores;stats_.writeBytes+=job.bytes;return true;
    }
    void run()noexcept{
        for(;;){Job job;
            {std::unique_lock<std::mutex> guard(lock_);changed_.wait(guard,[this]{return stopping_||!jobs_.empty();});if(stopping_)return;
             job=std::move(jobs_.front());jobs_.pop_front();activeIndex_=job.index;activeKey_=job.key;}
            bool ok=false;try{ok=store(job);}catch(...){}
            job.owner.reset();std::lock_guard<std::mutex> guard(lock_);if(!ok)++stats_.failures;
            pendingBytes_-=job.bytes;activeIndex_=~std::size_t(0);changed_.notify_all();
        }
    }
    const std::uint64_t capacity_,pendingLimit_;
    std::uint64_t used_=0,pendingBytes_=0,activeKey_=0;
    std::size_t activeIndex_=~std::size_t(0);
    HANDLE file_=INVALID_HANDLE_VALUE;
    bool attempted_=false,stopping_=false;
    Stats stats_;
    std::vector<Entry> entries_;
    std::deque<Job> jobs_;
    std::thread worker_;
    mutable std::mutex lock_;
    std::condition_variable changed_;
};
}
