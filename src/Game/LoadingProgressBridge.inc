void HorseRaceLoadingPhaseBegin(int phase, const char *task)
{
    if (hrRunning)
        hwMissionLoadProgressPhaseBegin(phase, task, hrLoadingSeconds());
}

void HorseRaceLoadingPhaseComplete(int phase)
{
    if (hrRunning)
        hwMissionLoadProgressPhaseComplete(phase, hrLoadingSeconds());
}

void HorseRaceLoadingTask(const char *task)
{
    if (hrRunning)
        hwMissionLoadProgressTask(task, hrLoadingSeconds());
}

void HorseRaceLoadingProgress(unsigned int completed, unsigned int total, const char *task)
{
    if (hrRunning)
        hwMissionLoadProgressItems(completed, total, task, hrLoadingSeconds());
}

