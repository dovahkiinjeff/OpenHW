#ifndef OPENHW_REMASTERED_FX_GRAPH_H
#define OPENHW_REMASTERED_FX_GRAPH_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwRmFxGraph HwRmFxGraph;
enum HwRmFxStyle {HW_RM_FX_COMBO,HW_RM_FX_SPRAY,HW_RM_FX_RING,HW_RM_FX_BEAM,
    HW_RM_FX_LIGHT,HW_RM_FX_TRAIL,HW_RM_FX_LENSFLARE,HW_RM_FX_HYPERSPACE};
enum HwRmFxValueType {HW_RM_FX_BOOL,HW_RM_FX_FLOAT,HW_RM_FX_INT,HW_RM_FX_STRING,
    HW_RM_FX_CURVE_FLOAT,HW_RM_FX_CURVE_VECTOR,HW_RM_FX_CURVE_COLOUR,HW_RM_FX_COLOUR,HW_RM_FX_FRAME_TIMES};
typedef struct HwRmFxProperty {
    const char *name,*text;
    unsigned type,count;
    const float *values;
} HwRmFxProperty;
typedef struct HwRmFxNode {
    const char *name;
    unsigned style,propertyCount;
    const HwRmFxProperty *properties;
} HwRmFxNode;
/* Immutable typed source graph; never a pre-generated flat particle list.
   All borrowed views remain valid until Free. Loading does not enable drawing. */
HwRmFxGraph *hwRmFxGraphRead(const void *bytes,size_t size,char *error,size_t capacity);
void hwRmFxGraphFree(HwRmFxGraph *graph);
unsigned hwRmFxGraphCount(const HwRmFxGraph *graph);
const HwRmFxNode *hwRmFxGraphAt(const HwRmFxGraph *graph,unsigned index);
int hwRmFxGraphFind(const HwRmFxGraph *graph,const char *name);
/* RM event choice list: select one comma token before resource resolution.
   Duplicate tokens retain their weighting; no fan-out or missing-name alias. */
int hwRmFxGraphSelect(const HwRmFxGraph *graph,const char *choices,unsigned random);
const HwRmFxProperty *hwRmFxGraphProperty(const HwRmFxNode *node,const char *name);
/* Curve samples use authored normalized time; no entity-radius multiplication,
   impact-specific corrections, LOD scaling, or component removal occurs here. */
int hwRmFxGraphSample(const HwRmFxProperty *property,float phase,float output[4]);
#ifdef __cplusplus
}
#endif
#endif
