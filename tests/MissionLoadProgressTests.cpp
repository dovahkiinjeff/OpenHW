#include "../src/Modern/MissionLoadProgress.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace
{
unsigned int checks = 0;
void require(bool ok, const char *message)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
HWMissionLoadProgressSnapshot snapshot()
{
    HWMissionLoadProgressSnapshot value{};
    hwMissionLoadProgressSnapshot(&value);
    return value;
}
bool equal(const char *a, const char *b) { return std::strcmp(a, b) == 0; }

void normalAndPartial()
{
    hwMissionLoadProgressReset(100);
    auto value = snapshot();
    require(value.active && value.currentPhase == -1 && value.totalPhases == 7 &&
        value.phasesComplete == 0 && value.percentHundredths == 0 && value.startedSeconds == 100,
        "reset declares exactly seven unresolved phases");
    for (int phase = 0; phase < 7; ++phase)
    {
        hwMissionLoadProgressPhaseBegin(phase, "Loading phase", 101 + phase * 10);
        value = snapshot();
        require(value.currentPhase == phase && value.phasesComplete == static_cast<unsigned>(phase),
            "phase begin does not auto-complete another phase");
        hwMissionLoadProgressItems(1, 2, "Loading current item", 102 + phase * 10);
        const auto partial = snapshot();
        require(partial.completedItems == 1 && partial.totalItems == 2 &&
            partial.percentHundredths == (static_cast<unsigned>(phase) * 10000u + 5000u) / 7,
            "phase item fraction contributes one seventh of real work");
        hwMissionLoadProgressItems(2, 2, "Loading current item", 103 + phase * 10);
        value = snapshot();
        require(value.phasesComplete == static_cast<unsigned>(phase) && value.percentHundredths < 10000,
            "item count alone cannot claim completed phases or 100 percent");
        hwMissionLoadProgressPhaseComplete(phase, 104 + phase * 10);
        value = snapshot();
        require(value.phasesComplete == static_cast<unsigned>(phase + 1) &&
            value.percentHundredths == static_cast<unsigned>((phase + 1) * 10000 / 7),
            "explicit completion resolves actual phase work");
        hwMissionLoadProgressPhaseComplete(phase, 105 + phase * 10);
        require(snapshot().percentHundredths == value.percentHundredths &&
            snapshot().phasesComplete == value.phasesComplete, "repeated phase completion is idempotent");
    }
    require(snapshot().percentHundredths == 10000 && snapshot().phasesComplete == 7,
        "all seven explicit completions produce 100 percent");
    hwMissionLoadProgressStop();
    require(!snapshot().active && snapshot().percentHundredths == 10000,
        "successful stop retains truthful completed percentage");
}

void monotonicAndOrder()
{
    hwMissionLoadProgressReset(10);
    hwMissionLoadProgressPhaseBegin(0, "Prepare", 11);
    hwMissionLoadProgressItems(90, 100, "Assets", 12);
    const auto earned = snapshot().percentHundredths;
    hwMissionLoadProgressItems(1, 1000, "Expanded asset list", 13);
    require(snapshot().percentHundredths == earned && snapshot().completedItems == 1 && snapshot().totalItems == 1000,
        "new item denominator keeps earned progress without falsifying current counts");
    hwMissionLoadProgressItems(100, 0, "Unknown count", 14);
    require(snapshot().percentHundredths == earned && snapshot().completedItems == 0 && snapshot().totalItems == 0,
        "zero denominator is unknown and never divides or regresses");
    hwMissionLoadProgressItems(std::numeric_limits<unsigned>::max(), 3, "Clamped", 15);
    require(snapshot().completedItems == 3 && snapshot().totalItems == 3 && snapshot().percentHundredths < 10000,
        "completed count clamps to total without overflow");
    hwMissionLoadProgressItems(std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max(), "Huge", 16);
    require(snapshot().percentHundredths == 1428 && snapshot().completedItems == std::numeric_limits<unsigned>::max(),
        "maximum counters retain correct arithmetic");
    hwMissionLoadProgressPhaseBegin(5, "Upload before prepare finished", 17);
    require(snapshot().phasesComplete == 0 && snapshot().currentPhase == 5 && snapshot().percentHundredths == 1428,
        "out-of-order begin retains unresolved previous phase");
    hwMissionLoadProgressPhaseComplete(0, 18);
    require(snapshot().phasesComplete == 1 && snapshot().currentPhase == 5,
        "out-of-order completion resolves only requested phase");
    hwMissionLoadProgressPhaseBegin(0, "Reusing completed cache", 19);
    const auto reused = snapshot();
    require(reused.currentPhase == 0 && reused.percentHundredths == 1428 && reused.phasesComplete == 1,
        "re-entering complete phase preserves progress and correct current identity");
    hwMissionLoadProgressItems(0, 9000, "Must not modify completed phase", 20);
    hwMissionLoadProgressTask("Must not restart completed phase", 21);
    require(equal(snapshot().task, reused.task) && snapshot().taskStartedSeconds == reused.taskStartedSeconds &&
        snapshot().percentHundredths == reused.percentHundredths && snapshot().totalItems == reused.totalItems,
        "completed cache reuse ignores new task/item mutations");
    const auto valid = snapshot();
    for (int phase : {-1, 7, 99, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
    {
        hwMissionLoadProgressPhaseBegin(phase, "Invalid", 22);
        hwMissionLoadProgressPhaseComplete(phase, 23);
        require(snapshot().currentPhase == valid.currentPhase && snapshot().phasesComplete == valid.phasesComplete &&
            snapshot().percentHundredths == valid.percentHundredths, "invalid phase IDs are no-ops");
    }
    hwMissionLoadProgressReset(30);
    for (int phase = 0; phase < 7; ++phase)
    {
        hwMissionLoadProgressPhaseBegin(phase, "Items finished, phase unresolved", 31 + phase);
        hwMissionLoadProgressItems(1, 1, nullptr, 31 + phase);
    }
    require(snapshot().percentHundredths == 9999 && snapshot().phasesComplete == 0,
        "all item fractions remain below 100 until all phases explicitly complete");
    for (int phase = 6; phase >= 0; --phase)
    {
        hwMissionLoadProgressPhaseComplete(phase, 40 + (6 - phase));
        require((snapshot().percentHundredths == 10000) == (phase == 0),
            "arbitrary completion order reaches 100 only on final required phase");
    }
}

void textAndTimes()
{
    hwMissionLoadProgressReset(100);
    hwMissionLoadProgressPhaseBegin(0, "Prepare", 101);
    hwMissionLoadProgressTask("Loading ship mesh", 105);
    for (int i = 0; i < 10; ++i)
    {
        hwMissionLoadProgressTask("Loading ship mesh", 106 + i);
        hwMissionLoadProgressItems(static_cast<unsigned>(i), 20, "Loading ship mesh", 106 + i);
        require(snapshot().taskStartedSeconds == 105, "repeated task/item refresh never resets task elapsed");
    }
    hwMissionLoadProgressTask("  File\r\nname\t 100%\001 ready  ", 120);
    require(equal(snapshot().task, "File name 100% ready") && snapshot().taskStartedSeconds == 120,
        "labels are single-line sanitized data, including literal percent");
    require(equal(snapshot().history[0], "Loading ship mesh"), "task change appends previous meaningful task");
    char caller[] = "Caller-owned filename";
    hwMissionLoadProgressTask(caller, 121); caller[0] = 'X';
    require(equal(snapshot().task, "Caller-owned filename"), "task label copied immediately");
    hwMissionLoadProgressTask(nullptr, 122); hwMissionLoadProgressTask("\r\n\t ", 123);
    require(equal(snapshot().task, "Caller-owned filename") && snapshot().taskStartedSeconds == 121,
        "empty labels do not clear or restart meaningful task");
    std::string longText(600, 'z');
    hwMissionLoadProgressTask(longText.c_str(), 124);
    require(std::strlen(snapshot().task) == 255 && snapshot().task[255] == 0, "long label bounded and terminated");
    std::string unicode(254, 'a'); unicode += "\xc3\xa9";
    hwMissionLoadProgressTask(unicode.c_str(), 125);
    require(std::strlen(snapshot().task) == 254, "UTF-8 truncation does not split final codepoint");
    for (const char *task : {"one", "two", "three", "four", "five"}) hwMissionLoadProgressTask(task, 126);
    auto value = snapshot();
    require(equal(value.task, "five") && equal(value.history[0], "four") && equal(value.history[1], "three") &&
        equal(value.history[2], "two") && equal(value.history[3], "one"), "ticker retains only four recent previous tasks");
    hwMissionLoadProgressPhaseComplete(0, 127);
    require(equal(snapshot().history[0], "five"), "phase completion appends completed task");
    hwMissionLoadProgressPhaseComplete(0, 128);
    require(equal(snapshot().history[1], "four"), "idempotent completion does not duplicate history");

    hwMissionLoadProgressPhaseBegin(1, "Next", 130);
    hwMissionLoadProgressTask("Earlier clock", 1);
    require(snapshot().taskStartedSeconds == 130, "backward clock cannot regress task timestamp");
    hwMissionLoadProgressTask("Nonfinite clock", std::numeric_limits<double>::quiet_NaN());
    require(snapshot().taskStartedSeconds == 130, "NaN timestamp uses last finite clock");
    hwMissionLoadProgressTask("Infinite clock", std::numeric_limits<double>::infinity());
    require(snapshot().taskStartedSeconds == 130, "infinite timestamp does not poison timer");
    hwMissionLoadProgressReset(std::numeric_limits<double>::quiet_NaN());
    require(snapshot().startedSeconds == 0 && snapshot().taskStartedSeconds == 0, "invalid reset time sanitized");
    hwMissionLoadProgressReset(-1);
    require(snapshot().startedSeconds == 0, "negative reset time sanitized");
}

void cancellationAndReuse()
{
    hwMissionLoadProgressReset(10);
    hwMissionLoadProgressTask("No phase yet", 11);
    hwMissionLoadProgressItems(1, 1, "No phase yet", 12);
    require(snapshot().currentPhase == -1 && snapshot().task[0] == 0 && snapshot().percentHundredths == 0,
        "item/task work needs a valid current phase");
    hwMissionLoadProgressPhaseBegin(0, "Started", 13);
    hwMissionLoadProgressItems(1, 2, "Partway", 14);
    hwMissionLoadProgressStop();
    const auto stopped = snapshot();
    require(!stopped.active && stopped.percentHundredths == 714, "abort Stop does not pretend 100 percent");
    hwMissionLoadProgressStop();
    hwMissionLoadProgressPhaseBegin(1, "Ignored", 15);
    hwMissionLoadProgressPhaseComplete(0, 16);
    hwMissionLoadProgressTask("Ignored", 17);
    hwMissionLoadProgressItems(2, 2, "Ignored", 18);
    const auto ignored = snapshot();
    require(ignored.currentPhase == stopped.currentPhase && ignored.percentHundredths == stopped.percentHundredths &&
        ignored.phasesComplete == stopped.phasesComplete && equal(ignored.task, stopped.task),
        "all updates after Stop are cheap no-ops");
    for (int load = 0; load < 3; ++load)
    {
        hwMissionLoadProgressReset(100 + load);
        auto value = snapshot();
        require(value.active && value.currentPhase == -1 && value.phasesComplete == 0 &&
            value.completedItems == 0 && value.totalItems == 0 && value.task[0] == 0 && value.history[0][0] == 0,
            "each load reset clears previous counts, history and tasks");
        for (int phase = 0; phase < 7; ++phase) hwMissionLoadProgressPhaseComplete(phase, 200 + load);
        require(snapshot().percentHundredths == 10000 && snapshot().phasesComplete == 7,
            "cached requirements may be explicitly completed without item loops");
        hwMissionLoadProgressPhaseBegin(2, "Previously completed cache", 250 + load);
        require(snapshot().currentPhase == 2 && snapshot().taskStartedSeconds == 200 + load &&
            snapshot().percentHundredths == 10000, "cached phase reuse retains finite completion timestamp");
        hwMissionLoadProgressStop();
    }
    hwMissionLoadProgressSnapshot(nullptr);
}
}

int main()
{
    require(!snapshot().active && snapshot().currentPhase == -1 && snapshot().totalPhases == 7,
        "inactive initial snapshot still declares fixed phase count");
    normalAndPartial(); monotonicAndOrder(); textAndTimes(); cancellationAndReuse();
    std::printf("Mission load progress CPU tests passed: %u runtime checks.\n", checks);
}
