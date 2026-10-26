#ifndef HW_REMASTERED_SCENE_TYPES_H
#define HW_REMASTERED_SCENE_TYPES_H
#include <stddef.h>

/* Translation boundary, deliberately independent of game/GL struct layouts.
   Materials have ten local samplers and 128 tightly packed float4 parameters.
   Source positions/normals remain object-space for animated HOD hierarchy. */
#define HW_BACKGROUND_TEXTURES 10
#define HW_BACKGROUND_PARAMETERS 128
/* Converted scenes retain full-resolution RGBA planet layers. Missions 3
   and 16 exceed 256 MiB in aggregate, while individual extents stay bounded. */
#define HW_RM_SCENE_MAX_BYTES (512u * 1024u * 1024u)
typedef struct HWBackgroundVertex
{
    float position[3], normal[3], tangent[3], binormal[3];
    float color[4], uv0[2], uv1[2];
} HWBackgroundVertex;

#ifdef __cplusplus
extern "C" {
#endif
unsigned int hwModernGraphicsCreateBackgroundProgram(const void *bytecode, size_t size);
int hwModernGraphicsRenderBackgroundFlares(unsigned int texture, const float *records,
    unsigned int count, float fade);
int hwModernGraphicsRenderBackgroundMesh(unsigned int program, const unsigned int *textures,
    const HWBackgroundVertex *vertices, unsigned int count, const float *parameters,
    const float *objectToWorld, unsigned int flags, float atmosphereExpansion);
#ifdef __cplusplus
}
#endif
#endif
