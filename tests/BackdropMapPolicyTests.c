#include "../src/Game/BackdropMapPolicy.h"
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    char name[64],other[64];int a,b;
    CHECK(hwBackdropAssetForName("BTG/ez02.btg")==2);
    CHECK(hwBackdropAssetForName("BTG\\EZ05.BTG")==5);
    CHECK(hwBackdropAssetForName("ez08")==8);
    CHECK(hwBackdropAssetForName("BTG/default.btg")==5);
    CHECK(hwBackdropAssetForName("BTG/ez04.btg")==4);
    CHECK(hwBackdropAssetForName("ez03.btg")==1);
    CHECK(hwBackdropAssetForName("other.btg")==0 && hwBackdropAssetForName(NULL)==0);
    CHECK(hwBackdropAssetForName("ez05.btg.bad")==0 && hwBackdropAssetForName("ez55.btg")==0);
    CHECK(hwBackdropContext(1,7,"mp-map","BTG/ez02.btg")==7);
    CHECK(hwBackdropContext(1,0,"mp-map","BTG/ez02.btg")==0);
    a=hwBackdropContext(0,7,"Multiplayer/MapA","BTG/ez02.btg");
    b=hwBackdropContext(0,7,"Multiplayer/MapB","BTG/ez02.btg");
    CHECK(a>=0x40000000 && b>=0x40000000 && a!=b);
    CHECK(a==hwBackdropContext(0,1,"multiplayer\\mapa","btg/ez02.btg"));
    hwBackdropLightingName(name,sizeof(name),a);hwBackdropLightingName(other,sizeof(other),b);
    CHECK(strncmp(name,"Multiplayer-",12)==0 && strcmp(name,other)!=0);
    hwBackdropLightingName(name,sizeof(name),7);CHECK(strcmp(name,"Mission07.rtxlight")==0);
    CHECK(hwBackdropContext(0,7,"mp-map","custom.btg")>=0x40000000);
    CHECK(hwBackdropContext(0,7,"","")==0);
    hwBackdropMapName(name,sizeof(name),a);CHECK(strncmp(name,"Multiplayer-",12)==0 && strstr(name,".rtxmap"));
    puts("Backdrop map selection and per-map lighting isolation passed.");return 0;
}
