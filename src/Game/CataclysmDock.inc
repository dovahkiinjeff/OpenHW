/* Typed Cataclysm cone/launch imports from the owned recovery and dock.c.
   HOD endpoints supply replacement geometry only. Do not add waypoint
   pass-through, speed floors, shortened waits or HW1 mothership doors. */
typedef struct CataDockRuntime {
    sdword size;
    vector lastConeHeading;
    real32 deadline;
} CataDockRuntime;
static CataDockRuntime *cataDockRuntime(Ship *ship)
{
    CataDockRuntime *state=ship->dockvars.customdockinfo;
    if(!state){
        state=ship->dockvars.customdockinfo=memAlloc(sizeof(*state),"CataDock",0);
        memset(state,0,sizeof(*state));state->size=sizeof(*state);
        if(ship->dockvars.dockstaticpoint && ship->dockvars.dockship)
            matMultiplyMatByVec(&state->lastConeHeading,&ship->dockvars.dockship->rotinfo.coordsys,
                &ship->dockvars.dockstaticpoint->conenormal);
    }
    return state;
}
static void cataDockRuntimeFree(Ship *ship)
{
    if(ship->dockvars.customdockinfo){memFree(ship->dockvars.customdockinfo);ship->dockvars.customdockinfo=NULL;}
}
static bool32 cataDockHasBay(Ship *host)
{
    Node *node;
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *part=(Ship *)listGetStructOfNode(node);
        if(part->shiptype==CataMothershipDockingBay && !(part->flags&SOF_Dead) &&
           part->clampInfo && part->clampInfo->host==(SpaceObjRotImpTarg *)host)return TRUE;
    }
    return FALSE;
}
static sdword cataDockFindPort(Ship *ship,Ship *host,bool32 launch,bool32 transfer)
{
    sdword i,chosen=-1;real32 nearest=1.0e30f;vector world,delta;
    const char *prefix;
    bool32 small=ship->staticinfo->shipclass==CLASS_Fighter || ship->staticinfo->shipclass==CLASS_Corvette;
    bool32 engineering=!cataDockHasBay(host);
    sdword pass;
    if(!host->dockInfo || !host->staticinfo->dockStaticInfo)return -1;
    /* Original small/medium ports fall back to engineering when the hangar
       is absent or full. Workers use the original medium class, not fighters. */
    for(pass=0;pass<2;pass++){
    if(transfer)prefix="Res";
    else if(engineering)prefix=small?(launch?"rpsmallout":"rpsmallin"):(launch?"rpmedout":"rpmedin");
    else prefix=small?(launch?"dbsmallout":"dbsmallin"):(launch?"dbmedout":"dbmedin");
    for(i=0;i<host->dockInfo->numDockPoints;i++){
        DockStaticPoint *point=&host->staticinfo->dockStaticInfo->dockstaticpoints[i];
        if(host->dockInfo->dockpoints[i].thisDockBusy || strncasecmp(point->name,prefix,strlen(prefix)))continue;
        if(transfer && (point->name[3]<'0'||point->name[3]>'9'))continue;
        matMultiplyMatByVec(&world,&host->rotinfo.coordsys,&point->position);vecAddTo(world,host->posinfo.position);
        vecSub(delta,ship->posinfo.position,world);
        if(vecMagnitudeSquared(delta)<nearest){chosen=i;nearest=vecMagnitudeSquared(delta);}
    }
    if(chosen>=0 || engineering || transfer)break;
    engineering=TRUE;
    }
    return chosen;
}
static void cataDockReserve(Ship *ship,Ship *host,sdword index)
{
    ship->dockvars.dockship=host;
    ship->dockvars.dockstaticpoint=&host->staticinfo->dockStaticInfo->dockstaticpoints[index];
    dockReserveDockPoint(ship,host,index);
}
static const char *cataNativeDockPath(Ship *ship,bool32 outgoing,bool32 transfer)
{
    unsigned side=(unsigned)ship->dockvars.reserveddocking&1;
    if(transfer)return outgoing?(side?"path6":"path4"):(side?"path5":"path3");
    if(ship->staticinfo->shipclass==CLASS_Fighter || ship->staticinfo->shipclass==CLASS_Corvette)
        return outgoing?"path12":"path8";
    return outgoing?(side?"path11":"path1"):(side?"path10":"path2");
}
static DockStaticPoint cataDockPoint(Ship *ship,Ship *host,bool32 outgoing,bool32 transfer)
{
    DockStaticPoint point=*ship->dockvars.dockstaticpoint;vector position;
    const char *path=cataNativeDockPath(ship,outgoing,transfer);
    if (outgoing && !transfer && ship->shiptype == CataProcessor && host->shiptype == CataMothership)
    {
        vector endpoint, outward;
        sdword count = meshRemasteredDockPoint(host, "path9", 0, &position);
        /* The HOD marks path9 for SuperCap/Controller departures. It exits
           under the hull. Keep the original dock reservation and release range. */
        if (count >= 2 && meshRemasteredDockPoint(host, "path9", (unsigned)(count - 1), &endpoint))
        {
            vecSub(outward, endpoint, position);
            if (vecMagnitudeSquared(outward) > 0.0f)
            {
                vecNormalize(&outward);
                point.position = position;
                point.conenormal = outward;
                point.headingdirection = 4;
                point.updirection = 2;
            }
        }
        return point;
    }
    if(outgoing){
        if(meshRemasteredDockPoint(host,path,0,&position))point.position=position;
    }else if(meshRemasteredDockEndpoint(host,path,&position))point.position=position;
    return point;
}
/* Retail 004675c0 uses the Worker's own Center attachment, not its origin. */
static DockStaticPoint *cataDockCenter(Ship *ship)
{
    DockStaticInfo *info=ship->staticinfo->dockStaticInfo;sdword i;
    if(info)for(i=0;i<info->numDockPoints;i++)
        if(!strcasecmp(info->dockstaticpoints[i].name,"Center"))return &info->dockstaticpoints[i];
    return NULL;
}
static bool32 cataDockClampedCone(Ship *ship,Ship *host,DockStaticPoint *point)
{
    vector origin,cone,delta,destination;real32 distance;
    matMultiplyMatByVec(&origin,&host->rotinfo.coordsys,&point->position);vecAddTo(origin,host->posinfo.position);
    matMultiplyMatByVec(&cone,&host->rotinfo.coordsys,&point->conenormal);
    vecSub(delta,ship->posinfo.position,origin);distance=fsqrt(vecMagnitudeSquared(delta));
    distance=max(point->mindist,min(distance,point->maxdist));
    vecScalarMultiply(destination,cone,distance);vecAddTo(destination,origin);
    if(MoveReachedDestinationVariable(ship,&destination,10.0f))return TRUE;
    aishipFlyToPointAvoidingObjsWithVel(ship,&destination,179,-80.0f,&host->posinfo.velocity);return FALSE;
}
static void cataDockMatrixDirection(vector *v,udword direction,const matrix *m)
{
    switch(direction&3){
    case 0:matGetVectFromMatrixCol1(*v,*m);break;
    case 1:matGetVectFromMatrixCol2(*v,*m);break;
    default:matGetVectFromMatrixCol3(*v,*m);break;
    }
    if(direction&4){vecMultiplyByScalar(*v,-1.0f);}
}
bool32 cataDockWorkerGrapplePosition(Ship *ship,vector *grip)
{
    vector mount;
    if(meshRemasteredMarkerPosition(ship,"salvage_grip",grip))return TRUE;
    if(!meshRemasteredMarkerPosition(ship,"worker_grapple_mount",&mount))return FALSE;
    /* The stock HOD's outward pads at Open phase 0.10 share a plane
       25.97 units below Root, centered 7.16 units aft. Use mount depth
       to preserve scale across installations. */
    vecSet(*grip,mount.x*(25.97f/21.56f),0.0f,mount.x*(7.16f/21.56f));
    return TRUE;
}

static bool32 cataDockContactPose(Ship *ship,Ship *host,const DockStaticPoint *point,
                                bool32 grapple,matrix *desired,vector *centerPosition)
{
    DockStaticPoint *center=cataDockCenter(ship);
    matrix local,world,offset;vector heading,up,right,a,b,c;
    if(!center)return FALSE; /* Retail also requires a Center for external docking. */
    a=point->conenormal;matCreateCoordSysFromHeading(&local,&a);
    cataDockMatrixDirection(&heading,point->headingdirection,&local);
    cataDockMatrixDirection(&up,point->updirection,&local);
    vecCrossProduct(right,heading,up);vecNormalize(&right);
    matCreateMatFromVecs(&local,&up,&right,&heading);matMultiplyMatByMat(&world,&host->rotinfo.coordsys,&local);
    vecScalarMultiply(a,center->conenormal,-1.0f);b.x=-a.y;b.y=a.x;b.z=0;
    if(vecMagnitudeSquared(b)==0.0f)return FALSE;
    vecNormalize(&b);vecCrossProduct(c,a,b);vecNormalize(&c);
    matCreateMatFromVecs(&offset,&a,&b,&c);matTranspose(&offset);matMultiplyMatByMat(desired,&world,&offset);
    *centerPosition=center->position;
    if(grapple && ship->shiptype==CataWorker)
        cataDockWorkerGrapplePosition(ship,centerPosition);
    return TRUE;
}

static bool32 cataDockOffset(Ship *ship,Ship *host,DockStaticPoint *point,bool32 grapple)
{
    matrix desired;vector heading,up,a,b,contact,center,destination,delta;
    real32 distance,avoid,collision;bool32 aligned,closeEnough,ready=TRUE;
    madanim *sa=ship->madBindings,*ha=host->madBindings;
    if(!cataDockContactPose(ship,host,point,grapple,&desired,&center))return FALSE;
    matMultiplyMatByVec(&contact,&host->rotinfo.coordsys,&point->position);vecAddTo(contact,host->posinfo.position);
    matMultiplyMatByVec(&delta,&ship->rotinfo.coordsys,&center);vecSub(destination,contact,delta);
    vecSub(delta,ship->posinfo.position,destination);distance=vecMagnitudeSquared(delta);
    closeEnough=fabs(delta.x)<5.0f && fabs(delta.y)<5.0f && fabs(delta.z)<5.0f;
    matGetVectFromMatrixCol1(a,ship->rotinfo.coordsys);matGetVectFromMatrixCol1(b,desired);
    aligned=vecDotProduct(a,b)>=0.996f;
    matGetVectFromMatrixCol2(a,ship->rotinfo.coordsys);matGetVectFromMatrixCol2(b,desired);aligned=aligned && vecDotProduct(a,b)>=0.996f;
    matGetVectFromMatrixCol3(a,ship->rotinfo.coordsys);matGetVectFromMatrixCol3(b,desired);aligned=aligned && vecDotProduct(a,b)>=0.996f;
    avoid=ship->staticinfo->staticheader.staticCollInfo.avoidcollspheresize;avoid*=avoid;
    collision=ship->staticinfo->staticheader.staticCollInfo.collspheresize;collision*=collision;
    if(sa && sa->nCurrentAnim!=MAD_NoAnimation && sa->flags==0 && (distance>=avoid || !sa->bPaused))ready=FALSE;
    if(ha && ha->nCurrentAnim!=MAD_NoAnimation && ha->flags==0 && !(distance<avoid && ha->bPaused))ready=FALSE;
    if(aligned && closeEnough && ready){
        matMultiplyMatByVec(&delta,&desired,&center);vecSub(ship->posinfo.position,contact,delta);
        ship->rotinfo.coordsys=desired;ship->posinfo.velocity=host->posinfo.velocity;return TRUE;
    }
    if(distance<avoid){
        if(sa && sa->nCurrentAnim!=MAD_NoAnimation && sa->flags==0)madAnimationPause(ship,FALSE);
        if(ha && ha->nCurrentAnim!=MAD_NoAnimation && ha->flags==0)madAnimationPause(host,FALSE);
    }
    if(!aligned){matGetVectFromMatrixCol3(heading,desired);matGetVectFromMatrixCol1(up,desired);aitrackHeadingAndUp(ship,&heading,&up,0.999f);}
    if(closeEnough)return FALSE;
    if((aligned && ready) || distance>collision)aishipFlyToPointAvoidingObjsWithVel(ship,&destination,0,-120.0f,&host->posinfo.velocity);
    else aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance *)ship,&host->posinfo.velocity);
    return FALSE;
}
static void cataDockWorkerAnimation(Ship *ship,sdword clip,bool32 pause)
{
    if(ship->shiptype==CataWorker && ship->madBindings && ship->madBindings->header &&
       clip<ship->madBindings->header->nAnimations && ship->madBindings->nCurrentAnim!=clip){
        madAnimationStart(ship,clip);if(pause)madAnimationPause(ship,TRUE);
    }
}
static void cataDockDeposit(Ship *ship)
{
    sdword resources=ship->resources;Player *owner=ship->playerowner;
    ship->resources=0;
    if(resources<=0)return;
    owner->resourceUnits+=resources;
    universe.gameStats.playerStats[owner->playerIndex].totalResourceUnitsCollected+=resources;
    universe.gameStats.playerStats[owner->playerIndex].totalResourceUnits+=resources;
    universe.gameStats.totalResourceUnitsCollected+=resources;
    if(singlePlayerGame && owner->playerIndex==0)singlePlayerGameInfo.resourceUnitsCollected+=resources;
}
/* Original dockFlyToConeInside: acquire the cone before final alignment. */
static bool32 cataDockApproach(Ship *ship,Ship *host,DockStaticPoint *point)
{
    vector origin,delta,heading,destination;real32 distance,clamped;
    matMultiplyMatByVec(&origin,&host->rotinfo.coordsys,&point->position);vecAddTo(origin,host->posinfo.position);
    matMultiplyMatByVec(&heading,&host->rotinfo.coordsys,&point->conenormal);
    vecSub(delta,ship->posinfo.position,origin);distance=fsqrt(vecMagnitudeSquared(delta));
    clamped=max(point->mindist,min(distance,point->maxdist));
    vecScalarMultiply(destination,heading,point->mindist);vecAddTo(destination,origin);
    if(distance*point->coneangle>=vecDotProduct(heading,delta)+5.0f){
        aishipFlyToPointAvoidingObjsWithVel(ship,&destination,
            clamped+500.0f>=distance?3:7,clamped+500.0f>=distance?-10.0f:-80.0f,&host->posinfo.velocity);
        return FALSE;
    }
    aitrackForceSteadyShip(ship);return TRUE;
}
/* Original dockFlyToFinalDockPoint, Somtaaw branch. */
static bool32 cataDockFinal(Ship *ship,Ship *host,DockStaticPoint *point)
{
    CataDockRuntime *state=cataDockRuntime(ship);
    vector destination,cone,delta,heading,up,projected,adjusted;real32 distance,dot,projection;bool32 aligned;
    matMultiplyMatByVec(&destination,&host->rotinfo.coordsys,&point->position);vecAddTo(destination,host->posinfo.position);
    if(MoveReachedDestinationVariable(ship,&destination,4.0f))return TRUE;
    matMultiplyMatByVec(&cone,&host->rotinfo.coordsys,&point->conenormal);
    GetDirectionVectorOfShip(&heading,point->headingdirection,host);GetDirectionVectorOfShip(&up,point->updirection,host);
    vecSub(delta,ship->posinfo.position,destination);distance=fsqrt(vecMagnitudeSquared(delta));
    if(distance<=0.0f)return TRUE;
    vecMultiplyByScalar(delta,1.0f/distance);dot=vecDotProduct(cone,delta);
    vecSub(adjusted,cone,state->lastConeHeading);vecMultiplyByScalar(adjusted,distance);vecAddTo(adjusted,ship->posinfo.position);
    ship->posinfo.position=adjusted;
    aligned=aitrackHeadingAndUp(ship,&heading,&up,0.995f);
    if(dot<0.0f)return TRUE;
    if(dot<0.995f && distance>100.0f){
        projection=dot*distance;vecScalarMultiply(projected,cone,projection);vecAddTo(projected,destination);
        aishipFlyToPointAvoidingObjsWithVel(ship,&projected,0,-20.0f,&host->posinfo.velocity);
    }
    if(aligned)aishipFlyToPointAvoidingObjsWithVel(ship,&destination,0,-120.0f,&host->posinfo.velocity);
    state->lastConeHeading=cone;return FALSE;
}
/* Original outward cone projection and release distance. No waypoint boost. */
static bool32 cataDockExit(Ship *ship,Ship *host,DockStaticPoint *point)
{
    vector origin,cone,delta,destination,projected;real32 projection;
    matMultiplyMatByVec(&origin,&host->rotinfo.coordsys,&point->position);vecAddTo(origin,host->posinfo.position);
    matMultiplyMatByVec(&cone,&host->rotinfo.coordsys,&point->conenormal);
    vecSub(delta,ship->posinfo.position,origin);
    if(vecMagnitudeSquared(cone)<=0.0f)return FALSE;
    projection=vecDotProduct(delta,cone)/vecMagnitudeSquared(cone);
    vecScalarMultiply(projected,cone,projection);vecAddTo(projected,origin);ship->posinfo.position=projected;
    if(vecMagnitudeSquared(delta)>=point->flyawaydist*point->flyawaydist)return TRUE;
    vecScalarMultiply(destination,cone,point->flyawaydist*4.0f);vecAddTo(destination,origin);
    aishipFlyToPointAvoidingObjsWithVel(ship,&destination,0,0.0f,&host->posinfo.velocity);return FALSE;
}
bool32 cataDockSalvageIntakePoint(Ship *host,sdword port,DockStaticPoint *point)
{
    DockStaticInfo *info=host->staticinfo->dockStaticInfo;
    if(!info || port<0 || port>=info->numDockPoints)return FALSE;
    *point=info->dockstaticpoints[port];
    if(host->shiptype==CataMothership){
        bool32 engineering=point->type==DPT_Med_In_Engineering;
        const char *path=engineering?"path8":!strcasecmp(point->name,"dbmedinsb")?"path10":"path2";
        vector outside,entry,axis;
        sdword count=meshRemasteredDockPoint(host,path,0,&outside);
        /* The engineering path turns inside the hull. The beacon crosses
           the mouth at the waypoint just before the internal turn. */
        if(count>=3 && meshRemasteredDockPoint(host,path,(unsigned)(count-(engineering?2:1)),&entry)){
            vecSub(axis,outside,entry);
            if(vecMagnitudeSquared(axis)>0.0f){
                vecNormalize(&axis);point->position=entry;point->conenormal=axis;
            }
        }
    }
    return TRUE;
}

bool32 cataDockSalvageStep(Ship *ship,Ship *host,sdword port,sdword stage)
{
    DockStaticPoint point;
    bool32 intake;
    if(!host || !host->staticinfo->dockStaticInfo || port<0 ||
       port>=host->staticinfo->dockStaticInfo->numDockPoints)return FALSE;
    point=host->staticinfo->dockStaticInfo->dockstaticpoints[port];
    ship->dockvars.dockship=host;
    ship->dockvars.dockstaticpoint=&host->staticinfo->dockStaticInfo->dockstaticpoints[port];
    intake=ship->shiptype==CataMessagePod;
    if(intake && !cataDockSalvageIntakePoint(host,port,&point))return FALSE;
    switch(stage){
    case 0:return cataDockApproach(ship,host,&point);
    case 1:
        if(!cataDockClampedCone(ship,host,&point))return FALSE;
        if(intake)matMultiplyMatByVec(&cataDockRuntime(ship)->lastConeHeading,&host->rotinfo.coordsys,&point.conenormal);
        return TRUE;
    case 2:return cataDockOffset(ship,host,&point,TRUE);
    case 3:return cataDockFinal(ship,host,&point);
    default:return FALSE;
    }
}
void cataDockRestoreSalvageContact(Ship *ship,Ship *host,sdword port)
{
    DockStaticInfo *info=host->staticinfo->dockStaticInfo;
    DockStaticPoint *point;
    matrix desired;
    vector center,contact,offset,relative;
    if(!ship->clampInfo || ship->clampInfo->host!=(SpaceObjRotImpTarg*)host ||
       !info || port<0 || port>=info->numDockPoints)return;
    point=&info->dockstaticpoints[port];
    if(!cataDockContactPose(ship,host,point,TRUE,&desired,&center))return;
    matMultiplyMatByVec(&offset,&desired,&center);
    matMultiplyVecByMat(&relative,&offset,&host->rotinfo.coordsys);
    vecSub(ship->clampInfo->clampOffset,point->position,relative);
    ship->rotinfo.coordsys=desired;ship->posinfo.velocity=host->posinfo.velocity;
    matTranspose(&desired);
    matMultiplyMatByMat(&ship->clampInfo->clampCoordsys,&desired,&host->rotinfo.coordsys);
    matTranspose(&ship->clampInfo->clampCoordsys);
    matMultiplyMatByVec(&contact,&host->rotinfo.coordsys,&ship->clampInfo->clampOffset);
    vecAdd(ship->posinfo.position,host->posinfo.position,contact);
}
void cataDockSalvageFinish(Ship *ship)
{
    cataDockRuntimeFree(ship);ship->dockvars.dockship=NULL;
    ship->dockvars.dockstaticpoint=NULL;ship->dockvars.reserveddocking=-1;
}
static bool32 LaunchShipFromSomtaaw(Ship *ship,Ship *host)
{
    DockStaticPoint point;sdword index;vector world;
    if(ship->dockvars.dockstate3==0){
        index=cataDockFindPort(ship,host,TRUE,FALSE);
        if(index<0)return FALSE;
        cataDockReserve(ship,host,index);point=cataDockPoint(ship,host,TRUE,FALSE);
        matMultiplyMatByVec(&world,&host->rotinfo.coordsys,&point.position);vecAddTo(world,host->posinfo.position);
        dockRemoveShipFromInside(ship,host);
        dockPutShipOutside(ship,host,&world,point.headingdirection,point.updirection);
        ship->dockvars.dockstate3=1;
        return FALSE; /* Original state 10 -> 12, not launch-and-advance. */
    }
    point=cataDockPoint(ship,host,TRUE,FALSE);
    if(cataDockExit(ship,host,&point)){
        dockFreeDockPoint(ship,host);cataDockRuntimeFree(ship);ship->dockingship=NULL;return TRUE;
    }
    return FALSE;
}
static bool32 ShipDocksAtSomtaaw(struct CommandToDo *command,Ship *ship,Ship *host)
{
    sdword index;CataDockRuntime *state;DockStaticPoint point;
    bool32 transfer=(command->dock.dockType&DOCK_TO_DEPOSIT_RESOURCES)!=0 || host->shiptype==CataProcessor;
    ship->dockingship=host;
    if(command->dock.dockType&DOCK_INSTANTANEOUSLY){
        /* End-of-mission Quick Dock bypasses movement and port queues. */
        if(ship->clampInfo && ship->clampInfo->host==(SpaceObjRotImpTarg *)host)
            unClampObj((SpaceObjRotImpTargGuidance *)ship);
        if(ship->dockvars.reserveddocking>=0)dockFreeDockPoint(ship,host);
        cataDockRuntimeFree(ship);
        univRemoveShipFromOutside(ship);
        if(command->dock.dockType&DOCK_FOR_RETIRE)return TRUE;
        dockPutShipInside(ship,host);return PERMANENTLY_DOCKED;
    }
    switch(ship->dockvars.dockstate2){
    case 0:
        index=cataDockFindPort(ship,host,FALSE,transfer);
        if(index<0){aitrackSteadyShip(ship);return FALSE;}
        cataDockReserve(ship,host,index);ship->dockvars.dockstate2=1;ship->dockvars.dockstate3=0;
        cataDockRuntime(ship);return FALSE;
    case 1:
        point=cataDockPoint(ship,host,FALSE,transfer);
        if(cataDockApproach(ship,host,&point)){
            state=cataDockRuntime(ship);
            matMultiplyMatByVec(&state->lastConeHeading,&host->rotinfo.coordsys,&point.conenormal);
            ship->dockvars.dockstate2=transfer?7:2;
        }
        return FALSE;
    case 2:
        point=cataDockPoint(ship,host,FALSE,transfer);
        if(!cataDockFinal(ship,host,&point))return FALSE;
        state=cataDockRuntime(ship);state->deadline=universe.totaltimeelapsed;
        dockFreeDockPoint(ship,host);
        univRemoveShipFromOutside(ship);
        if(command->dock.dockType&DOCK_FOR_RETIRE)return TRUE;
        dockPutShipInside(ship,host);ship->dockvars.dockstate2=4;
        return FALSE;
    case 7:
        point=cataDockPoint(ship,host,FALSE,TRUE);
        if(!cataDockClampedCone(ship,host,&point))return FALSE;
        cataDockWorkerAnimation(ship,3,TRUE);ship->dockvars.dockstate2=8;return FALSE;
    case 8:
        point=cataDockPoint(ship,host,FALSE,TRUE);
        if(!cataDockOffset(ship,host,&point,FALSE))return FALSE;
        state=cataDockRuntime(ship);state->deadline=universe.totaltimeelapsed;
        {
            /* Original external service deadline: four seconds to unload,
               plus six when damaged. No accelerated deposit/launch. */
            if(ship->resources>0)state->deadline+=4.0f;
            if(ship->health<ship->staticinfo->maxhealth)state->deadline+=6.0f;
            clampObjToObj((SpaceObjRotImpTargGuidance *)ship,(SpaceObjRotImpTargGuidance *)host);
            ship->dockvars.dockstate2=3;ship->dockvars.dockstate3=0;return FALSE;
        }
    case 3:
        ship->posinfo.velocity=host->posinfo.velocity;
        if(universe.totaltimeelapsed<=cataDockRuntime(ship)->deadline)return FALSE;
        cataDockDeposit(ship);
        unClampObj((SpaceObjRotImpTargGuidance *)ship);ship->dockvars.dockstate2=5;
        return FALSE;
    case 4:
        if(universe.totaltimeelapsed<=cataDockRuntime(ship)->deadline)return FALSE;
        if((ship->specialFlags&SPECIAL_STAY_TILL_EXPLICITLAUNCH) ||
           !(host->playerowner->autoLaunch&BIT0)){
            /* A later explicit launch initializes a fresh command. */
            cataDockRuntimeFree(ship);return PERMANENTLY_DOCKED;
        }
        dockInitShipForLaunch(ship);ship->dockvars.dockstate2=6;
        return FALSE;
    case 5:
        point=cataDockPoint(ship,host,FALSE,TRUE);
        if(!cataDockExit(ship,host,&point))return FALSE;
        dockFreeDockPoint(ship,host);cataDockWorkerAnimation(ship,2,FALSE);return TRUE;
    case 6:
        if(LaunchShipFromSomtaaw(ship,host))return FINISHED_LAUNCHING_FROM_DOCKING;
        return FALSE;
    }
    return FALSE;
}

