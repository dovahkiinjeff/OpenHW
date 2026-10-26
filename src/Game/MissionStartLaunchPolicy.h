#ifndef HW_MISSION_START_LAUNCH_POLICY_H
#define HW_MISSION_START_LAUNCH_POLICY_H

enum
{
    MISSION_START_LAUNCH_VANILLA = 0,
    MISSION_START_LAUNCH_HOLD_COMBAT = 1,
    MISSION_START_LAUNCH_HOLD_ALL = 2
};

static inline int missionStartHoldShip(unsigned int mode, int combatCraft, int utilityShip)
{
    return mode == MISSION_START_LAUNCH_HOLD_ALL ||
        (mode == MISSION_START_LAUNCH_HOLD_COMBAT && combatCraft && !utilityShip);
}

#endif
