#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "NativeCursorPolicy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "Cursor test failed at %d: %s\n", __LINE__, #condition); return 1; } } while (0)
int main(void)
{
    const unsigned char source[] = {255, 200, 0, 255, 0, 0, 0, 0};
    unsigned char resized[4], enlarged[32];
    hwCursorResizeRgba(source, 2, 1, resized, 1, 1);
    CHECK(resized[0] == 255 && resized[1] == 200 && resized[2] == 0);
    CHECK(resized[3] == 128); /* Transparent pixels cannot darken the arrow. */
    hwCursorResizeRgba(source, 2, 1, enlarged, 8, 1);
    CHECK(enlarged[0] == 255 && enlarged[3] == 255);
    CHECK(enlarged[31] == 0);
    hwCursorResizeRgba(resized, 1, 1, enlarged, 2, 2);
    CHECK(memcmp(enlarged, resized, 4) == 0);
    CHECK(memcmp(enlarged+12, resized, 4) == 0);
#ifdef _WIN32
    {
        SDL_Cursor *cursors[3];
        SDL_Surface *surface;
        int i;
        /* No window or game launch: exercise the real Windows cursor backend. */
        SDL_SetMainReady();
        CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
        for (i=0; i<3; ++i)
        {
            surface = SDL_CreateRGBSurfaceWithFormat(0, 16+i*16, 32+i*32, 32, SDL_PIXELFORMAT_RGBA32);
            CHECK(surface != NULL);
            SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 255, 200, 0, 255));
            cursors[i] = SDL_CreateColorCursor(surface, 0, 0);
            SDL_FreeSurface(surface);
            CHECK(cursors[i] != NULL);
            SDL_SetCursor(cursors[i]);
            CHECK(SDL_GetCursor() == cursors[i]);
        }
        SDL_ShowCursor(SDL_DISABLE); CHECK(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE);
        SDL_ShowCursor(SDL_ENABLE); CHECK(SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE);
        SDL_SetCursor(SDL_GetDefaultCursor());
        for (i=0; i<3; ++i) SDL_FreeCursor(cursors[i]);
        SDL_Quit();
    }
#endif
    puts("Native cursor alpha scaling and Windows handle/visibility lifecycle passed.");
    return 0;
}
