#ifndef OPENHW_AI_POLICY_H
#define OPENHW_AI_POLICY_H

#include "AIPlayer.h"
#include <string.h>
#include <stdio.h>

/* Expert inherits the configured Hard behavior, without indexing beyond the
   five legacy team difficulty levels or changing existing save structures. */
static AIPlayerLevel aiBaseDifficulty(AIPlayerLevel level)
{
    return level > AI_ADVANCED ? AI_ADVANCED : level;
}

static bool32 aiIsExpert(const Player *player)
{
    return !singlePlayerGame && player && player->aiPlayer &&
        player->aiPlayer->aiplayerDifficultyLevel == AI_EXPERT;
}

/* Difficulty changes decisions, never prices, damage, or simulation speed. */
static sdword aiProductionValue(const Player *player, sdword value)
{
    (void)player;
    return value;
}

/* Requests retain their role, including reconnaissance, repair and detection. */
static ShipType aiPreferredShip(const Player *player, ShipType type)
{
    (void)player;
    return type;
}

typedef struct AIStrategyProfile {
    real32 assaultInterval;
    sdword maxWaitingTeams;
    sdword researchLabs;
    sdword carrierInvestment;
    sdword counterStrength;
} AIStrategyProfile;

static const AIStrategyProfile *aiStrategy(const Player *player)
{
    static const AIStrategyProfile profiles[4] = {
        {480.0f, 2, 2, 6, 0},
        {300.0f, 3, 3, 5, 32},
        {180.0f, 4, 4, 4, 64},
        {120.0f, 5, 6, 3, 96}
    };
    sdword level = player && player->aiPlayer ? player->aiPlayer->aiplayerDifficultyLevel : 0;
    if (level < 0 || level > AI_EXPERT) level = 0;
    return &profiles[level];
}

/* Count only acquired knowledge; no hidden-world enemy census. */
static sdword aiKnownShipCount(const AIPlayer *ai, ShipType type)
{
    SelectCommand *known = ai->enemyShipsIAmAwareOf[type].selection;
    sdword i, count = 0;
    if (!known) return 0;
    for (i = 0; i < known->numShips; ++i)
        if (known->ShipPtr[i]->health > 0 && !(known->ShipPtr[i]->flags & SOF_Dead)) ++count;
    return count;
}

/* The second carrier becomes affordable after another carrier's worth of
   economic output. No artificial carrier count ceiling: normal unit caps apply. */
static sdword aiCarrierTarget(sdword economicOutput, sdword carrierCost)
{
    if (carrierCost <= 0) return 0;
    return 1 + economicOutput / (carrierCost * 3);
}

/* Accept retail AI_BEG/AI_INT/AI_ADV names and the descriptive source names. */
static bool32 aiCanonicalTweakName(const char *name, char *out, size_t capacity)
{
    const char *bracket = strchr(name, '[');
    const char *level = NULL;
    size_t prefix;
    if (!bracket) return FALSE;
    if (!strcmp(bracket, "[AI_BEG]")) level = "[AI_BEGINNER]";
    else if (!strcmp(bracket, "[AI_INT]")) level = "[AI_INTERMEDIATE]";
    else if (!strcmp(bracket, "[AI_ADV]")) level = "[AI_ADVANCED]";
    if (!level) return FALSE;
    prefix = (size_t)(bracket - name);
    if (prefix + strlen(level) + 1 > capacity) return FALSE;
    memcpy(out, name, prefix);
    strcpy(out + prefix, level);
    return TRUE;
}

#endif
