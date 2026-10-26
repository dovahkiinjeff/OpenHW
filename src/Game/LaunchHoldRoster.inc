/* Docked Launch Manager: enumerate live owned holds, independently of the
   original four-carrier FIB. No save fields or synchronized launch flags change. */
static bool32 lmHoldEligible(const Ship *ship)
{
    return ship != NULL && ship->playerowner == universe.curPlayerPtr &&
        /* Drone Frigate storage is driven by LaunchDrone/DDDFrigateHousekeep,
           not normal dock/launch commands (same exclusion as bulk autolaunch). */
        ship->shiptype != DDDFrigate &&
        !(ship->flags & (SOF_Dead | SOF_Hide)) && ship->staticinfo != NULL &&
        ship->shipsInsideMe != NULL &&
        (ship->staticinfo->maxDockableFighters > 0 ||
         ship->staticinfo->maxDockableCorvettes > 0 || ship->shipsInsideMe->insideList.num > 0);
}

static Ship *lmHoldAt(sdword wanted)
{
    Node *node;
    Ship *primary = universe.curPlayerPtr->PlayerMothership;
    if (wanted < 0) return NULL;
    if (lmHoldEligible(primary) && wanted-- == 0) return primary;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);
        if (ship != primary && lmHoldEligible(ship) && wanted-- == 0) return ship;
    }
    return NULL;
}

static sdword lmHoldCount(void)
{
    Node *node;
    Ship *primary = universe.curPlayerPtr->PlayerMothership;
    sdword count = lmHoldEligible(primary) ? 1 : 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);
        if (ship != primary && lmHoldEligible(ship)) count++;
    }
    return count;
}

static sdword lmHoldIndexOf(const Ship *wanted)
{
    Node *node;
    Ship *primary = universe.curPlayerPtr->PlayerMothership;
    sdword index = 0;
    if (lmHoldEligible(primary))
    {
        if (wanted == primary) return 0;
        index++;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);
        if (ship == primary || !lmHoldEligible(ship)) continue;
        if (ship == wanted) return index;
        index++;
    }
    return -1;
}

