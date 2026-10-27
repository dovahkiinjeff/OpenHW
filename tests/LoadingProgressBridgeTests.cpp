#include "../src/Modern/MissionLoadProgress.h"
#include <cstdio>
#include <cstdlib>
static bool hrRunning,singlePlayerGame,multiPlayerGame;
static double now;
static double hrLoadingSeconds(){return now;}
#include "../src/Game/LoadingProgressBridge.inc"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main()
{
    HWMissionLoadProgressSnapshot value{};
    for(int mode=0;mode<3;++mode)
    {
        singlePlayerGame=mode==0;multiPlayerGame=mode==2;hrRunning=true;now=100;
        hwMissionLoadProgressReset(now);
        for(int phase=0;phase<7;++phase)
        {
            ++now;HorseRaceLoadingPhaseBegin(phase,"Loading fleet");
            HorseRaceLoadingTask("Loading ship");HorseRaceLoadingProgress(3,8,"Preparing texture");
            hwMissionLoadProgressSnapshot(&value);
            CHECK(value.currentPhase==phase && value.completedItems==3 && value.totalItems==8);
            ++now;HorseRaceLoadingPhaseComplete(phase);
        }
        hwMissionLoadProgressSnapshot(&value);CHECK(value.percentHundredths==10000 && value.phasesComplete==7);
        hrRunning=false;hwMissionLoadProgressStop();
        HorseRaceLoadingPhaseBegin(0,"Not a load");hwMissionLoadProgressSnapshot(&value);CHECK(!value.active);
    }
    std::puts("Story, skirmish and network loading bridges passed.");return 0;
}
