#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
static SDL_bool grabs[3];
static Uint32 minimized;
static unsigned changes;
static Uint32 testID(SDL_Window *w){return (Uint32)(uintptr_t)w;}
static SDL_Window *testWindow(Uint32 id){return id<3?(SDL_Window *)(uintptr_t)id:NULL;}
static Uint32 testFlags(SDL_Window *w){return testID(w)==minimized?SDL_WINDOW_MINIMIZED:SDL_WINDOW_INPUT_FOCUS;}
static SDL_bool testGrab(SDL_Window *w){return grabs[testID(w)];}
static void testSet(SDL_Window *w,SDL_bool grab){grabs[testID(w)]=grab;++changes;}
#define SDL_GetWindowID testID
#define SDL_GetWindowFromID testWindow
#define SDL_GetWindowFlags testFlags
#define SDL_GetWindowMouseGrab testGrab
#define SDL_SetWindowMouseGrab testSet
#include "../src/SDL/MouseWindowConfinement.h"
int main(void)
{
    Uint32 owner=0;
    mouseWindowConfinementUpdate(&owner,testWindow(1));assert(owner==1 && grabs[1]);
    mouseWindowConfinementUpdate(&owner,testWindow(1));assert(changes==1);
    /* A drag-mode transition must not leave absolute/menu input unconfined. */
    grabs[1]=SDL_FALSE;mouseWindowConfinementUpdate(&owner,testWindow(1));assert(grabs[1]);
    mouseWindowConfinementUpdate(&owner,NULL);assert(!owner && !grabs[1]);
    mouseWindowConfinementUpdate(&owner,testWindow(1));assert(grabs[1]);
    mouseWindowConfinementUpdate(&owner,testWindow(2));assert(!grabs[1] && grabs[2]);
    minimized=2;mouseWindowConfinementUpdate(&owner,testWindow(2));assert(!owner && !grabs[2]);
    minimized=0;owner=99;mouseWindowConfinementUpdate(&owner,testWindow(1));assert(owner==1 && grabs[1]);
    mouseWindowConfinementUpdate(&owner,NULL);assert(!grabs[1]);
    return 0;
}
