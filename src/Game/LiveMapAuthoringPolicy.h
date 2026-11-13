#ifndef OPENHW_LIVE_MAP_AUTHORING_POLICY_H
#define OPENHW_LIVE_MAP_AUTHORING_POLICY_H
/* Public builds load authored environments but never open live map tools.
   Developer source builds can opt in explicitly at configure time. */
#ifndef HW_ENABLE_LIVE_MAP_AUTHORING
#define HW_ENABLE_LIVE_MAP_AUTHORING 0
#endif
static inline int hwLiveMapAuthoringEnabled(void)
{
    return HW_ENABLE_LIVE_MAP_AUTHORING != 0;
}
#endif
