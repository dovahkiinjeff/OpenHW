#ifndef OPENHW_MOUSE_WINDOW_CONFINEMENT_H
#define OPENHW_MOUSE_WINDOW_CONFINEMENT_H
#include <SDL2/SDL.h>

/* Window confinement is independent of relative drag mode and cursor artwork.
   Store an ID, not a pointer, because renderer switches can recreate windows. */
static void mouseWindowConfinementUpdate(Uint32 *owner,SDL_Window *focused)
{
    Uint32 next=focused?SDL_GetWindowID(focused):0;
    if(focused && (SDL_GetWindowFlags(focused)&SDL_WINDOW_MINIMIZED)) next=0;
    if(*owner && *owner!=next)
    {
        SDL_Window *previous=SDL_GetWindowFromID(*owner);
        if(previous) SDL_SetWindowMouseGrab(previous,SDL_FALSE);
    }
    *owner=next;
    if(next)
    {
        /* Never capture the keyboard: Alt-Tab and system shortcuts stay usable. */
        if(!SDL_GetWindowMouseGrab(focused)) SDL_SetWindowMouseGrab(focused,SDL_TRUE);
    }
}
#endif
