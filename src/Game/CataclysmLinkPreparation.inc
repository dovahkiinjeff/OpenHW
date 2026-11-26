/* Linking owns the pair only after cancelling its attack. Retraction uses
   the normal gun-animation cue and must finish before alignment/linking. */
static void mrStopAcolytePairAttack(Ship *first,Ship *second)
{
    MaxSelection pair={0};
    pair.numShips=2;pair.ShipPtr[0]=first;pair.ShipPtr[1]=second;
    clHalt(&universe.mainCommandLayer,(SelectCommand*)&pair);
    first->nextAnim=second->nextAnim=MAD_ANIMATION_NOTHING;
    madLinkInCloseGunsShip(first);madLinkInCloseGunsShip(second);
}
static bool32 mrAcolyteGunsRetracted(Ship *ship)
{
    if(!ship->madBindings || !ship->staticinfo->madStatic ||
       ship->staticinfo->madStatic->numGunCloseIndexes<1)return TRUE;
    madLinkInCloseGunsShip(ship);
    return ship->madGunStatus==MAD_STATUS_GUNS_CLOSED;
}
