void clLaunchMultipleShips(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr launchFrom)
{
    sdword numShips = selectcom->numShips;
    sdword i;
    SelectCommand selectone;

    if (numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships to launch");
#endif
        return;
    }

    if (launchFrom == NULL)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ship to launch from");
#endif
        return;
    }

    /* A Drone Frigate's children have their own deployment state machine.
       Ordinary launch commands would give them a second movement controller.
       Reject old/queued manual requests too; LaunchDrone does not use this API. */
    if (launchFrom->shiptype == DDDFrigate) return;

    selectone.numShips = 1;
    for (i=0;i<numShips;i++)
    {
        if (!(selectcom->ShipPtr[i]->attributes & ATTRIBUTES_Defector))      // don't let the defector launch
        {
            selectone.ShipPtr[0] = selectcom->ShipPtr[i];
            clLaunchShip(comlayer,&selectone,launchFrom);
        }
    }
}

