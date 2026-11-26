/* Original Cataclysm processSingleLaunchShipToDo, 00465860, ordinary
   hangar-exit branch (00466340): health := staticinfo->maxhealth.
   It does not depend on HW1 repairOtherShipRate or introduce a repair wait. */
static void cataDockServiceOnLaunch(Ship *ship)
{
    ShipStaticInfo *info;
    if(!singlePlayerGame || !hwIsCataclysmMission(spGetCurrentMission()) ||
       !ship || !(info=ship->staticinfo) || bitTest(ship->flags,SOF_Dead) ||
       ship->health<=0)return;
    /* Every ordinary hangar exit restores health, including Workers.
       Installed components use retail's separate construction branch. */
    if(info->shipclass!=CLASS_Mothership &&
       !(ship->shiptype>=CataMothershipCommand && ship->shiptype<=CATA_LAST_COMPONENT))
        ship->health=info->maxhealth;
}
