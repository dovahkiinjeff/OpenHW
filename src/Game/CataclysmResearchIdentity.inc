/* Local host slots carry original Cataclysm technology identities; these
   assignments do not change the HW1 research table or saved enum layout. */
static const struct CataResearchIdentity {TechnologyType slot;const char *name,*title,*description;} cataResearchIdentities[]={
    {CATA_FIGHTER_DRIVE_TECH,"TECH_DRIVE_FIGHTER","Fighter drive","Enables Somtaaw fighter construction."},
    {CATA_LINK_TECH,"TECH_LINK","Acolyte linking","Allows two Acolytes to link into an ACV, and an ACV to separate into two Acolytes."},
    {CATA_REPAIR_TECH,"TECH_REPAIR","Repair technology","Allows Workers to repair friendly ships."},
    {CATA_SALVAGE_TECH,"TECH_SALVAGE","Salvage technology","Allows Workers to salvage eligible targets and tow them to a processing ship."},
    {CATA_MIMIC_TECH,"TECH_MIMIC","Mimic technology","Enables Mimic technology and disguises."},
    {CATA_AFTERBURNER_TECH,"TECH_AFTERBURNER","Afterburner","Enables afterburners on compatible ships."}
};
sdword cataCampaignTechSlot(const char *name)
{
    unsigned i;if(!name)return -1;
    for(i=0;i<sizeof(cataResearchIdentities)/sizeof(cataResearchIdentities[0]);i++)
        if(!strcmp(name,cataResearchIdentities[i].name))return cataResearchIdentities[i].slot;
    return -1;
}
const char *cataCampaignTechName(TechnologyType tech)
{
    unsigned i;for(i=0;i<sizeof(cataResearchIdentities)/sizeof(cataResearchIdentities[0]);i++)
        if(cataResearchIdentities[i].slot==tech)return cataResearchIdentities[i].name;
    return NULL;
}
const char *cataCampaignTechTitle(TechnologyType tech)
{
    unsigned i;for(i=0;i<sizeof(cataResearchIdentities)/sizeof(cataResearchIdentities[0]);i++)
        if(cataResearchIdentities[i].slot==tech)return cataResearchIdentities[i].title;
    return NULL;
}
const char *cataCampaignTechDescription(TechnologyType tech)
{
    unsigned i;for(i=0;i<sizeof(cataResearchIdentities)/sizeof(cataResearchIdentities[0]);i++)
        if(cataResearchIdentities[i].slot==tech)return cataResearchIdentities[i].description;
    return NULL;
}
