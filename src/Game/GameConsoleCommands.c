#include "GameConsole.h"
#include "Globals.h"
#include "Universe.h"
#include "Select.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int token(const char **input,const char *expected)
{
    const char *p=*input;while(isspace((unsigned char)*p))++p;
    while(*expected){if(tolower((unsigned char)*p++)!=tolower((unsigned char)*expected++))return 0;}
    if(*p&&!isspace((unsigned char)*p))return 0;
    while(isspace((unsigned char)*p))++p;*input=p;return 1;
}
int gameConsoleExecute(const char *command,char *reply,size_t capacity)
{
    const char *p=command;int amount=0,i,count=0;
    if(!reply||!capacity)return 0;*reply=0;
    if(!gameIsRunning||!universe.curPlayerPtr){snprintf(reply,capacity,"Start a game before using player commands.");return 0;}
    /* Local mutations must never enter a synchronized network match. */
    if(multiPlayerGame){snprintf(reply,capacity,"Player commands are available in offline games only.");return 0;}
    if(!command){snprintf(reply,capacity,"Enter a command.");return 0;}
    if(token(&p,"player.additem")){
        if(!token(&p,"RU")||!isdigit((unsigned char)*p))goto ruUsage;
        do{int digit=*p++-'0';if(amount>(INT_MAX-digit)/10)goto ruUsage;amount=amount*10+digit;}while(isdigit((unsigned char)*p));
        while(isspace((unsigned char)*p))++p;
        if(*p)goto ruUsage;
        if(universe.curPlayerPtr->resourceUnits>INT_MAX-amount){snprintf(reply,capacity,"RU limit exceeded; nothing changed.");return 0;}
        universe.curPlayerPtr->resourceUnits+=amount;
        snprintf(reply,capacity,"Added %d RU. Total: %d.",amount,universe.curPlayerPtr->resourceUnits);return 1;
    }
    p=command;
    if(token(&p,"player.restorehealth")&&!*p){
        for(i=0;i<selSelected.numShips;i++){
            Ship *ship=selSelected.ShipPtr[i];
            if(!ship||!ship->staticinfo||(ship->flags&SOF_Dead)||ship->health<=0)continue;
            ship->health=ship->staticinfo->maxhealth;++count;
        }
        snprintf(reply,capacity,count?"Restored health for %d selected ship%s.":"No living ships selected.",count,count==1?"":"s");return 1;
    }
    snprintf(reply,capacity,"Unknown command. Use player.additem RU <value> or player.restorehealth.");return 0;
ruUsage:
    snprintf(reply,capacity,"Usage: player.additem RU <whole number from 0 to %d>.",INT_MAX);return 0;
}
