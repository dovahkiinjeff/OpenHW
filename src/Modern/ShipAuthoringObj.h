#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define HW_OBJ_MATERIALS 32
typedef struct HwAuthoringVertex {float position[3],normal[3],uv[2];unsigned material;} HwAuthoringVertex;
typedef struct HwAuthoringObj {
 unsigned vertexCount,materialCount;
 HwAuthoringVertex *vertices;char materials[HW_OBJ_MATERIALS][64];
} HwAuthoringObj;
/* Preview/import only. Bounded OBJ geometry, positive/negative indices,
   v/vt/vn and fan triangulation. Never executes MTL/scripts or opens files. */
int hwAuthoringObjRead(const void *data,size_t size,HwAuthoringObj *out,char *error,size_t errorSize);
void hwAuthoringObjFree(HwAuthoringObj *obj);
#ifdef __cplusplus
}
#endif
