#ifndef OPENHW_CATACLYSM_KAS_H
#define OPENHW_CATACLYSM_KAS_H
#include "KAS.h"
#include "../Modern/CataclysmScriptIds.h"
/* Scope is an original Cataclysm hash, not a pointer or an HW1 array index. */
extern uint32_t cataKasScope;
void cataKasVarCreateSet(uint32_t id,sdword value);
void cataKasVarSet(uint32_t id,sdword value);
sdword cataKasVarGet(uint32_t id);
void cataKasVarIncrement(uint32_t id);
void cataKasTimerCreate(uint32_t id);
void cataKasTimerSet(uint32_t id,sdword duration);
void cataKasTimerStart(uint32_t id);
void cataKasTimerCreateSetStart(uint32_t id,sdword duration);
sdword cataKasTimerExpired(uint32_t id);
sdword cataKasTimerExpiredDestroy(uint32_t id);
struct AITeam *cataKasTeam(uint32_t id);
GrowSelection *cataKasTeamShips(uint32_t id);
GrowSelection *cataKasSelection(uint32_t id);
Path *cataKasPath(uint32_t id);
Volume *cataKasVolume(uint32_t id);
hvector *cataKasVector(uint32_t id);
void cataKasVectorSet(uint32_t id,real32 x,real32 y,real32 z);
void cataKasFSMCreate(uint32_t id,KASInitFunction init,KASWatchFunction watch,struct AITeam *team);
void cataKasJump(uint32_t id,KASInitFunction init,KASWatchFunction watch);
void cataKasSend(struct AITeam *team,uint32_t message);
sdword cataKasReceived(uint32_t message);
sdword cataKasMessageCount(void);
uint32_t cataKasMessagePop(sdword index);
void cataKasSendFrom(struct AITeam *from,struct AITeam *to,uint32_t message);
#endif
