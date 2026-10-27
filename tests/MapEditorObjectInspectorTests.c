#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef int bool32,sdword;
typedef float real32;
enum {FALSE,TRUE,OBJ_DerelictType,OBJ_AsteroidType,OBJ_DustType,OBJ_GasType,OBJ_NebulaType,OBJ_ShipType};
typedef struct {float x,y,z;} vector;
typedef struct SpaceObjRot {vector position;float scale;} SpaceObjRot;
typedef struct MMETrackedObject {SpaceObjRot *object;int objtype,dirty;float rotationDegrees[3],scale;} MMETrackedObject;
static int stops,rotations,updates;
static vector *mmeObjectPosition(SpaceObjRot *o){return &o->position;}
static void mmeStopObjectMotion(SpaceObjRot *o){(void)o;++stops;}
static void univUpdateObjRotInfo(SpaceObjRot *o){(void)o;++updates;}
static void mmeRotateTrack(MMETrackedObject *t,int axis,float degrees){t->rotationDegrees[axis]+=degrees;++rotations;}
static float mmeObjectScale(SpaceObjRot *o){return o->scale;}
static void mmeSetObjectScale(SpaceObjRot *o,float value){o->scale=value;}
static float mmeClampF(float value,float low,float high){return value<low?low:value>high?high:value;}
static void mmeStatus(const char *s){(void)s;}
#include "../src/Game/MapEditorObjectInspector.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define NEAR(a,b) CHECK(fabsf((a)-(b))<.0001f)
int main(void)
{
    int row;SpaceObjRot object={{10,20,30},1};
    MMETrackedObject t={0};t.object=&object;t.objtype=OBJ_DerelictType;
    for(row=0;row<3;++row)
    {
        object.position=(vector){10,20,30};
        CHECK(mmeAdjustObjectInspector(&t,row,1,1,1));
        NEAR(object.position.x,10+(row==0?100:0));
        NEAR(object.position.y,20+(row==1?100:0));
        NEAR(object.position.z,30+(row==2?100:0));
        CHECK(mmeAdjustObjectInspector(&t,row,-1,.1f,10));
        NEAR(object.position.x,10);NEAR(object.position.y,20);NEAR(object.position.z,30);
    }
    for(row=3;row<6;++row)
    {
        CHECK(mmeAdjustObjectInspector(&t,row,1,1,1));
        NEAR(t.rotationDegrees[row-3],5);
        CHECK(mmeAdjustObjectInspector(&t,row,-1,.1f,1));
        NEAR(t.rotationDegrees[row-3],4.5f);
        NEAR(object.position.x,10);NEAR(object.position.y,20);NEAR(object.position.z,30);
    }
    CHECK(mmeAdjustObjectInspector(&t,6,1,1,1));NEAR(object.scale,1.1f);NEAR(t.scale,1.1f);
    object.scale=.05f;CHECK(mmeAdjustObjectInspector(&t,6,-1,1,1));NEAR(object.scale,.05f);
    object.scale=50;CHECK(mmeAdjustObjectInspector(&t,6,1,1,1));NEAR(object.scale,50);
    for(row=7;row<14;++row)CHECK(!mmeAdjustObjectInspector(&t,row,1,1,1));
    CHECK(!mmeAdjustObjectInspector(&t,-1,1,1,1));
    NEAR(object.position.x,10);NEAR(object.position.y,20);NEAR(object.position.z,30);
    CHECK(stops==6 && updates==6 && rotations==6 && t.dirty);
    t.objtype=OBJ_ShipType;CHECK(!mmeAdjustObjectInspector(&t,6,1,1,1));
    puts("Object inspector: independent XYZ, rotations, derelict scale and read-only rows passed.");return 0;
}
