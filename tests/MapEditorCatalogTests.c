#include <stdio.h>
#include <stdlib.h>
typedef int sdword,bool32;
enum {MME_ASSET_SHIP,MME_ASSET_DERELICT,MME_ASSET_RTX_DUST_VOLUME};
static struct {int kind;} mmeAssets[64];
static struct {int created,deleted;} mmeTracked[8];
static struct {int derelictDropdownOpen;} mme;
static int mmeAssetCount,mmeTrackedCount;
#include "../src/Game/MapEditorCatalog.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    int i;
    mmeAssetCount=56;mmeAssets[0].kind=MME_ASSET_SHIP;
    for(i=1;i<55;++i)mmeAssets[i].kind=MME_ASSET_DERELICT;
    mmeAssets[55].kind=MME_ASSET_RTX_DUST_VOLUME;
    CHECK(mmeAssetRowAt(0)==55 && mmeAssetRowAt(1)==-2);
    CHECK(mmeAssetRowCount()==3 && mmeAssetRowAt(2)==0 && mmeAssetRowAt(3)==-1);
    mme.derelictDropdownOpen=1;
    CHECK(mmeAssetRowCount()==56);
    for(i=0;i<54;++i)CHECK(mmeAssetRowAt(i+2)==i+1);
    CHECK(mmeAssetRowAt(56)==-1);
    mmeTrackedCount=5;
    mmeTracked[1].created=mmeTracked[2].created=mmeTracked[4].created=1;
    mmeTracked[2].deleted=1;mmeTracked[3].deleted=1;
    CHECK(mmeObjectRowCount(1)==2 && mmeObjectRowAt(0,1)==1 && mmeObjectRowAt(1,1)==4);
    CHECK(mmeObjectRowCount(0)==2 && mmeObjectRowAt(0,0)==0 && mmeObjectRowAt(1,0)==3);
    CHECK(mmeObjectRowAt(2,1)==-1 && mmeObjectRowAt(2,0)==-1);
    puts("Derelict dropdown ordering and Scene/Objects separation passed.");return 0;
}
