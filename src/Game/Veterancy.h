#ifndef ___VETERANCY_H
#define ___VETERANCY_H

#include "Types.h"

struct Ship;

#define VETERANCY_MAX_RANK 4

void veterancyInitialize(struct Ship *ship);
udword veterancyScore(const struct Ship *ship);
ubyte veterancyRankForScore(udword shipClass, udword score);
real32 veterancyFactor(const struct Ship *ship);
void veterancyCreditKill(struct Ship *attacker, struct Ship *victim);
const char *veterancyRankName(const struct Ship *ship);

#endif
