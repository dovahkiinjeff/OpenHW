#ifndef OPENHW_REMASTERED_FX_RENDERER_H
#define OPENHW_REMASTERED_FX_RENDERER_H
#include "RemasteredFxResources.h"
#include "RemasteredFxPlayback.h"
#include "Camera.h"
/* Precache CPU resources once, upload on the first world-space drawing pass.
   No game-file reads, image decodes or HOD loads occur inside draw callbacks. */
int rmFxRendererStartup(HwRmFxResources *resources);
void rmFxRendererShutdown(void);
void rmFxRendererBegin(Camera *camera);
void rmFxRendererEnd(void);
void rmFxRendererDraw(void *user,const HwRmFxDraw *draw);
#endif
