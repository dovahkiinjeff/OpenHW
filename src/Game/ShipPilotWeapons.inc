static real32 pilotWeaponRange(Ship *ship,GunStatic *st)
{
    real32 bonus=1;
    if(ship->staticinfo->shipclass==CLASS_Fighter) bonus=tacticsInfo.BulletRangeBonus[Tactics_Fighter][ship->tacticstype];
    else if(ship->staticinfo->shipclass==CLASS_Corvette) bonus=tacticsInfo.BulletRangeBonus[Tactics_Corvette][ship->tacticstype];
    return st->bulletrange*bonus;
}
bool32 shipPilotWeaponCanHit(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target)
{
    vector world,local,heading;
    GunStatic *st=gun->gunstatic;real32 dot;
    if(!target || (target->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace))) return FALSE;
    vecSub(world,target->collInfo.collPosition,ship->posinfo.position);
    matMultiplyVecByMat(&local,&world,&ship->rotinfo.coordsys);vecSubFrom(local,st->position);
    if(vecMagnitudeSquared(local)>pilotWeaponRange(ship,st)*pilotWeaponRange(ship,st)) return FALSE;
    vecNormalize(&local);
    if(st->guntype==GUN_Gimble)
    { dot=vecDotProduct(local,st->gunnormal);return dot>=st->cosmaxAngleFromNorm && dot<=st->cosminAngleFromNorm; }
    if(st->guntype==GUN_NewGimble) { matGetVectFromMatrixCol3(heading,gun->curgunorientation); }
    else { heading=st->gunnormal; }
    return vecDotProduct(local,heading)>=st->triggerHappy;
}
/* Pilot aiming uses a temporary point only for orientation. Projectile/event
   owners must always be real universe objects or NULL, never this stack object. */
void gunPilotAimAndFire(Ship *ship,const vector *point,Ship *target,bool32 fire,bool32 all)
{
    SpaceObjRotImpTarg aim;
    sdword i;
    bool32 locked=target && target==shipPilotTargetFor(ship) && shipPilotControls(ship);
    if(!ship->gunInfo) return;
    memset(&aim,0,sizeof(aim));
    aim.objtype=OBJ_AsteroidType;
    aim.posinfo.position=aim.collInfo.collPosition=*point;
    /* Cancel velocity prediction: the cursor is an absolute aim point. */
    for(i=0;i<ship->gunInfo->numGuns;i++)
    {
        Gun *g=&ship->gunInfo->guns[i];
        GunStatic *st=g->gunstatic;
        bool32 aligned=FALSE;
        vector local,world,tangent;
        real32 dot,c,sine,len;
        if(!st) continue;
        vecSub(world,*point,ship->posinfo.position);
        matMultiplyVecByMat(&local,&world,&ship->rotinfo.coordsys);
        vecSubFrom(local,st->position);
        if(locked && vecMagnitudeSquared(local)>pilotWeaponRange(ship,st)*pilotWeaponRange(ship,st)) continue;
        if(vecMagnitudeSquared(local)<0.000001f) continue;
        vecNormalize(&local);
        if(st->guntype==GUN_NewGimble)
        {
            aligned=gunOrientGimbleGun(ship,g,&aim);
            /* Off-axis shots follow the actual, rate-limited barrel pose. */
            if(!aligned) { matGetVectFromMatrixCol3(g->gunheading,g->curgunorientation); }
        }
        else if(st->guntype==GUN_Gimble)
        {
            /* Cone guns aim at the closest permitted direction, even when
               Fire All is off, so enabling it cannot cause a pose snap. */
            dot=vecDotProduct(local,st->gunnormal);
            c=max(st->cosmaxAngleFromNorm,min(st->cosminAngleFromNorm,dot));
            c=max(-1.0f,min(1.0f,c));
            aligned=dot>=st->cosmaxAngleFromNorm && dot<=st->cosminAngleFromNorm;
            tangent=st->gunnormal; vecMultiplyByScalar(tangent,dot);
            vecSub(tangent,local,tangent);
            len=(real32)sqrt(vecMagnitudeSquared(tangent));
            if(len<0.00001f)
            {
                vector axis={1,0,0};
                if(fabs(st->gunnormal.x)>0.9f) { axis.x=0; axis.y=1; }
                vecCrossProduct(tangent,st->gunnormal,axis);
                vecNormalize(&tangent);
            }
            else { vecMultiplyByScalar(tangent,1.0f/len); }
            sine=(real32)sqrt(max(0.0f,1-c*c));
            vecMultiplyByScalar(tangent,sine);
            g->gunheading=st->gunnormal; vecMultiplyByScalar(g->gunheading,c);
            vecAddTo(g->gunheading,tangent);
        }
        else
        {
            g->gunheading=st->gunnormal;
            aligned=vecDotProduct(local,g->gunheading)>=st->triggerHappy;
        }
        if(!fire || (shipPilotOwns(ship) && !shipPilotWeaponActive(i)) || ((!all || locked) && !aligned)) continue;
        if(ship->staticinfo->madStatic && ship->staticinfo->madStatic->numGunOpenIndexes>0 &&
           ship->madGunStatus!=MAD_STATUS_GUNS_OPEN)
        {
            if(ship->madGunStatus!=MAD_STATUS_GUNS_OPENING) madOpenGunsShip(ship);
            continue;
        }
        if(!gunCanShoot(ship,g)) continue;
        if(st->guntype==GUN_MissileLauncher || st->guntype==GUN_MineLauncher)
        {
            if(gunHasMissiles(g)) missileShoot(ship,g,aligned?(SpaceObjRotImpTarg *)target:NULL);
        }
        else gunShoot(ship,g,aligned?(SpaceObjRotImpTarg *)target:NULL);
    }
}
