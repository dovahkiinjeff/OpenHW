#ifndef HW_REMASTERED_BACKDROP_H
#define HW_REMASTERED_BACKDROP_H
#include "Types.h"
/* Prepare the current backdrop on the render thread during mission loading. */
bool32 remasteredBackdropPrepare(void);
bool32 remasteredBackdropRender(real32 fade);
bool32 remasteredBackdropActive(void);
void remasteredBackdropShutdown(void);
#endif
