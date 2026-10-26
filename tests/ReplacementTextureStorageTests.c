#include "ReplacementTextureStorage.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void)
{
    HWReplacementTextureStorage slot = {0};
    uint32_t colors[16];
    int count, i, transition;
    CHECK(!hwReplacementStorageOwns(&slot, slot.words));
    for (i = 0; i < 16; ++i) colors[i] = 0xff002000u + i;
    CHECK(hwReplacementStorageBegin(&slot,0,colors) == NULL);
    CHECK(hwReplacementStorageBegin(&slot,9,colors) == NULL);
    CHECK(hwReplacementStorageBegin(&slot,1,NULL) == NULL);
    for (count = 1; count <= 8; ++count)
    {
        CHECK(hwReplacementStorageBegin(&slot,count,colors) == slot.words);
        CHECK(hwReplacementStorageOwns(&slot,slot.words));
        CHECK(!hwReplacementStorageOwns(&slot,colors));
        for (i = 0; i < count; ++i) CHECK(slot.words[i] == UINT32_MAX);
        for (i = 0; i < count * 2; ++i) CHECK(slot.words[count+i] == colors[i]);
        slot.words[0] = 123;
        // Resident metadata stays allocated through any number of level changes.
        for (transition = 0; transition < 32; ++transition)
        {
            CHECK(slot.resident);
            CHECK(slot.words[0] == 123);
            CHECK(memcmp(slot.words+count,colors,count*8) == 0);
        }
        slot.resident = 0;
        CHECK(!hwReplacementStorageOwns(&slot,slot.words));
    }
    puts("PASS: replacement ownership, all fleet palettes, transition retention and slot reuse");
    return 0;
}
