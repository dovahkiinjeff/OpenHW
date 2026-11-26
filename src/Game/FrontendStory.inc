static fescreen *muiStoryScreen;
static char muiColorReturnScreen[80]="Main_game_screen";
static char muiCataDataStatus[256]="Configure Cataclysm data from the Launcher's main screen before starting.";
static void muiStoryAction(const char *name);
static bool32 muiCataValidate(void)
{
    CataStartingFleet fleet;CataSpeechPhrase speech;char path[1024];
    if(!hwCataDataOpenConfigured(muiCataDataStatus,sizeof(muiCataDataStatus))){
        snprintf(muiCataDataStatus,sizeof(muiCataDataStatus),"Set your Cataclysm data path on the Launcher's main screen.");return FALSE;
    }
    if(!hwCataMission1StartingFleet(&fleet,muiCataDataStatus,sizeof(muiCataDataStatus)))return FALSE;
    if(!hwCataOriginalText(0x008c83a8) || strcmp(hwCataOriginalText(0x008c83a8),"DestroyBombers") || hwCataWeaponDamage(0,0)<=0){
        snprintf(muiCataDataStatus,sizeof(muiCataDataStatus),"The original Cataclysm executable is missing or incompatible.");return FALSE;
    }
    if(!hwCataSpeechPhrase(0x100010d6,0,&speech,muiCataDataStatus,sizeof(muiCataDataStatus)))return FALSE;
    if(!hwCataAudioPath("SectSpeech.vce",path,sizeof(path))){
        snprintf(muiCataDataStatus,sizeof(muiCataDataStatus),"SectSpeech.vce is missing from the selected installation.");return FALSE;
    }
    if(!hwCataAudioPath("Music4Cat.wxd",path,sizeof(path))){
        snprintf(muiCataDataStatus,sizeof(muiCataDataStatus),"Music4Cat.wxd is missing from the selected installation.");return FALSE;
    }
    snprintf(muiCataDataStatus,sizeof(muiCataDataStatus),"Owned game data is ready. Mission 1: Hiigara.");return TRUE;
}
static void muiCataBegin(void)
{
    if(!muiCataValidate())return;
    cpModernRaceSelect(R1);spRequestNewCampaign(MISSION_CATA_1_HIIGARA);
    muiStoryAction("StartSinglePlayerGame");
}
static void muiDrawCataclysmStory(MuiStoryLayout *l)
{
    rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight);
    int x=mouseCursorX(),y=mouseCursorY(),left=l->preview.x0+uiScaleSize(20),top=l->preview.y0+uiScaleSize(24);
    bool32 artReady = muiEnsureArtwork(MUI_ART_CATA_BEAST_SHIP);
    muiDrawBackdrop(artReady ? MUI_ART_CATA_BEAST_SHIP : MUI_ART_FLEET);
    muiFill(&whole,colRGBA(3,6,9,artReady ? 60 : 100));
    muiFill(&l->panel,colRGBA(10,18,24,244));primRectOutline2(&l->panel,1,muiLineSoft);
    muiText(l->panel.x0+uiScaleSize(24),l->panel.y0+uiScaleSize(22),muiSignal,"CATACLYSM",muiBodyFont);
    muiText(l->panel.x0+uiScaleSize(24),l->panel.y0+uiScaleSize(48),muiPaper,"SOMTAAW / KUUN-LAN",muiHeadingFont);
    muiDrawButtonRect(&l->races[0],"PLAYER SETUP",FALSE,muiPointIn(&l->races[0],x,y));
    muiDrawButtonRect(&l->races[1],"LOAD STORY SAVE",FALSE,muiPointIn(&l->races[1],x,y));
    if (muiEnsureArtwork(MUI_ART_CATA_KUUN_LAN))
        muiConceptPanel(MUI_ART_CATA_KUUN_LAN,&l->preview,168);
    else
        muiFill(&l->preview,colRGBA(6,14,22,205));
    primRectOutline2(&l->preview,1,muiLineSoft);
    muiText(left,top,muiSignal,"MISSION 01 / HIIGARA",muiHeadingFont);
    muiText(left,top+uiScaleSize(54),muiPaper,"KIITH SOMTAAW",muiBodyFont);
    muiText(left,top+uiScaleSize(88),muiMuted,"Use your owned Cataclysm or Emergence game data.",muiBodyFont);
    muiText(left,top+uiScaleSize(120),muiMuted,"Your original installation remains unchanged.",muiBodyFont);
    muiText(left,top+uiScaleSize(176),muiCool,muiCataDataStatus,muiCompactFont);
    muiDrawButtonRect(&l->back,"BACK",FALSE,muiPointIn(&l->back,x,y));
    muiDrawButtonRect(&l->start,"BEGIN MISSION 1",TRUE,muiPointIn(&l->start,x,y));
}

void modernUIRememberColorReturn(void)
{
    const char *name=feStackIndex>0 && feStack[feStackIndex].screen ? feStack[feStackIndex].screen->name : NULL;
    snprintf(muiColorReturnScreen,sizeof(muiColorReturnScreen),"%s",name && strcmp(name,"Select_colour") ? name : "Main_game_screen");
}

static color muiPreviewEngineColor(color base,color stripe)
{
    return utyEngineTrailColorOverride?utyEngineTrailColor:teTrailEmissiveSeed(base,stripe);
}

static void muiDrawStory(regionhandle region)
{
    MuiStoryLayout l;rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight),view,swatch;
    int x=mouseCursorX(),y=mouseCursorY(),i;char title[96];
    (void)region;muiEnsureFonts();muiBuildStoryLayout(&l);
    if(muiStoryCataclysm){muiDrawCataclysmStory(&l);return;}
    muiDrawBackdrop(MUI_ART_FLEET);muiFill(&whole,colRGBA(3,6,9,100));
    muiFill(&l.panel,colRGBA(10,18,24,244));primRectOutline2(&l.panel,1,muiLineSoft);
    primLineThick2(l.panel.x0,l.panel.y0,l.panel.x1,l.panel.y0,uiScaleSize(3),muiSignal);
    muiText(l.panel.x0+uiScaleSize(24),l.panel.y0+uiScaleSize(22),muiSignal,"THE JOURNEY HOME",muiBodyFont);
    muiText(l.panel.x0+uiScaleSize(24),l.panel.y0+uiScaleSize(48),muiPaper,"STORY",muiHeadingFont);
    muiText(l.races[0].x0,l.races[0].y0-uiScaleSize(26),muiMuted,"SELECT YOUR FLEET",muiBodyFont);
    for(i=0;i<2;++i)muiDrawButtonRect(&l.races[i],i==0?"KUSHAN":"TAIIDAN",whichRaceSelected==(i==0?R1:R2),muiPointIn(&l.races[i],x,y));
    muiDrawButtonRect(&l.colors,"PLAYER SETUP",FALSE,muiPointIn(&l.colors,x,y));
    muiDrawButtonRect(&l.load,"LOAD STORY SAVE",FALSE,muiPointIn(&l.load,x,y));
    swatch=muiRect(l.colors.x0,l.load.y1+uiScaleSize(10),(l.colors.x0+l.colors.x1)/2-uiScaleSize(4),MIN2(l.back.y0-uiScaleSize(10),l.load.y1+uiScaleSize(36)));
    muiDrawColorSwatch(&swatch,utyBaseColor,FALSE);swatch.x0=swatch.x1+uiScaleSize(8);swatch.x1=l.colors.x1;muiDrawColorSwatch(&swatch,utyStripeColor,FALSE);
    muiFill(&l.preview,colRGB(6,14,22));primRectOutline2(&l.preview,1,muiLineSoft);
    view=l.preview;view.y0+=uiScaleSize(42);view.y1=l.thrust.y0-uiScaleSize(8);
    muiShipPreviewDraw(&view,whichRaceSelected,utyBaseColor,utyStripeColor,muiPreviewEngineColor(utyBaseColor,utyStripeColor));
    snprintf(title,sizeof(title),"%s / INTERCEPTOR",whichRaceSelected==R1?"KUSHAN":"TAIIDAN");
    muiText(l.preview.x0+uiScaleSize(14),l.preview.y0+uiScaleSize(12),muiCool,title,muiBodyFont);
    muiDrawButtonRect(&l.thrust,muiShipPreview.thrust?"MAX SPEED / STOP":"SIMULATE MAX SPEED",muiShipPreview.thrust,muiPointIn(&l.thrust,x,y));
    muiDrawButtonRect(&l.back,"BACK",FALSE,muiPointIn(&l.back,x,y));
    muiDrawButtonRect(&l.start,"BEGIN NEW STORY",TRUE,muiPointIn(&l.start,x,y));
}

static void muiStoryAction(const char *name)
{
    featom *atom=muiStoryScreen?feAtomFindInScreen(muiStoryScreen,(char *)name):NULL;
    if(!atom)return;
    if(bitTest(atom->flags,FAF_Link)) {
        felink *link=feLinkFindInScreen(muiStoryScreen,atom->name);
        if(link)feScreenStart(ghMainRegion,link->linkToName);
    } else feFunctionExecute(atom->name,atom,FALSE);
}

static udword muiStoryProcess(regionhandle region,smemsize ID,udword event,udword data)
{
    MuiStoryLayout l;int x=mouseCursorX(),y=mouseCursorY(),i;
    (void)region;(void)data;muiBuildStoryLayout(&l);
    if(muiStoryCataclysm){
        if(event==RPE_KeyDown){
            if(ID==ESCKEY)muiStoryAction("Create_new_game-TO-Main_game_screen");
            else if(ID==RETURNKEY)muiCataBegin();
            return RPR_Redraw;
        }
        if(event!=RPE_ReleaseLeft)return 0;
        if(muiPointIn(&l.start,x,y))muiCataBegin();
        else if(muiPointIn(&l.back,x,y))muiStoryAction("Create_new_game-TO-Main_game_screen");
        else if(muiPointIn(&l.races[1],x,y))muiStoryAction("Create_new_game-TO-Load_game");
        else if(muiPointIn(&l.races[0],x,y)){cpModernRaceSelect(R1);cpColorsPick(&utyBaseColor,&utyStripeColor,&whichRaceSelected);}
        return RPR_Redraw;
    }
    if(muiShipPreviewInput(&l.preview,event))return RPR_Redraw;
    if(event==RPE_KeyDown) {
        if(ID==ESCKEY)muiStoryAction("Create_new_game-TO-Main_game_screen");
        else if(ID==RETURNKEY)muiStoryAction("StartSinglePlayerGame");
        else if(ID==ARRLEFT || ID==ARRRIGHT)cpModernRaceSelect(whichRaceSelected==R1?R2:R1);
        return RPR_Redraw;
    }
    if(event!=RPE_ReleaseLeft)return 0;
    for(i=0;i<2;++i)if(muiPointIn(&l.races[i],x,y)){cpModernRaceSelect(i==0?R1:R2);return RPR_Redraw;}
    if(muiPointIn(&l.colors,x,y))cpColorsPick(&utyBaseColor,&utyStripeColor,&whichRaceSelected);
    else if(muiPointIn(&l.thrust,x,y))muiShipPreviewToggle();
    else if(muiPointIn(&l.start,x,y))muiStoryAction("StartSinglePlayerGame");
    else if(muiPointIn(&l.load,x,y))muiStoryAction("Create_new_game-TO-Load_game");
    else if(muiPointIn(&l.back,x,y))muiStoryAction("Create_new_game-TO-Main_game_screen");
    return RPR_Redraw;
}

void modernUIDrawFleetPreview(regionhandle region)
{
    color base,stripe;cpModernColorsSnapshot(&base,&stripe);
    muiFill(&region->rect,colRGB(6,14,22));
    bitSet(region->flags,RPE_DrawEveryFrame);
    muiShipPreviewDraw(&region->rect,whichRaceSelected,base,stripe,muiPreviewEngineColor(base,stripe));
    primRectOutline2(&region->rect,1,muiLineSoft);
}
void modernUICloseFleetPreview(void) {muiShipPreviewClose();}
