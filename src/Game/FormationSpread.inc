static void formationSpreadBody(const Ship *ship, FormationSpreadBody *body, bool32 spherical)
{
    const StaticCollInfo *info = &ship->staticinfo->staticheader.staticCollInfo;
    float lengths[3] = {info->uplength,info->rightlength,info->forwardlength};
    float corner[3] = {info->collrectoffset.x,info->collrectoffset.y,info->collrectoffset.z};
    unsigned k;
    body->offset[0]=ship->formationOffset.x;
    body->offset[1]=ship->formationOffset.y;
    body->offset[2]=ship->formationOffset.z;
    for(k=0;k<3;++k)
    {
        float radius = info->approxcollspheresize > 0 ? info->approxcollspheresize : info->collspheresize;
        body->half[k] = !spherical && isfinite(lengths[k]) && lengths[k] > 0 ? lengths[k]*0.5f : radius;
        if (!isfinite(body->half[k]) || body->half[k] <= 0) body->half[k]=1.0f;
        body->center[k] = !spherical && isfinite(corner[k]) && lengths[k]>0 ? corner[k]+lengths[k]*0.5f : 0.0f;
    }
}

static real32 formationSpreadMinimumForCommand(CommandToDo *command)
{
    sdword i, count=command->selection->numShips;
    Ship *center=NULL;
    bool32 sphere=command->formation.formationtype==SPHERE_FORMATION;
    bool32 attackingCenter=command->ordertype.order==COMMAND_ATTACK && command->formation.enders &&
        command->attack && command->attack->numTargets>0;
    FormationSpreadBody *bodies;
    real32 result;
    if (sphere && (command->ordertype.attributes & COMMAND_MASK_PROTECTING) && command->protect && command->protect->numShips>0)
        center=command->protect->ShipPtr[0];
    else if (sphere && attackingCenter)
    {
        SpaceObjRotImpTarg *target=command->selection->ShipPtr[0]->attackvars.attacktarget;
        if (!target) target=command->attack->TargetPtr[0];
        if (target->objtype==OBJ_ShipType) center=(Ship *)target;
    }
    bodies=memAlloc(sizeof(*bodies)*(count+1),"formationSpreadBounds",0);
    for(i=0;i<count;++i) formationSpreadBody(command->selection->ShipPtr[i],&bodies[i],sphere);
    if (!sphere || (!(command->ordertype.attributes & COMMAND_MASK_PROTECTING) && !attackingCenter))
        bodies[0].offset[0]=bodies[0].offset[1]=bodies[0].offset[2]=0.0f;
    if (center)
    {
        formationSpreadBody(center,&bodies[count],TRUE);
        bodies[count].offset[0]=bodies[count].offset[1]=bodies[count].offset[2]=0.0f;
        count++;
    }
    result=formationSpreadMinimum(bodies,count);
    memFree(bodies);
    return result;
}

static void formationSpreadScaleOffsets(CommandToDo *command, real32 scale)
{
    sdword i = command->formation.formationtype==SPHERE_FORMATION &&
        ((command->ordertype.attributes & COMMAND_MASK_PROTECTING) ||
         (command->ordertype.order==COMMAND_ATTACK && command->formation.enders &&
          command->attack && command->attack->numTargets>0)) ? 0 : 1;
    for(;i<command->selection->numShips;++i)
    {
        vecMultiplyByScalar(command->selection->ShipPtr[i]->formationOffset,scale);
    }
}

/* Called once after canonical offsets are generated, including tactics,
   formation changes and membership updates. Never compound a normal layout. */
static void formationSpreadApply(CommandToDo *command)
{
    unsigned ticks=formationSpreadTicks(command->formation.sortorder);
    real32 minimum, scale;
    if (ticks==FORMATION_SPREAD_DEFAULT) return;
    minimum=formationSpreadMinimumForCommand(command);
    command->formation.sortorder=formationSpreadPack(command->formation.sortorder,ticks,minimum);
    scale=formationSpreadStoredScale(command->formation.sortorder);
    formationSpreadScaleOffsets(command,scale);
}

unsigned formationGetSpread(const CommandToDo *command)
{
    return command ? formationSpreadTicks(command->formation.sortorder) : FORMATION_SPREAD_DEFAULT;
}

void formationSetSpreadForSelection(CommandLayer *layer, SelectCommand *selection, udword ticks)
{
    CommandToDo *command;
    sdword i;
    real32 oldScale, minimum, scale;
    (void)layer;
    if (!selection || selection->numShips<2 || ticks>FORMATION_SPREAD_MAX) return;
    command=selection->ShipPtr[0]->formationcommand;
    if (!command || !(command->ordertype.attributes & COMMAND_MASK_FORMATION) ||
        command->formation.formationtype> CUSTOM_FORMATION || command->selection->numShips<2) return;
    for(i=0;i<selection->numShips;++i)
        if (selection->ShipPtr[i]->formationcommand!=command ||
            selection->ShipPtr[i]->playerowner!=selection->ShipPtr[0]->playerowner ||
            (selection->ShipPtr[i]->flags & SOF_Dead)) return;
    if (formationGetSpread(command)==ticks) return;
    if (command->formation.formationtype==CUSTOM_FORMATION)
    {
        /* Custom formations ARE a snapshot of their original offsets. Change
           their scale relative to that snapshot, never recapture moving ships. */
        oldScale=formationSpreadStoredScale(command->formation.sortorder);
        minimum=formationGetSpread(command)==FORMATION_SPREAD_DEFAULT ?
            formationSpreadMinimumForCommand(command) : formationSpreadTight(command->formation.sortorder);
        command->formation.sortorder=formationSpreadPack(command->formation.sortorder,ticks,minimum);
        scale=formationSpreadStoredScale(command->formation.sortorder);
        formationSpreadScaleOffsets(command,scale/oldScale);
    }
    else
    {
        command->formation.sortorder=formationSpreadPack(command->formation.sortorder,ticks,1.0f);
        FormationCalculateOffsets(command);
    }
    /* Existing formation processing runs for halted/stationary groups too.
       Wake followers; preserve the leader, movement destination and orders. */
    for(i=1;i<command->selection->numShips;++i) command->selection->ShipPtr[i]->shipidle=FALSE;
}
