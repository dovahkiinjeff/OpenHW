#ifndef OPENHW_PILOT_COLLISION_POLICY_H
#define OPENHW_PILOT_COLLISION_POLICY_H
/* Closing speed, not absolute speed: formation travel and separating contact
   must not cause damage. Linear scaling matches the native ram formula. */
static float pilotRamScale(float closing,float reference)
{ return closing>0 && reference>0?closing/reference:0; }
static float pilotContactImpulse(float closing,float inverseMass1,float inverseMass2)
{ float sum=inverseMass1+inverseMass2;return closing>0 && sum>0?closing/sum:0; }
#endif
