#ifndef HW_CONTROL_GROUP_TAB_POLICY_H
#define HW_CONTROL_GROUP_TAB_POLICY_H

#define HW_CONTROL_GROUP_TAB_COUNT 10
/* Vanilla stores key 0 (displayed group 10) at index zero. */
static inline int controlGroupTabIndex(int slot) { return (slot + 1) % 10; }
static inline int controlGroupTabEdge(int width, int gap, int slot)
{
    return slot * (width + gap) / HW_CONTROL_GROUP_TAB_COUNT;
}

#endif
