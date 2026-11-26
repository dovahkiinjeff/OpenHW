static TechStatics cataResearch;
void cataCampaignResearchStatics(Player *player)
{
    sdword i;
    memset(&cataResearch,0,sizeof(cataResearch));
    for(i=0;i<NumTechnologies;i++)cataResearch.TechNeededToResearch[i]=RM_Disabled;
    for(i=0;i<(sdword)(sizeof(cataResearchIdentities)/sizeof(cataResearchIdentities[0]));i++){
        sdword slot=cataResearchIdentities[i].slot,duration=hwCataResearchDuration(cataResearchIdentities[i].name);
        if(duration<=0)cataScriptFault("Owned Cataclysm research definition is missing");
        cataResearch.TechNeededToResearch[slot]=0;cataResearch.TimeToComplete[slot]=duration;
    }
    /* Owned SPRSShipDepend sACV: fighter drive (starting tech) + Link. */
    cataResearch.TechNeededToBuildShip[CataACV]=TechToBit(CATA_LINK_TECH);
    player->researchinfo.techstat=&cataResearch;
}
