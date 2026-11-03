#include "../src/Game/BackdropMapPolicy.h"
#include "../src/Game/NisFadePolicy.h"
#include <math.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    char name[64],other[64];int a,b;
    { float fade=0; int i;
      for(i=0;i<59;i++) fade=hwNisFadeAdvance(fade,.995f,.995f/2.0f,1.0f/30.0f);
      CHECK(fade>.9f && fade<.995f);
      fade=hwNisFadeAdvance(fade,.995f,.995f/2.0f,1.0f/30.0f);
      CHECK(fabsf(fade-.995f)<.0001f);
      CHECK(hwNisFadeAdvance(.99f,0,-10.0f,.1f)==0);
      CHECK(hwNisFadeAdvance(.2f,1.0f,5.0f,.2f)==1.0f);
    }
    CHECK(hwBackdropAssetForName("BTG/ez02.btg")==2);
    CHECK(hwBackdropAssetForName("BTG\\EZ05.BTG")==5);
    CHECK(hwBackdropAssetForName("ez08")==8);
    CHECK(hwBackdropAssetForName("BTG/default.btg")==5);
    CHECK(hwBackdropAssetForName("BTG/ez04.btg")==4);
    CHECK(hwBackdropAssetForName("ez03.btg")==1);
    CHECK(hwBackdropAssetForName("other.btg")==0 && hwBackdropAssetForName(NULL)==0);
    CHECK(hwBackdropAssetForName("ez05.btg.bad")==0 && hwBackdropAssetForName("ez55.btg")==0);
    /* History lesson in Mission 11: scripted EZ16 -> EZ12 -> EZ10 -> EZ08
       -> EZ01 (Kharak) -> EZ11. No explicit environment keeps mission sky. */
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez16.btg",1)==16);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez12.btg",1)==12);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez10.btg",1)==10);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez08.btg",1)==8);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez01.btg",1)==1);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez11.btg",1)==11);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/ez01.btg",0)==11);
    CHECK(hwBackdropRenderAsset(1,11,"BTG/unknown.btg",1)==0);
    CHECK(hwBackdropRenderAsset(0,11,"BTG/ez08.btg",0)==8);
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
