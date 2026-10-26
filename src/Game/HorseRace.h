/****************************************

    Name: HorseRace.h

    Defines for begining game horse Race

*****************************************/
#ifndef ___HORSERACE_H
#define ___HORSERACE_H

#include "CommandNetwork.h"
#include "MaxMultiplayer.h"

/*=============================================================================
    defines:
=============================================================================*/

#ifdef _WIN32
#define HR_FIBFile          "FEMAN\\Horse_Race.fib"
#else
#define HR_FIBFile          "FEMAN/Horse_Race.fib"
#endif
#define HR_RaceScreen       "Horse_Race"
#define HR_RaceScreenNotNetwork "Horse_Race_NonNetwork"
#define HR_SingleRaceScreen "Horse_Race_Single"

#define HR_BackBarColor colRGB(75,75,75)
#define HR_ChatTextColor colRGB(0,200,256)

#define DOWNLOADMAP_BAR         0
#define UNIVERSE_BAR            1
#define ETG_BAR                 2
#define TEXTURE1_BAR            3
#define TEXTURE2_BAR            4

/*=============================================================================
    structures:
=============================================================================*/

#define MAX_HRSTATUS_STRING     100

typedef struct
{
    udword barnum[MAX_MULTIPLAYER_PLAYERS];
    real32 percent[MAX_MULTIPLAYER_PLAYERS];
    char hrstatusstr[MAX_MULTIPLAYER_PLAYERS][MAX_HRSTATUS_STRING];
}HorseStatus;

#define MAX_POSSIBLE_NUM_BARS 20
typedef struct HorseRaceBars
{
    sdword numBars;
    real32 perc[MAX_POSSIBLE_NUM_BARS];
} HorseRaceBars;

/*=============================================================================
    data:
=============================================================================*/

extern bool32 hrRunning;
extern bool32 hrBackgroundReinit;

extern HorseStatus horseracestatus;

/*=============================================================================
    Prototypes:
=============================================================================*/
void horseRaceInit(void);
void horseRaceShutdown(void);

void hrProcessPacket(ChatPacket *packet);

void recievedHorsePacketCB(ubyte *packet,udword sizeofpacket);

void HorseRaceBeginBar(uword barnum);
bool32 HorseRaceNext(real32 percent);

/* Local mission-loading detail; deliberately separate from network horse
   packets and their original percentages. Calls outside an active story load
   are ignored. Task/item callbacks only update data, never graphics state. */
enum
{
    HR_LOAD_PREPARE = 0,
    HR_LOAD_FLEET,
    HR_LOAD_WORLD,
    HR_LOAD_EFFECTS,
    HR_LOAD_TEXTURE_LAYOUT,
    HR_LOAD_TEXTURE_UPLOAD,
    HR_LOAD_FINALIZE
};
void HorseRaceLoadingPhaseBegin(int phase, const char *task);
void HorseRaceLoadingPhaseComplete(int phase);
void HorseRaceLoadingTask(const char *task);
void HorseRaceLoadingProgress(unsigned int completed, unsigned int total, const char *task);
/* Render-thread-only, safe loader boundaries. Throttled and reentry guarded. */
void HorseRaceLoadingRefresh(void);

#endif
