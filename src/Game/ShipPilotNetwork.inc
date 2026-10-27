/* Shared pilot simulation follows the same ordered packets as RTS commands.
   Never sample another client's keyboard, focus, camera, or selected ships. */
static Ship *pilotFindShip(uword id)
{
    Ship *s=(Ship *)IDToPtrTableIDToObj(&ShipIDToPtr,id);
    return s && s->objtype==OBJ_ShipType?s:NULL;
}
static bool32 pilotSessionValid(const ShipPilotState *state)
{
    Ship *s=state->ship;
    if(!s || pilotFindShip(state->id)!=s)return FALSE;
    if(s->playerowner->playerIndex!=state->ownerIndex ||
       (s->flags&(SOF_Dead|SOF_Hide|SOF_Disabled|SOF_Hyperspace|SOF_NISShip|SOF_Crazy|SOF_Clamped)) ||
       s->dockingship || !s->command || s->command->ordertype.order!=COMMAND_HALT)return FALSE;
    return state->squad?pilotFormation(s)!=NULL:s->command->selection->numShips==1;
}
Ship *shipPilotTargetFor(const Ship *ship)
{
    sdword i;
    if(!pilotNetworked())return shipPilotControls(ship)?shipPilotTarget():NULL;
    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++) {
        ShipPilotState *state=&networkPilots[i];
        Ship *target;
        if(!pilotSessionValid(state) || !pilotStateControls(state,ship) || state->cockpit || !state->target)continue;
        target=pilotFindShip(state->targetID);
        if(target==state->target && (target->flags&SOF_Targetable) &&
           !(target->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace|SOF_Cloaked)))return target;
    }
    return NULL;
}
static bool32 pilotFiniteVector(vector v)
{ return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z) && fabs(v.x)<1e12f&&fabs(v.y)<1e12f&&fabs(v.z)<1e12f; }
void shipPilotReceiveFrame(sdword from,const ShipPilotFrame *frame)
{
    ShipPilotState *state;
    Ship *s;
    sdword i;
    if(from<0 || from>=MAX_MULTIPLAYER_PLAYERS || !frame || frame->action>PILOT_FRAME_UPDATE)return;
    if((sdword)(frame->sequence-networkSequences[from])<=0)return;
    state=&networkPilots[from];
    if(frame->action==PILOT_FRAME_END) {
        networkSequences[from]=frame->sequence;
        if(state->ship && state->id==frame->ship) { free(state->weapons);memset(state,0,sizeof(*state)); }
        if(from==universe.curPlayerIndex && endShip==frame->ship)endPending=FALSE;
        return;
    }
    if(frame->squad>1 || frame->cockpit>1 || frame->commandMode>1 || frame->all>1 || frame->fire>1 ||
       frame->locked>1 || frame->navigation>4 || frame->held>127 || frame->weaponCount>PILOT_WEAPON_BYTES*8 ||
       !pilotFiniteVector(frame->aim) || !pilotFiniteVector(frame->navHeading) ||
       !isfinite(frame->throttle) || frame->throttle < -1 || frame->throttle > 1 ||
       !isfinite(frame->navRange) || frame->navRange<100 || frame->navRange>50000 ||
       !isfinite(frame->yawRate) || !isfinite(frame->pitchRate))return;
    s=pilotFindShip(frame->ship);
    if(!s || s->playerowner->playerIndex!=from ||
       (s->flags&(SOF_Dead|SOF_Hide|SOF_Disabled|SOF_Hyperspace|SOF_NISShip|SOF_Crazy|SOF_Clamped)) || s->dockingship)return;
    if((frame->cockpit && s->staticinfo->shipclass!=CLASS_Fighter) ||
       (frame->commandMode && s->staticinfo->shipclass>CLASS_Frigate))return;
    if(!state->ship || state->id!=frame->ship) {
        SelectCommand one;
        CommandToDo *formation=frame->squad?pilotFormation(s):NULL;
        if(frame->action!=PILOT_FRAME_BEGIN || (frame->squad && !formation))return;
        free(state->weapons);memset(state,0,sizeof(*state));
        state->weaponCount=s->gunInfo?s->gunInfo->numGuns:0;
        if(state->weaponCount>PILOT_WEAPON_BYTES*8) { state->weaponCount=0;return; }
        if(state->weaponCount) {
            state->weapons=(unsigned char *)calloc(state->weaponCount,1);
            if(!state->weapons) { state->weaponCount=0;return; }
        }
        state->ship=s;state->id=frame->ship;state->ownerIndex=from;state->squad=frame->squad;
        one.numShips=1;one.ShipPtr[0]=s;
        clHaltThese(&universe.mainCommandLayer,formation?formation->selection:&one);
        if(formation && pilotFormation(s))s->command->formation.formationLocked=FALSE;
    }
    if(!pilotSessionValid(state)) {
        free(state->weapons);memset(state,0,sizeof(*state));return;
    }
    networkSequences[from]=frame->sequence;
    state->cockpit=frame->cockpit;state->commandMode=frame->commandMode;
    state->all=frame->all;state->netFire=frame->fire;state->held=frame->held;
    state->throttle=frame->throttle;state->navigation=frame->navigation;
    state->navRange=frame->navRange;state->navHeading=frame->navHeading;
    if(vecMagnitudeSquared(state->navHeading)>0)vecNormalize(&state->navHeading);
    state->aim=frame->aim;state->pickedID=frame->picked;
    state->netYaw=frame->yawRate;state->netPitch=frame->pitchRate;
    state->target=frame->locked?pilotFindShip(frame->target):NULL;state->targetID=frame->target;
    state->navTarget=pilotFindShip(frame->navTarget);state->navID=frame->navTarget;
    for(i=0;i<state->weaponCount;i++)
        state->weapons[i]=i<frame->weaponCount && !!(frame->weapons[i/8]&(1u<<(i%8)));
    if(from==universe.curPlayerIndex && pilot.ship==s && pilot.id==state->id)pilot.awaiting=FALSE;
}
static void pilotSendInput(ubyte action)
{
    ShipPilotFrame frame;
    sdword k;
    bool32 focus=SDL_GetKeyboardFocus()!=NULL;
    memset(&frame,0,sizeof(frame));
    frame.sequence=++localSequence;frame.action=action;frame.ship=action==PILOT_FRAME_END?endShip:pilot.id;
    if(action!=PILOT_FRAME_END) {
        Ship *target;
        vector point;
        if(!pilot.ship)return;
        pilotThrottleUpdate();
        if(pilot.cockpit && focus)pilot.throttle=max(-1.0f,min(1.0f,pilot.throttle+
            ((keyIsHit(WKEY)?1:0)-(keyIsHit(SKEY)?1:0))*universe.phystimeelapsed*0.75f));
        frame.squad=pilot.squad;frame.cockpit=pilot.cockpit;frame.commandMode=pilot.commandMode;
        frame.throttle=focus?pilot.throttle:0;frame.all=pilot.all;
        frame.navigation=focus?pilot.navigation:0;frame.navRange=pilot.navRange;frame.navHeading=pilot.navHeading;
        frame.navTarget=pilot.navID;
        target=pilotPickTarget(pilot.ship,&universe.mainCameraCommand.actualcamera,&pilot.aim,&point);
        frame.aim=point;frame.picked=target?target->shipID.shipNumber:0xffff;
        if(shipPilotTarget()) { frame.locked=TRUE;frame.target=pilot.targetID; }
        frame.fire=focus && (keyIsHit(SPACEKEY) || (mouseLeftButton() && !pilot.uiPress && !pilotOverButton(mouseCursorX(),mouseCursorY())));
        for(k=0;k<KEY_TOTAL_KEYS;k++)if(focus && keyIsHit(k))frame.held|=pilotInputBit(k);
        if(focus && !mouseRightButton()) {
            real32 dt=max(.001f,universe.phystimeelapsed);
            frame.yawRate=-pilot.steerX*.003f/dt;frame.pitchRate=pilot.steerY*.003f/dt;
        }
        pilot.steerX=pilot.steerY=0;
        frame.weaponCount=(uword)min(pilot.weaponCount,PILOT_WEAPON_BYTES*8);
        for(k=0;k<frame.weaponCount;k++)if(pilot.weapons[k])frame.weapons[k/8]|=1u<<(k%8);
    }
    clSendPilotFrame(&frame);
}
void shipPilotUpdate(void)
{
    ShipPilotState local;
    sdword i;
    if(!pilotNetworked()) { pilotUpdateSimulation();return; }
    if(!playPackets) {
        if(endPending)pilotSendInput(PILOT_FRAME_END);
        if(shipPilotShip())pilotSendInput(pilot.awaiting?PILOT_FRAME_BEGIN:PILOT_FRAME_UPDATE);
    }
    local=pilot;networkSimulation=TRUE;
    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++) {
        if(!networkPilots[i].ship)continue;
        if(!pilotSessionValid(&networkPilots[i])) {
            free(networkPilots[i].weapons);memset(&networkPilots[i],0,sizeof(networkPilots[i]));continue;
        }
        pilot=networkPilots[i];pilotUpdateSimulation();networkPilots[i]=pilot;
    }
    pilot=local;networkSimulation=FALSE;
}
void shipPilotReset(void)
{
    sdword i;
    resettingPilots=TRUE;shipPilotExit();resettingPilots=FALSE;
    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)free(networkPilots[i].weapons);
    memset(networkPilots,0,sizeof(networkPilots));memset(networkSequences,0,sizeof(networkSequences));
    localSequence=0;endPending=FALSE;
}
