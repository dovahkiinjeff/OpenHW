#ifndef OPENHW_CATACLYSM_SCRIPT_BRIDGE_H
#define OPENHW_CATACLYSM_SCRIPT_BRIDGE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*CataScriptCallback)(void);
typedef enum CataScriptArgKind {CATA_ARG_NUMBER,CATA_ARG_OBJECT,CATA_ARG_TEXT,CATA_ARG_CALLBACK} CataScriptArgKind;
typedef struct CataScriptArg {
    CataScriptArgKind kind;
    union {intptr_t number;void *object;const char *text;CataScriptCallback callback;} value;
} CataScriptArg;
/* A finite, typed engine bridge. API IDs identify recovered semantics; they
   are never executable addresses and cannot call arbitrary original code. */
CataScriptArg cataScriptDispatch(unsigned api,const CataScriptArg *args,unsigned count);
void cataScriptFault(const char *message);
void *cataScriptCurrentTeam(void);
void *cataScriptCurrentShips(void);
int cataScriptMessageCount(void);
void cataScriptScope(int missionScope);
const char *cataScriptText(uint32_t originalDataAddress);
const char *cataScriptLocalizedText(uint32_t originalTableAddress);
void cataMission01Init(void);
void cataMission01Watch(void);
const void **cataMission01FunctionTable(void);
void cataMission02Init(void);
void cataMission02Watch(void);
const void **cataMission02FunctionTable(void);
#ifdef __cplusplus
}
#endif
#endif
