#define SAVE_LAYOUT_FUNCTION saveLayoutCurrent
#include "SaveGameLayout.inc"
#include <string.h>

void saveMigrateRecord(void *dest, const SaveLayoutRecord *to,
                       const void *src, const SaveLayoutRecord *from)
{
    size_t i, j;
    memset(dest, 0, to->size);
    for (i = 0; i < to->fieldCount; ++i)
    {
        const SaveLayoutField *a = &to->fields[i], *b = &from->fields[i];
        if (a->element)
        {
            for (j = 0; j < a->count && j < b->count; ++j)
                saveMigrateRecord((char *)dest + a->offset + j * a->element->size,
                    a->element, (const char *)src + b->offset + j * b->element->size,
                    b->element);
        }
        else
            memcpy((char *)dest + a->offset, (const char *)src + b->offset,
                   a->size < b->size ? a->size : b->size);
    }
}

static unsigned migrate(void *dest, const void *src, size_t size, int ai)
{
    const SaveGameLayout *layouts[] = {saveLayoutCurrent(), saveLayoutClassic()};
    const SaveLayoutRecord *to = ai ? layouts[0]->aiPlayer : layouts[0]->universe;
    unsigned i;
    for (i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i)
    {
        const SaveLayoutRecord *from = ai ? layouts[i]->aiPlayer : layouts[i]->universe;
        if (size == from->size)
        {
            saveMigrateRecord(dest, to, src, from);
            return layouts[i]->shipCount;
        }
    }
    return 0;
}
unsigned saveMigrateUniverse(void *dest, const void *src, size_t size)
{ return migrate(dest, src, size, 0); }
unsigned saveMigrateAIPlayer(void *dest, const void *src, size_t size)
{ return migrate(dest, src, size, 1); }
