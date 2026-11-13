/* Actual tutorial button creation and deferred restart, shared with offline regression. */
static char tutPendingRestart[sizeof(tutCurrLessonName)];

void tutShowNextButton(void)
{
    if(tutNextVisible)
        tutHideNextButton();

    tutAllocateRootRegion();

    tutNextAtom.x = tutTextRect.x0 + 128;
    tutNextAtom.y = tutTextRect.y1;
    tutNextAtom.width = 64;
    tutNextAtom.height = 32;

    tutNextRegion = uicChildButtonAlloc((controlhandle)tutRootRegion,
        (smemsize)&tutNextAtom, tutNextAtom.x, tutNextAtom.y,
        tutNextAtom.width, tutNextAtom.height, tutProcessNextButton, FA_Button);

    tutNextAtom.region = (void*)tutNextRegion;
    tutNextRegion->reg.atom = &tutNextAtom;
    tutNextRegion->screen = NULL;
    tutNextRegion->reg.flags |= RPM_MouseEvents;
    tutNextAtom.pData = NULL;

    tutNextRegion->reg.processFunction = uicButtonProcess;
    tutNextRegion->processFunction = tutProcessNextButton;
    tutNextVisible = TRUE;

    regDrawFunctionSet((regionhandle)tutNextRegion, tutDrawNextButtonFunction);
}

void tutHideNextButton(void)
{
    if(tutNextRegion)
    {
        regRegionDelete((regionhandle)tutNextRegion);
        tutDeallocateRootRegion();
    }

    tutNextRegion = NULL;
    tutNextVisible = FALSE;

}

udword tutProcessNextButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    if(event == CM_ButtonClick)
    {
        tutNextButtonState = 1;
        return 1;
    }

    return 0;
}

sdword tutNextButtonClicked(void)
{
sdword  RetVal;

    RetVal = tutNextButtonState;
    tutNextButtonState = 0;

    return RetVal;
}

void tutShowBackButton(void)
{
    if(tutBackVisible)
    {
        tutPrevVisible = FALSE;
        tutHideBackButton();
    }

    tutAllocateRootRegion();

    tutBackAtom.x = tutTextRect.x0;
    tutBackAtom.y = tutTextRect.y1;
    tutBackAtom.width = 128;
    tutBackAtom.height = 32;

    tutBackRegion = uicChildButtonAlloc((controlhandle)tutRootRegion,
        (smemsize)&tutBackAtom, tutBackAtom.x, tutBackAtom.y,
        tutBackAtom.width, tutBackAtom.height, tutProcessBackButton, FA_Button);

    tutBackAtom.region = (void*)tutBackRegion;
    tutBackRegion->reg.atom = &tutBackAtom;
    tutBackRegion->screen = NULL;
    tutBackRegion->reg.flags |= RPM_MouseEvents;
    tutBackAtom.pData = NULL;

    tutBackRegion->reg.processFunction = uicButtonProcess;
    tutBackRegion->processFunction = tutProcessBackButton;
    tutBackVisible = TRUE;

    regDrawFunctionSet((regionhandle)tutBackRegion, tutDrawBackButtonFunction);
}

void tutShowPrevButton(void)
{
    if (tutBackVisible)
    {
        tutHideBackButton();
    }
    tutPrevVisible = TRUE;
    tutShowBackButton();
}

void tutHideBackButton(void)
{
    if(tutBackRegion)
    {
        regRegionDelete((regionhandle)tutBackRegion);
        tutDeallocateRootRegion();
    }

    tutBackRegion = NULL;
    tutBackVisible = FALSE;
    tutPrevVisible = FALSE;

}

udword tutProcessBackButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    if(event == CM_ButtonClick)
    {
        if((tutPrevVisible) && (tutLastLessonName[0]))
            strcpy(tutCurrLessonName, tutLastLessonName);

        if (fileExists(tutCurrLessonName, 0) && (VerifySaveFile(tutCurrLessonName) == VERIFYSAVEFILE_OK))
        {
            /* The region walker still uses this button after the callback.
               Defer world teardown until the next outer task dispatch. */
            strcpy(tutPendingRestart, tutCurrLessonName);
            return 1;
        }
    }

    return 0;
}

void tutProcessDeferredRestart(void)
{
    char filename[sizeof(tutPendingRestart)];
    if (!tutPendingRestart[0]) return;
    strcpy(filename, tutPendingRestart);
    tutPendingRestart[0] = 0;
    tutEnableEverything();
    spMainScreen();
    tbForceTaskbar(FALSE);
    gameEnd();
    tutorial = TUTORIAL_ONLY;
    utyLoadSinglePlayerGameGivenFilename(filename);
}
