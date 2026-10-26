#include "ControlGroupTabPolicy.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)
int main(void)
{
    int slot, width, gap, previous, used = 0;
    for (slot = 0; slot < HW_CONTROL_GROUP_TAB_COUNT; ++slot)
    {
        int group = controlGroupTabIndex(slot);
        CHECK(group >= 0 && group < 10);
        CHECK(!(used & (1 << group)));
        CHECK(group == (slot == 9 ? 0 : slot + 1));
        used |= 1 << group;
    }
    CHECK(used == 1023);
    for (width = 240; width < 3000; ++width)
        for (gap = 1; gap <= 8; ++gap)
        {
            previous = 0;
            for (slot = 0; slot < 10; ++slot)
            {
                int start = controlGroupTabEdge(width, gap, slot);
                int end = controlGroupTabEdge(width, gap, slot + 1) - gap;
                CHECK(start >= previous && end > start && end <= width);
                previous = end;
            }
            CHECK(previous == width);
        }
    puts("PASS: all ten group mappings and non-overlapping tab layouts");
    return 0;
}
