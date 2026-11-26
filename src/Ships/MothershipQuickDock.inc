void mothershipCleanDoorForHSInstant(Ship *ship)
{
    MothershipSpec *spec;

    /* Race R1 also contains Somtaaw hulls. Their specifics and animations
       do not have the HW1 mothership door layout. */
    if(ship == NULL || ship->shiptype != Mothership || ship->shiprace != R1)return;
    spec = (MothershipSpec *)ship->ShipSpecifics;

    if(spec->doorCargo != NULL)
    {
        MothershipDettachObjectFromDoor(ship);
        spec->doorCargo = NULL;
    }

    if( ship->madDoorStatus != MAD_STATUS_DOOR_CLOSED)
    {
        //if door isn't closed
        if(ship->madBindings && ship->madBindings->bPaused)
        {
            //animation is paused...well,unpause it
            //so as to fix a bug in lukes code!
            madAnimationPause(ship,!ship->madBindings->bPaused);
        }
        if(ship->madBindings)madAnimationStop(ship);

        if(ship->staticinfo && ship->staticinfo->madStatic &&
           ship->staticinfo->madStatic->numDoorOpenIndexes>0)
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->DoorOpenIndexes[0];
        ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
        ship->madDoorStatus = MAD_STATUS_DOOR_CLOSED;
        ship->madAnimationFlags=0;
        ship->nextAnim = 0;
    }
}

