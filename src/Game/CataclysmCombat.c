/* Typed Cataclysm command/fire imports: owned executable 00421c30
   (attackSimple), 005b0260 (shoot-all-guns), 005bf3a0 (Processor command),
   005bbc50 (Mothership command). Engine ABI adapters, no ship-header aliases. */
#include "CataclysmShips.h"
#include "AIShip.h"
#include "Attack.h"
#include "Gun.h"
#include "FastMath.h"
#include "ShipSelect.h"
#include "Collision.h"
static void cataAttackSimple(Ship *ship,SpaceObjRotImpTarg *target)
{
    vector trajectory;real32 distance,range;
    if(!target || bitTest(ship->specialFlags,SPECIAL_Hyperspacing))return;
    if(ship->attackvars.multipleAttackTargets){gunShootGunsAtMultipleTargets(ship);return;}
    aishipGetTrajectory(ship,target,&trajectory);
    distance=fsqrt(vecMagnitudeSquared(trajectory));
    if(distance>0)vecMultiplyByScalar(trajectory,1.0f/distance);
    range=RangeToTargetGivenDist(ship,target,distance);
    gunShootGunsAtTarget(ship,target,range,&trajectory);
}
static void cataShootAllGunsAtTarget(Ship *ship,SpaceObjRotImpTarg *target)
{
    sdword index;Gun *gun;
    if(!ship->gunInfo)return;
    for(index=0;index<ship->gunInfo->numGuns;index++){
        gun=&ship->gunInfo->guns[index];if(!gunCanShoot(ship,gun))continue;
        switch(gun->gunstatic->guntype){
        case GUN_Fixed:case GUN_Gimble:case GUN_NewGimble:gunShoot(ship,gun,target);break;
        case GUN_MissileLauncher:case GUN_MineLauncher:
            if(gunHasMissiles(gun))missileShoot(ship,gun,target);break;
        }
    }
}
static void cataShipAttackSimpleNoRotate(Ship *ship,Ship *target,bool32 rotate)
{(void)rotate;cataAttackSimple(ship,(SpaceObjRotImpTarg*)target);}
static void cataProcessorAttackCommand(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    (void)maxdist;
    /* Original close range is 3000. The shared straight-forward routine was
       checked against 00421cf0: its Cata module gate is inapplicable here. */
    attackStraightForward(ship,target,ship->staticinfo->bulletRange[ship->tacticstype],3000.0f);
}
static void cataMothershipAttackCommand(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    (void)maxdist;
    /* Retail first cancels a later Siege Cannon charge, unavailable in M1. */
    cataAttackSimple(ship,target);
}
void cataShipConfigureCombat(ShipStaticInfo *info)
{
    if(info->shiptype!=CataProcessor && info->shiptype!=CataMothership)return;
    info->custshipheader.CustShipAttack=info->shiptype==CataProcessor?
        cataProcessorAttackCommand:cataMothershipAttackCommand;
    info->custshipheader.CustShipAttackPassive=cataShipAttackSimpleNoRotate;
    info->custshipheader.CustShipFire=cataShootAllGunsAtTarget;
}
