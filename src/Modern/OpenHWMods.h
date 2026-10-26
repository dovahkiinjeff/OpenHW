#ifndef OPENHW_MODS_H
#define OPENHW_MODS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OpenHWModEntry { char name[65]; int enabled; } OpenHWModEntry;
int openhwModsRead(const char* root,OpenHWModEntry* entries,int capacity,char* error,size_t errorSize);
int openhwModsSave(const char* root,const OpenHWModEntry* entries,int count,char* error,size_t errorSize);
/* Publish one validated ship and its native dependency folders. Never changes Data. */
int openhwModPublish(const char* root,const char* name,int append,const char* shipPath,
 const char* definition,size_t length,int replace,char* error,size_t errorSize);
#ifdef __cplusplus
}
#endif
#endif
