/* Only the legacy texture/state seams; text rasterization and drawing use the
   actual production ModernText.c and D3D12 recorder/replay. No game window. */
#include "glinc.h"
#include "render.h"
sdword MAIN_WindowWidth=32,MAIN_WindowHeight=32;
void trClearCurrent(void) {}
sdword rndTextureEnable(sdword enabled)
{
    sdword previous=glIsEnabled(GL_TEXTURE_2D);
    if(enabled) glEnable(GL_TEXTURE_2D);else glDisable(GL_TEXTURE_2D);
    return previous;
}
udword rndTextureEnvironment(udword mode)
{(void)mode;glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);return RTE_Modulate;}
sdword rndAdditiveBlends(sdword additive)
{glBlendFunc(GL_SRC_ALPHA,additive?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);return 0;}
