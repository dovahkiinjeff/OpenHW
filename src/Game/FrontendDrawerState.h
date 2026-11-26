#ifndef OPENHW_FRONTEND_DRAWER_STATE_H
#define OPENHW_FRONTEND_DRAWER_STATE_H
/* Mouse navigation takes ownership from a keyboard-opened drawer as soon as
   the pointer leaves it. Clicking a parent must not create a keyboard latch. */
static bool32 muiDrawerWantsOpen(bool32 *keyboard,bool32 parentHover,
    bool32 drawerHover,bool32 wasOpen,bool32 mouseMoved)
{
    if(mouseMoved && !parentHover && !drawerHover)*keyboard=FALSE;
    return *keyboard || parentHover || (wasOpen && drawerHover);
}
#endif
