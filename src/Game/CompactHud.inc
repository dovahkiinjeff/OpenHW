/* Three ordinary manager buttons for the middle Backspace HUD state. */
static sdword tbHudMode = HW_HUD_FULL;
static regionhandle tbCompactRegion;
static bool32 tbCompactAttached;
static bool32 tbHudOverlayWasHidden;
static featom tbCompactAtoms[3];

bool32 tbFullHudVisible(void)
{
    return hwHudVisible(tbHudMode, HW_HUD_FULL, gameIsRunning,
                        nisIsRunning, smFleetIntel);
}

static udword tbCompactProcess(regionhandle reg, smemsize id, udword event, udword data)
{
    featom *atom = (featom *)id;
    (void)reg; (void)data;
    if(event != CM_ButtonClick || !hwHudVisible(tbHudMode, HW_HUD_COMPACT,
        gameIsRunning, nisIsRunning, smFleetIntel)) return 0;
    if(atom == &tbCompactAtoms[0]) mrBuildShips(NULL, atom);
    else if(atom == &tbCompactAtoms[1]) mrResearch(NULL, atom);
    else if(atom == &tbCompactAtoms[2]) mrLaunch(NULL, atom);
    return RPR_Redraw;
}

static void tbCompactStartup(void)
{
    static char *names[3] = { "CSM_Build", "CSM_Research", "CSM_Launch" };
    sdword width=uiScaleSize(110), height=uiScaleSize(30), gap=uiScaleSize(4);
    sdword x=(MAIN_WindowWidth-width*3-gap*2)/2;
    sdword y=MAIN_WindowHeight-height-uiScaleSize(8), i;
    tbCompactRegion=regChildAlloc(ghMainRegion,0,x,y,width*3+gap*2,height,0,0);
    for(i=0;i<3;i++) {
        buttonhandle button;
        featom *atom=&tbCompactAtoms[i];
        memset(atom,0,sizeof(*atom));
        atom->name=names[i]; atom->type=FA_Button;
        button=uicChildButtonAlloc(tbCompactRegion,(smemsize)atom,
            x+i*(width+gap),y,width,height,tbCompactProcess,FA_Button);
        button->screen=NULL;
        button->reg.atom=atom; atom->region=button;
    }
    regLinkRemove(tbCompactRegion);
    tbCompactAttached=FALSE;
}

static void tbCompactUpdate(void)
{
    bool32 visible=hwHudVisible(tbHudMode,HW_HUD_COMPACT,gameIsRunning,
                               nisIsRunning,smFleetIntel);
    if(!tbCompactRegion) return;
    if(visible && !tbCompactAttached) {
        regChildInsert(tbCompactRegion,ghMainRegion);
        regSiblingMoveToFront(tbCompactRegion);
        tbCompactAttached=TRUE;
    } else if(!visible && tbCompactAttached) {
        regLinkRemove(tbCompactRegion); tbCompactAttached=FALSE;
    }
}

void tbCycleHudMode(void)
{
    if(!gameIsRunning || tutorial==TUTORIAL_ONLY || nisIsRunning || smFleetIntel) return;
    tbHudMode=hwHudNextMode(tbHudMode);
    if(tbHudMode==HW_HUD_HIDDEN) mdCloseAllManagers();
    tbEnsurePersistentInfo();
}
