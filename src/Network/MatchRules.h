#ifndef OPENHW_MATCH_RULES_H
#define OPENHW_MATCH_RULES_H
#include "OnlineProtocol.h"
#ifdef __cplusplus
extern "C" {
#endif
int hwMatchInstall(const OHLoad *load);
void hwMatchClear(void);
int hwMatchActive(void);
unsigned hwMatchOption(unsigned rule,unsigned localValue);
int hwMatchPlayerOption(unsigned rule,unsigned localValue,unsigned player);
const OHLoad *hwMatchConfiguration(void);
void hwMatchPrepareGame(void);
void hwMatchApplyTeams(void);
void hwMatchSeedRandom(void);
void hwMatchInitialDocking(void);
uint64_t hwMatchWorldSignature(void);
#ifdef __cplusplus
}
#endif
#endif
