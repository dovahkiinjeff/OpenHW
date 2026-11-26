#include "GameConsole.h"
#include "Universe.h"
#include "Globals.h"
#include "Select.h"
#include "FontReg.h"
#include "prim2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
Universe universe;
MaxSelection selSelected;
bool32 gameIsRunning=TRUE,multiPlayerGame=FALSE,universePause=FALSE;
static int checks,keyClears;
sdword MAIN_WindowWidth=1280,MAIN_WindowHeight=720,primModeEnabled=0;
void keyClearAll(void){++keyClears;}
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void primModeSetFunction2(void){primModeEnabled=1;}
void primRectSolid2(rectangle *rect,color ink){(void)ink;CHECK(rect->x0==0&&rect->x1==MAIN_WindowWidth);CHECK(rect->y1<MAIN_WindowHeight);}
void primLine2(sdword x0,sdword y0,sdword x1,sdword y1,color ink){(void)x0;(void)y0;(void)x1;(void)y1;(void)ink;}
fonthandle frFontRegister(char *name){CHECK(!strcmp(name,"Arial_12.hff"));return 1;}
fonthandle fontMakeCurrent(fonthandle handle){(void)handle;return 1;}
sdword fontWidth(char *value){return (sdword)strlen(value)*8;}
sdword fontHeight(char *value){(void)value;return 16;}
sdword fontPrint(sdword x,sdword y,color ink,char *value){(void)ink;CHECK(x>=0&&x+fontWidth(value)<=MAIN_WindowWidth);CHECK(y>=0&&y<MAIN_WindowHeight);return 0;}
static int key(SDL_Scancode code,int repeat){SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.scancode=code;event.key.repeat=(Uint8)repeat;return gameConsoleHandleEvent(&event);}
static int text(const char *value){SDL_Event event={0};event.type=SDL_TEXTINPUT;snprintf(event.text.text,sizeof(event.text.text),"%s",value);return gameConsoleHandleEvent(&event);}
int main(void)
{
    char reply[384];ShipStaticInfo info={0};Ship *ships;int i;
    universe.curPlayerPtr=&universe.players[0];universe.players[0].resourceUnits=50;
    CHECK(gameConsoleExecute("player.additem RU 250",reply,sizeof(reply)));CHECK(universe.players[0].resourceUnits==300);
    CHECK(gameConsoleExecute("  PLAYER.ADDITEM ru 0  ",reply,sizeof(reply)));CHECK(universe.players[0].resourceUnits==300);
    {const char *invalid[]={"player.additem RU -1","player.additem RU 2.5","player.additem RU 10garbage","player.additem RUs 10","player.additem RU","player.additem RU 999999999999999999999","player.additem RU +20","player.restorehealth extra","player.additemx RU 2"};
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){CHECK(!gameConsoleExecute(invalid[i],reply,sizeof(reply)));CHECK(universe.players[0].resourceUnits==300);}}
    universe.players[0].resourceUnits=INT_MAX-2;
    CHECK(!gameConsoleExecute("player.additem RU 3",reply,sizeof(reply)));CHECK(universe.players[0].resourceUnits==INT_MAX-2);
    CHECK(gameConsoleExecute("player.additem RU 2",reply,sizeof(reply)));CHECK(universe.players[0].resourceUnits==INT_MAX);
    universe.players[0].resourceUnits=0;
    info.maxhealth=1234;ships=calloc(MAX_MULTIPLAYER_PLAYERS+1000,sizeof(*ships));CHECK(ships!=NULL);
    selSelected.numShips=sizeof(selSelected.ShipPtr)/sizeof(selSelected.ShipPtr[0]);
    CHECK(selSelected.numShips>20);
    // Exercise the full real selection capacity, beyond typical group size.
    free(ships);ships=calloc(selSelected.numShips,sizeof(*ships));CHECK(ships!=NULL);
    for(i=0;i<selSelected.numShips;i++){ships[i].staticinfo=&info;ships[i].health=1;selSelected.ShipPtr[i]=&ships[i];}
    CHECK(gameConsoleExecute("player.restorehealth",reply,sizeof(reply)));
    for(i=0;i<selSelected.numShips;i++)CHECK(ships[i].health==info.maxhealth);
    selSelected.numShips=1;ships[0].health=3;CHECK(gameConsoleExecute("player.restorehealth",reply,sizeof(reply)));CHECK(ships[0].health==1234);
    ships[0].flags|=SOF_Dead;ships[0].health=0;CHECK(gameConsoleExecute("player.restorehealth",reply,sizeof(reply)));CHECK(ships[0].health==0);
    selSelected.numShips=0;CHECK(gameConsoleExecute("player.restorehealth",reply,sizeof(reply)));CHECK(strstr(reply,"No living")!=NULL);
    multiPlayerGame=TRUE;CHECK(!gameConsoleExecute("player.additem RU 100",reply,sizeof(reply)));CHECK(!gameConsoleExecute("player.restorehealth",reply,sizeof(reply)));CHECK(universe.players[0].resourceUnits==0);
    multiPlayerGame=FALSE;gameIsRunning=FALSE;CHECK(!gameConsoleExecute("player.additem RU 100",reply,sizeof(reply)));CHECK(!key(SDL_SCANCODE_GRAVE,0));gameIsRunning=TRUE;
    CHECK(key(SDL_SCANCODE_GRAVE,0));CHECK(universePause);CHECK(key(SDL_SCANCODE_GRAVE,1));CHECK(universePause);
    gameConsoleDraw();MAIN_WindowWidth=320;MAIN_WindowHeight=240;gameConsoleDraw();
    CHECK(text("`"));CHECK(text("player.additem RU 77"));CHECK(key(SDL_SCANCODE_RETURN,0));CHECK(universe.players[0].resourceUnits==77);
    CHECK(key(SDL_SCANCODE_UP,0));CHECK(key(SDL_SCANCODE_RETURN,0));CHECK(universe.players[0].resourceUnits==154);
    for(i=0;i<20;i++)CHECK(text("long input which exceeds width "));
    gameConsoleDraw();CHECK(key(SDL_SCANCODE_HOME,0));gameConsoleDraw();CHECK(key(SDL_SCANCODE_END,0));
    for(i=0;i<256;i++)key(SDL_SCANCODE_BACKSPACE,0);
    CHECK(key(SDL_SCANCODE_RETURN,1));CHECK(universe.players[0].resourceUnits==154);
    {SDL_Event event={0};event.type=SDL_MOUSEBUTTONDOWN;CHECK(gameConsoleHandleEvent(&event));event.type=SDL_KEYUP;CHECK(gameConsoleHandleEvent(&event));}
    CHECK(key(SDL_SCANCODE_ESCAPE,0));CHECK(!universePause);CHECK(!key(SDL_SCANCODE_H,0));
    universePause=TRUE;CHECK(key(SDL_SCANCODE_GRAVE,0));CHECK(key(SDL_SCANCODE_GRAVE,0));CHECK(universePause);
    universePause=FALSE;CHECK(key(SDL_SCANCODE_GRAVE,0));gameConsoleReset();CHECK(!universePause);CHECK(keyClears>=6);
    free(ships);printf("Game console: %d checks passed.\n",checks);return 0;
}
