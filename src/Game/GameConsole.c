#include "GameConsole.h"
#include "Globals.h"
#include "Universe.h"
#include "Key.h"
#include "FontReg.h"
#include "prim2d.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define CONSOLE_INPUT 256
#define CONSOLE_LINES 64
#define CONSOLE_HISTORY 16
static int opened,wasPaused,pausedByConsole,cursor,length,lineCount,scroll,historyCount,historyIndex;
static char input[CONSOLE_INPUT],draft[CONSOLE_INPUT];
static char lines[CONSOLE_LINES][384],history[CONSOLE_HISTORY][CONSOLE_INPUT];
static fonthandle consoleFont;
static void output(const char *line)
{
    if(lineCount==CONSOLE_LINES){memmove(lines,lines+1,sizeof(lines)-sizeof(lines[0]));--lineCount;}
    snprintf(lines[lineCount++],sizeof(lines[0]),"%s",line);scroll=0;
}
static void closeConsole(void)
{
    if(!opened)return;
    opened=0;if(pausedByConsole)universePause=wasPaused;pausedByConsole=0;SDL_StopTextInput();keyClearAll();
}
void gameConsoleReset(void)
{
    closeConsole();memset(input,0,sizeof(input));length=cursor=lineCount=scroll=historyCount=historyIndex=0;
    /* FontReg owns font lifetime; its advertised Unregister API is not
       implemented. Re-resolve after reset rather than retaining a stale slot. */
    consoleFont=0;
}
static void insertText(const char *text)
{
    for(;*text;text++){
        unsigned char ch=(unsigned char)*text;
        if(ch<32||ch>126||ch=='`'||ch=='~'||length>=CONSOLE_INPUT-1)continue;
        memmove(input+cursor+1,input+cursor,(size_t)(length-cursor+1));input[cursor++]=(char)ch;++length;
    }
}
static void recall(int direction)
{
    if(!historyCount)return;
    if(historyIndex==historyCount)snprintf(draft,sizeof(draft),"%s",input);
    historyIndex+=direction;if(historyIndex<0)historyIndex=0;if(historyIndex>historyCount)historyIndex=historyCount;
    snprintf(input,sizeof(input),"%s",historyIndex==historyCount?draft:history[historyIndex]);cursor=length=(int)strlen(input);
}
static void submit(void)
{
    char reply[384],echo[CONSOLE_INPUT+4];
    if(!length)return;
    snprintf(echo,sizeof(echo),"> %s",input);output(echo);
    gameConsoleExecute(input,reply,sizeof(reply));output(reply);
    if(!historyCount||strcmp(history[historyCount-1],input)){
        if(historyCount==CONSOLE_HISTORY){memmove(history,history+1,sizeof(history)-sizeof(history[0]));--historyCount;}
        snprintf(history[historyCount++],CONSOLE_INPUT,"%s",input);
    }
    input[0]=draft[0]=0;length=cursor=0;historyIndex=historyCount;
}
int gameConsoleHandleEvent(const SDL_Event *event)
{
    if(opened&&!gameIsRunning)closeConsole();
    if(event->type==SDL_KEYDOWN&&event->key.keysym.scancode==SDL_SCANCODE_GRAVE&&gameIsRunning){
        if(!event->key.repeat){
            if(opened)closeConsole();else{
                opened=1;wasPaused=universePause;pausedByConsole=!multiPlayerGame;if(pausedByConsole)universePause=TRUE;
                keyClearAll();SDL_StartTextInput();historyIndex=historyCount;
                if(!lineCount){output("OPENHW CONSOLE  /  ` OR ESC TO CLOSE");output("player.additem RU <value>  |  player.restorehealth");}
            }
        }
        return 1;
    }
    if(!opened)return 0;
    if(event->type==SDL_TEXTINPUT){insertText(event->text.text);return 1;}
    if(event->type==SDL_KEYDOWN){
        SDL_Scancode key=event->key.keysym.scancode;
        if(key==SDL_SCANCODE_ESCAPE){closeConsole();return 1;}
        if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER){if(!event->key.repeat)submit();}
        else if(key==SDL_SCANCODE_BACKSPACE&&cursor){memmove(input+cursor-1,input+cursor,(size_t)(length-cursor+1));--cursor;--length;}
        else if(key==SDL_SCANCODE_DELETE&&cursor<length){memmove(input+cursor,input+cursor+1,(size_t)(length-cursor));--length;}
        else if(key==SDL_SCANCODE_LEFT&&cursor)--cursor;
        else if(key==SDL_SCANCODE_RIGHT&&cursor<length)++cursor;
        else if(key==SDL_SCANCODE_HOME)cursor=0;
        else if(key==SDL_SCANCODE_END)cursor=length;
        else if(key==SDL_SCANCODE_UP)recall(-1);
        else if(key==SDL_SCANCODE_DOWN)recall(1);
        else if(key==SDL_SCANCODE_PAGEUP){scroll+=4;if(scroll>lineCount-1)scroll=lineCount-1;}
        else if(key==SDL_SCANCODE_PAGEDOWN){scroll-=4;if(scroll<0)scroll=0;}
        else if(key==SDL_SCANCODE_V&&(event->key.keysym.mod&KMOD_CTRL)){
            char *text=SDL_GetClipboardText();if(text){insertText(text);SDL_free(text);}
        }
        return 1;
    }
    return event->type==SDL_KEYUP||event->type==SDL_TEXTEDITING||
        event->type==SDL_MOUSEBUTTONDOWN||event->type==SDL_MOUSEBUTTONUP||
        event->type==SDL_MOUSEMOTION||event->type==SDL_MOUSEWHEEL||
        event->type==SDL_JOYAXISMOTION||event->type==SDL_JOYBUTTONDOWN||event->type==SDL_JOYBUTTONUP;
}
void gameConsoleDraw(void)
{
    rectangle panel;fonthandle previous;int rowHeight,rows,index,y,start,visibleStart;char prompt[CONSOLE_INPUT+4],caret[CONSOLE_INPUT+4];
    if(!opened||!gameIsRunning)return;
    if(!consoleFont)consoleFont=frFontRegister("Arial_12.hff");
    previous=fontMakeCurrent(consoleFont);primModeSet2();
    panel.x0=0;panel.y0=0;panel.x1=MAIN_WindowWidth;panel.y1=MAIN_WindowHeight*2/5;
    primRectSolid2(&panel,colRGBA(7,15,20,242));
    primLine2(0,panel.y1,MAIN_WindowWidth,panel.y1,colRGB(238,175,61));
    rowHeight=fontHeight("M")+4;rows=(panel.y1-32)/rowHeight-1;
    y=panel.y1-rowHeight-10;visibleStart=cursor;
    // Fit the input tail/caret at every resolution without writing over its edge.
    while(visibleStart>0&&fontWidth(input+visibleStart-1)<MAIN_WindowWidth-48)--visibleStart;
    snprintf(prompt,sizeof(prompt),"> %s",input+visibleStart);
    while(fontWidth(prompt)>MAIN_WindowWidth-32&&strlen(prompt)>2)prompt[strlen(prompt)-1]=0;
    fontPrint(16,y,colRGB(238,175,61),prompt);
    snprintf(caret,sizeof(caret),"> %.*s",cursor-visibleStart,input+visibleStart);
    if((SDL_GetTicks()/500)%2==0)fontPrint(16+fontWidth(caret),y,colRGB(238,175,61),"_");
    start=lineCount-scroll;for(index=start-1;index>=0&&rows-->0;--index){
        char line[384];snprintf(line,sizeof(line),"%s",lines[index]);
        while(fontWidth(line)>MAIN_WindowWidth-32&&line[0])line[strlen(line)-1]=0;
        y-=rowHeight;fontPrint(16,y,colRGB(207,220,218),line);
    }
    fontMakeCurrent(previous);
}
