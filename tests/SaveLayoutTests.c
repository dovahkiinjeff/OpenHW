#include "Universe.h"
#include "AIPlayer.h"
#include "SaveGameLayout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"Failed line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static void checkFields(const unsigned char *to, const SaveLayoutRecord *a,
                        const unsigned char *from, const SaveLayoutRecord *b)
{
    size_t i, j;
    CHECK(a->fieldCount == b->fieldCount);
    for (i = 0; i < a->fieldCount; ++i)
    {
        const SaveLayoutField *x = &a->fields[i], *y = &b->fields[i];
        CHECK(x->offset + x->size <= a->size);
        CHECK(y->offset + y->size <= b->size);
        if (x->element)
            for (j = 0; j < x->count; ++j)
                checkFields(to + x->offset + j * x->element->size, x->element,
                            from + y->offset + j * y->element->size, y->element);
        else
        {
            CHECK(memcmp(to + x->offset, from + y->offset, y->size) == 0);
            for (j = y->size; j < x->size; ++j) CHECK(to[x->offset + j] == 0);
        }
    }
}

static void testLayout(const SaveGameLayout *old)
{
    const SaveGameLayout *now = saveLayoutCurrent();
    int ai;
    printf("Testing ship table %u: universe=%zu AI=%zu\n",old->shipCount,
           old->universe->size,old->aiPlayer->size);
    for (ai = 0; ai < 2; ++ai)
    {
        const SaveLayoutRecord *a = ai ? now->aiPlayer : now->universe;
        const SaveLayoutRecord *b = ai ? old->aiPlayer : old->universe;
        unsigned char *src = malloc(b->size), *dest = malloc(a->size + 32);
        size_t i;
        for (i = 0; i < b->size; ++i) src[i] = (unsigned char)(i * 17 + 3);
        memset(dest, 0xa5, a->size + 32);
        CHECK((ai ? saveMigrateAIPlayer(dest,src,b->size) :
                    saveMigrateUniverse(dest,src,b->size)) == old->shipCount);
        checkFields(dest,a,src,b);
        for (i = a->size; i < a->size + 32; ++i) CHECK(dest[i] == 0xa5);
        CHECK(saveMigrateUniverse(dest,src,13) == 0);
        free(src); free(dest);
    }
}

static void testSave(const char *path)
{
    FILE *file = fopen(path,"rb");
    unsigned version, type;
    int size, universes = 0, ais = 0;
    int queues = 0, ships = 0;
    CHECK(file != NULL);
    CHECK(fread(&version,4,1,file) == 1);
    while (fread(&type,4,1,file) == 1)
    {
        unsigned char *bytes;
        CHECK(fread(&size,4,1,file) == 1 && size >= 0 && size < 64000000);
        bytes = malloc(size ? size : 1);
        CHECK(fread(bytes,1,size,file) == (size_t)size);
        if (type == 0x80000020u)
        {
            Universe converted;
            unsigned count = saveMigrateUniverse(&converted,bytes,size);
            CHECK(count != 0);
            CHECK(converted.numPlayers > 0 && converted.numPlayers <= MAX_MULTIPLAYER_PLAYERS);
            CHECK(converted.curPlayerIndex < converted.numPlayers);
            CHECK(converted.players[0].race == R1 || converted.players[0].race == R2);
            printf("  Universe %d -> %zu bytes (%u ships), players=%u RU=%d\n",
                   size,sizeof(converted),count,converted.numPlayers,converted.players[0].resourceUnits);
            ++universes;
        }
        else if (type == 0x88000000u)
        {
            AIPlayer converted;
            CHECK(saveMigrateAIPlayer(&converted,bytes,size) != 0);
            ++ais;
        }
        else if (type == 0x80000600u)
        {
            size_t prefix = offsetof(shipsinprogress,progress);
            size_t count;
            CHECK((size_t)size >= prefix);
            CHECK(((size_t)size - prefix) % sizeof(shipinprogress) == 0);
            count = ((size_t)size - prefix) / sizeof(shipinprogress);
            CHECK(count == TOTAL_STD_SHIPS || count == ResearchStationTower + 1 ||  count == TOTAL_NUM_SHIPS);
            ++queues;
        }
        else if (type == (0x40000010u | OBJ_ShipType))
        {
            /* Check the pre-veterancy tail splice against real ship records,
               including the custom-ship payload following the base struct. */
            size_t insertion = sizeof(((Ship *)0)->veterancyKills) + 4;
            size_t oldOffset = offsetof(Ship,sizeofShipSpecifics);
            unsigned specifics;
            ShipType shipType;
            if (version < 0x10ad002au) oldOffset -= insertion;
            CHECK((size_t)size >= oldOffset + sizeof(specifics));
            memcpy(&specifics,bytes + oldOffset,sizeof(specifics));
            memcpy(&shipType,bytes + offsetof(Ship,shiptype),sizeof(shipType));
            CHECK(shipType < TOTAL_NUM_SHIPS && specifics < 10000);
            ++ships;
        }
        free(bytes);
    }
    CHECK(feof(file) && universes == 1);
    fclose(file);
    printf("Save verified (version %08x, %d AI, %d queues, %d ships): %s\n",
           version,ais,queues,ships,path);
}

int main(int argc, char **argv)
{
    int i;
    testLayout(saveLayoutClassic());
    testLayout(saveLayoutCurrent());
    for (i = 1; i < argc; ++i) testSave(argv[i]);
    return 0;
}
