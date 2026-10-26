#ifndef HW_MISSION_LOAD_PROGRESS_H
#define HW_MISSION_LOAD_PROGRESS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    HW_MISSION_LOAD_PHASE_COUNT = 7,
    HW_MISSION_LOAD_TEXT_SIZE = 256,
    HW_MISSION_LOAD_HISTORY_LINES = 4
};

typedef struct HWMissionLoadProgressSnapshot
{
    unsigned int active;
    int currentPhase;
    unsigned int phasesComplete;
    unsigned int totalPhases;
    unsigned int percentHundredths;
    double startedSeconds;
    double taskStartedSeconds;
    char task[HW_MISSION_LOAD_TEXT_SIZE];
    char history[HW_MISSION_LOAD_HISTORY_LINES][HW_MISSION_LOAD_TEXT_SIZE];
    unsigned int completedItems;
    unsigned int totalItems;
} HWMissionLoadProgressSnapshot;

// Main/render thread only. Phase IDs correspond to HorseRace's fixed 0..6
// phases. This data-only module does not own or call the renderer/game/UI.
// Percent measures actual resolved work, never an estimate of remaining time.
void hwMissionLoadProgressReset(double now);
void hwMissionLoadProgressStop(void);
void hwMissionLoadProgressPhaseBegin(int phase, const char *label, double now);
void hwMissionLoadProgressPhaseComplete(int phase, double now);
void hwMissionLoadProgressTask(const char *label, double now);
void hwMissionLoadProgressItems(unsigned int completed, unsigned int total,
    const char *label, double now);
void hwMissionLoadProgressSnapshot(HWMissionLoadProgressSnapshot *snapshot);

#ifdef __cplusplus
}
#endif
#endif
