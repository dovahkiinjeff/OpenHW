#ifndef HW_RESOURCE_UNITS_H
#define HW_RESOURCE_UNITS_H
#include "Universe.h"
#include "Options.h"
#include "Globals.h"
#include <stdio.h>

/* Unlimited spending never fabricates a large bank balance: campaign fleet
   scaling, harvesting, saves and scripted RU values keep their real amounts. */
static bool32 resourceUnitsUnlimited(const Player *player)
{
    return singlePlayerGame && opUnlimitedRUs && player &&
        player == universe.curPlayerPtr;
}
static bool32 resourceUnitsCanAfford(const Player *player, sdword cost)
{
    return resourceUnitsUnlimited(player) || player->resourceUnits >= cost;
}
/* Return the amount actually charged, for spending statistics. */
static sdword resourceUnitsSpend(Player *player, sdword cost)
{
    if (resourceUnitsUnlimited(player)) return 0;
    player->resourceUnits -= cost;
    return cost;
}
static const char *resourceUnitsFormat(const Player *player, char *text, size_t size)
{
    if (resourceUnitsUnlimited(player)) snprintf(text, size, "UNLIMITED");
    else snprintf(text, size, "%d", player->resourceUnits);
    return text;
}
#endif
