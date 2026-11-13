static struct {
    regionhandle region;
    Ship *target;
    uword id;
    ShipAppearance pending;
    sdword channel;
} muiShipPaint;
static Ship *muiShipPaintSelection(void)
{
    Ship *ship=selSelected.numShips==1?selSelected.ShipPtr[0]:NULL;
    return ship && ship->objtype==OBJ_ShipType && ship->playerowner==universe.curPlayerPtr && !(ship->flags&SOF_Dead) ? ship : NULL;
}
static rectangle muiShipPaintButton(void) {return muiRect(MAIN_WindowWidth-160,180,MAIN_WindowWidth-88,244);}
static void muiShipPaintClose(void)
{
    regionhandle region=muiShipPaint.region;
    muiShipPaint.region=NULL;muiShipPaint.target=NULL;
    modernUICloseEditorColorPicker();
    if(region)regRegionDelete(region);
}
static bool32 muiShipPaintValid(void)
{
    Ship *ship=muiShipPaintSelection();
    return ship && ship==muiShipPaint.target && ship->shipID.shipNumber==muiShipPaint.id;
}
static rectangle muiShipPaintPanel(void)
{
    sdword width=uiScaleSize(420),height=uiScaleSize(434);
    sdword right=MAIN_WindowWidth-uiScaleSize(100),top=uiScaleSize(100);
    if(width>MAIN_WindowWidth-uiScaleSize(24))width=MAIN_WindowWidth-uiScaleSize(24);
    if(top+height>MAIN_WindowHeight-uiScaleSize(40))top=max(uiScaleSize(12),MAIN_WindowHeight-height-uiScaleSize(40));
    return muiRect(right-width,top,right,top+height);
}
static rectangle muiShipPaintRow(const rectangle *panel,sdword i)
{
    return muiRect(panel->x0+uiScaleSize(14),panel->y0+uiScaleSize(90+i*42),panel->x1-uiScaleSize(14),panel->y0+uiScaleSize(126+i*42));
}
static void muiShipPaintPickerApply(const real32 rgb[3],void *context)
{
    sdword channel=(sdword)(size_t)context;
    if(!muiShipPaint.region || !muiShipPaintValid() || channel<0 || channel>=SHIP_PAINT_COUNT)return;
    muiShipPaint.pending.colors[channel]=colRGB(colRealToUbyte(rgb[0]),colRealToUbyte(rgb[1]),colRealToUbyte(rgb[2]));
    muiShipPaint.pending.mask|=1u<<channel;
}
static void muiShipPaintDraw(regionhandle region)
{
    static const char *names[]={"HULL / BASE","STRIPES","ENGINE TRAILS / BULBS","NAVIGATION LIGHTS","RESOURCE BEAM","HYPERSPACE"};
    rectangle panel,button,row,chip;sdword i;
    if(!muiShipPaintValid()){muiShipPaintClose();return;}
    (void)region;muiEnsureFonts();panel=muiShipPaintPanel();
    muiFill(&panel,muiSurface);primRectOutline2(&panel,2,muiSignal);
    muiText(panel.x0+uiScaleSize(16),panel.y0+uiScaleSize(15),muiSignal,"SHIP COLORS",muiHeadingFont);
    muiText(panel.x0+uiScaleSize(16),panel.y0+uiScaleSize(46),muiPaper,ShipTypeToStr(muiShipPaint.target->shiptype),muiBodyFont);
    muiText(panel.x0+uiScaleSize(16),panel.y0+uiScaleSize(68),muiMuted,"Only this ship / uncheck to inherit fleet appearance",muiCompactFont);
    for(i=0;i<SHIP_PAINT_COUNT;++i){
        row=muiShipPaintRow(&panel,i);muiFill(&row,muiSurfaceRaised);
        button=muiRect(row.x0+uiScaleSize(6),row.y0+uiScaleSize(8),row.x0+uiScaleSize(26),row.y1-uiScaleSize(8));
        primRectOutline2(&button,1,muiMuted);if(muiShipPaint.pending.mask&(1u<<i)){button.x0+=3;button.x1-=3;button.y0+=3;button.y1-=3;muiFill(&button,muiSignal);}
        muiText(row.x0+uiScaleSize(34),row.y0+uiScaleSize(10),muiPaper,names[i],muiCompactFont);
        chip=muiRect(row.x1-uiScaleSize(80),row.y0+uiScaleSize(5),row.x1-uiScaleSize(6),row.y1-uiScaleSize(5));muiFill(&chip,muiShipPaint.pending.colors[i]);primRectOutline2(&chip,1,muiMuted);
    }
    button=muiRect(panel.x0+uiScaleSize(14),panel.y1-uiScaleSize(62),panel.x0+uiScaleSize(130),panel.y1-uiScaleSize(20));muiDrawButtonRect(&button,"RESET ALL",FALSE,FALSE);
    button=muiRect(panel.x1-uiScaleSize(254),panel.y1-uiScaleSize(62),panel.x1-uiScaleSize(138),panel.y1-uiScaleSize(20));muiDrawButtonRect(&button,"CANCEL",FALSE,FALSE);
    button=muiRect(panel.x1-uiScaleSize(130),panel.y1-uiScaleSize(62),panel.x1-uiScaleSize(14),panel.y1-uiScaleSize(20));muiDrawButtonRect(&button,"APPLY",TRUE,FALSE);
}
static udword muiShipPaintProcess(regionhandle region,smemsize ID,udword event,udword data)
{
    rectangle panel,row;sdword i,x=mouseCursorX(),y=mouseCursorY();
    (void)region;(void)data;
    if(!muiShipPaintValid()){muiShipPaintClose();return RPR_Redraw;}
    if(event==RPE_KeyDown && ID==ESCKEY){muiShipPaintClose();return RPR_Redraw;}
    if(event!=RPE_ReleaseLeft)return RPR_Redraw;
    panel=muiShipPaintPanel();
    if(!muiPointIn(&panel,x,y)){muiShipPaintClose();return RPR_Redraw;}
    for(i=0;i<SHIP_PAINT_COUNT;++i){
        row=muiShipPaintRow(&panel,i);
        if(muiPointIn(&row,x,y)){
            if(x<row.x0+uiScaleSize(30))muiShipPaint.pending.mask^=1u<<i;
            else{
                real32 rgb[3];color c=muiShipPaint.pending.colors[i];
                rgb[0]=colUbyteToReal(colRed(c));rgb[1]=colUbyteToReal(colGreen(c));rgb[2]=colUbyteToReal(colBlue(c));
                modernUIOpenEditorColorPicker("THIS SHIP / COLOR",rgb,1.0f,muiShipPaintPickerApply,(void *)(size_t)i);
            }
            return RPR_Redraw;
        }
    }
    if(y>=panel.y1-uiScaleSize(62) && y<panel.y1-uiScaleSize(20)){
        if(x<panel.x0+uiScaleSize(130)){muiShipPaint.pending.mask=0;return RPR_Redraw;}
        if(x>=panel.x1-uiScaleSize(130))shipAppearanceApply(muiShipPaint.target,&muiShipPaint.pending);
        muiShipPaintClose();
    }
    return RPR_Redraw;
}
static void muiShipPaintOpen(Ship *ship)
{
    regionhandle input;
    if(!ship)return;
    muiShipPaintClose();muiShipPaint.target=ship;muiShipPaint.id=ship->shipID.shipNumber;
    shipAppearanceGet(ship,&muiShipPaint.pending);
    /* Region processes children before applying ModalBreak. A modal base
       cannot also receive its own mouse events: the break masks them first. */
    muiShipPaint.region=regChildAlloc(ghMainRegion,0,0,0,MAIN_WindowWidth,MAIN_WindowHeight,0,RPE_ModalBreak|RPE_DrawEveryFrame);
    regDrawFunctionSet(muiShipPaint.region,muiShipPaintDraw);
    input=regChildAlloc(muiShipPaint.region,0,0,0,MAIN_WindowWidth,MAIN_WindowHeight,0,RPE_LeftClick);
    regFunctionSet(input,muiShipPaintProcess);
    regKeyChildAlloc(muiShipPaint.region,ESCKEY,RPE_KeyDown,(regionfunction)muiShipPaintProcess,1,ESCKEY);
    regSiblingMoveToFront(muiShipPaint.region);
}
static void muiShipPaintDrawButton(regionhandle region)
{
    rectangle r=muiShipPaintButton();
    if(!muiShipPaintSelection())return;
    if(region->rect.x1<=region->rect.x0 || region->rect.y1<=region->rect.y0)region->rect=r;
    else {
        if(region->rect.x0>r.x0)region->rect.x0=r.x0;
        if(region->rect.x1<r.x1)region->rect.x1=r.x1;
        if(region->rect.y0>r.y0)region->rect.y0=r.y0;
        if(region->rect.y1<r.y1)region->rect.y1=r.y1;
    }
    muiFill(&r,colRGB(44,39,20));primRectOutline2(&r,2,muiSignal);muiTextCentered(&r,muiPaper,"COLORS",muiCompactFont);
}
