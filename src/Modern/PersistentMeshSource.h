#ifndef HW_PERSISTENT_MESH_SOURCE_H
#define HW_PERSISTENT_MESH_SOURCE_H
#include <stddef.h>

/* Borrowed immutable model data. No packed/cast dependency on the game's C
   structures: offsets/strides are supplied with offsetof at registration. */
typedef struct HWPersistentMeshSource {
    const void *identity;
    const void *vertices;
    const void *normals;
    const void *polygons;
    unsigned int vertexCount, normalCount, polygonCount;
    unsigned int vertexStride, normalStride, polygonStride;
    unsigned int normalIndexOffset, faceNormalOffset, materialOffset;
    unsigned int indexOffset[3], uvOffset[3];
} HWPersistentMeshSource;

/* Exact inputs of Shader.c::shColourSet0 after shUpdateLighting. Directions
   are already in object space; positions/normals/UVs need not change per ship. */
typedef struct HWLegacyShadeState {
    float base[4];
    float direction[2][4];
    float diffuse[2][4];
    float dock[4]; /* RGB tint, factor */
    float parameters[4]; /* enabled, number of lights, reserved */
} HWLegacyShadeState;

#ifdef __cplusplus
extern "C" {
#endif
void hwglRegisterImmutableMesh(const HWPersistentMeshSource *source);
void hwglForgetImmutableMesh(const void *identity);
int hwglCanDrawImmutableMesh(const void *identity);
/* Returns polygons consumed, or zero for the unchanged immediate fallback. */
unsigned int hwglDrawImmutableMesh(const void *identity, unsigned int firstPolygon,
    unsigned int mode, const HWLegacyShadeState *shade);
#ifdef __cplusplus
}
#endif
#endif
