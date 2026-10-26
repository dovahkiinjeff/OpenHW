/* The retired volumetric resource-cloud bridge must never reactivate itself. */
#include "../src/Game/ModernHarvestableDust.c"
#include <stdio.h>
static unsigned submissions;
void hwModernGraphicsSetHarvestableDustVolumes(const HWModernVolumetricDustVolume *volumes,unsigned count)
{
    if(volumes || count){fprintf(stderr,"Retired resource clouds submitted active volumes\n");submissions=999;}
    else ++submissions;
}
int main(void)
{
    if(modernHarvestableDustActive() || modernHarvestableDustActiveFor(1) ||
       modernHarvestableDustHasOverride(1) || modernHarvestableDustSave())return 1;
    modernHarvestableDustReload();modernHarvestableDustResetOverride(1);
    modernHarvestableDustUpdate();
    if(submissions!=1)return 1;
    puts("Retired resource-cloud system stays inactive; RM sprite effects own harvestable clouds.");
    return 0;
}
