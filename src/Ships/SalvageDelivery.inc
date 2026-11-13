static void salCapDockWorld(Ship *host,sdword index,vector *normal,vector *position)
{
    DockStaticPoint *point=host->dockInfo->dockpoints[index].dockstaticpoint;
    vector local=point->position;
    /* Salvage delivery still follows the classic multi-stage route, including
       its orientation, queue reservation and door offsets. An RM path's final
       waypoint is inside the hangar; substituting only that position makes
       the classic approach and avoidance work against different geometry. */
    matMultiplyMatByVec(normal,&host->rotinfo.coordsys,&point->conenormal);
    matMultiplyMatByVec(position,&host->rotinfo.coordsys,&local);
    vecAddTo(*position,host->posinfo.position);
}

#define SAL_WITHIN_DOCKING_RANGE    6000.0f

sdword salCapFlyToWithinDockingRange(Ship *ship)
{
    real32 maxpushvelocity;

    SalCapCorvetteSpec *spec = (SalCapCorvetteSpec *)ship->ShipSpecifics;
    vector waiting=spec->dockwith->posinfo.position;
    real32 tolerance=SAL_WITHIN_DOCKING_RANGE;
    /* Reserve the delivery port from the established outer arrival envelope.
       Requiring a towed frigate to reach a 250-unit target beside the RM
       endpoint before reserving a port can trap it against the mothership's
       collision/avoidance hull. The later stages own the actual entry route. */
    if(MoveReachedDestinationVariable((Ship *)spec->target,&waiting,tolerance))
    {
        return TRUE;
    }


    //fly to point

    maxpushvelocity = salCapGetMaxPushingVelocity(ship,(Ship *)spec->target);
    if(spec->target->objtype == OBJ_ShipType)
    {
        if(((Ship *)spec->target)->specialFlags2 & SPECIAL_2_SalvageTargetAlreadyDriven)
        {
            goto dontflyme1;
        }
    }
    aishipFlyToPointAvoidingObjs((Ship *)spec->target,&waiting,AISHIP_PointInDirectionFlying,maxpushvelocity);
    salCapTrackHeadingAndUp(spec->target,0.999f);
    if(spec->target->objtype == OBJ_ShipType)
    {
        ((Ship *)spec->target)->specialFlags2 |= SPECIAL_2_SalvageTargetAlreadyDriven;
    }
dontflyme1:;

    return FALSE;
}
