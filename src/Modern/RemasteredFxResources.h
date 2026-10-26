#ifndef OPENHW_REMASTERED_FX_RESOURCES_H
#define OPENHW_REMASTERED_FX_RESOURCES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmFxResources HwRmFxResources;
typedef struct HwRmFxBitmapFrame {const char *image;unsigned row,column,rows,columns;} HwRmFxBitmapFrame;
typedef struct HwRmFxBitmap {const char *name;unsigned frameCount,start,loops;float fps;const HwRmFxBitmapFrame *frames;} HwRmFxBitmap;
typedef struct HwRmFxMeshes {const char *name;unsigned count;const char *const *models;} HwRmFxMeshes;
typedef struct HwRmFxHotspot {const char *image;float offset,scale,rotation,colour[4];} HwRmFxHotspot;
typedef struct HwRmFxFlare {const char *name;unsigned count;const HwRmFxHotspot *hotspots;float fade[4],washout[4],strength;} HwRmFxFlare;
HwRmFxResources *hwRmFxResourcesRead(const void *bytes,size_t size,char *error,size_t capacity);
void hwRmFxResourcesFree(HwRmFxResources *resources);
unsigned hwRmFxBitmapCount(const HwRmFxResources *resources);
const HwRmFxBitmap *hwRmFxBitmapAt(const HwRmFxResources *resources,unsigned index);
const HwRmFxBitmap *hwRmFxBitmapFind(const HwRmFxResources *resources,const char *sourceName);
unsigned hwRmFxMeshCount(const HwRmFxResources *resources);
const HwRmFxMeshes *hwRmFxMeshAt(const HwRmFxResources *resources,unsigned index);
const HwRmFxMeshes *hwRmFxMeshFind(const HwRmFxResources *resources,const char *sourceName);
const HwRmFxFlare *hwRmFxFlareFind(const HwRmFxResources *resources,const char *sourceName);
const HwRmFxBitmapFrame *hwRmFxBitmapSample(const HwRmFxBitmap *bitmap,float age,
    unsigned random,int randomBitmap,float rateScale);
#ifdef __cplusplus
}
#endif
#endif
