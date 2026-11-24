#include "../src/Network/OnlineMapSelection.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 OHMapSummary maps[8]={{"Small%d",2,2,0},{"Large%d",2,8,0},{"Medium%d",2,4,0},{"Unused%d",1,8,0}};
 CHECK(ohNextMap(maps,3,"Small%d",1,2)==1);
 CHECK(ohNextMap(maps,3,"Medium%d",1,2)==0);
 CHECK(ohNextMap(maps,3,"Small%d",-1,2)==2);
 CHECK(ohNextMap(maps,3,"small%d",1,4)==1);
 CHECK(ohNextMap(maps,3,"Medium%d",1,4)==1);
 CHECK(ohNextMap(maps,3,"Large%d",1,8)==1);
 CHECK(ohNextMap(maps,3,"Unknown%d",1,2)==0);
 CHECK(ohNextMap(maps,3,"Unknown%d",-1,2)==2);
 CHECK(ohNextMap(maps,1,"Small%d",1,4)==-1);
 CHECK(ohNextMap(maps,0,"Small%d",1,2)==-1);
 CHECK(ohNextMap(NULL,3,"Small%d",1,2)==-1);
 puts("Actual-count map selection, wrapping, canonical names and lobby-capacity filtering passed");return 0;
}
