static bool32 dockCaptureBuildAllRaceCompatible(const Ship *ship,
                                                   const Ship *dockwith)
{
    /* RTX-0075: mixed captured fleets remain mechanically separated by the
       hull's intrinsic race.  Restrict only the player's mixed fleet and only
       while CAPTURE / BUILD ALL is enabled; mission AI scripts and multiplayer
       retain the original docking rules. */
    if (!singlePlayerGame || !opCaptureBuildAll) return TRUE;
    if (ship == NULL || dockwith == NULL) return FALSE;
    if (ship->playerowner != universe.curPlayerPtr &&
        dockwith->playerowner != universe.curPlayerPtr)
        return TRUE;
    /* These neutral mission stations intentionally receive foreign visitors.
       Keep the original alliance, status and ship-class checks below; only
       the captured-fleet race restriction must not block mission docking. */
    if (dockwith->shiprace == Traders &&
        (dockwith->shiptype == JunkYardHQ || dockwith->shiptype == ResearchStation))
        return TRUE;
    return ship->shiprace == dockwith->shiprace;
}

bool32 ThisShipCanDockWith(Ship *ship,Ship *dockwith,DockType dockType)
{
    sdword thisShip;
    bool32 missionVisit = singlePlayerGame && ship != NULL && dockwith != NULL &&
        ship->playerowner == universe.curPlayerPtr && dockwith->shiprace == Traders &&
        dockwith->shiptype == JunkYardHQ && !(dockType & DOCK_FOR_RETIRE) &&
        (ship->staticinfo->shipclass == CLASS_Fighter || ship->staticinfo->shipclass == CLASS_Corvette);

    if (!missionVisit && !dockCaptureBuildAllRaceCompatible(ship, dockwith))
        return FALSE;

    if (dockwith->staticinfo->canReceiveSomething)
    {
        if ((dockwith->flags & (SOF_Dead|SOF_Disabled|SOF_Crazy|SOF_Hyperspace)) == 0)
        {
            /* Karos's communications relay is a neutral mission contact, not
               an allied fleet factory. Its scripted latch accepts every
               player-controlled fighter/corvette, including captured races. */
            if (missionVisit) return TRUE;
            if (allianceIsShipAlly(ship,dockwith->playerowner) || bitTest(dockwith->specialFlags, SPECIAL_FriendlyStatus))
            {
                thisShip = ThisShipIs(ship->staticinfo);
                if(dockType & DOCK_FOR_RETIRE)
                {
                    if(dockwith->staticinfo->canReceiveShipsForRetire)
                    {
                        if(dockwith->shiptype == ship->shiptype)
                            return FALSE;

                        if(shipRetireNeedsMothership(ship))
                        {
                            //need a 'big' docking bay
                            if(dockwith->shiptype == Mothership)
                            {
                                return TRUE;
                            }
                        }
                        else
                        {
                            return TRUE;
                        }
                    }
                }
                else
                {
                    if (dockwith->shiptype == P2FuelPod)
                    {
                        if ((ship->shiptype == P2AdvanceSwarmer) || (ship->shiptype == P2Swarmer))
                        {
                            return TRUE;
                        }
                        return FALSE;
                    }

                    if ((dockwith->staticinfo->canReceiveTheseShips[thisShip]) &&
                        (dockwith->shiptype != ship->shiptype))   // ships of same type cannot dock
                        return TRUE;
                }
            }
        }
    }

    return FALSE;
}

