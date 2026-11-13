#define SALCAP_FINAL_DOCK_TOLERANCE 25.0f
sdword salCapFlyToConeOrigin(Ship *ship,SpaceObjRotImpTargGuidanceShipDerelict *target)
{
    SalCapCorvetteSpec *spec = (SalCapCorvetteSpec *)ship->ShipSpecifics;
    vector coneheadingInWorldCoordSys;
    vector conepositionInWorldCoordSys;
    vector desiredHeading;
    vector grip,origin;
    bool32 nativeContact = salCapAlignKharSelimPoints(target);
    bool32 nativeGrip = nativeContact && salCapNativeGrip(ship,&grip);
    bool32 aligned=TRUE;

    SalvageStaticPoint *salvagestaticpoint = target->salvageInfo->salvagePoints[spec->salvageIndex].salvageStaticPoint;

    matMultiplyMatByVec(&coneheadingInWorldCoordSys,&target->rotinfo.coordsys,&salvagestaticpoint->conenormal);

    matMultiplyMatByVec(&conepositionInWorldCoordSys,&target->rotinfo.coordsys,&salvagestaticpoint->position);
    vecAddTo(conepositionInWorldCoordSys,target->posinfo.position);

    vecScalarMultiply(desiredHeading,coneheadingInWorldCoordSys,-1.0f);
    if(nativeGrip) {
        /* The controller rotates progressively. Do not freeze the clamp until
           the nose faces the approach normal; otherwise correcting the origin
           alone could latch a sideways or backward corvette to the wreck. */
        aligned=aitrackHeading(ship,&desiredHeading,0.999f);
        salCapContactOrigin(ship,&conepositionInWorldCoordSys,&grip,&origin);
        conepositionInWorldCoordSys=origin;
    }

    if (MoveReachedDestinationVariable(ship,&conepositionInWorldCoordSys,SALCAP_FINAL_DOCK_TOLERANCE) && aligned)
    {
        /* Clamp captures the current relative position, not the requested
           docking destination. Do not freeze the remaining arrival tolerance
           as a visible gap on the small native wreck contact. */
        if (nativeContact) ship->posinfo.position = conepositionInWorldCoordSys;
        ship->posinfo.velocity = target->posinfo.velocity;
        return TRUE;
    }
    else
    {
        if(!nativeGrip) aitrackHeading(ship,&desiredHeading,1.0);
        aishipFlyToPointAvoidingObjs(ship,&conepositionInWorldCoordSys,0,0.0f);
        return FALSE;
    }
}

