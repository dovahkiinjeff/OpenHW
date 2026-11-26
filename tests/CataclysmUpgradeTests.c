#include "Universe.h"
#include "CataclysmCampaign.h"
#include "CataclysmShips.h"
#include "AIVar.h"
#include "CommandWrap.h"
#include "Dock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
Universe universe;static AIVar values[256];static sdword used;static int checks;
static int dockOrders,haltOrders;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
AIVar *aivarFind(char *label){sdword i;for(i=0;i<used;i++)if(!strcmp(values[i].label,label))return &values[i];return NULL;}
AIVar *aivarCreate(char *label){AIVar *v;CHECK(used<256);v=&values[used++];memset(v,0,sizeof(*v));snprintf(v->label,sizeof(v->label),"%s",label);return v;}
sdword aivarValueGet(AIVar *v){return v->value;}void aivarValueSet(AIVar *v,sdword value){v->value=value;}
void aivarDestroy(AIVar *v){*v=values[--used];}
static bool32 active(void){return TRUE;}
bool32 cataShipUpgradeByDocking(const Ship *ship){return ship->shiptype==CataAcolyte;}
real32 cataShipUpgradeTimeModifier(const Ship *ship){(void)ship;return 1.2f;}
void clWrapHalt(CommandLayer *layer,SelectCommand *ships){(void)layer;haltOrders+=ships->numShips;}
void clWrapDock(CommandLayer *layer,SelectCommand *ships,DockType type,Ship *host)
{(void)layer;CHECK(type==DOCK_FOR_BOTHREFUELANDREPAIR && host==NULL);dockOrders+=ships->numShips;}
void cataScriptFault(const char *message){fprintf(stderr,"%s\n",message);exit(1);}
int hwCataTechnologyIndex(const char *name)
{
    static const char *names[]={"TECH_DRIVE_FIGHTER","TECH_LINK","TECH_REPAIR","TECH_SALVAGE","TECH_MIMIC","TECH_AFTERBURNER"};
    static const int ids[]={8,24,6,7,25,26};unsigned i;
    for(i=0;i<6;i++)if(!strcmp(name,names[i]))return ids[i];return -1;
}
unsigned hwCataTechnologyAbilityMask(unsigned index)
{return index==6?0x40000u:index==7?0x80000u:index==24?0x18u:index==25?0x20u:index==26?0x100u:0;}
unsigned hwCataShipAllowedUpgradeMask(unsigned original){return original==14?0xc0000u:(original==1?8u:0);}
sdword cataShipOriginalID(const Ship *ship){return ship->shiptype==CataWorker?14:(ship->shiptype==CataAcolyte?1:122);}
#include "../src/Game/CataclysmResearchIdentity.inc"
#include "../src/Game/CataclysmUpgrades.inc"
int main(void)
{
    Ship worker={0},enemy={0},pod={0},acolyte={0};
    MaxSelection selection={0};
    worker.shiptype=enemy.shiptype=CataWorker;pod.shiptype=CataMessagePod;acolyte.shiptype=CataAcolyte;
    worker.shipID.shipNumber=1;enemy.shipID.shipNumber=2;pod.shipID.shipNumber=3;
    universe.players[0].playerIndex=0;universe.players[1].playerIndex=1;
    universe.curPlayerPtr=&universe.players[0];
    worker.playerowner=pod.playerowner=&universe.players[0];enemy.playerowner=&universe.players[1];
    acolyte.shipID.shipNumber=4;acolyte.playerowner=worker.playerowner;
    worker.shiplink.structptr=&worker;enemy.shiplink.structptr=&enemy;pod.shiplink.structptr=&pod;
    worker.shiplink.next=&enemy.shiplink;enemy.shiplink.next=&pod.shiplink;universe.ShipList.head=&worker.shiplink;
    CHECK(!cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_SALVAGE));
    cataCampaignUpgradeActivate("TECH_SALVAGE");
    CHECK(universe.players[0].researchinfo.HasTechnology==0); /* Script activation is not research ownership. */
    CHECK(cataUpgradeValue("CU0_7",FALSE,0)==1);
    CHECK(cataUpgradeApplyShip(&worker)==CATA_UPGRADE_SALVAGE);
    CHECK(cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_SALVAGE));
    CHECK(!cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_REPAIR));
    CHECK(!cataCampaignShipHasUpgrade(&enemy,CATA_UPGRADE_SALVAGE));
    cataCampaignUpgradeActivate("TECH_MIMIC");CHECK(!(cataUpgradeApplyShip(&worker)&0x20u));
    CHECK(cataUpgradeApplyShip(&pod)==0);
    cataCampaignUpgradeActivate("");CHECK(cataUpgradeValue("CU0_7",FALSE,0)==0);
    CHECK(cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_SALVAGE));
    cataCampaignInitializeShipUpgrades(&acolyte);
    universe.players[0].researchinfo.HasTechnology=TechToBit(CATA_REPAIR_TECH)|TechToBit(CATA_LINK_TECH);
    CHECK(cataCampaignPendingUpgrade(&worker)==CATA_UPGRADE_REPAIR);
    CHECK(cataCampaignPendingUpgrade(&acolyte)==CATA_UPGRADE_LINK);
    CHECK(!cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_REPAIR));
    selection.numShips=3;selection.ShipPtr[0]=&worker;selection.ShipPtr[1]=&acolyte;selection.ShipPtr[2]=&enemy;
    cataCampaignOrderUpgrades((SelectCommand *)&selection);
    CHECK(dockOrders==1 && haltOrders==1);
    CHECK(!cataCampaignShipHasUpgrade(&acolyte,CATA_UPGRADE_LINK));
    cataCampaignDockedUpgrade(&acolyte,&enemy);
    CHECK(!cataCampaignShipHasUpgrade(&acolyte,CATA_UPGRADE_LINK));
    cataCampaignDockedUpgrade(&acolyte,&pod);
    CHECK(cataCampaignShipHasUpgrade(&acolyte,CATA_UPGRADE_LINK));
    universe.totaltimeelapsed=11.9f;
    cataCampaignUpdateShipUpgrade(&worker);
    CHECK(!cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_REPAIR));
    CHECK(aivarFind("CUTShip1")!=NULL);
    universe.totaltimeelapsed=12.1f;
    cataCampaignUpdateShipUpgrade(&worker);
    CHECK(aivarFind("CUTShip1")==NULL && cataCampaignPendingUpgrade(&worker)==0);
    CHECK(cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_REPAIR));
    CHECK(!cataCampaignShipHasUpgrade(&enemy,CATA_UPGRADE_REPAIR));
    cataCampaignCopyShipUpgrades(&enemy,&worker);
    CHECK(cataCampaignShipHasUpgrade(&enemy,CATA_UPGRADE_REPAIR));
    CHECK(cataCampaignShipHasUpgrade(&enemy,CATA_UPGRADE_SALVAGE));
    CHECK(universe.players[1].researchinfo.HasTechnology==0);
    {AIVar saved[256];memcpy(saved,values,sizeof(values));memset(values,0,sizeof(values));memcpy(values,saved,sizeof(values));CHECK(cataCampaignShipHasUpgrade(&worker,CATA_UPGRADE_REPAIR));}
    CHECK(!cataCampaignShipHasUpgrade(NULL,CATA_UPGRADE_REPAIR));
    printf("PASS: %d Cataclysm activation, hull permission and saved ability checks\n",checks);return 0;
}
