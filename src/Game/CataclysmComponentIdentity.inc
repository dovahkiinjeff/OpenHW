Ship *cataShipComponentHost(const Ship *ship)
{
    if(ship && ship->objtype==OBJ_ShipType &&
        ship->shiptype>=CataMothershipCommand && ship->shiptype<=CATA_LAST_COMPONENT &&
        ship->clampInfo && ship->clampInfo->host &&
        ship->clampInfo->host->objtype==OBJ_ShipType &&
        ((Ship *)ship->clampInfo->host)->shiptype==CataMothership)
        return (Ship *)ship->clampInfo->host;
    return NULL;
}

bool32 cataShipIsAttachedComponent(const Ship *ship)
{
    return cataShipComponentHost(ship)!=NULL;
}
