#include "CataclysmAI.h"
#include "SpaceObj.h"
#include "Universe.h"
#include "SinglePlayer.h"
bool32 cataNpcControlsObject(const void *object)
{
    const SpaceObj *obj=(const SpaceObj*)object;
    const Ship *ship=(const Ship*)object;
    return obj && singlePlayerGame && hwIsCataclysmMission(spGetCurrentMission()) &&
        obj->objtype==OBJ_ShipType && ship->playerowner &&
        (ship->shiptype==CataMothership || ship->playerowner!=&universe.players[0] ||
         (ship->specialFlags2&SPECIAL_2_CataScriptControlled));
}
