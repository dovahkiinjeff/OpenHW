#ifndef OPENHW_BADGE_LIBRARY_H
#define OPENHW_BADGE_LIBRARY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Owned permanent copies under Badges; records are lazy decoded. */
void hwBadgeInit(void);
int hwBadgeProbeMain(int argc,char **argv);
void hwBadgeShutdown(void);
void hwBadgeDiscover(void);
int hwBadgeCount(void);
void hwBadgeInfo(int index,char *id,size_t idSize,char *label,size_t labelSize);
void hwBadgeStatus(char *text,size_t capacity);
int hwBadgeImportFile(const char *path,char *id,size_t idSize);
int hwBadgeCustomImage(void);
void hwBadgeSelect(const char *id,int persist);
void hwBadgeSelected(char *id,size_t capacity);
const unsigned char *hwBadgePixels(const char *id,unsigned *width,unsigned *height);
void hwBadgeSetRemote(unsigned slot,const unsigned char *rgba,unsigned width,unsigned height);
void hwBadgeClearRemote(void);
const unsigned char *hwBadgePlayerPixels(unsigned slot,int local,unsigned *width,unsigned *height,const void **identity);
#ifdef __cplusplus
}
#endif
#endif
