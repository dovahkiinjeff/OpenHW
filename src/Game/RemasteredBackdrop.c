#include "../Modern/DiagnosticPolicy.h"
#include "RemasteredBackdrop.h"
#include "ModernMissionBackdrop.h"
#include "ModernGraphics.h"
#include "MissionEnum.h"
#include "BTG.h"
#include "BackdropMapPolicy.h"
#include "glinc.h"
#include "texreg.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern bool32 singlePlayerGame;
extern MissionEnum spGetCurrentMission(void);
static sdword loadedMission, attemptedMission;
static bool32 renderedBackdrop;
static udword skyTexture;
static GLuint starTexture;
static float *stars;
static unsigned int starCount;

static unsigned int readU32(const unsigned char *p)
{
    return p[0] | ((unsigned int)p[1]<<8) | ((unsigned int)p[2]<<16) | ((unsigned int)p[3]<<24);
}

#include "RemasteredBackdropScene.inc"

static void releaseBackdrop(void)
{
    releaseScene();
    if (skyTexture) { GLuint name = skyTexture; glDeleteTextures(1, &name); }
    if (starTexture) glDeleteTextures(1, &starTexture);
    SDL_free(stars);
    stars = NULL; starCount = 0; skyTexture = 0; starTexture = 0; loadedMission = 0;
    trClearCurrent();
}

void remasteredBackdropShutdown(void)
{
    renderedBackdrop = FALSE;
    releaseBackdrop();
    attemptedMission = 0;
}

static sdword backdropAssetMission(void)
{
    sdword mission = (sdword)spGetCurrentMission();
    if (singlePlayerGame) return mission >= 1 && mission <= 16 ? mission : 0;
    return hwBackdropAssetForName(btgLastBackground);
}

static bool32 loadBackdrop(sdword mission)
{
    char path[1024];
    char *base;
    size_t size = 0;
    unsigned char *bytes;
    unsigned int count, i;
    unsigned char radial[32*32*4];
    if (loadedMission == mission) return TRUE;
    if (attemptedMission == mission) return FALSE;
    releaseBackdrop();
    attemptedMission = mission;
    base = SDL_GetBasePath();
    if (base == NULL) return FALSE;
    if (getenv("HW_IMPORTED_ASSETS"))
        snprintf(path, sizeof(path), "%s/RemasteredBackgrounds/mission%02d_stars.bin", getenv("HW_IMPORTED_ASSETS"), mission);
    else
        snprintf(path, sizeof(path), "%sRemasteredBackgrounds/mission%02d_stars.bin", base, mission);
    bytes = SDL_LoadFile(path, &size);
    SDL_free(base);
    if (bytes == NULL) return FALSE;
    if (size < 8 || memcmp(bytes, "RMS1", 4) != 0)
    { SDL_free(bytes); return FALSE; }
    count = readU32(bytes+4);
    if (count > 100000 || size != 8+(size_t)count*28)
    { SDL_free(bytes); return FALSE; }
    stars = SDL_malloc((size_t)count*28);
    if (count && !stars) { SDL_free(bytes); return FALSE; }
    if (count) memcpy(stars, bytes+8, (size_t)count*28);
    SDL_free(bytes);
    for (i=0; i<count*7; ++i)
        if (!isfinite(stars[i])) { releaseBackdrop(); return FALSE; }
    for (i=0; i<count; ++i)
    {
        const float *star = stars+i*7;
        double length2 = (double)star[0]*star[0]+(double)star[1]*star[1]+(double)star[2]*star[2];
        if (length2 < 1.0e-12 || star[3] < 0 || star[4] < 0 || star[5] < 0 || star[6] < 0)
        { releaseBackdrop(); return FALSE; }
    }
    starCount = count;
    snprintf(path, sizeof(path), "RemasteredBackgrounds/mission%02d_sky.dds", mission);
    if (!modernMissionBackdropLoadSkyTexture(path, &skyTexture))
    { releaseBackdrop(); return FALSE; }
    base=SDL_GetBasePath();
    if (!loadScene(mission,getenv("HW_IMPORTED_ASSETS")?getenv("HW_IMPORTED_ASSETS"):base))
    { SDL_free(base); releaseBackdrop(); return FALSE; }
    SDL_free(base);
    for (i=0; i<32*32; ++i)
    {
        float x=((float)(i%32)+0.5f)/16.0f-1.0f;
        float y=((float)(i/32)+0.5f)/16.0f-1.0f;
        unsigned char value=(unsigned char)(255.0f*expf(-8.0f*(x*x+y*y)));
        radial[i*4]=radial[i*4+1]=radial[i*4+2]=value;
        radial[i*4+3]=255;
    }
    glGenTextures(1, &starTexture);
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, starTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, radial);
    trClearCurrent();
    loadedMission = mission;
    HW_DIAG_FPRINTF(stderr, "[RemasteredBackdrop] Mission %02d native sky + %u RM stars; BTG visuals bypassed.\n", mission, starCount);
    return TRUE;
}

bool32 remasteredBackdropRender(real32 fade)
{
    sdword mission = backdropAssetMission();
    renderedBackdrop = FALSE;
    if (mission == 0)
    {
        if (loadedMission) releaseBackdrop();
        attemptedMission = 0;
        return FALSE;
    }
    if (!loadBackdrop(mission)) return FALSE;
    if (fade < 0) fade = 1;
    if (fade > 1) fade = 1;
    if (!hwModernGraphicsRenderMissionSky(skyTexture, fade)) return FALSE;
    hwModernGraphicsRenderBackgroundStars(starTexture, stars, starCount, fade);
    renderScene(fade);
    renderedBackdrop = TRUE;
    return TRUE;
}

bool32 remasteredBackdropActive(void)
{
    return renderedBackdrop && loadedMission == backdropAssetMission();
}
