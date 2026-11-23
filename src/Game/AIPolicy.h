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

/* A 10% reduction in duration/cost, rounded up; free items stay free. */
static sdword aiProductionValue(const Player *player, sdword value)
{
    return aiIsExpert(player) && value > 0 ? value - value / 10 : value;
}

static ShipType aiPreferredShip(const Player *player, ShipType type)
{
    if (!aiIsExpert(player) || (player->race != R1 && player->race != R2)) return type;
    switch (type)
    {
        case LightInterceptor: case HeavyInterceptor: case CloakedFighter:
        case DefenseFighter: return AttackBomber;
        case LightCorvette: case MinelayerCorvette: case RepairCorvette:
        case SalCapCorvette: return HeavyCorvette;
        case StandardFrigate: case DDDFrigate: return IonCannonFrigate;
        default: return type;
    }
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
