/* Cataclysm NPC guidance port. Relic/Barking Dog algorithms; cross-checked
   against samuraicop/cataclysm f56d377d9d30c32bd2c48a198102ad36d170f47d.
   Typed OpenHW integration: physics step uses phystimeelapsed; retail ship
   careen/non-bumpable bit maps to OpenHW's named flags. No retail pointers. */
#include "CataclysmNPCTrack.h"
#include "CataclysmAI.h"
#include "AIShip.h"
#include "FastMath.h"
#include "Physics.h"
#include "StatScript.h"
#include "Universe.h"
#include <math.h>
#define DEBUG_AITRACK 0
#define AITRACK_FORCE_ZERO_SUPPRESSED(obj) (((SpaceObj*)(obj))->objtype==OBJ_ShipType?bitTest((obj)->flags,SOF_Crazy):bitTest((obj)->flags,SOF_NotBumpable))
extern real32 ERROR_VELOCITY_SQUARED,STILL_VELOCITY_LO,STILL_VELOCITY_HI,STILL_ROT_LO,STILL_ROT_HI;
extern real32 STABILIZE_Z_LO,STABILIZE_Z_HI,AITRACKRIGHT_UPACCURACY;
#undef aitrackStabilizeShip
#define aitrackStabilizeShip(ship) cataNpc_aitrackStabilizeGuidance((SpaceObjRotImpTargGuidance*)ship)
#undef aitrackVelocityVector
#define aitrackVelocityVector(ship,v) cataNpc_aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance*)ship,v)
#undef aitrackForceShipZeroRotation
#define aitrackForceShipZeroRotation(ship) cataNpc_aitrackForceZeroRotation(ship)
#undef aitrackHeading
#define aitrackHeading(ship,head,accuracy) cataNpc_aitrackHeadingFunc((SpaceObjRotImpTargGuidance*)ship,head,accuracy,accuracy,0,0)
bool32 cataNpc_aitrackIsStabilizeShip(Ship *ship)
{
    vector heading,up,right;
    bool32 stabilizedpitch;
    real32 rotx = ship->rotinfo.rotspeed.x;

    matGetVectFromMatrixCol3(heading,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,ship->rotinfo.coordsys);

    stabilizedpitch = isBetweenExclusive(heading.z,STABILIZE_Z_LO,STABILIZE_Z_HI);
    if (stabilizedpitch)
    {
        if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
    
    
    
            isBetweenExclusive(right.z,STABILIZE_Z_LO,STABILIZE_Z_HI) &&
            (up.z > 0) )
        {
            return TRUE;
        }
    }
    return FALSE;
}

bool32 cataNpc_aitrackStabilizeGuidance(SpaceObjRotImpTargGuidance *ship)
{
    real32 rotx = ship->rotinfo.rotspeed.x;
    real32 roty = ship->rotinfo.rotspeed.y;
    real32 rotz = ship->rotinfo.rotspeed.z;
    vector heading;
    vector up;
    vector right;
    real32 rotstr;
    real32 oneovertime;
    bool32 stabilizedpitch;

    StaticInfoHealthGuidance *shipstaticinfo;

    matGetVectFromMatrixCol3(heading,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,ship->rotinfo.coordsys);

    stabilizedpitch = isBetweenExclusive(heading.z,STABILIZE_Z_LO,STABILIZE_Z_HI);
    if (stabilizedpitch)
    {
        roty = ship->rotinfo.rotspeed.y = 0.0f;
    }

    if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&


          (stabilizedpitch) &&
        isBetweenExclusive(right.z,STABILIZE_Z_LO,STABILIZE_Z_HI) &&
        (up.z > 0) )
    {
        return TRUE;
    }

#ifdef gshaw

#endif

    shipstaticinfo = ship->staticinfo;
    oneovertime = 1.0f / universe.phystimeelapsed;

    if (rotx > 0)
    {
        rotstr = rotx * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
    }
    else
    {
        rotstr = -rotx * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
    }

    if (stabilizedpitch)
    {
        if (rotz > 1.0f)
        {
            rotstr = rotz * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
        else if (rotz < -1.0f)
        {
            rotstr = -rotz * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            if (right.z > 0)
            {
                physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ROLLRIGHT]*right.z,ROT_ROLLRIGHT);
            }
            else
            {
                physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ROLLLEFT]*(-right.z),ROT_ROLLLEFT);
            }
        }
    }
    else
    {
        if (rotz > 0.0f)
        {
            rotstr = rotz * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
        else
        {
            rotstr = -rotz * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }

        if (roty > 1.0f)
        {
            rotstr = roty * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
        else if (roty < -1.0f)
        {
            rotstr = -roty * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            if (heading.z < 0)
            {
                physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_PITCHUP]*(-heading.z),ROT_PITCHUP);
            }
            else
            {
                physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_PITCHDOWN]*heading.z,ROT_PITCHDOWN);
            }
        }
    }

#if 0
    desiredrotspeed = -heading.z
    if (ship->rotinfo.rotspeed.z < desiredrotspeed)
    {
        physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW],ROT_ABOUTYCCW);
    }
    else
    {
        physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ABOUTYCW],ROT_ABOUTYCW);
    }
#endif

    return FALSE;
}

bool32 cataNpc_aitrackIsZeroRotation(Ship *ship)
{
    real32 rotx = ship->rotinfo.rotspeed.x;
    real32 roty = ship->rotinfo.rotspeed.y;
    real32 rotz = ship->rotinfo.rotspeed.z;

    if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(roty,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(rotz,STILL_ROT_LO,STILL_ROT_HI))
    {
        return TRUE;
    }

    return FALSE;
}

bool32 cataNpc_aitrackZeroRotationAnywhere(Ship *ship)
{
    real32 rotx = ship->rotinfo.rotspeed.x;
    real32 roty = ship->rotinfo.rotspeed.y;
    real32 rotz = ship->rotinfo.rotspeed.z;
    vector heading;
    vector up;
    vector right;
    real32 rotstr;
    real32 oneovertime;

    ShipStaticInfo *shipstaticinfo;

    matGetVectFromMatrixCol3(heading,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,ship->rotinfo.coordsys);

    if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(roty,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(rotz,STILL_ROT_LO,STILL_ROT_HI))
    {
        return TRUE;
    }

    shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    oneovertime = 1.0f / universe.phystimeelapsed;

    if (rotx > 0)
    {
        rotstr = rotx * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
    }
    else
    {
        rotstr = -rotx * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
    }

    if (roty > 0)
    {
        rotstr = roty * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
    }
    else
    {
        rotstr = -roty * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
    }
    if (rotz > 0)
    {
        rotstr = rotz * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
    }
    else
    {
        rotstr = -rotz * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
    }

    return FALSE;
}

bool32 cataNpc_aitrackHeadingAndUp(Ship *ship,vector *desiredHeading,vector *desiredUp,real32 accuracy)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;

    vector curheading;
    vector curup;
    real32 dotProdCheckHeading;
    real32 dotProdCheckUp;

    real32 desiredrotspeed;
    real32 rotspeed;

    vector desiredHeadingInShipCoordSys;

    vector desiredUpInShipCoordSys;

    real32 rotstr;
    real32 oneovertime;

    if(ship->objtype == OBJ_ShipType)
    {
        if(ship->dontrotateever)
        {
            return TRUE;
        }
    }
    matGetVectFromMatrixCol3(curheading,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotinfo.coordsys);

    dotProdCheckHeading = vecDotProduct(*desiredHeading,curheading);
    dotProdCheckUp = vecDotProduct(*desiredUp,curup);

#if DEBUG_AITRACK
    dbgMessagef("\nHeading accuracy: %f",dotProdCheckHeading);
#endif

    if (dotProdCheckHeading <= -1.0f)   
    {
        
        physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ABOUTXCW],ROT_ABOUTXCW);
        return FALSE;
    }

    if (dotProdCheckUp <= -1.0f)   
    {
        
        physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ABOUTZCW],ROT_ABOUTZCW);
        return FALSE;
    }

    if ((dotProdCheckHeading >= accuracy) && (dotProdCheckUp >= accuracy))
    {
#if DEBUG_AITRACK
        dbgMessage("\nOn correct heading...");
#endif
        aitrackForceShipZeroRotation(ship);
        return TRUE;     
    }

    
    

    

    matMultiplyVecByMat(&desiredHeadingInShipCoordSys,desiredHeading,&ship->rotinfo.coordsys);

    

    matMultiplyVecByMat(&desiredUpInShipCoordSys,desiredUp,&ship->rotinfo.coordsys);

    oneovertime = 1.0f / universe.phystimeelapsed;

    
    if (desiredHeadingInShipCoordSys.y != 0)
    {
        if (desiredHeadingInShipCoordSys.z < 0.0f)
        {
            if (desiredHeadingInShipCoordSys.y < 0.0f)
            {
                desiredrotspeed = ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
            else
            {
                desiredrotspeed = -ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
        }
        else
        {
            desiredrotspeed = -desiredHeadingInShipCoordSys.y * ship->nonstatvars.turnspeed[TURN_ABOUTX];
        }

        rotspeed = ship->rotinfo.rotspeed.x;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
        }
    }

    
    if (desiredHeadingInShipCoordSys.x != 0)
    {
        desiredrotspeed = desiredHeadingInShipCoordSys.x * ship->nonstatvars.turnspeed[TURN_ABOUTY];
        rotspeed = ship->rotinfo.rotspeed.y;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
    }

    
    

    if (desiredUpInShipCoordSys.y != 0.0f)
    {
        desiredrotspeed = desiredUpInShipCoordSys.y * ship->nonstatvars.turnspeed[TURN_ABOUTZ];
        rotspeed = ship->rotinfo.rotspeed.z;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
    }

    return FALSE;
}

bool32 cataNpc_aitrackHeadingFunc(SpaceObjRotImpTargGuidance *ship,vector *desiredHeading,real32 accuracy,real32 upaccuracy,real32 sinbank,udword flags)
{
    StaticInfoHealthGuidance *shipstaticinfo = (StaticInfoHealthGuidance *)ship->staticinfo;

    vector curheading;
    vector curup;
    real32 dotProdCheckHeading;
    real32 dotProdCheckUp;

    real32 desiredrotspeed;
    real32 rotspeed;

    vector desiredHeadingInShipCoordSys;

    vector desiredUp = { 0.0f,0.0f,1.0f };
    vector desiredUpInShipCoordSys;

    real32 rotstr;
    real32 oneovertime;

    real32 yawcloseness = 0.0f;

    if(ship->objtype == OBJ_ShipType)
    {
        if(((Ship *)ship)->dontrotateever)    
        {
            return TRUE;
        }
    }

    if (desiredHeading->x == 0.0f && desiredHeading->y == 0.0f && desiredHeading->z == 0.0f)
    {
        return TRUE;
    }

    if (ship->nonstatvars.rotstrength[ROT_PITCHUP] == 0)
    {
        desiredHeading->z = 0.0f;
        vecNormalize(desiredHeading);       
    }

    matGetVectFromMatrixCol3(curheading,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotinfo.coordsys);

    aishipStatsDesiredHeading(*desiredHeading);
    aishipStatsActualHeading(curheading);

    dotProdCheckHeading = vecDotProduct(*desiredHeading,curheading);
    dotProdCheckUp = curup.z; 

#if DEBUG_AITRACK
    dbgMessagef("\nHeading accuracy: %f",dotProdCheckHeading);
#endif

    if (dotProdCheckHeading <= -1.0f)
    {
        
        physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ABOUTXCW],ROT_ABOUTXCW);
        return FALSE;
    }

    if ((dotProdCheckHeading >= accuracy) &&
        ((flags & (AITRACKHEADING_IGNOREUPVEC+AITRACKHEADING_DONTROLL)) || (dotProdCheckUp >= upaccuracy)) )
    {
#if DEBUG_AITRACK
        dbgMessage("\nOn correct heading...");
#endif
        if (!(flags & AITRACK_DONT_ZERO_ME) && !AITRACK_FORCE_ZERO_SUPPRESSED(ship))
        {
            ship->rotinfo.rotspeed.x = 0.0f;
            ship->rotinfo.rotspeed.y = 0.0f;
            ship->rotinfo.rotspeed.z = 0.0f;
        }
        return TRUE;     
    }

    
    

    

    matMultiplyVecByMat(&desiredHeadingInShipCoordSys,desiredHeading,&ship->rotinfo.coordsys);

    

    matMultiplyVecByMat(&desiredUpInShipCoordSys,&desiredUp,&ship->rotinfo.coordsys);

    oneovertime = 1.0f / universe.phystimeelapsed;

    if (desiredHeadingInShipCoordSys.y != 0)
    {
        if (desiredHeadingInShipCoordSys.z < 0.0f)
        {
            if (desiredHeadingInShipCoordSys.y < 0.0f)
            {
                desiredrotspeed = ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
            else
            {
                desiredrotspeed = -ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
        }
        else
        {
            desiredrotspeed = -desiredHeadingInShipCoordSys.y * ship->nonstatvars.turnspeed[TURN_ABOUTX];
        }

        if (sinbank != 0.0f)
        {
            if (desiredHeadingInShipCoordSys.z < 0.0f)
            {
                if (desiredHeadingInShipCoordSys.y < 0.0f)
                {
                    yawcloseness = sinbank;
                }
                else
                {
                    yawcloseness = -sinbank;
                }
            }
            else
            {
                yawcloseness = -desiredHeadingInShipCoordSys.y * sinbank;
            }
        }
        rotspeed = ship->rotinfo.rotspeed.x;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
        }
    }

    
    if (desiredHeadingInShipCoordSys.x != 0)
    {
        desiredrotspeed = desiredHeadingInShipCoordSys.x * ship->nonstatvars.turnspeed[TURN_ABOUTY];
        rotspeed = ship->rotinfo.rotspeed.y;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
    }

    if (flags & AITRACKHEADING_DONTROLL)
    {
        return FALSE;
    }

    
    

    if (desiredUpInShipCoordSys.y != yawcloseness)
    {
        desiredrotspeed = (desiredUpInShipCoordSys.y - yawcloseness) * ship->nonstatvars.turnspeed[TURN_ABOUTZ];
        rotspeed = ship->rotinfo.rotspeed.z;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
    }

    return FALSE;
}

bool32 cataNpc_aitrackHeadingWithBankPitchFunc(Ship *ship,vector *desiredHeading,real32 accuracy,real32 sinbank,real32 pitchdescend,real32 pitchturn,udword flags)
{
    vector curheading;
    vector desheading;
    real32 curheadingsqr,curheadingmag;
    real32 desheadingsqr,desheadingmag;

    vector curup;

    real32 dotprodheading;
    real32 dotProdCheckUp;

    real32 actualpitchturn;
    real32 finalpitch;

    real32 temp1,temp2;

    if(ship->objtype == OBJ_ShipType)
    {
        if(ship->dontrotateever)
        {
            
            return TRUE;
        }
    }
    matGetVectFromMatrixCol3(curheading,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotinfo.coordsys);

    

    desheading.x = desiredHeading->x;
    desheading.y = desiredHeading->y;
    

    desheadingsqr = desheading.x * desheading.x + desheading.y * desheading.y;
    curheadingsqr = curheading.x * curheading.x + curheading.y * curheading.y;
    if (desheadingsqr == 0.0f)       
    {
        desheading.x = 1.0f;
        desheadingsqr = 1.0f;
    }
    if (curheadingsqr == 0.0f)      
    {
        curheading.x = 1.0f;
        curheadingsqr = 1.0f;
    }

    curheadingmag = fsqrt(curheadingsqr);
    desheadingmag = fsqrt(desheadingsqr);
    temp1 = 1.0f / curheadingmag;
    temp2 = 1.0f / desheadingmag;

    curheading.x *= temp1;
    curheading.y *= temp1;
    desheading.x *= temp2;
    desheading.y *= temp2;

    dotprodheading = (curheading.x * desheading.x) + (curheading.y * desheading.y); 

    dotProdCheckUp = curup.z;   

    if ((dotprodheading >= accuracy) && (pitchdescend == 0.0f) &&
        isBetweenExclusive(curheading.z,STABILIZE_Z_LO,STABILIZE_Z_HI) &&
        ((flags & AITRACKHEADING_IGNOREUPVEC) || (dotProdCheckUp >= accuracy)) )
    {
#if DEBUG_AITRACK
        dbgMessage("\nCapital ship on correct heading...");
#endif
        ship->rotinfo.rotspeed.x = 0.0f;
        ship->rotinfo.rotspeed.y = 0.0f;
        ship->rotinfo.rotspeed.z = 0.0f;
        return TRUE;     
    }

    actualpitchturn = (1.0f - fabsf(dotprodheading)) * pitchturn;

    if (pitchdescend < 0.0f)
    {
        actualpitchturn = -actualpitchturn;
        if (pitchdescend < actualpitchturn)
        {
            finalpitch = pitchdescend;
        }
        else
        {
            finalpitch = actualpitchturn;
        }
    }
    else
    {
        if (pitchdescend > actualpitchturn)
        {
            finalpitch = pitchdescend;
        }
        else
        {
            finalpitch = actualpitchturn;
        }
    }

    

    desheading.z = (real32)tan(finalpitch);     

    temp1 = 1.0f / fsqrt(desheading.z * desheading.z + 1.0f);

    desheading.x *= temp1;       
    desheading.y *= temp1;
    desheading.z *= temp1;

    return cataNpc_aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship,&desheading,accuracy,accuracy, sinbank,flags);
}

bool32 cataNpc_aitrackRightVector(Ship *ship,vector *desiredRightVector,real32 accuracy)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;

    vector curright;
    vector curup;
    real32 dotProdCheckRight;
    real32 dotProdCheckUp;

    vector desiredRightInShipCoordSys;

    vector desiredUp = { 0.0f,0.0f,1.0f };
    vector desiredUpInShipCoordSys;

    real32 desiredrotspeed;
    real32 rotspeed;

    real32 rotstr;
    real32 oneovertime;

    if(ship->objtype == OBJ_ShipType)
    {
        if(ship->dontrotateever)
        {
            
            return TRUE;
        }
    }
    matGetVectFromMatrixCol2(curright,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotinfo.coordsys);

    dotProdCheckRight = vecDotProduct(*desiredRightVector,curright);
    dotProdCheckUp = curup.z; 

#if DEBUG_AITRACK
    dbgMessagef("\nHeading accuracy: %f",dotProdCheckRight);
#endif

    if (dotProdCheckRight <= -1.0f)
    {
        
        physApplyRotToObj((SpaceObjRot *)ship,ship->nonstatvars.rotstrength[ROT_ABOUTXCW],ROT_ABOUTXCW);
        return FALSE;
    }

    if ((dotProdCheckRight >= accuracy) && (dotProdCheckUp >= AITRACKRIGHT_UPACCURACY))
    {
#if DEBUG_AITRACK
        dbgMessage("\nOn correct heading...");
#endif
        aitrackForceShipZeroRotation(ship);
        return TRUE;     
    }

    
    

    

    matMultiplyVecByMat(&desiredRightInShipCoordSys,desiredRightVector,&ship->rotinfo.coordsys);

    

    matMultiplyVecByMat(&desiredUpInShipCoordSys,&desiredUp,&ship->rotinfo.coordsys);

    oneovertime = 1.0f / universe.phystimeelapsed;

    
    if (desiredRightInShipCoordSys.z != 0)
    {
        if (desiredRightInShipCoordSys.y < 0.0f)
        {
            if (desiredRightInShipCoordSys.z > 0.0f)
            {
                desiredrotspeed = ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
            else
            {
                desiredrotspeed = -ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
        }
        else
        {
            desiredrotspeed = desiredRightInShipCoordSys.z * ship->nonstatvars.turnspeed[TURN_ABOUTX];
        }

        rotspeed = ship->rotinfo.rotspeed.x;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaX * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
        }
    }

    
    

    if (desiredUpInShipCoordSys.z != 0)
    {
        if (desiredUpInShipCoordSys.x < 0.0f)
        {
            if (desiredUpInShipCoordSys.z < 0.0f)
            {
                desiredrotspeed = ship->nonstatvars.turnspeed[TURN_ABOUTY];
            }
            else
            {
                desiredrotspeed = -ship->nonstatvars.turnspeed[TURN_ABOUTY];
            }
        }
        else
        {
            desiredrotspeed = -desiredUpInShipCoordSys.z * ship->nonstatvars.turnspeed[TURN_ABOUTY];
        }

        rotspeed = ship->rotinfo.rotspeed.y;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaY * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
    }

    
    if (desiredRightInShipCoordSys.x != 0)
    {
        if (desiredRightInShipCoordSys.y < 0.0f)
        {
            if (desiredRightInShipCoordSys.x < 0.0f)
            {
                desiredrotspeed = ship->nonstatvars.turnspeed[TURN_ABOUTZ];
            }
            else
            {
                desiredrotspeed = -ship->nonstatvars.turnspeed[TURN_ABOUTZ];
            }
        }
        else
        {
            desiredrotspeed = -desiredRightInShipCoordSys.x * ship->nonstatvars.turnspeed[TURN_ABOUTZ];
        }

        rotspeed = ship->rotinfo.rotspeed.z;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->staticheader.momentOfInertiaZ * oneovertime;
            physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
    }

    return FALSE;
}

void cataNpc_aitrackVelocityVectorGuidance(SpaceObjRotImpTargGuidance *ship,vector *desiredVelocity)
{
    StaticInfoHealthGuidance *shipstaticinfo = (StaticInfoHealthGuidance *)ship->staticinfo;

    vector errorDifference;
    real32 errorMagSquared;
    real32 thruststr;
    real32 massOverTime;

    vector desiredVelocityInShipCoordSys;
    vector shipVelocityInShipCoordSys;

    

    vecSub(errorDifference,*desiredVelocity,ship->posinfo.velocity); 
    errorMagSquared = vecMagnitudeSquared(errorDifference);

#if DEBUG_AITRACK
    dbgMessagef("\nVelocity Error = %f",errorMagSquared);
#endif

    if (errorMagSquared < ERROR_VELOCITY_SQUARED)
    {
#if DEBUG_AITRACK
        dbgMessage("\nVelocity tracking locked...");
#endif
        return;     
    }

    
    

    
    

    matMultiplyVecByMat(&desiredVelocityInShipCoordSys,desiredVelocity,&ship->rotinfo.coordsys);
    matMultiplyVecByMat(&shipVelocityInShipCoordSys,&ship->posinfo.velocity,&ship->rotinfo.coordsys);

    massOverTime = shipstaticinfo->staticheader.mass / universe.phystimeelapsed;

    if (shipVelocityInShipCoordSys.x < desiredVelocityInShipCoordSys.x)
    {
        thruststr = (desiredVelocityInShipCoordSys.x - shipVelocityInShipCoordSys.x) * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_UP]),TRANS_UP);
    }
    else if (shipVelocityInShipCoordSys.x > desiredVelocityInShipCoordSys.x)
    {
        thruststr = (shipVelocityInShipCoordSys.x - desiredVelocityInShipCoordSys.x) * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_DOWN]),TRANS_DOWN);
    }

    if (shipVelocityInShipCoordSys.y < desiredVelocityInShipCoordSys.y)
    {
        thruststr = (desiredVelocityInShipCoordSys.y - shipVelocityInShipCoordSys.y) * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_RIGHT]),TRANS_RIGHT);
    }
    else if (shipVelocityInShipCoordSys.y > desiredVelocityInShipCoordSys.y)
    {
        thruststr = (shipVelocityInShipCoordSys.y - desiredVelocityInShipCoordSys.y) * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_LEFT]),TRANS_LEFT);
    }

    if (shipVelocityInShipCoordSys.z < desiredVelocityInShipCoordSys.z)
    {
        thruststr = (desiredVelocityInShipCoordSys.z - shipVelocityInShipCoordSys.z) * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_FORWARD]),TRANS_FORWARD);
    }
    else if (shipVelocityInShipCoordSys.z > desiredVelocityInShipCoordSys.z)
    {
        thruststr = (shipVelocityInShipCoordSys.z - desiredVelocityInShipCoordSys.z) * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_BACKWARD]),TRANS_BACKWARD);
    }

}

bool32 cataNpc_aitrackIsZeroVelocity(Ship *ship)
{
    vector shipVelocity;

    shipVelocity = ship->posinfo.velocity;

    if (isBetweenExclusive(shipVelocity.x,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.y,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.z,STILL_VELOCITY_LO,STILL_VELOCITY_HI) )
    {
        return TRUE;     
    }
    else
    {
        return FALSE;
    }
}

bool32 cataNpc_aitrackZeroForwardVelocity(Ship *ship)
{
    ShipStaticInfo *shipstaticinfo = ship->staticinfo;
    real32 thruststr;
    real32 massOverTime;
    real32 forwardvelocity;
    vector heading;

    matGetVectFromMatrixCol3(heading,ship->rotinfo.coordsys);
    forwardvelocity = vecDotProduct(heading,ship->posinfo.velocity);

    if (isBetweenExclusive(forwardvelocity,STILL_VELOCITY_LO,STILL_VELOCITY_HI))
    {
        return TRUE;    
    }

    massOverTime = shipstaticinfo->staticheader.mass / universe.phystimeelapsed;

    if (forwardvelocity < 0)
    {
        thruststr = -forwardvelocity * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_FORWARD]),TRANS_FORWARD);
    }
    else
    {
        thruststr = forwardvelocity * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_BACKWARD]),TRANS_BACKWARD);
    }

    return FALSE;
}

bool32 cataNpc_aitrackZeroVelocity(Ship *ship)
{
    vector shipVelocity;
    vector shipVelocityInShipCoordSys;
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    real32 thruststr;
    real32 massOverTime;

    shipVelocity = ship->posinfo.velocity;

    if (isBetweenExclusive(shipVelocity.x,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.y,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.z,STILL_VELOCITY_LO,STILL_VELOCITY_HI) )
    {
        return TRUE;     
    }

#ifdef gshaw

#endif

    
    matMultiplyVecByMat(&shipVelocityInShipCoordSys,&shipVelocity,&ship->rotinfo.coordsys);

    massOverTime = shipstaticinfo->staticheader.mass / universe.phystimeelapsed;
    if (shipVelocityInShipCoordSys.x < 0)
    {
        thruststr = -shipVelocityInShipCoordSys.x * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_UP]),TRANS_UP);
    }
    else
    {
        thruststr = shipVelocityInShipCoordSys.x * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_DOWN]),TRANS_DOWN);
    }

    if (shipVelocityInShipCoordSys.y < 0)
    {
        thruststr = -shipVelocityInShipCoordSys.y * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_RIGHT]),TRANS_RIGHT);
    }
    else
    {
        thruststr = shipVelocityInShipCoordSys.y * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_LEFT]),TRANS_LEFT);
    }

    if (shipVelocityInShipCoordSys.z < 0)
    {
        thruststr = -shipVelocityInShipCoordSys.z * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_FORWARD]),TRANS_FORWARD);
    }
    else
    {
        thruststr = shipVelocityInShipCoordSys.z * massOverTime;
        physApplyForceToObj((SpaceObj *)ship,capNumber(thruststr,ship->nonstatvars.thruststrength[TRANS_BACKWARD]),TRANS_BACKWARD);
    }

    return FALSE;
}

bool32 cataNpc_aitrackSteadyShipDriftOnly(Ship *ship)
{
    if (!cataNpc_aitrackZeroVelocity(ship))
    {
        return FALSE;
    }

    if (!AITRACK_FORCE_ZERO_SUPPRESSED(ship))
    {
        ship->posinfo.velocity.x = 0.0f;
        ship->posinfo.velocity.y = 0.0f;
        ship->posinfo.velocity.z = 0.0f;
    }

    return TRUE;
}

bool32 cataNpc_aitrackIsShipSteadyAnywhere(Ship *ship)
{
    return (cataNpc_aitrackIsZeroRotation(ship) & cataNpc_aitrackIsZeroVelocity(ship));
}

bool32 cataNpc_aitrackSteadyShipAnywhere(Ship *ship)
{
    bool32 stabilized = cataNpc_aitrackZeroRotationAnywhere(ship);
    bool32 zerovel = cataNpc_aitrackZeroVelocity(ship);

    if (stabilized && zerovel)
    {
        cataNpc_aitrackForceSteadyShip(ship);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

bool32 cataNpc_aitrackIsShipSteady(Ship *ship)
{
    if (isBetweenExclusive(ship->posinfo.velocity.x, STILL_VELOCITY_LO, STILL_VELOCITY_HI) &&
        isBetweenExclusive(ship->posinfo.velocity.y, STILL_VELOCITY_LO, STILL_VELOCITY_HI) &&
        isBetweenExclusive(ship->posinfo.velocity.z, STILL_VELOCITY_LO, STILL_VELOCITY_HI))
    {
        return cataNpc_aitrackIsStabilizeShip(ship);
    }
    cataNpc_aitrackIsStabilizeShip(ship);
    return FALSE;
}

bool32 cataNpc_aitrackSteadyShip(Ship *ship)
{
    bool32 stabilized = aitrackStabilizeShip(ship);
    bool32 zerovel = cataNpc_aitrackZeroVelocity(ship);


    if (stabilized && zerovel)
    {
        if (!AITRACK_FORCE_ZERO_SUPPRESSED(ship))
        {
            ship->posinfo.velocity.x = 0.0f;
            ship->posinfo.velocity.y = 0.0f;
            ship->posinfo.velocity.z = 0.0f;
            ship->rotinfo.rotspeed.x = 0.0f;
            ship->rotinfo.rotspeed.y = 0.0f;
            ship->rotinfo.rotspeed.z = 0.0f;
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void cataNpc_aitrackForceSteadyShip(Ship *ship)
{
    if (AITRACK_FORCE_ZERO_SUPPRESSED(ship))
    {
        return;
    }

    ship->posinfo.velocity.x = 0.0f;
    ship->posinfo.velocity.y = 0.0f;
    ship->posinfo.velocity.z = 0.0f;

    ship->rotinfo.rotspeed.x = 0.0f;
    ship->rotinfo.rotspeed.y = 0.0f;
    ship->rotinfo.rotspeed.z = 0.0f;
}

void cataNpc_aitrackForceGuidanceZeroRotation(SpaceObjRotImpTargGuidance *ship)
{
    if (AITRACK_FORCE_ZERO_SUPPRESSED(ship))
    {
        return;
    }

    ship->posinfo.velocity.x = 0.0f;
    ship->posinfo.velocity.y = 0.0f;
    ship->posinfo.velocity.z = 0.0f;
}

void cataNpc_aitrackForceZeroRotation(Ship *ship)
{
    if (AITRACK_FORCE_ZERO_SUPPRESSED(ship))
    {
        return;
    }

    ship->rotinfo.rotspeed.x = 0.0f;
    ship->rotinfo.rotspeed.y = 0.0f;
    ship->rotinfo.rotspeed.z = 0.0f;
}

void cataNpc_aitrackForceHeading(Ship *ship, vector *heading, vector *upvector)
{
    vector rightvec;
    if (AITRACK_FORCE_ZERO_SUPPRESSED(ship))
    {
        return;
    }

    vecCrossProduct(rightvec,*heading,*upvector);
    
    ship->rotinfo.coordsys.m11 = upvector->x;
    ship->rotinfo.coordsys.m21 = upvector->y;
    ship->rotinfo.coordsys.m31 = upvector->z;
    
    ship->rotinfo.coordsys.m12 = rightvec.x;
    ship->rotinfo.coordsys.m22 = rightvec.y;
    ship->rotinfo.coordsys.m32 = rightvec.z;
    
    ship->rotinfo.coordsys.m13 = heading->x;
    ship->rotinfo.coordsys.m23 = heading->y;
    ship->rotinfo.coordsys.m33 = heading->z;
}

bool32 cataNpc_MoveReachedDestinationVariable(Ship *ship,vector *destination,real32 bounds)
{
    vector lefttogo;
    real32 negbounds = -bounds;

    vecSub(lefttogo,*destination,ship->posinfo.position);

    if ((isBetweenExclusive(lefttogo.x,negbounds,bounds)) &&
        (isBetweenExclusive(lefttogo.y,negbounds,bounds)) &&
        (isBetweenExclusive(lefttogo.z,negbounds,bounds)))
    {
#ifdef gshaw

#endif
        return TRUE;
    }
    else
    {
#ifdef gshaw

#endif
        return FALSE;
    }
}

bool32 cataNpc_MoveLeftToGo(Ship *ship,vector *destination,real32 bounds)
{
    vector lefttogo;

    vecSub(lefttogo, ship->posinfo.position, *destination);

    return (vecMagnitudeSquared(lefttogo) < (bounds * bounds));
}

real32 cataNpc_MoveLeftToGoDistance(Ship *ship,vector *destination)
{
    real32 absx = fabsf(destination->x - ship->posinfo.position.x);
    real32 absy = fabsf(destination->y - ship->posinfo.position.y);
    real32 absz = fabsf(destination->z - ship->posinfo.position.z);

    return max(max(absx,absy),absz);
}

bool32 cataNpc_aitrackRotationSpeed(Ship *ship, real32 desiredrotspeed, uword track)
{
    real32 rotstr;
    real32 oneovertime;
    real32 rotspeed;

    oneovertime = 1.0f / universe.phystimeelapsed;

    switch (track)
    {
        case ROT_ABOUTXCCW :
        case ROT_ABOUTXCW  :
            if (isBetweenExclusive(ship->rotinfo.rotspeed.x,
                desiredrotspeed+STILL_ROT_LO,
                desiredrotspeed+STILL_ROT_HI))
                return(TRUE);
            rotspeed = ship->rotinfo.rotspeed.x;
            rotstr = (desiredrotspeed - rotspeed) * ship->staticinfo->staticheader.momentOfInertiaX * oneovertime;
        break;
        case ROT_ABOUTYCCW :
        case ROT_ABOUTYCW  :
            if (isBetweenExclusive(ship->rotinfo.rotspeed.y,
                desiredrotspeed+STILL_ROT_LO,
                desiredrotspeed+STILL_ROT_HI))
                return(TRUE);
            rotspeed = ship->rotinfo.rotspeed.y;
            rotstr = (desiredrotspeed - rotspeed) * ship->staticinfo->staticheader.momentOfInertiaY * oneovertime;
        break;
        case ROT_ABOUTZCCW :
        case ROT_ABOUTZCW  :
            if (isBetweenExclusive(ship->rotinfo.rotspeed.z,
                desiredrotspeed+STILL_ROT_LO,
                desiredrotspeed+STILL_ROT_HI))
                return(TRUE);
            rotspeed = ship->rotinfo.rotspeed.z;
            rotstr = (desiredrotspeed - rotspeed) * ship->staticinfo->staticheader.momentOfInertiaZ * oneovertime;
        break;
    }

    if (rotspeed < desiredrotspeed)
    {
        physApplyRotToObj((SpaceObjRot *)ship,capNumber(rotstr,ship->nonstatvars.rotstrength[track]),track);
        return(FALSE);
    }
    else
    {
        rotstr = -capNumber(-rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]);
        physApplyRotToObj((SpaceObjRot *)ship,rotstr,track);
        return(FALSE);
    }
}
