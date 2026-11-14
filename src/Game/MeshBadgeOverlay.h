#ifndef OPENHW_MESH_BADGE_OVERLAY_H
#define OPENHW_MESH_BADGE_OVERLAY_H
void meshBadgeSetMotionOwner(unsigned long long owner);
void meshBadgeRegister(void *object,const void *part,int badge,float scale);
void meshBadgeForget(void *mesh);
void meshBadgeShutdown(void);
void meshBadgeDraw(void *object,unsigned player,int local);
#endif
