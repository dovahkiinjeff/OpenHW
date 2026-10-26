#include "MissionLoadProgress.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
struct Phase
{
    bool complete = false;
    unsigned int fraction = 0;
    unsigned int completedItems = 0;
    unsigned int totalItems = 0;
    double taskStartedSeconds = 0;
    char task[HW_MISSION_LOAD_TEXT_SIZE] = {};
};
struct Progress
{
    HWMissionLoadProgressSnapshot snapshot{0, -1, 0, HW_MISSION_LOAD_PHASE_COUNT};
    Phase phases[HW_MISSION_LOAD_PHASE_COUNT];
    double lastSeconds = 0;
} progress;

bool validPhase(int phase) { return phase >= 0 && phase < HW_MISSION_LOAD_PHASE_COUNT; }
double timestamp(double now)
{
    if (std::isfinite(now) && now > progress.lastSeconds) progress.lastSeconds = now;
    return progress.lastSeconds;
}

// Preserve user-visible punctuation (including '%') as data. Whitespace and
// control characters cannot introduce extra ticker lines or escape the row.
void line(char (&destination)[HW_MISSION_LOAD_TEXT_SIZE], const char *source)
{
    char result[HW_MISSION_LOAD_TEXT_SIZE] = {};
    size_t size = 0;
    bool space = false;
    if (source != nullptr)
    {
        for (; *source != '\0' && size + 1 < sizeof(result); ++source)
        {
            const auto character = static_cast<unsigned char>(*source);
            if (character <= 32 || character == 127)
            {
                if (size != 0) space = true;
                continue;
            }
            if (space)
            {
                if (size + 2 >= sizeof(result)) break;
                result[size++] = ' ';
                space = false;
            }
            result[size++] = static_cast<char>(character);
        }
    }
    // Do not leave a partial UTF-8 codepoint at the truncation boundary.
    if (size != 0)
    {
        size_t start = size - 1;
        while (start != 0 && (static_cast<unsigned char>(result[start]) & 0xc0u) == 0x80u) --start;
        const unsigned char lead = static_cast<unsigned char>(result[start]);
        const size_t bytes = lead >= 0xf0u && lead <= 0xf4u ? 4 :
            (lead >= 0xe0u && lead <= 0xefu ? 3 : (lead >= 0xc2u && lead <= 0xdfu ? 2 : 1));
        if (bytes > size - start) size = start;
    }
    result[size] = '\0';
    std::memcpy(destination, result, sizeof(result));
}

void history(const char *task)
{
    if (task == nullptr || task[0] == '\0' || std::strcmp(task, progress.snapshot.history[0]) == 0) return;
    for (unsigned int i = HW_MISSION_LOAD_HISTORY_LINES - 1; i != 0; --i)
        std::memcpy(progress.snapshot.history[i], progress.snapshot.history[i - 1], HW_MISSION_LOAD_TEXT_SIZE);
    line(progress.snapshot.history[0], task);
}

void refresh()
{
    auto &snapshot = progress.snapshot;
    unsigned long long fraction = 0;
    snapshot.phasesComplete = 0;
    for (const auto &phase : progress.phases)
    {
        if (phase.complete) ++snapshot.phasesComplete;
        fraction += phase.complete ? 10000u : phase.fraction;
    }
    snapshot.totalPhases = HW_MISSION_LOAD_PHASE_COUNT;
    snapshot.percentHundredths = snapshot.phasesComplete == HW_MISSION_LOAD_PHASE_COUNT ? 10000u :
        std::min(9999u, static_cast<unsigned int>(fraction / HW_MISSION_LOAD_PHASE_COUNT));
    if (validPhase(snapshot.currentPhase))
    {
        const auto &phase = progress.phases[snapshot.currentPhase];
        std::memcpy(snapshot.task, phase.task, HW_MISSION_LOAD_TEXT_SIZE);
        snapshot.taskStartedSeconds = phase.taskStartedSeconds;
        snapshot.completedItems = phase.completedItems;
        snapshot.totalItems = phase.totalItems;
    }
}

void changeTask(Phase &phase, const char *label, double now)
{
    char sanitized[HW_MISSION_LOAD_TEXT_SIZE] = {};
    line(sanitized, label);
    if (sanitized[0] == '\0' || std::strcmp(phase.task, sanitized) == 0) return;
    history(phase.task);
    std::memcpy(phase.task, sanitized, sizeof(sanitized));
    phase.taskStartedSeconds = now;
}
}

extern "C" void hwMissionLoadProgressReset(double now)
{
    progress = {};
    if (std::isfinite(now) && now > 0) progress.lastSeconds = now;
    progress.snapshot.active = 1;
    progress.snapshot.currentPhase = -1;
    progress.snapshot.totalPhases = HW_MISSION_LOAD_PHASE_COUNT;
    progress.snapshot.startedSeconds = progress.snapshot.taskStartedSeconds = progress.lastSeconds;
}

extern "C" void hwMissionLoadProgressStop(void) { progress.snapshot.active = 0; }

extern "C" void hwMissionLoadProgressPhaseBegin(int phaseId, const char *label, double now)
{
    if (!progress.snapshot.active || !validPhase(phaseId)) return;
    const double time = timestamp(now);
    auto &snapshot = progress.snapshot;
    if (snapshot.currentPhase != phaseId)
    {
        history(snapshot.task);
        snapshot.currentPhase = phaseId;
    }
    auto &phase = progress.phases[phaseId];
    if (!phase.complete)
    {
        if (phase.task[0] == '\0') phase.taskStartedSeconds = time;
        changeTask(phase, label, time);
    }
    refresh();
}

extern "C" void hwMissionLoadProgressPhaseComplete(int phaseId, double now)
{
    if (!progress.snapshot.active || !validPhase(phaseId)) return;
    auto &phase = progress.phases[phaseId];
    if (phase.complete) return;
    const double time = timestamp(now);
    if (phase.task[0] == '\0') phase.taskStartedSeconds = time;
    phase.complete = true;
    phase.fraction = 10000;
    phase.completedItems = phase.totalItems;
    history(phase.task);
    refresh();
}

extern "C" void hwMissionLoadProgressTask(const char *label, double now)
{
    if (!progress.snapshot.active || !validPhase(progress.snapshot.currentPhase)) return;
    auto &phase = progress.phases[progress.snapshot.currentPhase];
    if (phase.complete) return;
    changeTask(phase, label, timestamp(now));
    refresh();
}

extern "C" void hwMissionLoadProgressItems(unsigned int completed, unsigned int total,
    const char *label, double now)
{
    if (!progress.snapshot.active || !validPhase(progress.snapshot.currentPhase)) return;
    auto &phase = progress.phases[progress.snapshot.currentPhase];
    if (phase.complete) return;
    changeTask(phase, label, timestamp(now));
    phase.totalItems = total;
    phase.completedItems = std::min(completed, total);
    if (total != 0)
    {
        const auto fraction = static_cast<unsigned int>(10000ull * phase.completedItems / total);
        phase.fraction = std::max(phase.fraction, std::min(9999u, fraction));
    }
    refresh();
}

extern "C" void hwMissionLoadProgressSnapshot(HWMissionLoadProgressSnapshot *snapshot)
{ if (snapshot != nullptr) *snapshot = progress.snapshot; }
