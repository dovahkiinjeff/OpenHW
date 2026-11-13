#ifdef NDEBUG
#undef NDEBUG
#endif
#include "HudVisibilityPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    int mode, desired, running, cinematic, intel;
    assert(hwHudNextMode(HW_HUD_FULL)==HW_HUD_COMPACT);
    assert(hwHudNextMode(HW_HUD_COMPACT)==HW_HUD_HIDDEN);
    assert(hwHudNextMode(HW_HUD_HIDDEN)==HW_HUD_FULL);
    for(mode=0;mode<HW_HUD_MODE_COUNT;mode++)
    for(desired=0;desired<HW_HUD_MODE_COUNT;desired++)
    for(running=0;running<2;running++)
    for(cinematic=0;cinematic<2;cinematic++)
    for(intel=0;intel<2;intel++)
        assert(hwHudVisible(mode,desired,running,cinematic,intel)==
               (mode==desired && running && !cinematic && !intel));
    puts("PASS: HUD cycle and cinematic suppression for all display states");
    return 0;
}
