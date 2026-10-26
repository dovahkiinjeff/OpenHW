/* Render-thread cache of the exact legacy triangle descriptors. Dynamic
   material/paint/engine inputs are checked per material, not per triangle. */
#ifndef MESH_SURFACE_CACHE_BUDGET
#define MESH_SURFACE_CACHE_BUDGET (32u*1024u*1024u)
#endif
#define MESH_SURFACE_CACHE_BUCKETS 257u
typedef struct MeshSurfaceMaterialKey {
    const materialentry *identity;
    color diffuse,ambient;
    udword flags,fullAmbient;
} MeshSurfaceMaterialKey;
typedef struct MeshSurfaceMaterialRef { unsigned short index; MeshSurfaceMaterialKey key; } MeshSurfaceMaterialRef;
typedef struct MeshSurfaceCacheEntry {
    struct MeshSurfaceCacheEntry *bucketNext,*newer,*older;
    const polygonobject *object;
    const polyentry *polygons;
    const materialentry *materials;
    sdword scheme,count;
    float engineBlend,baseColor[3];
    unsigned engineVariant,references;
    size_t bytes;
    HWModernTriangleSurface *surfaces;
    MeshSurfaceMaterialRef *keys;
} MeshSurfaceCacheEntry;
static MeshSurfaceCacheEntry *meshSurfaceBuckets[MESH_SURFACE_CACHE_BUCKETS];
static MeshSurfaceCacheEntry *meshSurfaceNewest,*meshSurfaceOldest;
static size_t meshSurfaceCacheBytes;
static unsigned meshSurfaceCacheEntries;
static unsigned long long meshSurfaceCacheHits,meshSurfaceCacheMisses;
static unsigned char meshSurfaceMaterialSeen[8192];
static unsigned meshSurfaceBucket(const polygonobject *object,const materialentry *materials,sdword scheme)
{ return (unsigned)((((uintptr_t)object>>4)^((uintptr_t)materials>>4)^(unsigned)scheme)%MESH_SURFACE_CACHE_BUCKETS); }
static MeshSurfaceMaterialKey meshSurfaceMaterialKey(materialentry *materials,unsigned index,sdword scheme)
{
    materialentry *material=meshEngineMaterial(&materials[index]);
    MeshSurfaceMaterialKey key;memset(&key,0,sizeof(key));
    key.identity=material;key.diffuse=meshModernMaterialDiffuse(material,scheme);
    key.ambient=material->ambient;key.flags=material->flags;key.fullAmbient=material->nFullAmbient;
    return key;
}
static void meshSurfaceUnlinkAge(MeshSurfaceCacheEntry *entry)
{
    if(entry->newer)entry->newer->older=entry->older;else meshSurfaceNewest=entry->older;
    if(entry->older)entry->older->newer=entry->newer;else meshSurfaceOldest=entry->newer;
}
static void meshSurfaceMakeNewest(MeshSurfaceCacheEntry *entry)
{
    entry->newer=NULL;entry->older=meshSurfaceNewest;
    if(meshSurfaceNewest)meshSurfaceNewest->newer=entry;else meshSurfaceOldest=entry;
    meshSurfaceNewest=entry;
}
static void meshSurfaceRemove(MeshSurfaceCacheEntry *entry)
{
    unsigned bucket=meshSurfaceBucket(entry->object,entry->materials,entry->scheme);
    MeshSurfaceCacheEntry **link=&meshSurfaceBuckets[bucket];
    while(*link && *link!=entry)link=&(*link)->bucketNext;
    if(*link)*link=entry->bucketNext;
    meshSurfaceUnlinkAge(entry);meshSurfaceCacheBytes-=entry->bytes;--meshSurfaceCacheEntries;
    free(entry->surfaces);free(entry);
}
static void meshSurfaceCacheClear(void)
{ while(meshSurfaceOldest)meshSurfaceRemove(meshSurfaceOldest); }
static MeshSurfaceCacheEntry *meshSurfaceCacheFind(const polygonobject *object,materialentry *materials,sdword scheme)
{
    MeshSurfaceCacheEntry *entry;
    for(entry=meshSurfaceBuckets[meshSurfaceBucket(object,materials,scheme)];entry;entry=entry->bucketNext) {
        unsigned i;
        if(entry->object!=object || entry->materials!=materials || entry->scheme!=scheme ||
           entry->polygons!=object->pPolygonList || entry->count!=object->nPolygons ||
           memcmp(&entry->engineBlend,&meshEngineBlend,sizeof(float)))continue;
        for(i=0;i<entry->references;++i) {
            MeshSurfaceMaterialKey key=meshSurfaceMaterialKey(materials,entry->keys[i].index,scheme);
            if(memcmp(&key,&entry->keys[i].key,sizeof(key)))break;
        }
        if(i!=entry->references)continue;
        meshSurfaceUnlinkAge(entry);meshSurfaceMakeNewest(entry);++meshSurfaceCacheHits;return entry;
    }
    ++meshSurfaceCacheMisses;return NULL;
}
static void meshSurfaceCacheStore(const polygonobject *object,materialentry *materials,sdword scheme,
    const HWModernTriangleSurface *surfaces,const float baseColor[3],unsigned engineVariant)
{
    size_t surfaceBytes=(size_t)object->nPolygons*sizeof(*surfaces),bytes;
    unsigned short *indices;unsigned references=0,i,bucket;
    MeshSurfaceCacheEntry *entry,*old;
    if(surfaceBytes>MESH_SURFACE_CACHE_BUDGET/2 || object->nPolygons<=0)return;
    indices=(unsigned short *)malloc((size_t)(object->nPolygons<65536?object->nPolygons:65536)*sizeof(*indices));
    if(!indices)return;
    memset(meshSurfaceMaterialSeen,0,sizeof(meshSurfaceMaterialSeen));
    for(i=0;i<(unsigned)object->nPolygons;++i) {
        unsigned index=object->pPolygonList[i].iMaterial,mask=1u<<(index&7);
        if(!(meshSurfaceMaterialSeen[index>>3]&mask)) {
            meshSurfaceMaterialSeen[index>>3]|=(unsigned char)mask;indices[references++]=(unsigned short)index;
        }
    }
    bytes=sizeof(*entry)+surfaceBytes+(size_t)references*sizeof(MeshSurfaceMaterialRef);
    if(bytes>MESH_SURFACE_CACHE_BUDGET){free(indices);return;}
    bucket=meshSurfaceBucket(object,materials,scheme);
    for(old=meshSurfaceBuckets[bucket];old;old=old->bucketNext)
        if(old->object==object && old->materials==materials && old->scheme==scheme){meshSurfaceRemove(old);break;}
    while(meshSurfaceOldest && (meshSurfaceCacheBytes+bytes>MESH_SURFACE_CACHE_BUDGET || meshSurfaceCacheEntries>=256))
        meshSurfaceRemove(meshSurfaceOldest);
    entry=(MeshSurfaceCacheEntry *)calloc(1,sizeof(*entry));
    if(!entry){free(indices);return;}
    entry->surfaces=(HWModernTriangleSurface *)malloc(bytes-sizeof(*entry));
    if(!entry->surfaces){free(indices);free(entry);return;}
    entry->keys=(MeshSurfaceMaterialRef *)((unsigned char *)entry->surfaces+surfaceBytes);
    memcpy(entry->surfaces,surfaces,surfaceBytes);memcpy(entry->baseColor,baseColor,sizeof(entry->baseColor));
    entry->object=object;entry->polygons=object->pPolygonList;entry->materials=materials;
    entry->scheme=scheme;entry->count=object->nPolygons;entry->engineBlend=meshEngineBlend;
    entry->engineVariant=engineVariant;entry->references=references;entry->bytes=bytes;
    for(i=0;i<references;++i){entry->keys[i].index=indices[i];entry->keys[i].key=meshSurfaceMaterialKey(materials,indices[i],scheme);}
    free(indices);entry->bucketNext=meshSurfaceBuckets[bucket];meshSurfaceBuckets[bucket]=entry;
    meshSurfaceMakeNewest(entry);meshSurfaceCacheBytes+=bytes;++meshSurfaceCacheEntries;
}
