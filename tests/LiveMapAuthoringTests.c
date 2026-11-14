#include "LiveMapAuthoringPolicy.h"
#include <stdio.h>
static int online;
int hwMatchActive(void){return online;}
int main(void){
    /* Offline campaign and local/LAN matches do not activate server rules. */
    online=0;if(hwLiveMapAuthoringEnabled()!=(HW_ENABLE_LIVE_MAP_AUTHORING!=0))return 1;
    online=0;if(hwLiveMapAuthoringEnabled()!=(HW_ENABLE_LIVE_MAP_AUTHORING!=0))return 2;
    online=1;if(hwLiveMapAuthoringEnabled())return 3;
    online=0;if(hwLiveMapAuthoringEnabled()!=(HW_ENABLE_LIVE_MAP_AUTHORING!=0))return 4;
    puts("PASS: single-player/local authoring, online blocking and session transitions");return 0;
}
