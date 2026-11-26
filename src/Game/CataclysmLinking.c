/* Restored from our verified 2026-10-13 linking implementation.
   Progression is driven by simulation ticks, not rendering. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "CataclysmLinking.h"
#include "CataclysmCampaign.h"
#include "SaveGame.h"
#include "Debug.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "CommandDefs.h"
#include "Select.h"
#include "Teams.h"
#include "MeshAnim.h"
#include "mainrgn.h"
#include "Globals.h"
#include "SoundEvent.h"
#include "ShipSelect.h"
#include "ShipAppearance.h"
#include "main.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
/* Verbatim archived mainrgn.c block: original lines 855-1336.
   Engine integration excerpt; see README.md and src/SDL/mainrgn.c for context. */

#define MR_MAX_PENDING_LINKS (COMMAND_MAX_SHIPS / 2)
#define MR_ACOLYTE_STAGING_HALF_SEPARATION 100.0f
#define MR_ACOLYTE_DOCKED_HALF_SEPARATION 0.0f
#define MR_LINK_WAITING_FOR_RETRACTION (-1)
#include "CataclysmLinkPreparation.inc"
typedef struct mrPendingAcolyteTransition
{
    ShipID first;
    ShipID second;
    real32 startTime;
    real32 phaseTime;
    real32 finishTime;
    bool32 unlinking;
    bool32 animationStarted;
    vector firstStart;
    vector secondStart;
    vector firstAligned;
    vector secondAligned;
    vector firstDocked;
    vector secondDocked;
    vector unlinkControl1;
    vector unlinkControl2;
    vector center;
    matrix baseOrientation;
} mrPendingAcolyteTransition;

static mrPendingAcolyteTransition mrPendingAcolyteTransitions[MR_MAX_PENDING_LINKS];
static sdword mrPendingAcolyteTransitionCount = 0;

/* Pointer-free transition records use persistent ship IDs and simulation
   timestamps. Keep the animation in progress when a mission save is loaded. */
void mrSaveAcolyteTransitions(void)
{
    SaveInfoNumber(0x434c4e4b);SaveInfoNumber(mrPendingAcolyteTransitionCount);
    if(mrPendingAcolyteTransitionCount)SaveStructureOfSize(mrPendingAcolyteTransitions,
        mrPendingAcolyteTransitionCount*sizeof(mrPendingAcolyteTransitions[0]));
}
void mrLoadAcolyteTransitions(void)
{
    sdword marker,count;mrCancelAcolyteTransitions();
    if(!LoadInfoNumberOptional(&marker))return;
    if(marker!=0x434c4e4b)dbgFatalf(DBG_Loc,"Invalid saved Acolyte transition block");
    count=LoadInfoNumber();
    if(count<0 || count>MR_MAX_PENDING_LINKS)dbgFatalf(DBG_Loc,"Invalid saved Acolyte transition count");
    if(count)LoadStructureOfSizeToAddress(mrPendingAcolyteTransitions,count*sizeof(mrPendingAcolyteTransitions[0]));
    mrPendingAcolyteTransitionCount=count;
}

void mrCancelAcolyteTransitions(void)
{
    mrPendingAcolyteTransitionCount = 0;
    memset(mrPendingAcolyteTransitions, 0, sizeof(mrPendingAcolyteTransitions));
}

static real32 mrTransitionSmoothStep(real32 value)
{
    value = min(1.0f, max(0.0f, value));
    return value * value * (3.0f - 2.0f * value);
}

static void mrTransitionSetPosition(Ship *ship, const vector *start,
                                    const vector *finish, real32 amount)
{
    ship->posinfo.position.x = start->x + (finish->x - start->x) * amount;
    ship->posinfo.position.y = start->y + (finish->y - start->y) * amount;
    ship->posinfo.position.z = start->z + (finish->z - start->z) * amount;
    vecZeroVector(ship->posinfo.velocity);
}

static void mrTransitionSetRoll(Ship *ship, const matrix *base, real32 angle)
{
    real32 cosine = (real32)cos((real64)angle);
    real32 sine = (real32)sin((real64)angle);

    ship->rotinfo.coordsys = *base;
    ship->rotinfo.coordsys.m11 = cosine * base->m11 + sine * base->m12;
    ship->rotinfo.coordsys.m21 = cosine * base->m21 + sine * base->m22;
    ship->rotinfo.coordsys.m31 = cosine * base->m31 + sine * base->m32;
    ship->rotinfo.coordsys.m12 = -sine * base->m11 + cosine * base->m12;
    ship->rotinfo.coordsys.m22 = -sine * base->m21 + cosine * base->m22;
    ship->rotinfo.coordsys.m32 = -sine * base->m31 + cosine * base->m32;
}

bool32 mrAcolyteIsTransitioning(Ship *ship)
{
    sdword index;
    for (index = 0; index < mrPendingAcolyteTransitionCount; ++index)
    {
        if (mrPendingAcolyteTransitions[index].first.shipNumber == ship->shipID.shipNumber ||
            mrPendingAcolyteTransitions[index].second.shipNumber == ship->shipID.shipNumber)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void mrStartAcolyteAnimation(Ship *first, Ship *second, const char *name)
{
    Ship *ships[2] = {first, second};
    sdword actor;
    for (actor = 0; actor < 2; ++actor)
    {
        Ship *ship = ships[actor];
        madheader *header;
        sdword animation;
        if (ship->madBindings == NULL || ship->staticinfo->madStatic == NULL ||
            ship->staticinfo->madStatic->header == NULL) continue;
        header = ship->staticinfo->madStatic->header;
        /* The generic name lookup is fatal on a missing animation. Model
           animation is optional here: trajectory/roll can still complete. */
        for (animation = 0; animation < header->nAnimations; ++animation)
            if (header->anim[animation].name != NULL &&
                SDL_strcasecmp(header->anim[animation].name, name) == 0)
            {
                /* A closed gun pose can be held by a paused CombatIn clip.
                   Release that owner/cue before starting the link clip. */
                if(ship->madBindings->nCurrentAnim!=MAD_NoAnimation){
                    if(ship->madBindings->bPaused)madAnimationPause(ship,FALSE);
                    madAnimationStop(ship);
                }
                ship->madAnimationFlags=0;
                ship->cuedAnimationType=MAD_ANIMATION_NOTHING;
                ship->nextAnim=MAD_ANIMATION_NOTHING;
                madAnimationStart(ship, animation);
                break;
            }
        if (animation == header->nAnimations)
            fprintf(stderr,"[AcolyteLink] ship=%u missing optional animation '%s'; retaining flight transition.\n",
                (unsigned)ship->shipID.shipNumber,name);
    }
}

static void mrCopyTransformState(Ship *destination, Ship *source)
{
    ShipAppearance appearance;
    destination->rotinfo.coordsys = source->rotinfo.coordsys;
    destination->posinfo.velocity = source->posinfo.velocity;
    destination->tacticstype = source->tacticstype;
    destination->colorScheme = source->colorScheme;
    shipAppearanceGet(source,&appearance);
    shipAppearanceApply(destination,&appearance);
    destination->hotKeyGroup = source->hotKeyGroup;
    destination->veterancyRank = source->veterancyRank;
    memcpy(destination->veterancyKills, source->veterancyKills,
           sizeof(destination->veterancyKills));
}

static void mrFinishAcolyteLink(Ship *first, Ship *second)
{
    Ship *acv;
    vector midpoint;
    real32 healthFraction;

    midpoint.x = (first->posinfo.position.x + second->posinfo.position.x) * 0.5f;
    midpoint.y = (first->posinfo.position.y + second->posinfo.position.y) * 0.5f;
    midpoint.z = (first->posinfo.position.z + second->posinfo.position.z) * 0.5f;
    healthFraction = (first->health + second->health) /
                     (first->staticinfo->maxhealth + second->staticinfo->maxhealth);
    acv = univAddShip(CataACV, R1, &midpoint, first->playerowner, 0);
    mrCopyTransformState(acv, first);
    acv->posinfo.velocity.x = (first->posinfo.velocity.x + second->posinfo.velocity.x) * 0.5f;
    acv->posinfo.velocity.y = (first->posinfo.velocity.y + second->posinfo.velocity.y) * 0.5f;
    acv->posinfo.velocity.z = (first->posinfo.velocity.z + second->posinfo.velocity.z) * 0.5f;
    acv->health = acv->staticinfo->maxhealth * min(1.0f, max(0.0f, healthFraction));
    acv->veterancyRank = max(first->veterancyRank, second->veterancyRank);
    unitCapCreateShip(acv, acv->playerowner);
    univReallyRemoveShipForTransformation(first);
    univReallyRemoveShipForTransformation(second);
    selSelectionAddSingleShip(&selSelected, acv);
}

void mrUpdateAcolyteTransitions(void)
{
    sdword index = 0;
    while (index < mrPendingAcolyteTransitionCount)
    {
        mrPendingAcolyteTransition *pending = &mrPendingAcolyteTransitions[index];
        Ship *first = ShipIDtoShip(pending->first, TRUE);
        Ship *second = ShipIDtoShip(pending->second, TRUE);
        if (first == NULL || second == NULL || first == second ||
            first->shiptype != CataAcolyte || second->shiptype != CataAcolyte ||
            first->playerowner != second->playerowner ||
            bitTest(first->flags | second->flags, SOF_Dead | SOF_Hide))
        {
            fprintf(stderr,"[AcolyteLink] cancelled invalid actors: IDs=%u,%u types=%d,%d.\n",
                (unsigned)pending->first.shipNumber,(unsigned)pending->second.shipNumber,
                first ? (int)first->shiptype : -1,second ? (int)second->shiptype : -1);
            mrPendingAcolyteTransitions[index] =
                mrPendingAcolyteTransitions[--mrPendingAcolyteTransitionCount];
            continue;
        }
        if (!pending->unlinking && pending->animationStarted==MR_LINK_WAITING_FOR_RETRACTION)
        {
            bool32 firstReady=mrAcolyteGunsRetracted(first);
            bool32 secondReady=mrAcolyteGunsRetracted(second);
            if(!firstReady || !secondReady) {++index;continue;}
            /* Keep the original alignment/link duration; retraction time is
               an earlier stage, never time borrowed from the link motion. */
            real32 delay=universe.totaltimeelapsed-pending->startTime;
            pending->startTime+=delay;pending->phaseTime+=delay;pending->finishTime+=delay;
            pending->animationStarted=FALSE;
        }
        if (pending->unlinking)
        {
            real32 amount = mrTransitionSmoothStep(
                (universe.totaltimeelapsed - pending->startTime) /
                (pending->finishTime - pending->startTime));
            mrTransitionSetPosition(first, &pending->firstStart,
                                    &pending->firstAligned, amount);
            real32 inverse = 1.0f - amount;
            /* Only the lower hull leaves the linked pose.  Start downward,
               then curve outward and back up alongside the stationary hull. */
            second->posinfo.position.x =
                inverse * inverse * inverse * pending->secondStart.x +
                3.0f * inverse * inverse * amount * pending->unlinkControl1.x +
                3.0f * inverse * amount * amount * pending->unlinkControl2.x +
                amount * amount * amount * pending->secondAligned.x;
            second->posinfo.position.y =
                inverse * inverse * inverse * pending->secondStart.y +
                3.0f * inverse * inverse * amount * pending->unlinkControl1.y +
                3.0f * inverse * amount * amount * pending->unlinkControl2.y +
                amount * amount * amount * pending->secondAligned.y;
            second->posinfo.position.z =
                inverse * inverse * inverse * pending->secondStart.z +
                3.0f * inverse * inverse * amount * pending->unlinkControl1.z +
                3.0f * inverse * amount * amount * pending->unlinkControl2.z +
                amount * amount * amount * pending->secondAligned.z;
            vecZeroVector(second->posinfo.velocity);
            mrTransitionSetRoll(first, &pending->baseOrientation, 0.0f);
            mrTransitionSetRoll(second, &pending->baseOrientation,
                                3.14159265358979f * (1.0f - amount));
        }
        else if (universe.totaltimeelapsed < pending->phaseTime)
        {
            real32 amount = mrTransitionSmoothStep(
                (universe.totaltimeelapsed - pending->startTime) /
                (pending->phaseTime - pending->startTime));
            mrTransitionSetPosition(first, &pending->firstStart,
                                    &pending->firstAligned, amount);
            mrTransitionSetPosition(second, &pending->secondStart,
                                    &pending->secondAligned, amount);
            mrTransitionSetRoll(first, &pending->baseOrientation, 0.0f);
            mrTransitionSetRoll(second, &pending->baseOrientation,
                                3.14159265358979f * amount);
        }
        else
        {
            real32 amount;
            if (!pending->animationStarted)
            {
                mrStartAcolyteAnimation(first, second, "Link");
                pending->animationStarted = TRUE;
            }
            amount = mrTransitionSmoothStep(
                (universe.totaltimeelapsed - pending->phaseTime) /
                (pending->finishTime - pending->phaseTime));
            mrTransitionSetPosition(first, &pending->firstAligned,
                                    &pending->firstDocked, amount);
            mrTransitionSetPosition(second, &pending->secondAligned,
                                    &pending->secondDocked, amount);
            mrTransitionSetRoll(first, &pending->baseOrientation, 0.0f);
            mrTransitionSetRoll(second, &pending->baseOrientation,
                                3.14159265358979f);
        }
        if (universe.totaltimeelapsed >= pending->finishTime)
        {
            if (!pending->unlinking)
            {
                mrFinishAcolyteLink(first, second);
            }
            mrPendingAcolyteTransitions[index] =
                mrPendingAcolyteTransitions[--mrPendingAcolyteTransitionCount];
            continue;
        }
        ++index;
    }
}

void mrLinkAcolytes(char *string, featom *atom)
{
    Ship *acolytes[COMMAND_MAX_SHIPS];
    bool32 paired[COMMAND_MAX_SHIPS];
    sdword count = 0, index;
    (void)string;
    (void)atom;

    if (multiPlayerGame || playPackets || universePause || mrDisabled) return;
    if(!GetShipStaticInfoSafe(CataACV,R1) || !cataCampaignLinkAvailable())return;
    for (index = 0; index < selSelected.numShips; ++index)
    {
        Ship *ship = selSelected.ShipPtr[index];
        if (ship->shiptype == CataAcolyte &&
            ship->playerowner == universe.curPlayerPtr &&
            cataCampaignShipCanLink(ship) &&
            !bitTest(ship->flags, SOF_Dead) &&
            !mrAcolyteIsTransitioning(ship))
        {
            acolytes[count++] = ship;
        }
    }

    memset(paired, 0, sizeof(paired));

    /* Greedily pair every Acolyte with its nearest unpaired neighbor. */
    for (index = 0; index + 1 < count &&
                    mrPendingAcolyteTransitionCount < MR_MAX_PENDING_LINKS;
         index += 2)
    {
        Ship *first;
        Ship *second;
        Ship *swapShip;
        vector midpoint;
        vector up;
        matrix sharedOrientation;
        real32 verticalOrder;
        real32 firstTravel;
        real32 secondTravel;
        real32 alignmentDistance;
        real32 alignmentSpeed;
        real32 alignmentDuration;
        real32 nearestDistance = REALlyBig;
        sdword firstIndex = -1;
        sdword secondIndex = -1;
        sdword candidate, other;
        mrPendingAcolyteTransition *pending;

        for (candidate = 0; candidate < count; ++candidate)
        {
            if (paired[candidate]) continue;
            for (other = candidate + 1; other < count; ++other)
            {
                real32 dx, dy, dz, distance;
                if (paired[other]) continue;
                dx = acolytes[candidate]->posinfo.position.x -
                     acolytes[other]->posinfo.position.x;
                dy = acolytes[candidate]->posinfo.position.y -
                     acolytes[other]->posinfo.position.y;
                dz = acolytes[candidate]->posinfo.position.z -
                     acolytes[other]->posinfo.position.z;
                distance = dx * dx + dy * dy + dz * dz;
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    firstIndex = candidate;
                    secondIndex = other;
                }
            }
        }
        if (firstIndex < 0 || secondIndex < 0) break;
        paired[firstIndex] = paired[secondIndex] = TRUE;
        first = acolytes[firstIndex];
        second = acolytes[secondIndex];

        midpoint.x = (first->posinfo.position.x + second->posinfo.position.x) * 0.5f;
        midpoint.y = (first->posinfo.position.y + second->posinfo.position.y) * 0.5f;
        midpoint.z = (first->posinfo.position.z + second->posinfo.position.z) * 0.5f;
        sharedOrientation = first->rotinfo.coordsys;
        up.x = sharedOrientation.m12;
        up.y = sharedOrientation.m22;
        up.z = sharedOrientation.m32;
        verticalOrder =
            (first->posinfo.position.x - second->posinfo.position.x) * up.x +
            (first->posinfo.position.y - second->posinfo.position.y) * up.y +
            (first->posinfo.position.z - second->posinfo.position.z) * up.z;
        /* Keep the upper Acolyte first so its upright transform becomes the
           resulting ACV transform.  The lower docking partner rolls 180
           degrees around the shared forward axis. */
        if (verticalOrder < 0.0f)
        {
            swapShip = first;
            first = second;
            second = swapShip;
        }
        first->rotinfo.coordsys = sharedOrientation;
        second->rotinfo.coordsys = sharedOrientation;
        vecZeroVector(first->posinfo.velocity);
        vecZeroVector(second->posinfo.velocity);
        pending = &mrPendingAcolyteTransitions[mrPendingAcolyteTransitionCount++];
        pending->first = first->shipID;
        pending->second = second->shipID;
        pending->startTime = universe.totaltimeelapsed;
        pending->unlinking = FALSE;
        pending->animationStarted = MR_LINK_WAITING_FOR_RETRACTION;
        pending->firstStart = first->posinfo.position;
        pending->secondStart = second->posinfo.position;
        pending->center = midpoint;
        pending->baseOrientation = sharedOrientation;
        pending->firstAligned.x = midpoint.x + up.x * MR_ACOLYTE_STAGING_HALF_SEPARATION;
        pending->firstAligned.y = midpoint.y + up.y * MR_ACOLYTE_STAGING_HALF_SEPARATION;
        pending->firstAligned.z = midpoint.z + up.z * MR_ACOLYTE_STAGING_HALF_SEPARATION;
        pending->secondAligned.x = midpoint.x - up.x * MR_ACOLYTE_STAGING_HALF_SEPARATION;
        pending->secondAligned.y = midpoint.y - up.y * MR_ACOLYTE_STAGING_HALF_SEPARATION;
        pending->secondAligned.z = midpoint.z - up.z * MR_ACOLYTE_STAGING_HALF_SEPARATION;
        pending->firstDocked.x = midpoint.x + up.x * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
        pending->firstDocked.y = midpoint.y + up.y * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
        pending->firstDocked.z = midpoint.z + up.z * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
        pending->secondDocked.x = midpoint.x - up.x * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
        pending->secondDocked.y = midpoint.y - up.y * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
        pending->secondDocked.z = midpoint.z - up.z * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
        firstTravel = (real32)sqrt(
            (pending->firstAligned.x - pending->firstStart.x) *
                (pending->firstAligned.x - pending->firstStart.x) +
            (pending->firstAligned.y - pending->firstStart.y) *
                (pending->firstAligned.y - pending->firstStart.y) +
            (pending->firstAligned.z - pending->firstStart.z) *
                (pending->firstAligned.z - pending->firstStart.z));
        secondTravel = (real32)sqrt(
            (pending->secondAligned.x - pending->secondStart.x) *
                (pending->secondAligned.x - pending->secondStart.x) +
            (pending->secondAligned.y - pending->secondStart.y) *
                (pending->secondAligned.y - pending->secondStart.y) +
            (pending->secondAligned.z - pending->secondStart.z) *
                (pending->secondAligned.z - pending->secondStart.z));
        alignmentDistance = max(firstTravel, secondTravel);
        alignmentSpeed = max(1.0f, first->staticinfo->staticheader.maxvelocity);
        /* Smoothstep reaches 1.5 times its average speed at mid-curve.  Scale
           duration accordingly so even that fastest point never exceeds the
           Acolyte's actual flight speed, regardless of starting separation. */
        alignmentDuration = max(0.80f,
                                1.5f * alignmentDistance / alignmentSpeed);
        pending->phaseTime = pending->startTime + alignmentDuration;
        pending->finishTime = pending->phaseTime + 1.60f;
        mrStopAcolytePairAttack(first,second);
    }
    soundEvent(NULL, UI_Click);
}

void mrUnlinkACVs(char *string, featom *atom)
{
    Ship *acvs[COMMAND_MAX_SHIPS];
    Ship *created[COMMAND_MAX_SHIPS];
    sdword count = 0, createdCount = 0, index;
    (void)string;
    (void)atom;

    if (multiPlayerGame || playPackets || universePause || mrDisabled) return;
    if(!GetShipStaticInfoSafe(CataAcolyte,R1))return;
    for (index = 0; index < selSelected.numShips; ++index)
    {
        Ship *ship = selSelected.ShipPtr[index];
        if (ship->shiptype == CataACV &&
            ship->playerowner == universe.curPlayerPtr &&
            !bitTest(ship->flags, SOF_Dead))
        {
            acvs[count++] = ship;
        }
    }

    for (index = 0; index < count && createdCount+2<=COMMAND_MAX_SHIPS &&
        mrPendingAcolyteTransitionCount<MR_MAX_PENDING_LINKS; ++index)
    {
        Ship *acv = acvs[index];
        real32 healthFraction = acv->health * acv->staticinfo->oneOverMaxHealth;
        sdword side;
        for (side = -1; side <= 1; side += 2)
        {
            vector position = acv->posinfo.position;
            Ship *acolyte;
            sdword unlinkAnimation;
            acolyte = univAddShip(CataAcolyte, R1, &position, acv->playerowner, 0);
            mrCopyTransformState(acolyte, acv);
            acolyte->health = acolyte->staticinfo->maxhealth *
                               min(1.0f, max(0.0f, healthFraction));
            unitCapCreateShip(acolyte, acolyte->playerowner);
            if (acolyte->madBindings != NULL && acolyte->staticinfo->madStatic != NULL)
            {
                unlinkAnimation = madAnimIndexFindByName(
                    acolyte->staticinfo->madStatic->header, "Unlink");
                if (unlinkAnimation >= 0) madAnimationStart(acolyte, unlinkAnimation);
            }
            created[createdCount++] = acolyte;
        }
        if (createdCount >= 2 && mrPendingAcolyteTransitionCount < MR_MAX_PENDING_LINKS)
        {
            mrPendingAcolyteTransition *pending =
                &mrPendingAcolyteTransitions[mrPendingAcolyteTransitionCount++];
            vector up;
            pending->first = created[createdCount - 2]->shipID;
            pending->second = created[createdCount - 1]->shipID;
            pending->startTime = universe.totaltimeelapsed;
            pending->phaseTime = pending->startTime;
            pending->finishTime = pending->startTime + 2.2667f;
            pending->unlinking = TRUE;
            pending->animationStarted = TRUE;
            pending->center = acv->posinfo.position;
            pending->baseOrientation = acv->rotinfo.coordsys;
            up.x = acv->rotinfo.coordsys.m12;
            up.y = acv->rotinfo.coordsys.m22;
            up.z = acv->rotinfo.coordsys.m32;
            pending->firstStart.x = pending->center.x + up.x * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
            pending->firstStart.y = pending->center.y + up.y * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
            pending->firstStart.z = pending->center.z + up.z * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
            pending->secondStart.x = pending->center.x - up.x * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
            pending->secondStart.y = pending->center.y - up.y * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
            pending->secondStart.z = pending->center.z - up.z * MR_ACOLYTE_DOCKED_HALF_SEPARATION;
            pending->firstDocked = pending->firstStart;
            pending->secondDocked = pending->secondStart;
            pending->firstAligned = pending->firstStart;
            /* Counter the root's return to its neutral pose: the upper hull
               stays visually fixed, not just its simulation origin. */
            pending->firstAligned.x += acv->rotinfo.coordsys.m11 * MAD_ACOLYTE_LINK_ROOT_OFFSET;
            pending->firstAligned.y += acv->rotinfo.coordsys.m21 * MAD_ACOLYTE_LINK_ROOT_OFFSET;
            pending->firstAligned.z += acv->rotinfo.coordsys.m31 * MAD_ACOLYTE_LINK_ROOT_OFFSET;
            pending->secondAligned.x = pending->center.x + acv->rotinfo.coordsys.m11 * 250.0f;
            pending->secondAligned.y = pending->center.y + acv->rotinfo.coordsys.m21 * 250.0f;
            pending->secondAligned.z = pending->center.z + acv->rotinfo.coordsys.m31 * 250.0f;
            pending->unlinkControl1.x = pending->secondStart.x - up.x * 240.0f;
            pending->unlinkControl1.y = pending->secondStart.y - up.y * 240.0f;
            pending->unlinkControl1.z = pending->secondStart.z - up.z * 240.0f;
            pending->unlinkControl2.x = pending->secondAligned.x - up.x * 240.0f;
            pending->unlinkControl2.y = pending->secondAligned.y - up.y * 240.0f;
            pending->unlinkControl2.z = pending->secondAligned.z - up.z * 240.0f;
            created[createdCount - 2]->posinfo.position = pending->firstStart;
            created[createdCount - 1]->posinfo.position = pending->secondStart;
            mrTransitionSetRoll(created[createdCount - 1],
                                &pending->baseOrientation, 3.14159265358979f);
        }
    }

    for (index = 0; index < createdCount/2; ++index)
    {
        univReallyRemoveShipForTransformation(acvs[index]);
    }
    for (index = 0; index < createdCount; ++index)
    {
        selSelectionAddSingleShip(&selSelected, created[index]);
    }
    soundEvent(NULL, UI_Click);
}
