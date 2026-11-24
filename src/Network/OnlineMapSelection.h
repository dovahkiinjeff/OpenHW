#ifndef OPENHW_ONLINE_MAP_SELECTION_H
#define OPENHW_ONLINE_MAP_SELECTION_H
#include "OnlineProtocol.h"
static int ohMapNameEqual(const char *a,const char *b)
{
    unsigned x,y;
    if(!a||!b)return 0;
    do {x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x+='a'-'A';if(y>='A'&&y<='Z')y+='a'-'A';
        if(x!=y)return 0;
    }while(x);
    return 1;
}
static int ohNextMap(const OHMapSummary *maps,int count,const char *current,int direction,int players)
{
    int i,index,step;
    if(!maps||count<=0||count>OH_MAX_MAPS||players<1||players>OH_MAX_PLAYERS)return -1;
    step=direction<0?-1:1;index=step>0?-1:0;
    for(i=0;i<count;i++)if(ohMapNameEqual(maps[i].map,current)){index=i;break;}
    for(i=0;i<count;i++){
        index=(index+step+count)%count;
        if(maps[index].map[0]&&maps[index].maxplayers>=players)return index;
    }
    return -1;
}
#endif
