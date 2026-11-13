/* The Khar-Selim uses a Remastered wreck with differently positioned hull
   geometry. Its native HOD contains five authored salvage joints; use them
   only when the native mesh is actually loaded, keeping classic fallback. */
bool32 salCapAlignKharSelimPoints(SpaceObjRotImpTargGuidanceShipDerelict *target)
{
    Derelict *wreck;
    lodinfo *levels;
    SalvageStaticPoint *points[5];
    hmatrix markers[5];
    sdword i, level;
    if (!target || target->objtype != OBJ_DerelictType || !target->salvageInfo ||
        target->salvageInfo->numSalvagePoints != 5) return FALSE;
    wreck = (Derelict *)target;
    if (!wreck->staticinfo || wreck->staticinfo->derelicttype != Shipwreck) return FALSE;
    levels = wreck->staticinfo->staticheader.LOD;
    for (level=0; levels && level<levels->nLevels; ++level)
    {
        float scale; unsigned nativeLOD;
        void *mesh=levels->level[level].pData;
        if ((levels->level[level].flags & LM_LODType) != LT_Mesh ||
            !meshNativeVisualParameters(mesh,&scale,&nativeLOD)) continue;
        for (i=0; i<5; ++i)
        {
            char name[32];
            points[i]=target->salvageInfo->salvagePoints[i].salvageStaticPoint;
            if (!points[i]) return FALSE;
            sprintf(name,"SalvagePoint%d",(int)i);
            if (!meshNativeVisualRestMarker(mesh,name,(float *)&markers[i])) return FALSE;
        }
        for (i=0; i<5; ++i)
        {
            hmatGetVectFromHMatrixCol4(points[i]->position,markers[i]);
            hmatGetVectFromHMatrixCol3(points[i]->conenormal,markers[i]);
            vecNormalize(&points[i]->conenormal);
        }
        return TRUE;
    }
    return FALSE;
}

/* Resolve the native salvager's own contact at rest. This is independent of
   camera LOD, visibility and presentation animation instances. */
static bool32 salCapNativeGrip(Ship *ship,vector *grip)
{
    lodinfo *levels;
    sdword level;
    if(!ship || ship->shiptype!=SalCapCorvette || !ship->staticinfo || !grip) return FALSE;
    levels=ship->staticinfo->staticheader.LOD;
    for(level=0;levels && level<levels->nLevels;++level) {
        hmatrix marker;float scale;unsigned nativeLOD;void *mesh=levels->level[level].pData;
        if((levels->level[level].flags&LM_LODType)!=LT_Mesh ||
           !meshNativeVisualParameters(mesh,&scale,&nativeLOD)) continue;
        if(!meshNativeVisualRestMarker(mesh,"salvage_grip",(float*)&marker)) return FALSE;
        hmatGetVectFromHMatrixCol4(*grip,marker);
        return TRUE;
    }
    return FALSE;
}

static void salCapContactOrigin(Ship *ship,const vector *contact,const vector *grip,vector *origin)
{
    vector gripWorld;
    matMultiplyMatByVec(&gripWorld,&ship->rotinfo.coordsys,(vector*)grip);
    vecSub(*origin,*contact,gripWorld);
}

/* Save files retain a clamped object's old relative offset. Refresh it after
   every object reference has been restored, rather than requiring the player
   to abandon an in-progress Mission 2 salvage operation and order it again. */
void salCapRestoreKharSelimContact(Ship *ship)
{
    SalCapCorvetteSpec *spec;
    SpaceObjRotImpTargGuidanceShipDerelict *target;
    SalvageStaticPoint *point;
    vector offset,origin,grip;
    if (!ship || ship->shiptype != SalCapCorvette) return;
    spec = (SalCapCorvetteSpec *)ship->ShipSpecifics;
    target = spec->target;
    if (!salCapAlignKharSelimPoints(target) || !ship->clampInfo ||
        ship->clampInfo->host != (SpaceObjRotImpTarg *)target ||
        spec->salvageIndex < 0 ||
        spec->salvageIndex >= target->salvageInfo->numSalvagePoints) return;
    point = target->salvageInfo->salvagePoints[spec->salvageIndex].salvageStaticPoint;
    if (!point) return;
    matMultiplyMatByVec(&offset,&target->rotinfo.coordsys,&point->position);
    vecAdd(ship->posinfo.position,target->posinfo.position,offset);
    if(salCapNativeGrip(ship,&grip)) {
        salCapContactOrigin(ship,&ship->posinfo.position,&grip,&origin);
        ship->posinfo.position=origin;
    }
    /* The clamp stores the ship origin in host-local space, not the surface
       marker. Keep the saved relative orientation and current script state. */
    vecSub(offset,ship->posinfo.position,target->posinfo.position);
    matMultiplyVecByMat(&ship->clampInfo->clampOffset,&offset,&target->rotinfo.coordsys);
    ship->posinfo.velocity = target->posinfo.velocity;
}

