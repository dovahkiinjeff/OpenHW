/* Owned 00498c10/004f41a0/004f3fc0 activation, 00514140 ability mapping and
   the ability-mask portion of 00514490. Armor/weapon-slot upgrade stages
   remain separate imports; never manufacture their values from HW1 techs. */
static sdword cataUpgradeValue(const char *key,bool32 write,sdword value)
{
    AIVar *var=aivarFind((char*)key);
    if(write){if(!var)var=aivarCreate((char*)key);aivarValueSet(var,value);}
    return var?aivarValueGet(var):0;
}
static udword cataUpgradeOwnerMask(Player *owner)
{
    static udword cachedResearch,cachedMask;
    char key[32];
    udword research=owner->researchinfo.HasTechnology;
    snprintf(key,sizeof(key),"CUMOwner%u",(unsigned)owner->playerIndex);
    if (research!=cachedResearch)
    {
        unsigned i;
        cachedResearch=research;
        cachedMask=0;
        for(i=0;i<sizeof(cataResearchIdentities)/sizeof(cataResearchIdentities[0]);i++){
            const struct CataResearchIdentity *tech=&cataResearchIdentities[i];
            if(research&TechToBit(tech->slot)){
                sdword original=hwCataTechnologyIndex(tech->name);
                if(original>=0)cachedMask|=hwCataTechnologyAbilityMask(original);
            }
        }
    }
    return (udword)cataUpgradeValue(key,FALSE,0)|cachedMask;
}
void cataCampaignUpgradeActivate(const char *name)
{
    char key[32];sdword tech,i;udword mask;Player *owner=&universe.players[0];
    if(!name)return;
    if(!*name){
        /* Retail clears the activation array here, not acquired research or
           ability masks already calculated for ships. */
        for(i=0;i<42;i++){snprintf(key,sizeof(key),"CU0_%d",i);cataUpgradeValue(key,TRUE,0);}return;
    }
    tech=hwCataTechnologyIndex(name);if(tech<0 || tech>=42)cataScriptFault("Invalid original Cataclysm upgrade activation");
    snprintf(key,sizeof(key),"CU0_%d",tech);cataUpgradeValue(key,TRUE,1);
    mask=cataUpgradeOwnerMask(owner)|hwCataTechnologyAbilityMask(tech);
    cataUpgradeValue("CUMOwner0",TRUE,(sdword)mask);
}
static udword cataUpgradeAvailableMask(const Ship *ship)
{
    sdword original=cataShipOriginalID(ship);
    if(original<0 || !ship->playerowner)return 0;
    return cataUpgradeOwnerMask(ship->playerowner)&hwCataShipAllowedUpgradeMask(original);
}
static udword cataUpgradeApplyShip(Ship *ship)
{
    char key[32];
    udword mask=cataUpgradeAvailableMask(ship);
    if(!ship->playerowner || cataShipOriginalID(ship)<0)return 0;
    snprintf(key,sizeof(key),"CUMShip%u",(unsigned)ship->shipID.shipNumber);
    cataUpgradeValue(key,TRUE,(sdword)mask);return mask;
}
void cataCampaignInitializeShipUpgrades(Ship *ship){cataUpgradeApplyShip(ship);}
/* 00543950 preserves the captured hull's installed abilities, rather than
   replacing them with its new owner's currently researched abilities. */
void cataCampaignCopyShipUpgrades(Ship *destination,Ship *source)
{
    char sourceKey[32],destinationKey[32];AIVar *installed;udword mask;
    snprintf(sourceKey,sizeof(sourceKey),"CUMShip%u",(unsigned)source->shipID.shipNumber);
    installed=aivarFind(sourceKey);mask=installed?(udword)aivarValueGet(installed):cataUpgradeApplyShip(source);
    snprintf(destinationKey,sizeof(destinationKey),"CUMShip%u",(unsigned)destination->shipID.shipNumber);
    cataUpgradeValue(destinationKey,TRUE,(sdword)mask);
}
bool32 cataCampaignShipHasUpgrade(Ship *ship,udword ability)
{
    char key[32];udword mask;AIVar *var;
    if(!ship || !ship->playerowner)return FALSE;
    snprintf(key,sizeof(key),"CUMShip%u",(unsigned)ship->shipID.shipNumber);var=aivarFind(key);
    if(!var && active() && IS_CATA_SHIP(ship->shiptype) && ship->playerowner==universe.curPlayerPtr)return FALSE;
    mask=var?(udword)aivarValueGet(var):cataUpgradeApplyShip(ship);
    return (mask&ability)!=0;
}
udword cataCampaignPendingUpgrade(const Ship *ship)
{
    char key[32];
    udword available;
    if(!active() || !ship || !ship->playerowner || ship->playerowner!=universe.curPlayerPtr ||
       ship->shiprace!=R1 || !IS_CATA_SHIP(ship->shiptype) ||
       (ship->flags&(SOF_Dead|SOF_Disabled|SOF_Hyperspace)) ||
       (ship->specialFlags2&(SPECIAL_2_CataUpgrading|SPECIAL_2_CataScriptControlled)))return 0;
    available=cataUpgradeAvailableMask(ship);
    if(!available)return 0;
    snprintf(key,sizeof(key),"CUMShip%u",(unsigned)ship->shipID.shipNumber);
    return available & ~(udword)cataUpgradeValue(key,FALSE,0);
}

void cataCampaignOrderUpgrades(SelectCommand *selection)
{
    MaxSelection dock={0},inPlace={0};
    sdword i;
    for(i=0;i<selection->numShips;i++)
    {
        Ship *ship=selection->ShipPtr[i];
        udword missing=cataCampaignPendingUpgrade(ship);
        char key[32];
        real32 duration=0.0f,finish;
        sdword packed;
        if(!missing || ship->dockingship || bitTest(ship->flags,SOF_Hide))continue;
        if(cataShipUpgradeByDocking(ship))
        {
            dock.ShipPtr[dock.numShips++]=ship;
            continue;
        }
        /* 00513f40 adds ten seconds for each ability, except the twenty
           second cloak field. 00514540 scales by the original SHP modifier. */
        if(missing&0x10000u)duration+=10.0f;
        for(;missing;missing&=missing-1)duration+=10.0f;
        duration*=cataShipUpgradeTimeModifier(ship);
        if(duration<=0.0f){cataUpgradeApplyShip(ship);continue;}
        finish=universe.totaltimeelapsed+duration;
        memcpy(&packed,&finish,sizeof(packed));
        snprintf(key,sizeof(key),"CUTShip%u",(unsigned)ship->shipID.shipNumber);
        cataUpgradeValue(key,TRUE,packed);
        bitSet(ship->specialFlags2,SPECIAL_2_CataUpgrading);
        inPlace.ShipPtr[inPlace.numShips++]=ship;
    }
    if(inPlace.numShips)clWrapHalt(&universe.mainCommandLayer,(SelectCommand *)&inPlace);
    if(dock.numShips)clWrapDock(&universe.mainCommandLayer,(SelectCommand *)&dock,DOCK_FOR_BOTHREFUELANDREPAIR,NULL);
}

void cataCampaignUpdateShipUpgrade(Ship *ship)
{
    char key[32];
    AIVar *timer;
    sdword packed;
    real32 finish;
    snprintf(key,sizeof(key),"CUTShip%u",(unsigned)ship->shipID.shipNumber);
    timer=aivarFind(key);
    packed=timer?aivarValueGet(timer):0;
    memcpy(&finish,&packed,sizeof(finish));
    if(timer && universe.totaltimeelapsed<finish)return;
    bitClear(ship->specialFlags2,SPECIAL_2_CataUpgrading);
    if(timer)
    {
        aivarDestroy(timer);
        cataUpgradeApplyShip(ship);
    }
}

void cataCampaignDockedUpgrade(Ship *ship,const Ship *host)
{
    if(host->playerowner==ship->playerowner && cataCampaignPendingUpgrade(ship) &&
       cataShipUpgradeByDocking(ship))cataUpgradeApplyShip(ship);
}
