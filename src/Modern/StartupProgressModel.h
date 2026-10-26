#pragma once
#include "WindowsSplash.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace hwmodern {
// A checklist percentage measures completed requirements, not elapsed time.
// Optional requirements are explicitly resolved as skipped; a blocked compiler
// never advances the count merely because the UI continues repainting.
class StartupProgressModel {
    enum class State { Pending, Running, Complete, Skipped };
    struct Task { std::string id, label; State state = State::Pending; double began = 0; };
    std::vector<Task> tasks_;
    std::vector<size_t> stack_;
    HWWindowsSplashSnapshot snapshot_{};
    static void copy(char *to, const std::string &from) {
        const size_t size = std::min(from.size(), size_t(HW_SPLASH_TEXT_SIZE - 1));
        std::memcpy(to, from.data(), size); to[size] = 0;
    }
    size_t find(const char *id) const {
        if (id) for (size_t i=0;i<tasks_.size();++i) if(tasks_[i].id==id) return i;
        return tasks_.size();
    }
    void history(const std::string &text) {
        if (text.empty() || text == snapshot_.history[0]) return;
        for (unsigned i=HW_SPLASH_HISTORY_LINES-1;i>0;--i)
            std::memcpy(snapshot_.history[i],snapshot_.history[i-1],HW_SPLASH_TEXT_SIZE);
        copy(snapshot_.history[0],text);
    }
    void refresh(double now) {
        snapshot_.completed = snapshot_.skipped = 0;
        for (const auto &task:tasks_) {
            if(task.state==State::Complete || task.state==State::Skipped) ++snapshot_.completed;
            if(task.state==State::Skipped) ++snapshot_.skipped;
        }
        snapshot_.total = static_cast<unsigned>(tasks_.size());
        snapshot_.percentHundredths = snapshot_.total
            ? static_cast<unsigned>(10000ull*snapshot_.completed/snapshot_.total) : 0;
        if (!stack_.empty()) {
            const auto &task=tasks_[stack_.back()];
            copy(snapshot_.task,task.label); snapshot_.taskStartedSeconds=task.began;
        } else {
            copy(snapshot_.task,snapshot_.completed==snapshot_.total ? "Startup complete" : "Preparing next startup task");
            snapshot_.taskStartedSeconds=now;
        }
        ++snapshot_.revision;
    }
public:
    void reset(double now) {
        tasks_.clear(); stack_.clear(); snapshot_={}; snapshot_.active=1;
        snapshot_.startedSeconds=snapshot_.taskStartedSeconds=now;
        copy(snapshot_.task,"Preparing startup checklist");
    }
    void plan(const char *id,const char *label) {
        if(!id || !id[0] || find(id)!=tasks_.size()) return;
        tasks_.push_back({id,label ? label : id,State::Pending,0});
        snapshot_.total=static_cast<unsigned>(tasks_.size());
    }
    bool begin(const char *id,const char *label,double now) {
        if(!snapshot_.active || snapshot_.failed) return false;
        const auto index=find(id); if(index==tasks_.size()) return false;
        auto &task=tasks_[index];
        if(task.state==State::Complete || task.state==State::Skipped) return false;
        if(task.state==State::Running) return true;
        if(label && label[0]) task.label=label;
        task.state=State::Running; task.began=now; stack_.push_back(index);
        copy(snapshot_.detail,task.label); history(task.label); refresh(now); return true;
    }
    bool finish(const char *id,const char *detail,bool skip,double now) {
        if(!snapshot_.active || snapshot_.failed) return false;
        const auto index=find(id); if(index==tasks_.size()) return false;
        auto &task=tasks_[index];
        if(task.state==State::Complete || task.state==State::Skipped) return false;
        task.state=skip?State::Skipped:State::Complete;
        stack_.erase(std::remove(stack_.begin(),stack_.end(),index),stack_.end());
        const std::string message=detail && detail[0] ? detail :
            task.label+(skip?" / not required":" / complete");
        copy(snapshot_.detail,message); history(message); refresh(now); return true;
    }
    void skipPending(const char *prefix,const char *reason,double now) {
        if(!snapshot_.active || snapshot_.failed || !prefix) return;
        bool changed=false;
        for(auto &task:tasks_) if(task.state==State::Pending && task.id.compare(0,std::strlen(prefix),prefix)==0) {
            task.state=State::Skipped; changed=true;
        }
        if(changed) { history(reason ? reason : "Optional startup checks not required"); refresh(now); }
    }
    void detail(const char *text) {
        if(!snapshot_.active || snapshot_.failed || !text || !text[0] || text==std::string(snapshot_.detail)) return;
        copy(snapshot_.detail,text); history(text); ++snapshot_.revision;
    }
    void fail(const char *text) {
        if(!snapshot_.active) return;
        snapshot_.failed=1; copy(snapshot_.detail,text?text:"Startup failed");
        history(snapshot_.detail); ++snapshot_.revision;
    }
    void stop() { snapshot_.active=0; ++snapshot_.revision; }
    const HWWindowsSplashSnapshot &snapshot() const { return snapshot_; }
    bool running(const char *id) const {
        const auto index=find(id);
        return snapshot_.active && !snapshot_.failed && index<tasks_.size() && tasks_[index].state==State::Running;
    }
};
}
