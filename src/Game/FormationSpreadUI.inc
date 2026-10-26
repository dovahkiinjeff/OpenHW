/* Selection-driven, nonmodal live spacing control. Ships move under their
   existing formation command; this never issues Move/Halt or teleports them. */
static regionhandle muiFormationSpreadRegion;
static bool32 muiFormationSpreadDragging;
static CommandToDo *muiFormationSpreadDragCommand;
static Ship *muiFormationSpreadDragLeader;
static unsigned muiFormationSpreadPending, muiFormationSpreadSent;
static Uint32 muiFormationSpreadLastSend;

static CommandToDo *muiSelectedFormation(void)
{
    sdword i;
    Ship *first;
    CommandToDo *command;
    if (!muiCommandBarVisible() || smSensorsActive || cmActive || rmGUIActive || lmActive ||
        muiDossier.active || modernUILightingEditorActive() || modernMapEditorActive() ||
        selSelected.numShips<2) return NULL;
    first=selSelected.ShipPtr[0];
    command=first->formationcommand;
    if (!command || !(command->ordertype.attributes & COMMAND_MASK_FORMATION) ||
        command->formation.formationtype>CUSTOM_FORMATION || command->selection->numShips<2) return NULL;
    for(i=0;i<selSelected.numShips;++i)
        if (selSelected.ShipPtr[i]->shiptype!=first->shiptype ||
            selSelected.ShipPtr[i]->shiprace!=first->shiprace ||
            selSelected.ShipPtr[i]->formationcommand!=command ||
            selSelected.ShipPtr[i]->playerowner!=universe.curPlayerPtr ||
            (selSelected.ShipPtr[i]->flags & SOF_Dead)) return NULL;
    /* Spacing changes the complete formation, so a same-type subset of a
       mixed formation must not expose a control that also moves other types. */
    for(i=0;i<command->selection->numShips;++i)
        if (command->selection->ShipPtr[i]->shiptype!=first->shiptype ||
            command->selection->ShipPtr[i]->shiprace!=first->shiprace) return NULL;
    return command;
}

static rectangle muiFormationSpreadPanel(void)
{
    sdword width=uiScaleSize(252),height=uiScaleSize(90),margin=uiScaleSize(12);
    sdword x=MAIN_WindowWidth-margin-width, y=MAIN_WindowHeight*35/100;
    rectangle bottom;
    tbGetInfoBarRect(&bottom);
    if (y+height>bottom.y0-margin) y=bottom.y0-margin-height;
    if (y<margin) y=margin;
    return muiRect(x,y,x+width,y+height);
}

static rectangle muiFormationSpreadTrack(const rectangle *panel)
{
    return muiRect(panel->x0+uiScaleSize(16),panel->y0+uiScaleSize(43),
        panel->x1-uiScaleSize(16),panel->y0+uiScaleSize(57));
}

static void muiFormationSpreadSend(CommandToDo *command, bool32 force)
{
    Uint32 now=SDL_GetTicks();
    if (playPackets || (universePause && !opPauseOrders)) return;
    if (muiFormationSpreadPending==muiFormationSpreadSent) return;
    if (!force && now-muiFormationSpreadLastSend<33) return;
    if (muiSelectedFormation()!=command) return;
    clWrapFormationSpread(&universe.mainCommandLayer,(SelectCommand *)&selSelected,muiFormationSpreadPending);
    muiFormationSpreadSent=muiFormationSpreadPending;
    muiFormationSpreadLastSend=now;
}

static void muiFormationSpreadFromMouse(CommandToDo *command, const rectangle *panel, bool32 force)
{
    rectangle track=muiFormationSpreadTrack(panel);
    sdword x=mouseCursorX(), width=track.x1-track.x0;
    unsigned ticks;
    if (width<=0) return;
    if (x<track.x0) x=track.x0;
    if (x>track.x1) x=track.x1;
    ticks=(unsigned)((x-track.x0)*FORMATION_SPREAD_MAX/width);
    if (abs(x-(track.x0+track.x1)/2)<=uiScaleSize(2)) ticks=FORMATION_SPREAD_DEFAULT;
    muiFormationSpreadPending=ticks;
    muiFormationSpreadSend(command,force);
}

static void muiFormationSpreadDraw(regionhandle region)
{
    CommandToDo *command=muiSelectedFormation();
    rectangle panel,track,line,thumb,label;
    unsigned ticks;
    sdword center,x;
    char text[64];
    region->rect=muiRect(0,0,0,0);
    if (!command)
    {
        muiFormationSpreadDragging=FALSE;
        return;
    }
    panel=muiFormationSpreadPanel();region->rect=panel;
    if (muiFormationSpreadDragging)
    {
        if (command!=muiFormationSpreadDragCommand || command->selection->ShipPtr[0]!=muiFormationSpreadDragLeader)
            muiFormationSpreadDragging=FALSE;
        else
        {
            muiFormationSpreadFromMouse(command,&panel,!mouseLeftButton());
            if (!mouseLeftButton()) muiFormationSpreadDragging=FALSE;
        }
    }
    ticks=muiFormationSpreadDragging ? muiFormationSpreadPending : formationGetSpread(command);
    muiEnsureFonts();muiFill(&panel,muiSurface);primRectOutline2(&panel,1,muiLine);
    line=panel;line.x1=line.x0+uiScaleSize(2);muiFill(&line,muiSignal);
    muiText(panel.x0+uiScaleSize(12),panel.y0+uiScaleSize(9),muiSignal,"FORMATION SPREAD",muiBodyFont);
    snprintf(text,sizeof(text),"%s / %d SHIPS",TypeOfFormationToNiceStr(command->formation.formationtype),command->selection->numShips);
    muiText(panel.x0+uiScaleSize(12),panel.y0+uiScaleSize(27),muiMuted,text,muiTinyFont);
    track=muiFormationSpreadTrack(&panel);center=(track.x0+track.x1)/2;
    line=track;line.y0=(track.y0+track.y1)/2-1;line.y1=line.y0+uiScaleSize(2);muiFill(&line,muiLineSoft);
    x=track.x0+(sdword)((track.x1-track.x0)*ticks/FORMATION_SPREAD_MAX);
    line.x0=min(center,x);line.x1=max(center,x);muiFill(&line,muiSignalSoft);
    line=muiRect(center-1,track.y0,center+1,track.y1);muiFill(&line,muiLine);
    thumb=muiRect(x-uiScaleSize(3),track.y0,x+uiScaleSize(3),track.y1);muiFill(&thumb,muiSignal);
    label=muiRect(track.x0,panel.y0+uiScaleSize(65),track.x0+uiScaleSize(54),panel.y1-uiScaleSize(7));
    muiTextCentered(&label,muiMuted,"TIGHT",muiTinyFont);
    label.x0=center-uiScaleSize(43);label.x1=center+uiScaleSize(43);
    muiTextCentered(&label,ticks==FORMATION_SPREAD_DEFAULT ? muiSignal : muiMuted,"DEFAULT",muiTinyFont);
    label.x0=track.x1-uiScaleSize(54);label.x1=track.x1;
    muiTextCentered(&label,muiMuted,"2x",muiTinyFont);
}

static udword muiFormationSpreadProcess(regionhandle region, smemsize ID, udword event, udword data)
{
    CommandToDo *command=muiSelectedFormation();
    rectangle panel,track;
    (void)ID;(void)data;
    if (!command || playPackets || (universePause && !opPauseOrders)) return 0;
    if (muiFormationSpreadDragging && (command!=muiFormationSpreadDragCommand ||
        command->selection->ShipPtr[0]!=muiFormationSpreadDragLeader)) muiFormationSpreadDragging=FALSE;
    panel=muiFormationSpreadPanel();track=muiFormationSpreadTrack(&panel);
    track.y0-=uiScaleSize(8);track.y1+=uiScaleSize(8);
    if (event==RPE_DoubleLeft)
    {
        clWrapFormationSpread(&universe.mainCommandLayer,(SelectCommand *)&selSelected,FORMATION_SPREAD_DEFAULT);
        muiFormationSpreadDragging=FALSE;return RPR_Redraw;
    }
    if (event==RPE_PressLeft && muiPointIn(&track,mouseCursorX(),mouseCursorY()))
    {
        muiFormationSpreadDragging=TRUE;
        muiFormationSpreadDragCommand=command;
        muiFormationSpreadDragLeader=command->selection->ShipPtr[0];
        muiFormationSpreadSent=formationGetSpread(command);
        muiFormationSpreadLastSend=0;
        muiFormationSpreadFromMouse(command,&panel,TRUE);
    }
    else if (muiFormationSpreadDragging && (event==RPE_HoldLeft || event==RPE_ReleaseLeft))
    {
        muiFormationSpreadFromMouse(command,&panel,event==RPE_ReleaseLeft);
        if (event==RPE_ReleaseLeft) muiFormationSpreadDragging=FALSE;
    }
    regRecursiveSetDirty(region);
    return RPR_Redraw; /* Consume panel clicks instead of selecting the world. */
}
