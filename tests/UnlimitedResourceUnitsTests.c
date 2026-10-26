#include "ResourceUnits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Universe universe;
bool32 singlePlayerGame;
udword opUnlimitedRUs;
static int checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    static Player human,ai;
    char text[32];int i;
    universe.curPlayerPtr=&human;singlePlayerGame=TRUE;
    CHECK(!opUnlimitedRUs && !resourceUnitsUnlimited(&human));
    CHECK(!resourceUnitsCanAfford(&human,1));
    human.resourceUnits=73;
    CHECK(resourceUnitsCanAfford(&human,73));CHECK(!resourceUnitsCanAfford(&human,74));
    CHECK(resourceUnitsSpend(&human,23)==23 && human.resourceUnits==50);
    opUnlimitedRUs=TRUE;
    for(i=0;i<10000;++i) {
        CHECK(resourceUnitsCanAfford(&human,SDWORD_Max));
        CHECK(resourceUnitsSpend(&human,SDWORD_Max)==0 && human.resourceUnits==50);
    }
    CHECK(!strcmp(resourceUnitsFormat(&human,text,sizeof(text)),"UNLIMITED"));
    /* Incomes/saves remain ordinary amounts; disabling resumes that balance. */
    human.resourceUnits+=120;opUnlimitedRUs=FALSE;
    CHECK(human.resourceUnits==170 && !resourceUnitsCanAfford(&human,171));
    CHECK(!strcmp(resourceUnitsFormat(&human,text,sizeof(text)),"170"));
    CHECK(resourceUnitsSpend(&human,170)==170 && human.resourceUnits==0);
    opUnlimitedRUs=TRUE;
    CHECK(resourceUnitsCanAfford(&human,8000));CHECK(resourceUnitsSpend(&human,8000)==0 && human.resourceUnits==0);
    ai.resourceUnits=5;
    CHECK(!resourceUnitsUnlimited(&ai) && !resourceUnitsCanAfford(&ai,6));
    CHECK(resourceUnitsSpend(&ai,3)==3 && ai.resourceUnits==2);
    CHECK(!resourceUnitsUnlimited(NULL));
    singlePlayerGame=FALSE;
    CHECK(!resourceUnitsUnlimited(&human) && !resourceUnitsCanAfford(&human,1));
    human.resourceUnits=25;CHECK(resourceUnitsSpend(&human,25)==25 && human.resourceUnits==0);
    CHECK(!strcmp(resourceUnitsFormat(&human,text,sizeof(text)),"0"));
    printf("Unlimited RU policy passed: %d checks; real balance, AI and multiplayer preserved\n",checks);
    return 0;
}
