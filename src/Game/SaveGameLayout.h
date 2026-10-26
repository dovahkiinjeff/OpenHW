#ifndef SAVE_GAME_LAYOUT_H
#define SAVE_GAME_LAYOUT_H
#include <stddef.h>
typedef struct SaveLayoutRecord SaveLayoutRecord;
typedef struct {
    size_t offset, size;
    const SaveLayoutRecord *element;
    size_t count;
} SaveLayoutField;
struct SaveLayoutRecord {
    size_t size, fieldCount;
    const SaveLayoutField *fields;
};
typedef struct {
    unsigned shipCount;
    const SaveLayoutRecord *universe, *aiPlayer;
} SaveGameLayout;
const SaveGameLayout *saveLayoutClassic(void);
const SaveGameLayout *saveLayoutCurrent(void);
/* Returns the saved ship-table length, or zero for an unsupported layout. */
unsigned saveMigrateUniverse(void *dest, const void *src, size_t size);
unsigned saveMigrateAIPlayer(void *dest, const void *src, size_t size);
void saveMigrateRecord(void *dest, const SaveLayoutRecord *to,
                       const void *src, const SaveLayoutRecord *from);
#endif
