#ifndef HW_LAUNCH_HOLD_LAYOUT_H
#define HW_LAUNCH_HOLD_LAYOUT_H

#define LM_HOLD_PAGE_SIZE 5
#define LM_HOLD_BUTTON_WIDTH 56
#define LM_HOLD_BUTTON_HEIGHT 54
#define LM_HOLD_BUTTON_STEP 68

static inline int lmHoldPageCount(int count)
{
    return count > 0 ? 1 + (count - 1) / LM_HOLD_PAGE_SIZE : 1;
}
static inline int lmHoldClampPage(int count, int page)
{
    int last = lmHoldPageCount(count) - 1;
    return page < 0 ? 0 : page > last ? last : page;
}
static inline int lmHoldSlotIndex(int count, int page, int slot)
{
    int index;
    if (slot < 0 || slot >= LM_HOLD_PAGE_SIZE || page != lmHoldClampPage(count, page)) return -1;
    index = page * LM_HOLD_PAGE_SIZE + slot;
    return index < count ? index : -1;
}

#endif
