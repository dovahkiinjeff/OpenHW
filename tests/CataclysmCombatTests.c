#include "Universe.h"
#include "Gun.h"
#include "AIShip.h"
#include "Attack.h"
#include "Collision.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks,single,multiple,approaches,fired;
static float gunRange,closeRange;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
void aishipGetTrajectory(Ship *ship,SpaceObjRotImpTarg *target,vector *trajectory)
{(void)ship;(void)target;vecSet(*trajectory,100,0,0);}
real32 RangeToTargetGivenDist(Ship *ship,SpaceObjRotImpTarg *target,real32 distance)
{(void)ship;(void)target;return distance;}
bool32 gunShootGunsAtTarget(Ship *ship,SpaceObjRotImpTarg *target,real32 range,vector *trajectory)
{(void)ship;(void)target;CHECK(range==100&&trajectory->x==1);++single;return TRUE;}
bool32 gunShootGunsAtMultipleTargets(Ship *ship){(void)ship;++multiple;return TRUE;}
void attackStraightForward(Ship *ship,SpaceObjRotImpTarg *target,real32 range,real32 close)
{(void)ship;(void)target;++approaches;gunRange=range;closeRange=close;}
bool32 gunCanShoot(Ship *ship,Gun *gun){(void)ship;return !gun->gunstatic->cataWeaponDisabled;}
void gunShoot(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target){(void)ship;(void)gun;(void)target;++fired;}
void missileShoot(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target){(void)ship;(void)gun;(void)target;++fired;}
#include "../src/Game/CataclysmCombat.c"
#include "../src/Game/CataclysmWeaponFx.h"
int main(void)
{
    Ship ship={0};ShipStaticInfo info={0},before={0};SpaceObjRotImpTarg target={0};
    GunStatic gun={0};GunInfo *guns=calloc(1,sizeof(GunInfo));CHECK(guns!=NULL);
    guns->numGuns=1;guns->guns[0].gunstatic=&gun;ship.gunInfo=guns;ship.staticinfo=&info;
    info.shiptype=CataProcessor;info.bulletRange[0]=5000;
    cataShipConfigureCombat(&info);
    CHECK(info.custshipheader.CustShipAttack&&info.custshipheader.CustShipFire);
    info.custshipheader.CustShipAttack(&ship,&target,0);
    CHECK(approaches==1&&gunRange==5000&&closeRange==3000);
    info.shiptype=CataMothership;cataShipConfigureCombat(&info);
    info.custshipheader.CustShipAttack(&ship,&target,0);CHECK(single==1);
    ship.attackvars.multipleAttackTargets=(AttackTargets*)&target;
    info.custshipheader.CustShipAttack(&ship,&target,0);CHECK(multiple==1);
    ship.specialFlags=SPECIAL_Hyperspacing;
    info.custshipheader.CustShipAttack(&ship,&target,0);CHECK(multiple==1);
    ship.specialFlags=0;gun.guntype=GUN_Gimble;
    info.custshipheader.CustShipFire(&ship,&target);CHECK(fired==1);
    gun.cataWeaponDisabled=TRUE;info.custshipheader.CustShipFire(&ship,&target);CHECK(fired==1);
    gun.cataDefinition=TRUE;gun.cataWeaponClass=0;CHECK(cataEnergyGun(&ship,0));
    gun.cataWeaponClass=4;CHECK(cataEnergyGun(&ship,0));
    gun.cataWeaponClass=10;CHECK(cataEnergyGun(&ship,0));
    gun.cataWeaponClass=14;CHECK(cataEnergyGun(&ship,0));
    gun.cataWeaponClass=3;CHECK(!cataEnergyGun(&ship,0));
    gun.cataDefinition=FALSE;gun.cataWeaponClass=0;CHECK(!cataEnergyGun(&ship,0));
    info.shiptype=ResourceController;before=info;cataShipConfigureCombat(&info);
    CHECK(!memcmp(&info,&before,sizeof(info)));
    free(guns);printf("PASS: %d original Cataclysm combat/energy ownership checks.\n",checks);return 0;
}
