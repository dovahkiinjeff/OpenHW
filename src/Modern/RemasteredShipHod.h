#ifndef HW_REMASTERED_SHIP_HOD_H
#define HW_REMASTERED_SHIP_HOD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct HwHodVertex { float position[3], normal[3], uv[2], badgeUv[2]; } HwHodVertex;
typedef struct HwHodTriangle { unsigned short vertex[3], material; } HwHodTriangle;
typedef struct HwHodShip {
    unsigned int vertexCount, triangleCount;
    HwHodVertex *vertices;
    HwHodTriangle *triangles;
} HwHodShip;
/* Strict interceptor-only HOD2 reader. No output files, no global overrides.
   Caller releases successful results with hwHodShipFree. */
int hwHodShipRead(const void *data, size_t size, HwHodShip *result,
                  char *error, size_t errorSize);
void hwHodShipFree(HwHodShip *ship);

/* Fleet scene reader: retains each LOD/part and the source joint hierarchy.
   This does NOT enable a replacement or alter gameplay collision/hardpoints.
   Positions/transforms are in RM coordinates, with no interceptor-specific scale.
   Caller must validate material, hardpoint and animation bindings before use. */
typedef struct HwRmHodScene HwRmHodScene;
typedef struct HwRmHodJoint {
    const char *name, *parent;
    float position[3], rotation[3], gimbalLimits[3];
} HwRmHodJoint;
typedef struct HwRmHodParameter {
    const char *name;
    unsigned int type, texture; /* type 5 = texture index; type 4 = float4 */
    float value[4];
} HwRmHodParameter;
typedef struct HwRmHodMaterial {
    const char *name, *shader;
    unsigned int parameterCount;
    const HwRmHodParameter *parameters;
} HwRmHodMaterial;
typedef struct HwRmHodMip {
    unsigned int width, height;
    size_t offset, size; /* within the decoded texture pool */
} HwRmHodMip;
typedef struct HwRmHodTexture {
    const char *name, *format; /* DXT1/DXT3/DXT5/8888; preserve source encoding */
    unsigned int mipCount;
    const HwRmHodMip *mips;
} HwRmHodTexture;
typedef struct HwRmHodPart {
    unsigned int material, vertexMask, vertexCount, indexCount;
    const HwHodVertex *vertices;
    const unsigned short *indices;
    /* Authored packed RGBA (R in bits 24..31, A in 0..7). Kept separately
       so the proven vertex ABI/layout and textured materials stay unchanged. */
    const unsigned int *vertexColors;
} HwRmHodPart;
typedef struct HwRmHodMesh {
    const char *name, *parent;
    unsigned int lod, partCount;
    const HwRmHodPart *parts;
} HwRmHodMesh;
/* Read-only authored docking metadata in RM coordinates. Flags are preserved,
   not interpreted as HW1 gameplay policy. These paths do not enable docking. */
typedef struct HwRmHodDockPoint {
    float position[3], rotation[9], tolerance, maxSpeed;
    unsigned int flags[2];
} HwRmHodDockPoint;
typedef struct HwRmHodDockPath {
    const char *name, *parent, *compatibleShips, *linkedPath;
    unsigned int flags[5], reserved, pointCount;
    const HwRmHodDockPoint *points;
} HwRmHodDockPath;
unsigned int hwRmHodSceneDockPathCount(const HwRmHodScene *scene);
const HwRmHodDockPath *hwRmHodSceneDockPath(const HwRmHodScene *scene, unsigned int index);
int hwRmHodSceneRead(const void *data, size_t size, HwRmHodScene **result,
                     char *error, size_t errorSize);
void hwRmHodSceneFree(HwRmHodScene *scene);
unsigned int hwRmHodSceneJointCount(const HwRmHodScene *scene);
unsigned int hwRmHodSceneMeshCount(const HwRmHodScene *scene);
unsigned int hwRmHodSceneMaterialCount(const HwRmHodScene *scene);
const HwRmHodJoint *hwRmHodSceneJoint(const HwRmHodScene *scene, unsigned int index);
const HwRmHodMesh *hwRmHodSceneMesh(const HwRmHodScene *scene, unsigned int index);
const HwRmHodMaterial *hwRmHodSceneMaterial(const HwRmHodScene *scene, unsigned int index);
unsigned int hwRmHodSceneTextureCount(const HwRmHodScene *scene);
const HwRmHodTexture *hwRmHodSceneTexture(const HwRmHodScene *scene, unsigned int index);
/* Explicit, lazy texture-pool decompression. Geometry audits don't allocate the
   image pool. Borrowed bytes remain owned by scene until free; no archive writes. */
int hwRmHodSceneTextureData(HwRmHodScene *scene, const unsigned char **data,
    size_t *size, char *error, size_t errorSize);
/* Resolve the original material parameter directly to its embedded mip.
   No generated labels, files, resizing or channel conversion. Borrowed output
   remains valid until scene free. DXT payloads stay compressed for GPU upload. */
typedef struct HwRmHodTextureView {
    const char *name, *format;
    unsigned int width, height;
    const unsigned char *bytes;
    size_t size;
} HwRmHodTextureView;
int hwRmHodMaterialTexture(HwRmHodScene *scene,unsigned int material,
    const char *parameter,unsigned int mip,HwRmHodTextureView *view,
    char *error,size_t errorSize);
/* Non-render SCAR/collision tails are NOT visual LODs. This reader bounds them
   but does not interpret them; they must not replace HW1 gameplay collision. */
size_t hwRmHodSceneAuxiliaryVertexBytes(const HwRmHodScene *scene);
size_t hwRmHodSceneAuxiliaryIndexBytes(const HwRmHodScene *scene);
#ifdef __cplusplus
}
#endif
#endif
