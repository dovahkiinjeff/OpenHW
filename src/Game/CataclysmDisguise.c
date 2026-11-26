#include "CataclysmDisguise.h"
#include "CataclysmCampaign.h"
#include "AIVar.h"
#include "Attack.h"
#include "Randy.h"
#include "SoundEvent.h"
#include "Universe.h"
#include <ctype.h>
#include <stdio.h>

enum
{
    CATA_DISGUISE_ABILITY = 0x20,
    CATA_DISGUISE_LINK_ABILITY = 0x10,
    CATA_DISGUISE_ASTEROID_GROUP = 8,
    CATA_DISGUISE_GROUP_COUNT = 9,
    CATA_DISGUISE_GROUP_CAPACITY = 84,
    CATA_DISGUISE_MORPH_TICKS = 20,
    CATA_DISGUISE_START_SOUND = 0x817,
    CATA_DISGUISE_REVEAL_SOUND = 0x818
};

static sdword readValue(char *key)
{
    AIVar *var = aivarFind(key);
    return var ? aivarValueGet(var) : 0;
}

static void writeValue(char *key, sdword number)
{
    AIVar *var = aivarFind(key);
    if (!var) var = aivarCreate(key);
    aivarValueSet(var, number);
}

void cataDisguiseRegister(sdword group, const char *name)
{
    char countKey[40], entryKey[40];
    sdword count;
    ShipType type;
    if (!name || group < 0 || group >= CATA_DISGUISE_GROUP_COUNT) return;
    if (group == 2 || group == CATA_DISGUISE_ASTEROID_GROUP)
    {
        cataDisguiseSetGroupLimit(CATA_DISGUISE_ASTEROID_GROUP, 3);
        writeValue("CataDisguiseCount8", 4);
        return;
    }

    type = StrToShipType((char *)name);
    if (type >= TOTAL_NUM_SHIPS) return;
    snprintf(countKey, sizeof(countKey), "CataDisguiseCount%d", group);
    count = readValue(countKey);
    if (count < 0 || count >= CATA_DISGUISE_GROUP_CAPACITY) return;
    snprintf(entryKey, sizeof(entryKey), "CataDisguiseEntry%d_%d", group, count);
    writeValue(entryKey, type);
    writeValue(countKey, count + 1);
}

void cataDisguiseSetGroupLimit(sdword group, sdword limit)
{
    char key[40];
    if (group == 2) group = CATA_DISGUISE_ASTEROID_GROUP;
    if (group < 0 || group >= CATA_DISGUISE_GROUP_COUNT || limit < 0) return;
    snprintf(key, sizeof(key), "CataDisguiseLimit%d", group);
    writeValue(key, limit);
}

/* Use saved ship IDs and tick counts. The disguise borrows a loaded model
   while the original ship keeps its collision, health, orders and weapons. */
static void shipKey(char *key, size_t size, const Ship *ship, char field)
{
    snprintf(key, size, "CataDisguise%c%u", field, (unsigned)ship->shipID.shipNumber);
}

static sdword shipValue(const Ship *ship, char field)
{
    char key[40];
    shipKey(key, sizeof(key), ship, field);
    return readValue(key);
}

static void saveShipValue(const Ship *ship, char field, sdword value)
{
    char key[40];
    shipKey(key, sizeof(key), ship, field);
    writeValue(key, value);
}

void cataDisguiseForget(Ship *ship)
{
    const char *field;
    if (!bitTest(ship->specialFlags2, SPECIAL_2_CataDisguising)) return;
    for (field = "TACQR"; *field; ++field)
    {
        char key[40];
        AIVar *var;
        shipKey(key, sizeof(key), ship, *field);
        var = aivarFind(key);
        if (var) aivarDestroy(var);
    }
    bitClear(ship->specialFlags2, SPECIAL_2_CataDisguising);
}

static const StaticHeader *disguiseHeader(sdword type, ShipRace race)
{
    ShipStaticInfo *info;
    if (type < 0)
    {
        sdword asteroid = Asteroid0 - type - 1;
        if (asteroid >= NUM_ASTEROIDTYPES) return NULL;
        return &asteroidStaticInfos[asteroid].staticheader;
    }
    if (type >= TOTAL_NUM_SHIPS) return NULL;
    if (race >= NUM_RACES) return NULL;
    info = GetShipStaticInfoSafe((ShipType)type, race);
    return info ? &info->staticheader : NULL;
}

static sdword visibleType(const Ship *ship)
{
    udword now = universe.univUpdateCounter;
    udword transition;
    if (!bitTest(ship->specialFlags2, SPECIAL_2_CataDisguising)) return TOTAL_NUM_SHIPS;
    transition = (udword)shipValue(ship, 'Q');
    if ((sdword)(now - transition) < 0) return TOTAL_NUM_SHIPS;
    transition = (udword)shipValue(ship, 'R');
    if (transition && (sdword)(now - transition) >= 0) return TOTAL_NUM_SHIPS;
    return shipValue(ship, 'T');
}

lodinfo *cataDisguiseVisual(const Ship *ship, sdword *color, real32 *morph, real32 *radius)
{
    const udword now = universe.univUpdateCounter;
    udword revealTick;
    sdword type, ticks;
    const StaticHeader *header;
    *morph = 0.0f;
    if (!bitTest(ship->specialFlags2, SPECIAL_2_CataDisguising)) return NULL;
    revealTick = (udword)shipValue(ship, 'R');
    ticks = (sdword)((udword)shipValue(ship, 'Q') - now);
    if (revealTick)
    {
        ticks = (sdword)(revealTick - now);
        if (ticks <= 0) return NULL;
        *morph = (real32)(CATA_DISGUISE_MORPH_TICKS - ticks) / CATA_DISGUISE_MORPH_TICKS;
    }
    else if (ticks > 0)
    {
        *morph = (real32)ticks / CATA_DISGUISE_MORPH_TICKS;
    }
    type = shipValue(ship, 'T');
    header = disguiseHeader(type, (ShipRace)shipValue(ship, 'A'));
    if (!header || !header->LOD) return NULL;
    *color = shipValue(ship, 'C');
    /* Retail morphs disguise vertices toward a sphere around the real hull. */
    *radius = 1.2f * max(header->staticCollInfo.collspheresize,
                         ship->staticinfo->staticheader.staticCollInfo.collspheresize);
    return header->LOD;
}

bool32 cataDisguiseConcealsRaider(const Ship *ship, const Player *observer)
{
    return ship->shiptype == P1StandardCorvette && ship->shiprace == P1 &&
        observer && observer != ship->playerowner &&
        bitTest(ship->specialFlags2, SPECIAL_2_CataDisguising) &&
        shipValue(ship, 'T') == AdvanceSupportFrigate &&
        shipValue(ship, 'A') == R1 && !shipValue(ship, 'R');
}

bool32 cataDisguiseIsAsteroid(const Ship *ship)
{
    return visibleType(ship) < 0;
}

static bool32 canDisguise(Ship *ship)
{
    if (ship->flags & SOF_Dead) return FALSE;
    /* The Turanic Standard Corvette has the disguise built into its retail
       hull. Mimic/MCV still require their installed research ability. */
    if (ship->shiprace == P1 && ship->shiptype == P1StandardCorvette) return TRUE;
    return (ship->shiptype == CataMimic || ship->shiptype == CataMCV) &&
        cataCampaignShipHasUpgrade(ship, CATA_DISGUISE_ABILITY);
}

static void queueDisguise(Ship *ship, sdword type, ShipRace race, sdword color)
{
    const StaticHeader *header;
    if (!canDisguise(ship)) return;
    if (type >= 0 && ship->shiptype != P1StandardCorvette)
    {
        ShipStaticInfo *info;
        if (race >= NUM_RACES) return;
        info = GetShipStaticInfoSafe((ShipType)type, race);
        if (!info || info->shipclass <= CLASS_Destroyer ||
            (ship->shiptype == CataMimic && (info->shipclass == CLASS_Frigate ||
                info->shipclass == CLASS_Resource || info->shipclass == CLASS_NonCombat)) ||
            (ship->shiptype == CataMCV && info->shipclass == CLASS_Fighter &&
                !cataCampaignShipHasUpgrade(ship, CATA_DISGUISE_LINK_ABILITY)) ||
            (type >= CataMothershipCommand && type <= CATA_LAST_COMPONENT) ||
            type == CataProcessor) return;
    }
    header = disguiseHeader(type, race);
    if (!header || !header->LOD) return;
    if (color < 0 || color >= MAX_MULTIPLAYER_PLAYERS) color = 0;
    saveShipValue(ship, 'T', type);
    saveShipValue(ship, 'A', race);
    saveShipValue(ship, 'C', color);
    saveShipValue(ship, 'Q', (sdword)(universe.univUpdateCounter + CATA_DISGUISE_MORPH_TICKS));
    saveShipValue(ship, 'R', 0);
    bitSet(ship->specialFlags2, SPECIAL_2_CataDisguising);
    if (soundEventUsesCataclysm()) soundEvent(ship, CATA_DISGUISE_START_SOUND);
}

static sdword asteroidDisguise(Ship *ship, sdword asteroid)
{
    if (asteroid == 0)
    {
        bool32 odd = (ranRandom(RANDOM_ETG) & 1) != 0;
        asteroid = ship->shiptype == CataMimic ? (odd ? 2 : 1) : (odd ? 3 : 2);
    }
    if (asteroid == 3 && ship->shiptype == CataMimic) return 0;
    if (asteroid == 1 && ship->shiptype == CataMCV &&
        !cataCampaignShipHasUpgrade(ship, CATA_DISGUISE_LINK_ABILITY)) return 0;
    return -asteroid - 1;
}

void cataDisguiseApply(GrowSelection *ships, const char *name, sdword color)
{
    sdword i, asteroid = -1, type = TOTAL_NUM_SHIPS;
    ShipRace race = R1;
    if (!ships || !ships->selection || !name) return;
    if (tolower((unsigned char)name[0]) == 'a' && name[1] >= '0' && name[1] <= '3' && !name[2])
        asteroid = name[1] - '0';
    else
    {
        type = StrToShipType((char *)name);
        if (type >= TOTAL_NUM_SHIPS) return;
        if (name[0] == 'k' || name[0] == 'K') race = R1;
        else if (name[0] == 't' || name[0] == 'T') race = R2;
        else race = GetValidRaceForShipType((ShipType)type);
    }
    for (i = 0; i < ships->selection->numShips; ++i)
    {
        Ship *ship = ships->selection->ShipPtr[i];
        sdword form = asteroid < 0 ? type : asteroidDisguise(ship, asteroid);
        if (asteroid < 0 || form < 0) queueDisguise(ship, form, race, color);
    }
}

static bool32 reveal(Ship *ship)
{
    if (visibleType(ship) == TOTAL_NUM_SHIPS)
        cataDisguiseForget(ship);
    else if (!shipValue(ship, 'R'))
    {
        saveShipValue(ship, 'R', (sdword)(universe.univUpdateCounter + CATA_DISGUISE_MORPH_TICKS));
        if (soundEventUsesCataclysm()) soundEvent(ship, CATA_DISGUISE_REVEAL_SOUND);
    }
    return TRUE;
}

void cataDisguiseRemove(GrowSelection *ships)
{
    sdword i;
    if (!ships || !ships->selection) return;
    for (i = 0; i < ships->selection->numShips; ++i) reveal(ships->selection->ShipPtr[i]);
}

static void mimicStaticInit(char *directory, char *name, ShipStaticInfo *info)
{
    (void)directory;
    (void)name;
    info->canSpecialBandBoxFriendlies = TRUE;
}

static bool32 mimicDisguise(Ship *ship, void *command)
{
    const SelectAnyCommand *targets = command;
    SpaceObjRotImpTarg *target;
    if (!targets || targets->numTargets == 0) return TRUE;
    target = targets->TargetPtr[0];
    if (target->objtype == OBJ_ShipType)
    {
        Ship *source = (Ship *)target;
        queueDisguise(ship, source->shiptype, source->shiprace, source->colorScheme);
    }
    else if (target->objtype == OBJ_AsteroidType)
    {
        sdword shape = ((Asteroid *)target)->asteroidtype - Asteroid0;
        sdword form = asteroidDisguise(ship, shape == 1 || shape == 3 ? shape : 2);
        if (form == 0) form = asteroidDisguise(ship, 2);
        queueDisguise(ship, form, R1, 0);
    }
    return TRUE;
}

static void mimicPassive(Ship *ship, Ship *target, bool32 rotate)
{
    if (visibleType(ship) != TOTAL_NUM_SHIPS) return;
    if (rotate) attackPassiveRotate(ship, target);
    else attackPassive(ship, target);
}

static void mimicAttack(Ship *ship, SpaceObjRotImpTarg *target, real32 range)
{
    if (visibleType(ship) == TOTAL_NUM_SHIPS) attackStraightForward(ship, target, range, 0);
}

static void mcvAttack(Ship *ship, SpaceObjRotImpTarg *target, real32 range)
{
    if (visibleType(ship) == TOTAL_NUM_SHIPS) attackStraightForward(ship, target, range, 1000.0f);
}

CustShipHeader CataMimicHeader = {
    .shiptype = CataMimic,
    .CustShipStaticInit = mimicStaticInit,
    .CustShipAttack = mimicAttack,
    .CustShipAttackPassive = mimicPassive,
    .CustShipSpecialActivate = reveal,
    .CustShipSpecialTarget = mimicDisguise
};

CustShipHeader CataMCVHeader = {
    .shiptype = CataMCV,
    .CustShipStaticInit = mimicStaticInit,
    .CustShipAttack = mcvAttack,
    .CustShipAttackPassive = mimicPassive,
    .CustShipSpecialActivate = reveal,
    .CustShipSpecialTarget = mimicDisguise
};
