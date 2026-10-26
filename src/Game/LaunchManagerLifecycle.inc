/* Retire the UI state before releasing its regions/list. Docking notifications
   can arrive during launch and destruction; they must never refresh a dying
   list. Both Close and Launch All finish through this same, idempotent path. */
static void lmDismiss(void)
{
    regionhandle closing=lmBaseRegion;
    bool32 restoreOverlay=lmIoSaveState;
    if (closing==NULL && !lmActive) return;
    lmActive=FALSE;
    lmBaseRegion=NULL;
    lmHoldRegion=NULL;
    lmListWindow=NULL;
    shipsin=NULL;
    lmIoSaveState=FALSE;
    lmRenderEverythingCounter=0;
    if (closing!=NULL) feScreenDelete(closing);
    svClose();
    if (restoreOverlay) ioEnable();
    soundEvent(NULL,UI_ManagerExit);
    if (!mdManagerDockEnabled())
    {
        soundEvent(NULL,UI_SoundOfSpace);
        tbDisable=FALSE;
    }
    mrRenderMainScreen=TRUE;
}
