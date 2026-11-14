#ifndef OPENHW_LIVE_MAP_AUTHORING_POLICY_H
#define OPENHW_LIVE_MAP_AUTHORING_POLICY_H
#include "MatchRules.h"
/* Authoring is available offline, including local multiplayer, never online. */
#ifndef HW_ENABLE_LIVE_MAP_AUTHORING
#define HW_ENABLE_LIVE_MAP_AUTHORING 1
#endif
static inline int hwLiveMapAuthoringEnabled(void)
{
    return HW_ENABLE_LIVE_MAP_AUTHORING != 0 && !hwMatchActive();
}
#endif
