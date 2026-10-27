/*=============================================================================
    Name    : HorseRace.c
    Purpose : Logic for the horse race and loading chat screens.

    Created 6/16/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <SDL2/SDL.h>
#include "stb_image.h"

#include "HorseRace.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "AutoDownloadMap.h"
#include "Chatting.h"
#include "CommandNetwork.h"
#include "Debug.h"
#include "Demo.h"
#include "ETG.h"
#include "FEFlow.h"
#include "FEReg.h"
#include "File.h"
#include "font.h"
#include "FontReg.h"
#include "glinc.h"
#include "LinkedList.h"
#include "main.h"
#include "Memory.h"
#include "mouse.h"
#include "MultiplayerGame.h"
#include "NIS.h"
#include "prim2d.h"
#include "Region.h"
#include "render.h"
#include "SinglePlayer.h"
#include "StringSupport.h"
#include "Task.h"
#include "Teams.h"
#include "texreg.h"
#include "TimeoutTimer.h"
#include "Titan.h"
#include "Tweak.h"
#include "UIControls.h"
#include "uiscale.h"
#include "Universe.h"
#include "utility.h"
#include "MissionLoadProgress.h"

#ifdef HW_ENABLE_D3D12_BACKEND
#include "ModernGraphics.h"
#endif

#ifdef _WIN32
    #define strcasecmp _stricmp
    #include <direct.h>
    #include <io.h>
    #include <windows.h>
#else
    #include <dirent.h>
    #include <unistd.h>
#endif


#define ShouldHaveMousePtr (FALSE)

extern Uint32 utyTimerLast;

/*=============================================================================
    defines:
=============================================================================*/

#ifdef __APPLE__
    #define HR_SCALE_MISSION_LOADING_SCREENS  TRUE
#else
    #define HR_SCALE_MISSION_LOADING_SCREENS  FALSE
#endif

#define HR_PlayerNameFont   "Arial_12.hff"
#define MAX_CHAT_TEXT       64
#define NUM_CHAT_LINES      10

real32 HorseRacePlayerDropoutTime = 10.0f;     // tweakable
color HorseRaceDropoutColor = colRGB(75,75,75);

/*=============================================================================
    data:
=============================================================================*/

/*extern HDC hGLDeviceContext;*/

static rectangle hrSinglePlayerPos;
static color hrSinglePlayerLoadingBarColor = colRGB(255,63,63);  // pastel red
static fonthandle hrModernHeadingFont;
static fonthandle hrModernBodyFont;

static sdword JustInit;
static sdword localbar;
static bool32 hrLoadingDrawInProgress = FALSE;
static Uint64 hrLoadingLastDraw = 0;

// Pixels and info about the background image chosen
static bool32 hrBackgroundInitFrame = 0;
static long hrBackgroundDirty = 0;
bool32 hrBackgroundReinit = FALSE;
static long hrBackXSize, hrBackYSize;
static GLfloat hrBackXFrac, hrBackYFrac;
static GLuint hrBackgroundTexture = 0;

static bool32 hrScaleMissionLoadingScreens = HR_SCALE_MISSION_LOADING_SCREENS;

static regionhandle hrDecRegion;

HorseStatus horseracestatus;
static udword hrProgressCounter = 0;
extern HorseRaceBars horseBarInfo;

TTimer hrPlayerDropoutTimers[MAX_MULTIPLAYER_PLAYERS];

fibfileheader *hrScreensHandle = NULL;

fonthandle      playernamefont=0;
color           hrBackBarColor=HR_BackBarColor;
color           hrChatTextColor=HR_ChatTextColor;

bool32            PlayersAlreadyDrawnDropped[MAX_MULTIPLAYER_PLAYERS];

textentryhandle ChatTextEntryBox = NULL;

regionhandle   hrBaseRegion       = NULL;
regionhandle   hrProgressRegion   = NULL;
regionhandle   hrChatBoxRegion    = NULL;
regionhandle   hrAbortLoadConfirm = NULL;

region horseCrapRegion =
{
    {-1,-1,-1,-1},             // rectangle
    NULL,                      // draw function
    NULL,                      // process function
    NULL, NULL,                // parent, child
    NULL, NULL,                // previous, next
    0, 0,                      // flags, status,
    0,                         // nKeys
    {0},                       // keys
    0,                         // userID
#if REG_ERROR_CHECKING
    REG_ValidationKey,         // validation key
#endif
    0,                         // tabstop
    UNINITIALISED_LINKED_LIST, // cutouts;
    {0,0},                     // drawstyle[2];
    0,                         // lastframedrawn;
    NULL                       // atom;
};

bool32 hrRunning=FALSE;

ChatPacket chathistory[NUM_CHAT_LINES];

udword      chatline=0;

void horseRaceRender(void);
void hrDirtyProgressBar(void);

static double hrLoadingSeconds(void)
{
    return (double)SDL_GetTicks64() / 1000.0;
}

#include "LoadingProgressBridge.inc"

void HorseRaceLoadingRefresh(void)
{
    Uint64 now = SDL_GetTicks64();
    if (!hrRunning || hrLoadingDrawInProgress ||
        (hrLoadingLastDraw != 0 && now - hrLoadingLastDraw < 100)) return;
    hrDirtyProgressBar();
    horseRaceRender();
}
void hrDrawPlayersProgress(featom *atom, regionhandle region);
void hrDrawChatBox(featom *atom, regionhandle region);
void hrDrawFile(char* filename, sdword x, sdword y);
void hrChatTextEntry(char *name, featom *atom);
void hrAbortLoadingYes(char *name, featom *atom);
void hrAbortLoadingNo(char *name, featom *atom);

real32 horseRaceGetPacketPercent(real32 barPercent);

//HR_ChatTextEntry
//extern nisheader *utyTeaserHeader;

/*=============================================================================
    Functions
=============================================================================*/
fedrawcallback hrDrawCallback[] =
{
    {hrDrawPlayersProgress,     "HR_DrawPlayersProgress"    },
    {hrDrawChatBox,             "HR_DrawChatBox"            },
    {NULL,          NULL}
};

fecallback hrCallBack[] =
{
    {hrChatTextEntry,           "HR_ChatTextEntry"          },
    {hrAbortLoadingYes,         "HR_AbortLoadingYes"        },
    {hrAbortLoadingNo,          "HR_AbortLoadingNo"         },
    {NULL,          NULL}
};

void hrDirtyProgressBar(void)
{
    if (hrRunning)
    {
        if (hrProgressRegion != NULL)
        {
#ifdef DEBUG_STOMP
            regVerify(hrProgressRegion);
#endif
            hrProgressRegion->status |= RSF_DrawThisFrame;
        }
    }
}

void hrDirtyChatBox(void)
{
    if (hrRunning)
    {
        if (hrChatBoxRegion != NULL)
        {
#ifdef DEBUG_STOMP
            regVerify(hrChatBoxRegion);
#endif
            hrChatBoxRegion->status |= RSF_DrawThisFrame;
        }
    }
}

void hrBarDraw(rectangle *rect, color back, color fore, real32 percent)
{
    rectangle temp;
//  primRectSolid2(rect, back);

    if (percent > 1.0)
    {
        percent = 1.0;
    }

    temp.x0 = rect->x0;
    temp.y0 = rect->y0;
    temp.x1 = rect->x0 + (sdword)((rect->x1-rect->x0)*percent);
    temp.y1 = rect->y1;

    primRectSolid2(&temp, fore);
}

/* Fit using the game's actual current font metrics. Resource paths never
   overwrite the timer/percentage or escape the loading panel. */
static void hrLoadingPrintFit(sdword x, sdword y, sdword width, color tint, const char *text)
{
    char fitted[HW_MISSION_LOAD_TEXT_SIZE];
    size_t length;
    if (width <= 0 || text == NULL) return;
    snprintf(fitted, sizeof(fitted), "%s", text);
    length = strlen(fitted);
    if (fontWidth(fitted) > width)
    {
        while (length > 0)
        {
            fitted[--length] = 0;
            if (length + 3 < sizeof(fitted))
            {
                memcpy(fitted + length, "...", 4);
                if (fontWidth(fitted) <= width) break;
                fitted[length] = 0;
            }
        }
        if (length == 0 && fontWidth(fitted) > width) fitted[0] = 0;
    }
    fontPrint(x, y, tint, fitted);
}

static void hrLoadingElapsed(double elapsed, char *text, size_t capacity)
{
    unsigned long long seconds = (unsigned long long)fmax(0.0, elapsed);
    if (seconds >= 3600)
        snprintf(text, capacity, "%llu:%02llu:%02llu", seconds / 3600,
            seconds / 60 % 60, seconds % 60);
    else
        snprintf(text, capacity, "%llu:%02llu", seconds / 60, seconds % 60);
}

static void hrDrawLoadingPanel(void)
{
    real32 percent;
    fonthandle currentfont = fontMakeCurrent(playernamefont);
        static const char *missionTitles[] = {
            "UNKNOWN DESTINATION", "KHARAK SYSTEM",
            "OUTSKIRTS OF KHARAK SYSTEM", "RETURN TO KHARAK",
            "GREAT WASTELANDS", "GREAT WASTELANDS", "DIAMOND SHOALS",
            "GARDENS OF KADESH", "CATHEDRAL OF KADESH",
            "SEA OF LOST SOULS", "SUPER NOVA RESEARCH STATION",
            "TENHAUSER GATE", "GALACTIC CORE", "THE KAROS GRAVEYARD",
            "BRIDGE OF SIGHS", "CHAPEL PERILOUS", "HIIGARA"
        };
        static const char *stages[] = {
            "PREPARING MISSION", "LOADING FLEET ASSETS", "BUILDING MISSION WORLD",
            "LOADING EFFECTS", "PREPARING TEXTURES", "UPLOADING TEXTURES",
            "FINALIZING COMMAND SYSTEMS"
        };
        HWMissionLoadProgressSnapshot loading;
        rectangle panel;
        rectangle bar;
        rectangle fill;
        rectangle accent;
        char mission[96];
        char line[256];
        char elapsed[32], taskElapsed[32];
        sdword margin, padding, bodyHeight, headingHeight, lineHeight, gap;
        sdword historyRows = HW_MISSION_LOAD_HISTORY_LINES;
        sdword networkRows = multiPlayerGame ? sigsNumPlayers : 0;
        if (networkRows < 0 || networkRows > MAX_MULTIPLAYER_PLAYERS) networkRows = 0;
        sdword panelHeight, x, y, textWidth, row;
        sdword missionIndex = spGetCurrentMission();
        hwMissionLoadProgressSnapshot(&loading);
        percent = (real32)loading.percentHundredths / 10000.0f;
        if (hrModernBodyFont == 0)
        {
            hrModernBodyFont = frFontRegister("simplixssk_13.hff");
            hrModernHeadingFont = frFontRegister("eurosewideheavy_i17.hff");
            if (hrModernHeadingFont == 0) hrModernHeadingFont = hrModernBodyFont;
        }
        fontMakeCurrent(hrModernHeadingFont);
        headingHeight = fontHeight(" ");
        fontMakeCurrent(hrModernBodyFont);
        bodyHeight = fontHeight(" ");
        gap = uiScaleSize(3);
        if (gap < 1) gap = 1;
        lineHeight = bodyHeight + gap;
        margin = uiScaleSize(28);
        padding = uiScaleSize(18);
        if (margin < 4) margin = 4;
        if (padding < 4) padding = 4;
        panelHeight = padding * 2 + headingHeight + lineHeight * (6 + historyRows + networkRows + (networkRows > 0)) + uiScaleSize(10);
        while (historyRows > 0 && panelHeight > MAIN_WindowHeight * 3 / 5)
        {
            --historyRows;
            panelHeight -= lineHeight;
        }
        panel.x0 = margin;
        panel.x1 = MAIN_WindowWidth - margin;
        panel.y1 = MAIN_WindowHeight - margin;
        panel.y0 = panel.y1 - panelHeight;
        if (panel.y0 < margin) panel.y0 = margin;
        primRectTranslucent2(&panel, colRGBA(8, 15, 21, 226));
        primRectOutline2(&panel, 1, colRGB(78, 105, 114));
        accent = panel;
        accent.x1 = accent.x0 + uiScaleSize(4);
        primRectSolid2(&accent, colRGB(222, 162, 62));
        if (!singlePlayerGame)
            snprintf(mission, sizeof(mission), "%s / %.70s", multiPlayerGame ? "MULTIPLAYER" : "SKIRMISH",
                tpGameCreated.DisplayMapName[0] ? tpGameCreated.DisplayMapName : "LOCAL BATTLE");
        else if (missionIndex >= 0 && missionIndex <
            (sdword)(sizeof(missionTitles) / sizeof(missionTitles[0])))
            snprintf(mission, sizeof(mission), "MISSION %02d / %s",
                     missionIndex, missionTitles[missionIndex]);
        else
            snprintf(mission, sizeof(mission), "MISSION %02d / FLEET TRANSIT",
                     missionIndex);
        x = panel.x0 + padding;
        y = panel.y0 + padding;
        textWidth = panel.x1 - padding - x;
        hrLoadingPrintFit(x, y, textWidth, colRGB(222, 162, 62), "FLEET COMMAND / TRANSIT");
        y += lineHeight;
        fontMakeCurrent(hrModernHeadingFont);
        hrLoadingPrintFit(x, y, textWidth, colRGB(225, 222, 206), mission);
        y += headingHeight + gap;
        fontMakeCurrent(hrModernBodyFont);
        snprintf(line, sizeof(line), "%s", loading.currentPhase >= 0 &&
            loading.currentPhase < HW_MISSION_LOAD_PHASE_COUNT ? stages[loading.currentPhase] : "PREPARING MISSION");
        hrLoadingPrintFit(x, y, textWidth, colRGB(105, 184, 196), line);
        y += lineHeight;
        hrLoadingPrintFit(x, y, textWidth, colRGB(225, 222, 206), loading.task);
        y += lineHeight;
        hrLoadingElapsed(hrLoadingSeconds() - loading.startedSeconds, elapsed, sizeof(elapsed));
        hrLoadingElapsed(hrLoadingSeconds() - loading.taskStartedSeconds, taskElapsed, sizeof(taskElapsed));
        if (loading.totalItems != 0)
            snprintf(line, sizeof(line), "%u / %u ITEMS  |  ELAPSED %s  |  TASK %s",
                loading.completedItems, loading.totalItems, elapsed, taskElapsed);
        else
            snprintf(line, sizeof(line), "ELAPSED %s  |  TASK %s", elapsed, taskElapsed);
        hrLoadingPrintFit(x, y, textWidth, colRGB(143, 157, 157), line);
        y += lineHeight;
        if (networkRows > 0)
        {
            hrLoadingPrintFit(x, y, textWidth, colRGB(222, 162, 62), "PLAYER READINESS");
            y += lineHeight;
            for (row=0;row<networkRows;++row)
            {
                real32 ready=horseracestatus.percent[row];
                if (!isfinite(ready) || ready < 0) ready=0;
                if (ready > 1) ready=1;
                snprintf(line,sizeof(line),"%.80s / %u%% / %.100s",
                    tpGameCreated.playerInfo[row].PersonalName,(unsigned)(ready*100),
                    playerHasDroppedOutOrQuit(row) ? "DISCONNECTED" : horseracestatus.hrstatusstr[row]);
                hrLoadingPrintFit(x,y,textWidth,colRGB(143,157,157),line);
                y += lineHeight;
            }
        }
        hrLoadingPrintFit(x, y, textWidth, colRGB(222, 162, 62), "RECENT ACTIVITY");
        y += lineHeight;
        for (row = 0; row < historyRows; ++row)
        {
            hrLoadingPrintFit(x, y, textWidth, colRGB(143, 157, 157), loading.history[row]);
            y += lineHeight;
        }
        snprintf(line, sizeof(line), "%u.%02u%% WORK COMPLETED  /  %u OF %u PHASES COMPLETE",
            loading.percentHundredths / 100, loading.percentHundredths % 100,
            loading.phasesComplete, loading.totalPhases);
        hrLoadingPrintFit(x, y, textWidth, colRGB(225, 222, 206), line);
        bar.x0 = x;
        bar.x1 = panel.x1 - padding;
        bar.y0 = y + lineHeight;
        bar.y1 = bar.y0 + uiScaleSize(7);
        primRectSolid2(&bar, colRGB(25, 37, 44));
        fill = bar;
        fill.x1 = fill.x0 + (sdword)((fill.x1 - fill.x0) * percent);
        primRectSolid2(&fill, colRGB(105, 184, 196));
        primRectOutline2(&bar, 1, colRGB(78, 105, 114));
        ++hrProgressCounter;
    fontMakeCurrent(currentfont);
}

void hrDrawPlayersProgress(featom *atom, regionhandle region)
{
    /* Rendering is owned by horseRaceRender, independent of the old FIB's
       single-player-only progress region and its dirty flags. */
    hrProgressRegion = region;
}

void hrDrawChatBox(featom *atom, regionhandle region)
{
    fonthandle currentfont;
    sdword     x,y,i;
    char       name[128];

    hrChatBoxRegion = region;

    currentfont = fontMakeCurrent(playernamefont);

    primRectSolid2(&region->rect,colRGB(0,0,0));
    feStaticRectangleDraw(region);

    x = region->rect.x0+10;
    y = region->rect.y0;

    for (i=0;i<NUM_CHAT_LINES;i++)
    {
        if (chathistory[i].message[0]!=0)
        {
            x = region->rect.x0;
            sprintf(name,"%s >",tpGameCreated.playerInfo[chathistory[i].packetheader.frame].PersonalName);
            fontPrintf(x,y,tpGameCreated.playerInfo[chathistory[i].packetheader.frame].baseColor,"%s",name);
            x+=fontWidth(name)+10;
            //fontShadowSet(FS_E | FS_SE | FS_S);
            fontPrintf(x,y,hrChatTextColor,"%s",chathistory[i].message);
            //fontShadowSet(FS_NONE);
            y+= fontHeight(" ");
        }
    }

    fontMakeCurrent(currentfont);
}

void hrChooseSinglePlayerBitmap(char* pFilenameBuffer)
{
    char fname[128];
    sdword x, y, width, height;

    memset(&hrSinglePlayerPos, 0, sizeof(hrSinglePlayerPos));

    //image itself
#if defined(HW_GAME_RAIDER_RETREAT)
    if (spGetCurrentMission() == MISSION_5B_TURANIC_RAIDER_PLANETOID)
    {
#ifdef _WIN32
        sprintf(fname, "SinglePlayer\\mission05_OEM\\loading.jpg");
#else
        sprintf(fname, "SinglePlayer/mission05_OEM/loading.jpg");
#endif
    }
    else
#endif
    {
#ifdef _WIN32
        sprintf(fname, "SinglePlayer\\mission%02d\\loading.jpg", spGetCurrentMission());
#else
        sprintf(fname, "SinglePlayer/mission%02d/loading.jpg", spGetCurrentMission());
#endif
    }

    /* Optional native high-resolution replacement, preserving the original
       mission artwork as a fallback. stb_image detects the PNG format. */
    {
        char highResolution[128];char *name=strstr(fname,"loading.jpg");
        if(name){
            snprintf(highResolution,sizeof(highResolution),"%.*sloading_4k.png",(int)(name-fname),fname);
            if(fileExists(highResolution,0))strcpy(fname,highResolution);
        }
    }
    if (!fileExists(fname, 0))
    {
        pFilenameBuffer[0] = '\0';
        return;
    }

    strcpy(pFilenameBuffer, fname);

    // loading bar position
    // NB: the single player loading images are not all aligned properly...
    #define SP_LOADING_IMAGE_PROGRESS_BAR_X        43
    #define SP_LOADING_IMAGE_PROGRESS_BAR_Y       133
    #define SP_LOADING_IMAGE_PROGRESS_BAR_WIDTH   151
    #define SP_LOADING_IMAGE_PROGRESS_BAR_HEIGHT    2

    x      = hrScaleMissionLoadingScreens
           ? feResRepositionScaledX (SP_LOADING_IMAGE_PROGRESS_BAR_X)
           : feResRepositionCentredX(SP_LOADING_IMAGE_PROGRESS_BAR_X);
      
    y      = hrScaleMissionLoadingScreens
           ? feResRepositionScaledY (SP_LOADING_IMAGE_PROGRESS_BAR_Y)
           : feResRepositionCentredY(SP_LOADING_IMAGE_PROGRESS_BAR_Y);
    
    width  = hrScaleMissionLoadingScreens
           ? (SP_LOADING_IMAGE_PROGRESS_BAR_WIDTH  * FE_SCALE_TO_FIT_FACTOR_RELIC_SCREEN)
           :  SP_LOADING_IMAGE_PROGRESS_BAR_WIDTH;
    
    height = hrScaleMissionLoadingScreens
           ? (SP_LOADING_IMAGE_PROGRESS_BAR_HEIGHT * FE_SCALE_TO_FIT_FACTOR_RELIC_SCREEN)
           :  SP_LOADING_IMAGE_PROGRESS_BAR_HEIGHT;
    
    hrSinglePlayerPos.x0 = x;
    hrSinglePlayerPos.y0 = y;
    hrSinglePlayerPos.x1 = x + width;
    hrSinglePlayerPos.y1 = y + height;
}

void hrChooseRandomBitmap(char *output)
{
    char directory[PATH_MAX], pattern[PATH_MAX], line[PATH_MAX];
    int count = 0, wanted, pass;
    filehandle handle;
    output[0] = '\0';
    snprintf(directory, sizeof(directory), "%s", filePathPrepend("ScreenShots", FF_UserSettingsPath));
    wanted = 0;
    for (pass = 0; pass < 2; ++pass)
    {
        int index = 0;
#ifdef _WIN32
        struct _finddata_t entry;
        intptr_t search;
        snprintf(pattern, sizeof(pattern), "%s/*.jpg", directory);
        search = _findfirst(pattern, &entry);
        if (search != -1)
        {
            do
            {
                if (entry.attrib & (_A_SUBDIR | _A_HIDDEN)) continue;
                if (pass == 1 && index == wanted)
                    snprintf(output, PATH_MAX, "%s/%s", directory, entry.name);
                ++index;
            } while (_findnext(search, &entry) == 0);
            _findclose(search);
        }
#else
        DIR *dp = opendir(directory);
        struct dirent *entry;
        if (dp != NULL)
        {
            while ((entry = readdir(dp)) != NULL)
            {
                size_t n = strlen(entry->d_name);
                if (entry->d_name[0] == '.' || n < 4 ||
                    strcasecmp(entry->d_name + n - 4, ".jpg") != 0) continue;
                if (pass == 1 && index == wanted)
                    snprintf(output, PATH_MAX, "%s/%s", directory, entry->d_name);
                ++index;
            }
            closedir(dp);
        }
#endif
        if (pass == 0)
        {
            count = index;
            if (count == 0) break;
            wanted = (utyTimerLast % 32777) % count;
        }
    }
    if (output[0] != '\0') return;
    /* The screenshot list is optional and may not ship with a compact install.
       Close it on both passes; repeated skirmishes must not exhaust file slots. */
    for (pass = 0; pass < 2; ++pass)
    {
        int index = 0;
        handle = fileOpen("ScreenShots/ShotList.script", FF_ReturnNULLOnFail | FF_TextMode);
        if (!handle) return;
        while (fileLineRead(handle, line, sizeof(line)) != FR_EndOfFile)
        {
            if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;
            if (pass == 1 && index == wanted)
                snprintf(output, PATH_MAX, "ScreenShots/%s", line);
            ++index;
        }
        fileClose(handle);
        if (pass == 0)
        {
            if (index == 0) return;
            wanted = (utyTimerLast % 32777) % index;
        }
    }
}

typedef struct
{
    unsigned char v[2][2];
} ColorQuad;


//
// Takes X & Y values normalized from 0 to 1
// returns a bilinear interpolated pixel value
//
unsigned char hrBilinear(ColorQuad *pQuad, double X, double Y)
{
    float Top, Bot, Final;
    long FinalLong;

    Top = (float)pQuad->v[0][0] + ((float)pQuad->v[1][0] - (float)pQuad->v[0][0]) * X;
    Bot = (float)pQuad->v[0][1] + ((float)pQuad->v[1][1] - (float)pQuad->v[0][1]) * X;

    Final = (Top + (Bot - Top) * Y) + 0.5f;

    FinalLong = (long)Final;

    // Clamp it to 0 - 255
    if(FinalLong > 255) FinalLong = (FinalLong >> 31) & 0xff;
    return (unsigned char)FinalLong;
}

unsigned long hrGetInterpPixel(unsigned char *pSrcImg, long XSize, float Xf, float Yf)
{
    unsigned long Result, x, y, xx, yy;
    unsigned char *pCol;
    double xfrac, yfrac, xint, yint;
    ColorQuad r, g, b;

    x = (unsigned long)Xf;
    y = (unsigned long)Yf;

    xfrac = modf(Xf, &xint);
    yfrac = modf(Yf, &yint);

    for(yy=0; yy<2; yy++)
    {
        for(xx=0; xx<2; xx++)
        {
            pCol = &pSrcImg[ (((y+yy) * XSize) + x+xx) * 3 ];
            r.v[xx][yy] = pCol[0];
            g.v[xx][yy] = pCol[1];
            b.v[xx][yy] = pCol[2];
        }
    }

    Result  = (unsigned long)hrBilinear(&r, xfrac, yfrac);
    Result |= (unsigned long)hrBilinear(&g, xfrac, yfrac) << 8;
    Result |= (unsigned long)hrBilinear(&b, xfrac, yfrac) << 16;
    Result |= 0xff000000;

    return Result;
}


long hrShipsToLoadForRace(ShipRace shiprace)
{
    long ShipsToLoad = 0;
    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    ShipStaticInfo *shipstaticinfo;

    firstshiptype = FirstShipTypeOfRace[shiprace];
    lastshiptype = LastShipTypeOfRace[shiprace];
    for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
    {
        shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);

        if( bitTest(shipstaticinfo->staticheader.infoFlags, IF_InfoNeeded) && !bitTest(shipstaticinfo->staticheader.infoFlags, IF_InfoLoaded) )
            ShipsToLoad++;
    }

    return ShipsToLoad;
}

static real32 hrArtworkCubic(real32 x)
{
    x=fabsf(x);
    if(x<1)return 1.5f*x*x*x-2.5f*x*x+1;
    if(x<2)return -.5f*x*x*x+2.5f*x*x-4*x+2;
    return 0;
}

/* Separable Catmull-Rom reconstruction avoids magnifying bilinear blocks.
   Only the upload copy changes; original mission image files stay intact. */
static unsigned char *hrUpscaleArtwork(const unsigned char *source,int w,int h,int outW,int outH)
{
    float *rows;unsigned char *output;int x,y,c,tap;
    rows=malloc((size_t)outW*h*3*sizeof(float));output=malloc((size_t)outW*outH*3);
    if(!rows||!output){free(rows);free(output);return NULL;}
    for(x=0;x<outW;x++){
        float p=(x+.5f)*w/outW-.5f;int base=(int)floorf(p);float weights[4];
        for(tap=0;tap<4;tap++)weights[tap]=hrArtworkCubic(p-(base+tap-1));
        for(y=0;y<h;y++)for(c=0;c<3;c++){
            float value=0;for(tap=0;tap<4;tap++){int sx=base+tap-1;if(sx<0)sx=0;if(sx>=w)sx=w-1;value+=source[((size_t)y*w+sx)*3+c]*weights[tap];}
            rows[((size_t)y*outW+x)*3+c]=value;
        }
    }
    for(y=0;y<outH;y++){
        float p=(y+.5f)*h/outH-.5f;int base=(int)floorf(p);float weights[4];
        for(tap=0;tap<4;tap++)weights[tap]=hrArtworkCubic(p-(base+tap-1));
        for(x=0;x<outW;x++)for(c=0;c<3;c++){
            float value=0;for(tap=0;tap<4;tap++){int sy=base+tap-1;if(sy<0)sy=0;if(sy>=h)sy=h-1;value+=rows[((size_t)sy*outW+x)*3+c]*weights[tap];}
            if(value<0)value=0;if(value>255)value=255;output[((size_t)y*outW+x)*3+c]=(unsigned char)(value+.5f);
        }
    }
    free(rows);return output;
}

#include "LoadingArtwork.inc"

void hrInitBackground(void)
{
    char hrImageName[PATH_MAX];
    ubyte *fileData;
    int width, height, channels;

    hrImageName[0] = 0;
    if (singlePlayerGame)
    {
        hrChooseSinglePlayerBitmap(hrImageName);
    }
    else
    {
        hrChooseRandomBitmap(hrImageName);
    }

    /* No screenshot is a normal outcome on a fresh installation. */
    {
        int fileSize = 0;
        fileData = hrReadOptionalArtwork(hrImageName, &fileSize);
        if (fileData == NULL)
            fileData = hrReadOptionalArtwork("UI/concept_hangar.png", &fileSize);
        if (fileData == NULL) return;
    {

        unsigned char *pixels = stbi_load_from_memory(fileData, fileSize, &width, &height, &channels, 3);
        unsigned char *upscaled=NULL;GLint unpackAlignment;
        if(!pixels||width<=0||height<=0){if(pixels)stbi_image_free(pixels);free(fileData);return;}
        {
            float factor=fminf(3840.0f/width,2160.0f/height);
            if(factor>1){
                int outW=(int)(width*factor+.5f),outH=(int)(height*factor+.5f);
                upscaled=hrUpscaleArtwork(pixels,width,height,outW,outH);
                if(upscaled){width=outW;height=outH;}
            }
        }
        
        hrBackXSize = width;
        hrBackYSize = height;

        glGenTextures(1, &hrBackgroundTexture);
        trClearCurrent();
        glBindTexture(GL_TEXTURE_2D, hrBackgroundTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpackAlignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, hrBackXSize, hrBackYSize,
                    0, GL_RGB, GL_UNSIGNED_BYTE, upscaled?upscaled:pixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT,unpackAlignment);free(upscaled);
        
        stbi_image_free(pixels);
        free(fileData);
        
        hrBackXFrac = 1.0f;
        hrBackYFrac = 1.0f;
    }
    }
}

void hrRectSolidTextured2(rectangle *rect)
{
    GLfloat t[8] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };
    GLfloat v[8] = { primScreenToGLX(rect->x0), primScreenToGLY(rect->y1 - 1),
                     primScreenToGLX(rect->x1), primScreenToGLY(rect->y1 - 1),
                     primScreenToGLX(rect->x0), primScreenToGLY(rect->y0),
                     primScreenToGLX(rect->x1), primScreenToGLY(rect->y0) };

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    rndTextureEnvironment(RTE_Replace);
    rndTextureEnable(TRUE);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, v);
    glTexCoordPointer(2, GL_FLOAT, 0, t);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    rndTextureEnable(FALSE);
    rndTextureEnvironment(RTE_Modulate);
}

//don't mind if this is inefficient as it only
//gets called once per image anyway
void hrDrawFile(char* filename, sdword x, sdword y)
{
    udword handle;
    rectangle rect;
    lifheader* lif = trLIFFileLoad(filename, Pyrophoric);

    rndTextureEnable(TRUE);
    rndAdditiveBlends(FALSE);
    glEnable(GL_BLEND);

    glGenTextures(1, &handle);
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lif->width, lif->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, lif->data);
    
    // FIXME: there's no LiF scaling here, just translation of the image
    x = hrScaleMissionLoadingScreens
      ? feResRepositionScaledX(x)
      : feResRepositionCentredX(x);
      
    y = hrScaleMissionLoadingScreens
      ? feResRepositionScaledY(y)
      : feResRepositionCentredY(y);
    
    x -= lif->width  >> 1;
    y -= lif->height >> 1;
    rect.x0 = x;
    rect.y0 = y;
    rect.x1 = x + lif->width;
    rect.y1 = y + lif->height;
    hrRectSolidTextured2(&rect);

    glDeleteTextures(1, &handle);
    memFree(lif);

    glDisable(GL_BLEND);
}

void hrDrawBackground(void)
{
    rndClearToBlack();

    if (hrBackgroundTexture)
    {
        real32 imageAspect = (real32)hrBackXSize / (real32)hrBackYSize;
        real32 screenAspect = (real32)MAIN_WindowWidth / (real32)MAIN_WindowHeight;
        real32 u0 = 0.0f, u1 = hrBackXFrac;
        real32 v0 = 0.0f, v1 = hrBackYFrac;
        real32 drawWidth=(real32)MAIN_WindowWidth,drawHeight=(real32)MAIN_WindowHeight,left,top;
        GLfloat v[8], t[8];

        sdword oldTex = rndTextureEnable(TRUE);
        udword oldMode = rndTextureEnvironment(RTE_Replace);
        bool32 cull = glIsEnabled(GL_CULL_FACE) ? TRUE : FALSE;
        bool32 blend = glIsEnabled(GL_BLEND) ? TRUE : FALSE;
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);

        trClearCurrent();
        glBindTexture(GL_TEXTURE_2D, hrBackgroundTexture);

        /* Fit the complete artwork. Preserve its proportions on widescreen,
           ultrawide and portrait displays without zooming/cropping details. */
        if (screenAspect > imageAspect)
        {
            drawWidth=drawHeight*imageAspect;
        }
        else
        {
            drawHeight=drawWidth/imageAspect;
        }
        t[0] = u0; t[1] = v0;
        t[2] = u1; t[3] = v0;
        t[4] = u0; t[5] = v1;
        t[6] = u1; t[7] = v1;

        left=(MAIN_WindowWidth-drawWidth)*.5f;top=(MAIN_WindowHeight-drawHeight)*.5f;
        v[0] = primScreenToGLX(left);           v[1] = primScreenToGLY(top);
        v[2] = primScreenToGLX(left+drawWidth); v[3] = primScreenToGLY(top);
        v[4] = primScreenToGLX(left);           v[5] = primScreenToGLY(top+drawHeight);
        v[6] = primScreenToGLX(left+drawWidth); v[7] = primScreenToGLY(top+drawHeight);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, t);
        glVertexPointer(2, GL_FLOAT, 0, v);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        rndTextureEnvironment(oldMode);
        rndTextureEnable(oldTex);
        if (cull) glEnable(GL_CULL_FACE);
        if (!blend) glDisable(GL_BLEND);
    }
}

void hrShutdownBackground(void)
{
    // free the memory required by the cached background bitmap

    if (hrBackgroundTexture != 0)
    {
        glDeleteTextures(1, &hrBackgroundTexture);
        hrBackgroundTexture = 0;
    }
    hrBackgroundInitFrame = 0;
    hrBackgroundReinit = FALSE;
}

void hrAbortLoadingYes(char *name, featom *atom)
{
    hrAbortLoadingGame = TRUE;

    if (multiPlayerGame)
        SendDroppingOutOfLoad(sigsPlayerIndex);

    if (hrAbortLoadConfirm)
    {
        feScreenDelete(hrAbortLoadConfirm);
        hrAbortLoadConfirm = NULL;
    }
}

void hrAbortLoadingNo(char *name, featom *atom)
{
    if (hrAbortLoadConfirm)
    {
        feScreenDelete(hrAbortLoadConfirm);
        hrAbortLoadConfirm = NULL;
    }
}

void hrChatTextEntry(char *name, featom *atom)
{
    char *string;
    ChatPacket temp;
    sdword     width;
    fonthandle fhsave;
    char  testwidth[MAX_CHATSTRING_LENGTH+40];

    if (FEFIRSTCALL(atom))
    {
        // initialize button here
        ChatTextEntryBox = (textentryhandle)atom->pData;
        uicTextEntryInit(ChatTextEntryBox,UICTE_NoLossOfFocus|UICTE_ChatTextEntry);
        uicTextBufferResize(ChatTextEntryBox,MAX_CHATSTRING_LENGTH-2);
        return;
    }

    switch (uicTextEntryMessage(atom))
    {
        case CM_AcceptText :
            string = ((textentryhandle)atom->pData)->textBuffer;
            sendChatMessage(ALL_PLAYER_MASK^PLAYER_MASK(sigsPlayerIndex),string,(uword)sigsPlayerIndex);
            dbgMessagef("text entry: %s",string);
            strcpy(temp.message,string);
            temp.packetheader.frame = (uword)sigsPlayerIndex;
            hrProcessPacket((struct ChatPacket *)&temp);
            uicTextEntrySet(ChatTextEntryBox,"",0);
        break;
        case CM_KeyPressed :
            fhsave = fontMakeCurrent(((textentryhandle)atom->pData)->currentFont); //select the appropriate font
            sprintf(testwidth, "%s >  %s", playerNames[sigsPlayerIndex], ((textentryhandle)atom->pData)->textBuffer);
            width = fontWidth(testwidth);
            fontMakeCurrent(fhsave);
            if (width > (atom->width-30))
            {
                uicBackspaceCharacter((textentryhandle)atom->pData);
            }
        break;
    }
}

HorseRaceBars horseBarInfo;
extern bool8 etgHasBeenStarted;

void horseGetNumBars(HorseRaceBars *horsebars)
{
    sdword i;
    bool32 enablebar[MAX_POSSIBLE_NUM_BARS];
    real32 totalperc;

    dbgAssertOrIgnore(horseTotalNumBars.numBars > 0);
    for (i=0;i<horseTotalNumBars.numBars;i++)
    {
        switch (i)
        {
            case DOWNLOADMAP_BAR:
                enablebar[i] = autodownloadmapRequired();
                break;

            case UNIVERSE_BAR:
                enablebar[i] = TRUE;
                break;

            case ETG_BAR:
                enablebar[i] = etgHasBeenStarted ? FALSE : TRUE;
                break;

            case TEXTURE1_BAR:
            case TEXTURE2_BAR:
#if TR_NIL_TEXTURE
                enablebar[i] = GLOBAL_NO_TEXTURES ? FALSE : TRUE;
#else
                enablebar[i] = TRUE;
#endif
                break;

            default:
                dbgFatalf(DBG_Loc,"You must specify whether or not to load bar %i here",i);
                break;
        }
    }

    horsebars->numBars = 0;

    // now, based on enablebar[i], selectively add bars that will be loaded:

    for (i=0;i<horseTotalNumBars.numBars;i++)
    {
        if (enablebar[i])
        {
            horsebars->perc[horsebars->numBars++] = horseTotalNumBars.perc[i];
        }
    }

    dbgAssertOrIgnore(horsebars->numBars > 0);

    // now renormalize percentages to 1.0
    for (totalperc = 0.0f,i=0;i<horsebars->numBars;i++)
    {
        totalperc += horsebars->perc[i];
    }

    dbgAssertOrIgnore(totalperc > 0.0f);
    if (totalperc != 1.0f)
    {
        for (i=0;i<horsebars->numBars;i++)
        {
            horsebars->perc[i] /= totalperc;
        }
    }
}

void horseRaceInit(void)
{
    udword i;
    horseGetNumBars(&horseBarInfo);

    //initialize current bar to the 0th bar
    localbar=0;

    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        horseracestatus.barnum[i] = 0;
        horseracestatus.percent[i] = 0;
        horseracestatus.hrstatusstr[i][0] = 0;
        if (autodownloadmapRequired())
            TTimerStart(&hrPlayerDropoutTimers[i],HorseRacePlayerDropoutTime*2.0f);     // give double time for autodownloads
        else
            TTimerStart(&hrPlayerDropoutTimers[i],HorseRacePlayerDropoutTime);
        PlayersAlreadyDrawnDropped[i] = FALSE;
    }

    for (i=0;i<NUM_CHAT_LINES;i++)
    {
        chathistory[i].message[0] = 0;
    }

    listInit(&horseCrapRegion.cutouts);

    JustInit = TRUE;

    if (!hrScreensHandle)
    {
        feCallbackAddMultiple(hrCallBack);
        feDrawCallbackAddMultiple(hrDrawCallback);
        hrScreensHandle = feScreensLoad(HR_FIBFile);
    }

    if (!ShouldHaveMousePtr) mouseCursorHide();

    if (singlePlayerGame)
    {
        hrBaseRegion = feScreenStart(&horseCrapRegion, HR_SingleRaceScreen);
    }
    else
    {
        hrBaseRegion = feScreenStart(&horseCrapRegion, (multiPlayerGame) ? HR_RaceScreen : HR_RaceScreenNotNetwork);
    }

    playernamefont = frFontRegister(HR_PlayerNameFont);

    hrRunning=TRUE;
    hrBackgroundDirty = 3;
    hrBackgroundReinit = FALSE;
    hrLoadingLastDraw = 0;
    hrLoadingDrawInProgress = FALSE;
    hwMissionLoadProgressReset(hrLoadingSeconds());
    HorseRaceLoadingPhaseBegin(HR_LOAD_PREPARE,
        singlePlayerGame ? "Preparing mission loading systems" : "Preparing battle loading systems");
}

void horseRaceShutdown(void)
{
    sdword i;
    HWMissionLoadProgressSnapshot loading;

    hwMissionLoadProgressSnapshot(&loading);
    if (hrRunning && !hrAbortLoadingGame &&
        loading.percentHundredths == 10000 && !hrLoadingDrawInProgress)
    {
        hrDirtyProgressBar();
        horseRaceRender();
    }
    hwMissionLoadProgressStop();

    if (!ShouldHaveMousePtr) mouseCursorShow();

    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        TTimerClose(&hrPlayerDropoutTimers[i]);
    }

    hrRunning=FALSE;
    if (hrAbortLoadConfirm)
    {
        feScreenDelete(hrAbortLoadConfirm);
        hrAbortLoadConfirm = NULL;
    }

    feScreenDelete(hrBaseRegion);

    hrShutdownBackground();
    hrProgressCounter = 0;
    
    hrBaseRegion = NULL;
    hrProgressRegion = NULL;
    hrChatBoxRegion = NULL;
    ChatTextEntryBox = NULL;
}

void horseRaceWaitForNetworkGameStartShutdown(void)
{
    if (multiPlayerGame)
    {
        while (!multiPlayerGameUnderWay) ;
    }
}

real32 lasttime;

void HorseRaceBeginBar(uword barnum)
{
    //udword i;
    HorsePacket packet;
    lasttime = 0.0f;
//    if(utyTeaserHeader) return;

    if(JustInit)
    {
        JustInit = FALSE;
    }
    else
    {
        localbar++;
    }

    //send packet
    packet.packetheader.type = PACKETTYPE_HORSERACE;
    packet.playerindex = (uword) sigsPlayerIndex;
    packet.packetheader.numberOfCommands = 0;                    //don't need probably
    packet.barnum = (uword) localbar;
    packet.percent = horseRaceGetPacketPercent(0.0f);

    if (multiPlayerGame)
    {
        SendHorseRacePacket((ubyte *)&packet,sizeof(HorsePacket));
    }
    else
    {
        recievedHorsePacketCB((ubyte *)&packet,sizeof(HorsePacket));
    }
}

extern void mousePoll(void);
extern udword regRegionProcess(regionhandle reg, udword mask);
extern sdword regProcessingRegions;
extern sdword regRenderEventIndex;

void hrUncleanDecorative(void)
{
    if (hrDecRegion != NULL)
    {
        hrDecRegion->drawFunction = ferDrawDecorative;
    }
}

void horseRaceRender(void)
{
    SDL_Event e;
    hrLoadingDrawInProgress = TRUE;
    hrLoadingLastDraw = SDL_GetTicks64();
    hrDirtyProgressBar();

#ifdef HW_ENABLE_D3D12_BACKEND
    /* HorseRace owns its own render loop.  Without a presenter frame the
       DXGI surface retains a stale blue/black image until Windows exposes the
       window (Alt-Tab). */
    hwModernGraphicsBeginFrame((unsigned int)MAIN_WindowWidth,
                               (unsigned int)MAIN_WindowHeight);
#endif

    regRenderEventIndex = 0;

    //special-case code for double-clicks

/*    if (keyIsHit(LMOUSE_DOUBLE))
    {
        keyPressUp(LMOUSE_DOUBLE);
        utyDoubleClick();
    }
    if (demDemoRecording)
    {
        memcpy((ubyte *)&keyScanCode[0], (ubyte *)&keySaveScan[0], sizeof(keyScanCode));//freeze a snapshot of the key state
        demStateSave();
    }
    else if (demDemoPlaying)
    {
        demStateLoad();
    }*/

    if (hrBackgroundReinit)
    {
        if (hrBackgroundTexture != 0)
        {
            glDeleteTextures(1, &hrBackgroundTexture);
            hrBackgroundTexture = 0;
        }
        hrBackgroundReinit = FALSE;
        hrBackgroundDirty = 3;
        hrBackgroundInitFrame = 0;
    }

    // Make sure the Homeworld text gets drawn on the correct frames
    if (hrBackgroundDirty)
    {
        regRecursiveSetDirty(&horseCrapRegion);
        hrDecRegion = NULL;
    }
    else
    {
        regionhandle reg;

        // "Clean" the region with the homeworld logo
        // in it so it doesn't re-draw all the time

        reg = horseCrapRegion.child;
        if (reg)
        {
            reg = reg->child;
        }

        while (reg && reg->drawFunction != &ferDrawDecorative)
        {
            reg = reg->next;
        }

        if (reg)
        {
            void regNULLRenderFunction(regionhandle region);
            hrDecRegion = reg;
            reg->drawFunction = regNULLRenderFunction;
        }
    }

    if (TitanActive)
        titanPumpEngine();

    SDL_Delay(0);

    if (!SDL_PollEvent(0))
    {
        regProcessingRegions = TRUE;
        regRegionProcess(horseCrapRegion.child, 0xffffffff);
        regProcessingRegions = FALSE;
        if (ChatTextEntryBox!=NULL)
        {
            bitSet(ChatTextEntryBox->reg.status,RSF_KeyCapture);
            keyBufferClear();
        }
    }
    else
    {
        regRegionProcess(horseCrapRegion.child, 0xffffffff);
        while (SDL_PollEvent(0))
        {
            if (SDL_WaitEvent(&e))
            {
                HandleEvent(&e);

                if (multiPlayerGame)
                {
                    if (keyIsStuck(ESCKEY))
                    {
                        keyClearSticky(ESCKEY);                      //clear the sticky bit
                        if (!hrAbortLoadConfirm)
                        {
                            if (!hrAbortLoadingGame)    // if not already aborting
                            {
                                hrAbortLoadConfirm = feScreenStart(&horseCrapRegion, "AbortLoadConfirm");
                            }
                        }
                        else
                        {
                            feScreenDelete(hrAbortLoadConfirm);
                            hrAbortLoadConfirm = NULL;
                        }
                    }
                }

                regProcessingRegions = TRUE;
                if (hrAbortLoadConfirm!=NULL)
                {
                    ;
                }
                else if (ChatTextEntryBox!=NULL)
                {
                    regRegionProcess(&ChatTextEntryBox->reg, 0xffffffff);
                }
                regProcessingRegions = FALSE;
            }
            if (ChatTextEntryBox!=NULL)
            {
                bitSet(ChatTextEntryBox->reg.status,RSF_KeyCapture);
                keyBufferClear();
            }

            if (TitanActive)
                titanPumpEngine();

            SDL_Delay(0);
        }
    }

    // All of the hacked stuff from the render task

    //glColor3ub(colRed(RND_StarColor), colGreen(RND_StarColor), colBlue(RND_StarColor));

    if (ShouldHaveMousePtr)
    {
        rndClearToBlack();
        glClear(GL_DEPTH_BUFFER_BIT);
    }

//    primErrorMessagePrint();
    //default rendering scheme is primitives on any
    //functions which want it off should set it back on when done


    // I know this looks weird, but it's correct
    if(hrBackgroundInitFrame == 1)
    {
        hrInitBackground();
    }

    /* Draw every frame.  The D3D12 presenter cannot depend on the old
       OpenGL back-buffer persistence trick used after the first three draws. */
    hrDrawBackground();

    regFunctionsDraw();                                //render all regions
    if (!hrAbortLoadConfirm) hrDrawLoadingPanel();
    primErrorMessagePrint();

    hrUncleanDecorative();

    // We want the init code to be called on the 2nd pass.  That way, the screen erases,
    // Then we incur the delay of loading the background.
    // For two frames -after that- we'll draw the background,
    // then just draw the progress bars.
    if(hrBackgroundDirty && hrBackgroundInitFrame)
    {
        if (hrBackgroundDirty > 0)
        {
            hrBackgroundDirty--;
        }
    }

    hrBackgroundInitFrame++;

    if (ShouldHaveMousePtr)
    {
        // set the cursor type, reset the variables then draw the mouse cursor
        mouseSelectCursorSetting();
        mouseSetCursorSetting();
        mousePoll();
        mouseDraw();                                        //draw mouse atop everything
    }

    rndFlush();
#ifdef HW_ENABLE_D3D12_BACKEND
    hwModernGraphicsEndFrame();
#endif
    primErrorMessagePrint();
    hrLoadingDrawInProgress = FALSE;
}

//function that returns the doctored percentage for conversion of
//multiple bars to a single bar.

real32 horseRaceGetPacketPercent(real32 barPercent)
{
    real32 percent=0.0f;
    sdword i;
    for(i=0;i<localbar;i++)
    {
        //add previous bar percentiles...
        percent += horseBarInfo.perc[i];
    }
    //add on current bars contribution scaled by appropriate ammount
    percent += barPercent*horseBarInfo.perc[localbar];
    return percent;
}

bool32 HorseRaceNext(real32 percent)
{
    Uint32 lp;
    HorsePacket packet;
    real32 temptime;
    udword i;
    sdword dontrenderhack = FALSE;
    bool32 sendhrpackethack = FALSE;
//    static real32 lastper = 0.0f;
    static sdword modulusCounter = 0;

    if (!hrRunning) return percent >= 1.0f;
    if (percent > 1.0f) percent = 1.0f;
//    if(utyTeaserHeader)  return TRUE;

    /*glClear(GL_COLOR_BUFFER_BIT);
    regFunctionsDraw();                                 //render all regions
    glFlush();                                          //ensure all draws complete*/

    if (TitanActive)
        titanPumpEngine();

    packet.packetheader.type = PACKETTYPE_HORSERACE;
    packet.playerindex = (uword) sigsPlayerIndex;
    packet.packetheader.numberOfCommands = 0;                    //don't need probably
    packet.barnum = (uword) localbar;
    //packet.current = (uword) localcurrent;
    //packet.max = (uword) localmax;

    packet.percent = horseRaceGetPacketPercent(percent);

    //render first
    if (lasttime == 0.0f)
    {
        horseRaceRender();
        dontrenderhack = TRUE;
    }

    if (multiPlayerGame && (startingGameState == AUTODOWNLOAD_MAP) && (sigsPlayerIndex == 0))
    {
        sendhrpackethack = TRUE;
        SendHorseRacePacket((ubyte *)&packet,sizeof(HorsePacket));      // always send horse race packet when autouploading map
    }

    lp = SDL_GetTicks();
    temptime = (float)lp;
    if(temptime - lasttime < horseRaceRenderTime)
    {
        if(lasttime > temptime)
            lasttime = temptime;
        return FALSE;     //don't return
    }
    lasttime = temptime;

//    if(lastper != percent)
//    {

    if (multiPlayerGame)
    {
        if (!sendhrpackethack)
        {
            if (modulusCounter <= 0)
            {
                modulusCounter = 6;
            }
            modulusCounter--;
            if (modulusCounter == 0)
            {
                SendHorseRacePacket((ubyte *)&packet,sizeof(HorsePacket));
            }
        }
    }
    else
    {
        recievedHorsePacketCB((ubyte *)&packet,sizeof(HorsePacket));
    }

//    }
//    lastper = percent;

    if (!dontrenderhack)
    {
        horseRaceRender();
    }

    if(multiPlayerGame)
    {
        for(i=0;i<sigsNumPlayers;i++)
        {
            if (i != sigsPlayerIndex)       // don't check myself
            {
                if (!playerHasDroppedOutOrQuit(i))
                {
                    if (TTimerUpdate(&hrPlayerDropoutTimers[i]))
                    {
                        PlayerDroppedOut(i,TRUE);
                    }
                }
            }
        }

        if(localbar == (horseBarInfo.numBars - 1))
        {
            for(i=0;i<sigsNumPlayers;i++)
            {
                if (!playerHasDroppedOutOrQuit(i))
                {
                    if(horseracestatus.percent[i] < 0.999f)
                    {
                        return FALSE;
                    }
                }
            }
        }
        return TRUE;
    }

    return TRUE;
}

void hrProcessPacket(struct ChatPacket *packet)
{
    sdword  i;
    bool32    done=FALSE;

    for (i=0;i<NUM_CHAT_LINES;i++)
    {
        if (chathistory[i].message[0]==0)
        {
            strcpy(chathistory[i].message,(*((ChatPacket *)packet)).message);
            chathistory[i].packetheader.frame = (*((ChatPacket *)packet)).packetheader.frame;
            done = TRUE;
            break;
        }
    }

    if (!done)
    {
        for (i=0;i<NUM_CHAT_LINES-1;i++)
        {
            chathistory[i] = chathistory[i+1];
            strcpy(chathistory[i].message,chathistory[i+1].message);
        }
        strcpy(chathistory[i].message,(*((ChatPacket *)packet)).message);
        chathistory[i].packetheader.frame = (*((ChatPacket *)packet)).packetheader.frame;
    }

    hrDirtyChatBox();
}

void recievedHorsePacketCB(ubyte *packet,udword sizeofpacket)
{
    //captain should have recieved a packet here...
    HorsePacket *hp = (HorsePacket *)packet;
    udword i;

    i = hp->playerindex;
    dbgAssertOrIgnore(i < MAX_MULTIPLAYER_PLAYERS);

    if (multiPlayerGame)
    {
        titanDebug("hr: recv hrpacket from %d status %d",i,playersReadyToGo[i]);
        if (!playerHasDroppedOutOrQuit(i))
        {
            TTimerReset(&hrPlayerDropoutTimers[i]);
        }
    }

    hrDirtyProgressBar();

    horseracestatus.barnum[i] = hp->barnum;
    horseracestatus.percent[i] = hp->percent;
}
