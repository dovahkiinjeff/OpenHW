#ifndef HW_REPLACEMENT_TEXTURE_STORAGE_H
#define HW_REPLACEMENT_TEXTURE_STORAGE_H
#include <stdint.h>
#include <string.h>

/* Resident RGB handles and fleet colors belong to the registry slot, not to
   the legacy resizable palette heap. No disk-size reload may invalidate them. */
#define HW_REPLACEMENT_PALETTES 8
typedef struct HWReplacementTextureStorage {
    uint32_t words[3 * HW_REPLACEMENT_PALETTES];
    int resident;
} HWReplacementTextureStorage;

static void *hwReplacementStorageBegin(HWReplacementTextureStorage *storage,
                                      int count, const void *colors)
{
    int i;
    if (count < 1 || count > HW_REPLACEMENT_PALETTES || colors == NULL)
        return NULL;
    for (i = 0; i < count; ++i) storage->words[i] = UINT32_MAX;
    memcpy(storage->words + count, colors, (size_t)count * 2 * sizeof(uint32_t));
    storage->resident = 1;
    return storage->words;
}

static int hwReplacementStorageOwns(const HWReplacementTextureStorage *storage,
                                   const void *pointer)
{
    return storage->resident && pointer == storage->words;
}
#endif
