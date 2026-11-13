#ifndef HW_HUD_VISIBILITY_POLICY_H
#define HW_HUD_VISIBILITY_POLICY_H

enum { HW_HUD_FULL, HW_HUD_COMPACT, HW_HUD_HIDDEN, HW_HUD_MODE_COUNT };

static int hwHudNextMode(int mode) { return (mode + 1) % HW_HUD_MODE_COUNT; }
static int hwHudVisible(int mode, int desired, int running, int cinematic, int fleetIntel)
{
    return running && !cinematic && !fleetIntel && mode == desired;
}

#endif
