#ifndef HW_MILITARY_PARADE_LAYOUT_H
#define HW_MILITARY_PARADE_LAYOUT_H
#include "ObjTypes.h"
#include "Vector.h"
typedef struct ParadeShipLayout {
    bool32 enabled;
    real32 position[3]; /* right, forward, up, relative to the host */
    real32 rotation[3]; /* degrees around those same axes; Rz Ry Rx */
    real32 growth[3];   /* displacement per additional ship of this identity */
} ParadeShipLayout;
typedef struct ParadeLayoutDocument {
    ParadeShipLayout ships[NUM_RACES][TOTAL_NUM_SHIPS];
} ParadeLayoutDocument;
void paradeLayoutGet(ShipRace host, ParadeLayoutDocument *out);
bool32 paradeLayoutApply(ShipRace host, const ParadeLayoutDocument *document);
bool32 paradeLayoutSave(ShipRace host, const ParadeLayoutDocument *document);
bool32 paradeLayoutReload(ShipRace host);
bool32 paradeLayoutSaveMultiplayer(ShipRace host,const char *name,const ParadeLayoutDocument *document);
bool32 paradeLayoutLoadMultiplayer(ShipRace host,const char *name,ParadeLayoutDocument *document);
bool32 paradeLayoutValid(const ParadeShipLayout *entry);
void paradeLayoutRotate(const real32 rotation[3], const real32 input[3], real32 output[3]);
bool32 paradeLayoutIdentityValid(ShipRace race, ShipType type);
/* Default layout is supplied by the original parade scripts, not a new guess. */
bool32 paradeLayoutDefault(ShipRace host, ShipRace race, ShipType type, ParadeShipLayout *out);
/* Pure lookup: editor previews never create simulation ships. */
bool32 paradeLayoutRuntime(ShipRace host, ShipRace race, ShipType type, sdword ordinal,
    ParadeShipLayout *entry, real32 position[3]);
#endif
