#include "Mesh.h"
#include "ModernGraphics.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks, failAllocation;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static materialentry *engineOverride;
static color palette;
static float meshEngineBlend;
static materialentry *meshEngineMaterial(materialentry *material) { return engineOverride ? engineOverride : material; }
static color meshModernMaterialDiffuse(const materialentry *material,sdword scheme)
{ return material->diffuse ^ palette ^ (color)scheme; }
static void *cacheMalloc(size_t bytes) { return failAllocation ? NULL : malloc(bytes); }
static void *cacheCalloc(size_t count,size_t bytes) { return failAllocation ? NULL : calloc(count,bytes); }
#define malloc cacheMalloc
#define calloc cacheCalloc
#define MESH_SURFACE_CACHE_BUDGET (128u*1024u)
#include "../src/Game/MeshSurfaceCache.inc"
#undef malloc
#undef calloc
int main(void)
{
    materialentry materials[3]={{0}},alternate={0};
    polyentry polygons[2]={{0}},replacement[2]={{0}};
    polygonobject object={0},objects[300];
    HWModernTriangleSurface surfaces[2],original[2];
    MeshSurfaceCacheEntry *hit;
    float base[3]={0.25f,0.75f,0.125f};
    unsigned i;
    memset(surfaces,0x39,sizeof(surfaces));memcpy(original,surfaces,sizeof(original));
    polygons[1].iMaterial=1;object.nPolygons=2;object.pPolygonList=polygons;
#define STORE() meshSurfaceCacheStore(&object,materials,0,surfaces,base,0x100)
#define FIND() meshSurfaceCacheFind(&object,materials,0)
#define INVALIDATE(statement) do { STORE();CHECK(FIND()!=NULL);statement;CHECK(FIND()==NULL); } while(0)
    CHECK(FIND()==NULL);STORE();hit=FIND();CHECK(hit!=NULL);
    CHECK(hit->surfaces!=surfaces);CHECK(!memcmp(hit->surfaces,original,sizeof(original)));
    CHECK(!memcmp(hit->baseColor,base,sizeof(base)));CHECK(hit->engineVariant==0x100);
    memset(surfaces,0,sizeof(surfaces));CHECK(!memcmp(hit->surfaces,original,sizeof(original)));
    materials[2].diffuse=123;CHECK(FIND()==hit); /* Unused material does not invalidate. */
    INVALIDATE(materials[0].diffuse=42);
    INVALIDATE(materials[1].ambient=87);
    INVALIDATE(materials[0].flags=MDF_SelfIllum);
    INVALIDATE(materials[1].nFullAmbient=4);
    INVALIDATE(palette=0xff00);
    INVALIDATE(engineOverride=&alternate);
    INVALIDATE(engineOverride=NULL);
    INVALIDATE(meshEngineBlend=0.5f);
    INVALIDATE(object.pPolygonList=replacement);
    INVALIDATE(object.nPolygons=1);
    STORE();CHECK(meshSurfaceCacheFind(&object,materials,1)==NULL);
    meshSurfaceCacheStore(&object,materials,1,surfaces,base,0);
    CHECK(meshSurfaceCacheFind(&object,materials,1)!=NULL);CHECK(FIND()!=NULL);
    meshSurfaceCacheClear();CHECK(meshSurfaceCacheBytes==0);CHECK(meshSurfaceCacheEntries==0);
    failAllocation=1;STORE();CHECK(FIND()==NULL);CHECK(meshSurfaceCacheBytes==0);failAllocation=0;
    memset(objects,0,sizeof(objects));
    for(i=0;i<300;++i){objects[i]=object;meshSurfaceCacheStore(&objects[i],materials,0,surfaces,base,0);}
    CHECK(meshSurfaceCacheBytes<=MESH_SURFACE_CACHE_BUDGET);CHECK(meshSurfaceCacheEntries<=256);
    CHECK(meshSurfaceCacheFind(&objects[0],materials,0)==NULL);
    CHECK(meshSurfaceCacheFind(&objects[299],materials,0)!=NULL);
    /* Touch an old entry, then force eviction: recently used entries survive. */
    CHECK(meshSurfaceCacheFind(&objects[100],materials,0)!=NULL);
    STORE();CHECK(meshSurfaceCacheFind(&objects[100],materials,0)!=NULL);
    meshSurfaceCacheClear();CHECK(meshSurfaceCacheBytes==0);CHECK(meshSurfaceOldest==NULL);
    CHECK(meshSurfaceNewest==NULL);CHECK(meshSurfaceCacheEntries==0);
    for(i=0;i<MESH_SURFACE_CACHE_BUCKETS;++i)CHECK(meshSurfaceBuckets[i]==NULL);
    printf("Mesh surface cache: %d checks passed\n",checks);return 0;
}
