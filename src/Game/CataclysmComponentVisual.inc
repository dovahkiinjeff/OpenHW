/* Shared by fleet reveal, save restoration and render-list construction.
   The replacement embeds these four classic parts, but has separate native
   lower-deck, ore-canister and optional module models. */
bool32 cataShipEmbeddedInNativeHull(Ship *ship)
{
    Ship *host;vector marker;
    if(!ship || !ship->clampInfo || !ship->clampInfo->host ||
       ship->clampInfo->host->objtype!=OBJ_ShipType)return FALSE;
    switch(ship->shiptype){
    case CataMothershipCommand:case CataMothershipEngineering:
    case CataMothershipEnginesLower:case CataMothershipEnginesUpper:break;
    default:return FALSE;
    }
    host=(Ship *)ship->clampInfo->host;
    return host->shiptype==CataMothership &&
        meshRemasteredMarkerPosition(host,"native_hull",&marker);
}
