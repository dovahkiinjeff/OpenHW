/* Typed compatibility for our recovered Cataclysm script callbacks.
   Variables/timers use HW1's existing save stores; no executable addresses,
   32-bit struct offsets or integer-cast pointers are executed. */
#include "CataclysmKAS.h"
#include "AIPlayer.h"
#include "AITeam.h"
#include "AIVar.h"
#include "KASFunc.h"
#include "Timer.h"
#include "Debug.h"
#include <stdio.h>
extern AITeam *CurrentTeamP;

uint32_t cataKasScope;

void cataKasVarCreateSet(uint32_t id,sdword value)
{
    char key[32];AIVar *var;
    cataScriptVariableKey(cataKasScope,id,key,sizeof(key));
    var=aivarFind(key);if(!var)var=aivarCreate(key);
    aivarValueSet(var,value);
}
sdword cataKasVarGet(uint32_t id)
{
    char key[32];AIVar *var;
    cataScriptVariableKey(cataKasScope,id,key,sizeof(key));var=aivarFind(key);
    return var?aivarValueGet(var):0;
}
void cataKasVarSet(uint32_t id,sdword value)
{
    char key[32];AIVar *var;
    cataScriptVariableKey(cataKasScope,id,key,sizeof(key));var=aivarFind(key);
    if(var)aivarValueSet(var,value);
}
void cataKasVarIncrement(uint32_t id)
{
    char key[32];AIVar *var;
    cataScriptVariableKey(cataKasScope,id,key,sizeof(key));var=aivarFind(key);
    if(var)aivarValueSet(var,aivarValueGet(var)+1);
}
void cataKasTimerCreate(uint32_t id){char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));timTimerCreate(key);}
void cataKasTimerSet(uint32_t id,sdword duration){char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));timTimerSet(key,duration);}
void cataKasTimerStart(uint32_t id){char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));timTimerStart(key);}
void cataKasTimerCreateSetStart(uint32_t id,sdword duration){char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));timTimerCreateSetStart(key,duration);}
sdword cataKasTimerExpired(uint32_t id){char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));return timTimerExpired(key);}
sdword cataKasTimerExpiredDestroy(uint32_t id){char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));return timTimerExpiredDestroy(key);}

AITeam *cataKasTeam(uint32_t id)
{
    sdword i;if(!aiCurrentAIPlayer)return NULL;
    for(i=0;i<aiCurrentAIPlayer->teamsUsed;i++){
        AITeam *team=aiCurrentAIPlayer->teams[i];
        if(cataScriptHash(team->kasLabel)==id)return team;
    }
    return NULL;
}
GrowSelection *cataKasTeamShips(uint32_t id){AITeam *team=cataKasTeam(id);return team?&team->shipList:NULL;}
GrowSelection *cataKasSelection(uint32_t id)
{
    char key[32];cataScriptScopedKey(0,id,key,sizeof(key));return kasGrowSelectionPtr(key);
}
void cataKasFSMCreate(uint32_t id,KASInitFunction init,KASWatchFunction watch,AITeam *team)
{
    uint32_t scope=cataKasScope;char key[32];
    if(!team)dbgFatalf(DBG_Loc,"Unresolved Cataclysm scripted team %08X",id);
    cataScriptScopedKey(0,id,key,sizeof(key));kasFSMCreate(key,init,watch,team);cataKasScope=scope;
}
void cataKasJump(uint32_t id,KASInitFunction init,KASWatchFunction watch)
{
    char key[32];cataScriptScopedKey(0,id,key,sizeof(key));kasJump(key,init,watch);
}
void cataKasSend(AITeam *team,uint32_t message)
{
    cataKasSendFrom(CurrentTeamP,team,message);
}
/* Retail Cataclysm queues carry numeric messages and hold 64 entries. Store
   them in the existing saved variable bank rather than truncating IDs into
   pointers or changing the legacy HW1 MsgQueue/save layout. */
static sdword cataQueueValue(AITeam *team,sdword slot,sdword sender,bool32 set,sdword value)
{
    char key[32];AIVar *var;
    if(!team)return 0;
    snprintf(key,sizeof(key),"CQ%08X_%d_%d",cataScriptHash(team->kasLabel),slot,sender);
    var=aivarFind(key);
    if(set){if(!var)var=aivarCreate(key);aivarValueSet(var,value);}
    return var?aivarValueGet(var):0;
}
void cataKasSendFrom(AITeam *from,AITeam *to,uint32_t message)
{
    sdword count;
    if(!to)dbgFatal(DBG_Loc,"Cataclysm message target team is missing");
    count=cataQueueValue(to,-1,0,FALSE,0);
    if(count<0 || count>=64)dbgFatal(DBG_Loc,"Cataclysm mission message queue overflow");
    cataQueueValue(to,count,0,TRUE,(sdword)message);
    cataQueueValue(to,count,1,TRUE,from?(sdword)cataScriptHash(from->kasLabel):0);
    cataQueueValue(to,-1,0,TRUE,count+1);
}
sdword cataKasMessageCount(void){return cataQueueValue(CurrentTeamP,-1,0,FALSE,0);}
uint32_t cataKasMessagePop(sdword index)
{
    sdword count=cataKasMessageCount();uint32_t message,sender;
    if(index<0 || index>=count)return 0;
    message=(uint32_t)cataQueueValue(CurrentTeamP,index,0,FALSE,0);
    sender=(uint32_t)cataQueueValue(CurrentTeamP,index,1,FALSE,0);
    cataQueueValue(CurrentTeamP,index,0,TRUE,cataQueueValue(CurrentTeamP,count-1,0,FALSE,0));
    cataQueueValue(CurrentTeamP,index,1,TRUE,cataQueueValue(CurrentTeamP,count-1,1,FALSE,0));
    cataQueueValue(CurrentTeamP,-1,0,TRUE,count-1);
    CurrentTeamP->msgSender=sender?cataKasTeam(sender):NULL;
    return message;
}
sdword cataKasReceived(uint32_t message)
{
    sdword i,count=cataKasMessageCount();
    for(i=0;i<count;i++)if((uint32_t)cataQueueValue(CurrentTeamP,i,0,FALSE,0)==message){cataKasMessagePop(i);return 1;}
    return 0;
}
