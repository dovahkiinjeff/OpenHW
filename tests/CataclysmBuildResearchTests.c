#include "Universe.h"
#include "ConsMgr.h"
#include "CataclysmCampaign.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ITEM_CLASS 0
#define ITEM_SHIP 1
#include "../src/Game/CataclysmBuildList.inc"
static int checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int hwCataResearchDuration(const char *name){
    if(!strcmp(name,"TECH_LINK") || !strcmp(name,"TECH_AFTERBURNER"))return 175;
    if(!strcmp(name,"TECH_REPAIR") || !strcmp(name,"TECH_SALVAGE"))return 30;
    if(!strcmp(name,"TECH_DRIVE_FIGHTER"))return 45;
    if(!strcmp(name,"TECH_MIMIC"))return 200;
    CHECK(0);return 0;
}
void cataScriptFault(const char *message){fprintf(stderr,"%s\n",message);exit(1);}

#include "../src/Game/CataclysmResearchIdentity.inc"
#include "../src/Game/CataclysmResearchStatics.inc"
int main(void)
{
    Player player={0};int i,acvs=0;ShipClass heading=CLASS_Fighter;
    cataCampaignResearchStatics(&player);
    CHECK(player.researchinfo.techstat!=NULL);
    CHECK(player.researchinfo.techstat->TimeToComplete[CATA_LINK_TECH]==175);
    CHECK(player.researchinfo.techstat->TimeToComplete[CATA_REPAIR_TECH]==30);
    CHECK(player.researchinfo.techstat->TimeToComplete[CATA_SALVAGE_TECH]==30);
    CHECK(cataCampaignTechSlot("TECH_REPAIR")==CATA_REPAIR_TECH);
    CHECK(cataCampaignTechSlot("TECH_SALVAGE")==CATA_SALVAGE_TECH);
    CHECK(cataCampaignTechSlot("unknown")==-1);
    CHECK(!strcmp(cataCampaignTechName(CATA_LINK_TECH),"TECH_LINK"));
    CHECK(!strcmp(cataCampaignTechTitle(CATA_REPAIR_TECH),"Repair technology"));
    for(i=0;cmShipsAvailableSomtaaw[i].nJobs!=-1;i++){
        if(cmShipsAvailableSomtaaw[i].itemtype==ITEM_CLASS)heading=(ShipClass)cmShipsAvailableSomtaaw[i].type;
        else if(cmShipsAvailableSomtaaw[i].type==CataACV){++acvs;CHECK(heading==CLASS_Corvette);}
    }
    CHECK(acvs==1);
    CHECK(player.researchinfo.techstat->TechNeededToBuildShip[CataACV]==TechToBit(CATA_LINK_TECH));
    CHECK((player.researchinfo.techstat->TechNeededToBuildShip[CataACV]&player.researchinfo.HasTechnology)!=player.researchinfo.techstat->TechNeededToBuildShip[CataACV]);
    player.researchinfo.HasTechnology|=TechToBit(CATA_LINK_TECH);
    CHECK((player.researchinfo.techstat->TechNeededToBuildShip[CataACV]&player.researchinfo.HasTechnology)==player.researchinfo.techstat->TechNeededToBuildShip[CataACV]);
    /* Reinitializing statics on a saved profile retains the acquired bit. */
    cataCampaignResearchStatics(&player);CHECK(player.researchinfo.HasTechnology&TechToBit(CATA_LINK_TECH));
    CHECK(player.researchinfo.techstat->TechNeededToBuildShip[CataAcolyte]==0);
    printf("PASS: %d ACV build and research checks\n",checks);return 0;
}
