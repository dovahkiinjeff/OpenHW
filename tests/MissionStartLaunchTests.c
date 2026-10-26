#include "MissionStartLaunchPolicy.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)
int main(void)
{
    unsigned int mode;
    int combat, utility;
    for (mode = 0; mode < 4; ++mode)
        for (combat = 0; combat < 2; ++combat)
            for (utility = 0; utility < 2; ++utility)
                CHECK(missionStartHoldShip(mode, combat, utility) ==
                    (mode == 2 || (mode == 1 && combat && !utility)));
    CHECK(missionStartHoldShip(1, 1, 0)); /* fighter / combat corvette */
    CHECK(!missionStartHoldShip(1, 1, 1)); /* repair / salvage corvette */
    CHECK(!missionStartHoldShip(1, 0, 1)); /* resource collector */
    CHECK(missionStartHoldShip(2, 0, 1)); /* all means utilities too */
    CHECK(!missionStartHoldShip(999, 1, 0)); /* invalid settings use vanilla */
    puts("PASS: campaign launch modes and utility exceptions");
    return 0;
}
