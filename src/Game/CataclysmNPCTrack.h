#ifndef OPENHW_CATACLYSM_NPC_TRACK_H
#define OPENHW_CATACLYSM_NPC_TRACK_H
#include "AITrack.h"
bool32 cataNpc_aitrackIsStabilizeShip(Ship *ship);
bool32 cataNpc_aitrackStabilizeGuidance(SpaceObjRotImpTargGuidance *ship);
bool32 cataNpc_aitrackIsZeroRotation(Ship *ship);
bool32 cataNpc_aitrackZeroRotationAnywhere(Ship *ship);
bool32 cataNpc_aitrackHeadingAndUp(Ship *ship,vector *desiredHeading,vector *desiredUp,real32 accuracy);
bool32 cataNpc_aitrackHeadingFunc(SpaceObjRotImpTargGuidance *ship,vector *desiredHeading,real32 accuracy,real32 upaccuracy,real32 sinbank,udword flags);
bool32 cataNpc_aitrackHeadingWithBankPitchFunc(Ship *ship,vector *desiredHeading,real32 accuracy,real32 sinbank,real32 pitchdescend,real32 pitchturn,udword flags);
bool32 cataNpc_aitrackRightVector(Ship *ship,vector *desiredRightVector,real32 accuracy);
void cataNpc_aitrackVelocityVectorGuidance(SpaceObjRotImpTargGuidance *ship,vector *desiredVelocity);
bool32 cataNpc_aitrackIsZeroVelocity(Ship *ship);
bool32 cataNpc_aitrackZeroForwardVelocity(Ship *ship);
bool32 cataNpc_aitrackZeroVelocity(Ship *ship);
bool32 cataNpc_aitrackSteadyShipDriftOnly(Ship *ship);
bool32 cataNpc_aitrackIsShipSteadyAnywhere(Ship *ship);
bool32 cataNpc_aitrackSteadyShipAnywhere(Ship *ship);
bool32 cataNpc_aitrackIsShipSteady(Ship *ship);
bool32 cataNpc_aitrackSteadyShip(Ship *ship);
void cataNpc_aitrackForceSteadyShip(Ship *ship);
void cataNpc_aitrackForceGuidanceZeroRotation(SpaceObjRotImpTargGuidance *ship);
void cataNpc_aitrackForceZeroRotation(Ship *ship);
void cataNpc_aitrackForceHeading(Ship *ship, vector *heading, vector *upvector);
bool32 cataNpc_MoveReachedDestinationVariable(Ship *ship,vector *destination,real32 bounds);
bool32 cataNpc_MoveLeftToGo(Ship *ship,vector *destination,real32 bounds);
real32 cataNpc_MoveLeftToGoDistance(Ship *ship,vector *destination);
bool32 cataNpc_aitrackRotationSpeed(Ship *ship, real32 desiredrotspeed, uword track);
#endif
