/* Exercise the production side table/material code with a recording texture
   registry. No game, display, graphics driver, or player file is touched. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned udword,color;typedef int sdword,bool32;typedef size_t trhandle;
#define TRUE 1
#define FALSE 0
#define MAX_MULTIPLAYER_PLAYERS 8
#define OBJ_ShipType 1
#define SOF_Dead 2
#define TR_Invalid ((trhandle)-1)
#define TR_RegistrySize 256
#define MDF_BaseColor 1
#define MDF_StripeColor 2
#define PATH_MAX 512
#define colWhite 0xffffffffu
#define trIndex(h) ((h)&65535)
enum {SHIP_PAINT_BASE,SHIP_PAINT_STRIPE,SHIP_PAINT_TRAIL,SHIP_PAINT_NAV,SHIP_PAINT_RESOURCE,SHIP_PAINT_HYPERSPACE,SHIP_PAINT_COUNT};
typedef struct ShipAppearance {udword mask;color colors[6];} ShipAppearance;
typedef struct Ship {int objtype;unsigned flags;struct {unsigned short shipNumber;}shipID;int colorScheme;} Ship;
typedef struct trcolorinfo {color base,detail;}trcolorinfo;
typedef struct {trcolorinfo textureColor;color trailColors[4];}Scheme;
static Scheme teColorSchemes[8];
static color utyNavLightColor=3,utyResourceBeamColor=4,utyHyperspaceColor=5;
typedef struct materialentry {unsigned flags;int bTexturesRegistered;trhandle *texture;char *textureNameSave;color ambient,diffuse;} materialentry;
typedef struct meshdata {int nLocalMaterials,nPublicMaterials;materialentry *localMaterial;} meshdata;
typedef struct {char *fileName;}Registry;
static Registry registry[256];static int refs[256],nextTexture=2,refreshes;
#define trStructureGet(h) (&registry[trIndex(h)])
static trhandle trTextureRegister(char *path,trcolorinfo *info,void *mesh){int n=nextTexture++;(void)path;(void)info;(void)mesh;assert(n<256);refs[n]=1;return (trhandle)n;}
static void trTextureUnregister(trhandle handle){assert(refs[handle]==1);refs[handle]=0;}
static void trRegistryRefresh(void){++refreshes;}
#include "../src/Game/ShipAppearanceMesh.inc"
typedef Ship SpaceObj;
/* Use a named selection type for the production save extension's bounds checks. */
typedef struct TestSelection {int numShips;Ship **ShipPtr;}TestSelection;
static TestSelection selectedRegistry;
static struct {TestSelection *selection;} SpaceObjRegistry={&selectedRegistry};
static int savedNumbers[100],savedCount,readNumber;
static void SaveInfoNumber(int value){assert(savedCount<100);savedNumbers[savedCount++]=value;}
static int LoadInfoNumber(void){assert(readNumber<savedCount);return savedNumbers[readNumber++];}
static int LoadInfoNumberOptional(int *value){if(readNumber==savedCount)return 0;*value=LoadInfoNumber();return 1;}
static int SpaceObjRegistryGetID(SpaceObj *ship){int i;for(i=0;i<selectedRegistry.numShips;++i)if(selectedRegistry.ShipPtr[i]==ship)return i;return -1;}
#include "../src/Game/ShipAppearanceSave.inc"
int main(void)
{
    Ship a={1,0,{17},0},b={1,0,{18},0},savedB=b;
    ShipAppearance style={3,{0xff123456,0xffabcdef,0xffaa3300,0xff44aa33,0xff33aabb,0xff9944cc}},read;
    trhandle originals[8]={1,1,1,1,1,1,1,1};
    materialentry material={MDF_BaseColor,1,originals,"hull",0xffeeeeee,0xffeeeeee};
    materialentry original=material,*copy;
    meshdata mesh={1,0,&material};
    unsigned i;
    teColorSchemes[0].textureColor.base=0xff112233;teColorSchemes[0].textureColor.detail=0xff334455;registry[1].fileName="hull";
    assert(shipAppearanceApply(&a,&style));assert(!memcmp(&b,&savedB,sizeof(b)));
    assert(shipAppearanceColor(&a,0,123)==style.colors[0]);assert(shipAppearanceColor(&b,0,123)==123);
    shipAppearanceGet(&a,&read);assert(read.mask==3 && read.colors[1]==style.colors[1]);
    paintSetOwner(&b);assert(meshAppearanceMaterials(&mesh,0)==&material);
    assert(paintSurfaceVariant(2)==2);
    paintSetOwner(&a);copy=meshAppearanceMaterials(&mesh,0);assert(copy!=&material && copy->diffuse==style.colors[0]);
    assert(paintSurfaceVariant(2)!=2 && (paintSurfaceVariant(2)&0x1ffu)==2);
    assert(paintMaterialHandle(copy)==copy->texture[0] && paintMaterialHandle(&material)==TR_Invalid);
    assert(!memcmp(&material,&original,sizeof(material)));assert(teColorSchemes[0].textureColor.base==0xff112233);
    assert(copy->texture!=originals && copy->texture[0]!=1);assert(paintOriginalMaterial(copy)==&material);
    assert(meshAppearanceMaterials(&mesh,0)==copy && refreshes==1);
    paintForgetMesh(&mesh);assert(refs[2]==0);
    copy=meshAppearanceMaterials(&mesh,0);assert(copy!=&material && refreshes==2);
    shipAppearanceRetire(&a);assert(refs[3]==0 && !shipAppearanceHas(&a,0));
    style.mask=63;assert(shipAppearanceApply(&b,&style));shipAppearanceGet(&b,&read);
    assert(!memcmp(&style,&read,sizeof(style)));
    b.flags=SOF_Dead;assert(!shipAppearanceApply(&b,&style));b.flags=0;
    style.mask=64;assert(!shipAppearanceApply(&a,&style));
    {
        Ship *objects[2]={&a,&b};ShipAppearance expected;
        selectedRegistry.ShipPtr=objects;selectedRegistry.numShips=2;
        shipAppearanceGet(&b,&expected);shipAppearanceSave();
        shipAppearanceClear();shipAppearanceLoad();shipAppearanceGet(&b,&read);
        assert(readNumber==savedCount && !memcmp(&expected,&read,sizeof(read)));
        assert(!shipAppearanceAt(17,&read));
    }
    shipAppearanceClear();assert(!shipAppearanceAt(18,&read));
    for(i=2;i<(unsigned)nextTexture;++i)assert(refs[i]==0);
    puts("Ship appearance isolation, material reuse, lifetime and all six channels passed");
    return 0;
}
