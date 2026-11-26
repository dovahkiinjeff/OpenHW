/* Original 00499c70 sets KAS launch flags and issues dock mode 2, which is
   LAUNCH_AT_SPECIFIC_SHIP in Cataclysm's enum. HW1's dock types are bitmasks. */
static void cataLaunchNewReinforcements(GrowSelection *ships,Ship *host)
{
    sdword i;
    for(i=0;i<ships->selection->numShips;i++){
        Ship *ship=ships->selection->ShipPtr[i];
        bitClear(ship->specialFlags,SPECIAL_STAY_TILL_EXPLICITLAUNCH);
        bitSet(ship->specialFlags,SPECIAL_LaunchedFromKas|SPECIAL_KasCheckDoneLaunching);
    }
    clLaunchMultipleShips(&universe.mainCommandLayer,ships->selection,host);
}
