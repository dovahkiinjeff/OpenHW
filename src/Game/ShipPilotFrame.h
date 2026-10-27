#ifndef OPENHW_SHIP_PILOT_FRAME_H
#define OPENHW_SHIP_PILOT_FRAME_H
#include "Types.h"
#include "Vector.h"
/* Pointer-free command payload. Inputs are executed only after the captain
   echoes them in the normal lockstep stream; cameras remain client-local. */
#define PILOT_WEAPON_BYTES 128
#define PILOT_FRAME_END 0
#define PILOT_FRAME_BEGIN 1
#define PILOT_FRAME_UPDATE 2
typedef struct ShipPilotFrame {
    udword sequence;
    uword ship, target, navTarget, picked;
    uword weaponCount, held;
    ubyte action, squad, cockpit, commandMode, all, fire, locked, navigation;
    vector aim, navHeading;
    real32 throttle, navRange, yawRate, pitchRate;
    ubyte weapons[PILOT_WEAPON_BYTES];
} ShipPilotFrame;
#endif
