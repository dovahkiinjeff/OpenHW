#define SAVE_LAYOUT_FUNCTION saveLayoutCurrent
#include "SaveGameLayout.inc"
#include <string.h>

static const SaveGameLayout *(*const supportedLayouts[])(void) = {
    saveLayoutCurrent, saveLayoutCataclysm01, saveLayoutClassic
};

int saveLayoutSupportsShipCount(unsigned shipCount)
{
    size_t i;
    for (i = 0; i < sizeof(supportedLayouts) / sizeof(supportedLayouts[0]); ++i)
    {
        if (supportedLayouts[i]()->shipCount == shipCount) return 1;
    }
    return 0;
}

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
    const SaveGameLayout *current = saveLayoutCurrent();
    const SaveLayoutRecord *to = ai ? current->aiPlayer : current->universe;
    size_t i;
    for (i = 0; i < sizeof(supportedLayouts) / sizeof(supportedLayouts[0]); ++i)
    {
        const SaveGameLayout *layout = supportedLayouts[i]();
        const SaveLayoutRecord *from = ai ? layout->aiPlayer : layout->universe;
        if (size == from->size)
        {
            saveMigrateRecord(dest, to, src, from);
            return layout->shipCount;
        }
    }
    return 0;
}
unsigned saveMigrateUniverse(void *dest, const void *src, size_t size)
{ return migrate(dest, src, size, 0); }
unsigned saveMigrateAIPlayer(void *dest, const void *src, size_t size)
{ return migrate(dest, src, size, 1); }
