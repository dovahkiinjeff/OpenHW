#ifndef OPENHW_CATACLYSM_AI_H
#define OPENHW_CATACLYSM_AI_H
#include "Types.h"
/* Mission-scoped original navigation/tracking/manoeuvre settings. These are
   rebuilt from owned data, never saved pointers or retail executable calls. */
void cataAISelectProfile(bool32 enabled);
bool32 cataNpcControlsObject(const void *object);
#endif
