#include "../src/Game/CampaignColorPolicy.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);return 1;}}while(0)
int main(void)
{
    CHECK(hwKeepMissionPlayerColors(1,1,0,2,0,1)); /* explicit Somtaaw identity */
    CHECK(!hwKeepMissionPlayerColors(1,0,0,2,0,1)); /* original Cata default */
    CHECK(hwKeepMissionPlayerColors(0,1,0,2,0,1)); /* HW1 identity */
    CHECK(hwKeepMissionPlayerColors(0,0,0,2,0,1)); /* unchanged HW1 fallback */
    CHECK(!hwKeepMissionPlayerColors(1,1,1,2,0,0)); /* NPC original colors */
    CHECK(!hwKeepMissionPlayerColors(1,1,6,2,0,1)); /* scripted ally palette */
    CHECK(hwKeepMissionPlayerColors(0,0,1,4,1,0)); /* configured match colors */
    puts("Campaign identity colors: passed.");return 0;
}
