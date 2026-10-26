#include "../Modern/DiagnosticPolicy.h"
#include "Veterancy.h"

#include <string.h>
#include <stdio.h>

#include "ClassDefs.h"
#include "CommandWrap.h"
#include "SpaceObj.h"
#include "Universe.h"
#include "Options.h"

static const udword veterancyClassWeights[NUM_CLASSES] =
{
    20, /* Mothership */
    8,  /* Heavy Cruiser */
    10, /* Carrier */
    9,  /* Destroyer */
    5,  /* Frigate */
    3,  /* Corvette */
    1,  /* Fighter */
    1,  /* Resource */
    1   /* Non-combat */
};

static const udword veterancyThresholds[5][VETERANCY_MAX_RANK] =
{
    { 1,  2,  3,   4}, /* Fighter */
    { 2,  4,  6,   8}, /* Corvette */
    { 3,  6, 12,  16}, /* Frigate */
    { 5, 10, 20,  40}, /* Destroyer */
    {10, 30, 60, 100}  /* Capital/support */
};

static const real32 veterancyFactors[VETERANCY_MAX_RANK + 1] =
{
    1.0f, 1.1f, 1.2f, 1.3f, 1.4f
};

static const char *veterancyNames[VETERANCY_MAX_RANK + 1] =
{
    "Flight Sergeant", "Lieutenant", "Flight Leader",
    "Squadron Leader", "Wing Commander"
};

static udword veterancyThresholdRow(udword shipClass)
{
    switch (shipClass)
    {
        case CLASS_Fighter:   return 0;
        case CLASS_Corvette:  return 1;
        case CLASS_Frigate:   return 2;
        case CLASS_Destroyer: return 3;
        default:              return 4;
    }
}

void veterancyInitialize(Ship *ship)
{
    if (ship == NULL) return;
    memset(ship->veterancyKills, 0, sizeof(ship->veterancyKills));
    ship->veterancyRank = 0;
}

udword veterancyScore(const Ship *ship)
{
    udword score = 0;
    udword i;
    if (ship == NULL) return 0;
    for (i = 0; i < NUM_CLASSES; ++i)
        score += ship->veterancyKills[i] * veterancyClassWeights[i];
    return score;
}

ubyte veterancyRankForScore(udword shipClass, udword score)
{
    udword row = veterancyThresholdRow(shipClass);
    ubyte rank = 0;
    ubyte level;
    for (level = 0; level < VETERANCY_MAX_RANK; ++level)
        if (score >= veterancyThresholds[row][level]) rank = level + 1;
    return rank;
}

real32 veterancyFactor(const Ship *ship)
{
    ubyte rank = ship != NULL ? ship->veterancyRank : 0;
    if (!opRankSystem) return 1.0f;
    if (rank > VETERANCY_MAX_RANK) rank = VETERANCY_MAX_RANK;
    return veterancyFactors[rank];
}

void veterancyCreditKill(Ship *attacker, Ship *victim)
{
    udword victimClass;
    ubyte oldRank;
    static udword diagnosticCount=0;
    bool32 diagnosticLog=diagnosticCount<128;
    if(diagnosticLog)
    {
        ++diagnosticCount;
        HW_DIAG_FPRINTF(stderr,"[Veterancy] fatal-hit credit attempt: enabled=%u attacker=%p victim=%p attackerPlayer=%p victimPlayer=%p.\n",
            (unsigned)opRankSystem,(void *)attacker,(void *)victim,
            attacker ? (void *)attacker->playerowner : NULL,
            victim ? (void *)victim->playerowner : NULL);
    }
    if (!opRankSystem) return;
    if (attacker == NULL || victim == NULL || attacker == victim) return;
    if (attacker->playerowner == NULL || victim->playerowner == NULL) return;
    if (attacker->playerowner == victim->playerowner) return;

    victimClass = victim->staticinfo->shipclass;
    if (victimClass >= NUM_CLASSES) return;

    oldRank = attacker->veterancyRank;
    ++attacker->veterancyKills[victimClass];
    attacker->veterancyRank = veterancyRankForScore(
        attacker->staticinfo->shipclass, veterancyScore(attacker));
    if(diagnosticLog)
        HW_DIAG_FPRINTF(stderr,"[Veterancy] ship=%u class=%u victimClass=%u score=%u rank=%u->%u.\n",
            (unsigned)attacker->shipID.shipNumber,(unsigned)attacker->staticinfo->shipclass,
            (unsigned)victimClass,(unsigned)veterancyScore(attacker),(unsigned)oldRank,(unsigned)attacker->veterancyRank);

    if (attacker->veterancyRank > oldRank &&
        attacker->playerowner == universe.curPlayerPtr)
    {
        char message[256];
        snprintf(message, sizeof(message), "%s has gained a rank: %s",
                 ShipTypeToNiceStr(attacker->shiptype),
                 veterancyRankName(attacker));
        clCommandMessage(message);
    }
}

const char *veterancyRankName(const Ship *ship)
{
    ubyte rank = ship != NULL ? ship->veterancyRank : 0;
    if (rank > VETERANCY_MAX_RANK) rank = VETERANCY_MAX_RANK;
    return veterancyNames[rank];
}
