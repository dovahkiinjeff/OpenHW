#ifndef OPENHW_GAME_CONSOLE_H
#define OPENHW_GAME_CONSOLE_H
#include <stddef.h>
#include <SDL2/SDL.h>
int gameConsoleHandleEvent(const SDL_Event *event);
void gameConsoleDraw(void);
void gameConsoleReset(void);
int gameConsoleExecute(const char *command,char *reply,size_t capacity);
#endif
