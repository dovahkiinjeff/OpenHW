/* Called after alliance, ship-category and live-host eligibility checks.
   Cataclysm's hangars have no HW1 fuel pump fields. Their custom docking
   handler supplies service without those legacy prerequisites. */
static bool32 dockHostSupportsPurpose(const ShipStaticInfo *host,DockType purpose)
{
    if(host->shiptype==CataMothership && host->canReceiveShips &&
       (purpose&(DOCK_FOR_REFUEL|DOCK_FOR_REPAIR|DOCK_FOR_BOTHREFUELANDREPAIR)))return TRUE;
    return ((purpose&DOCK_TO_DEPOSIT_RESOURCES) && host->canReceiveResources) ||
        ((purpose&DOCK_FOR_REFUEL) && host->canReceiveShips && host->pumpFuelRate>0) ||
        ((purpose&DOCK_FOR_REPAIR) && host->canReceiveShips && host->repairOtherShipRate>0) ||
        ((purpose&DOCK_FOR_BOTHREFUELANDREPAIR) && host->canReceiveShips && host->pumpFuelRate>0 && host->repairOtherShipRate>0) ||
        ((purpose&DOCK_PERMANENTLY) && host->canReceiveShipsPermanently) ||
        ((purpose&DOCK_FOR_RETIRE) && host->canReceiveShipsForRetire);
}
