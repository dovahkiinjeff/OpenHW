bool32 ThisShipCanDockWith(Ship *ship,Ship *dockwith,DockType dockType)
{
    sdword thisShip;
    if (!ship || !dockwith || !ship->staticinfo || !dockwith->staticinfo) return FALSE;
    bool32 missionVisit = singlePlayerGame && ship != NULL && dockwith != NULL &&
        ship->playerowner == universe.curPlayerPtr && dockwith->shiprace == Traders &&
        dockwith->shiptype == JunkYardHQ && !(dockType & DOCK_FOR_RETIRE) &&
        (ship->staticinfo->shipclass == CLASS_Fighter || ship->staticinfo->shipclass == CLASS_Corvette);

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

