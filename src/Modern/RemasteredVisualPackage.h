#ifndef HW_REMASTERED_VISUAL_PACKAGE_H
#define HW_REMASTERED_VISUAL_PACKAGE_H
#include "RemasteredVisualAsset.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Inspect the explicit gameplay member before static ship initialization.
   Returns 1 only for an OpenHW v1 definition with the requested identity. */
int hwRmShipDefinitionGameplay(const void *manifest, size_t size, const char *type,
    char *relative, size_t capacity);
typedef struct HwRmVisualPackage HwRmVisualPackage;
/* Read-only caller-owned IO. Successful reads are released after parsing;
   neither a Lua VM nor gameplay ship/stat imports are involved. Separate R1,
   P1 and qualified Derelict_<type> headers prevent cross-scope activation.
   The legacy intact Scaffold header remains compatible. Derelicts are rigid
   visual-only hulls (no MAD/markers/attachments or ship weapon configuration). */
typedef int (*HwRmPackageReadFile)(void *user,const char *relative,
    const void **bytes,size_t *size);
typedef void (*HwRmPackageReleaseFile)(void *user,const void *bytes);
typedef int (*HwRmPackageConfigure)(void *user,HwRmVisualAsset *asset,
    const char *classicType,char *error,size_t errorSize);
HwRmVisualPackage *hwRmVisualPackageRead(const char *classicType,
    const void *manifest,size_t size,HwRmPackageReadFile read,
    HwRmPackageReleaseFile release,HwRmPackageConfigure configure,void *user,
    char *error,size_t errorSize);
void hwRmVisualPackageFree(HwRmVisualPackage *package);
float hwRmVisualPackageScale(const HwRmVisualPackage *package);
const char *hwRmVisualPackageMaterial(const HwRmVisualPackage *package,
    unsigned int component,unsigned int material);
/* Transfer ownership to the mesh registry only after successful construction. */
HwRmVisualAsset *hwRmVisualPackageTakeAsset(HwRmVisualPackage *package);
#ifdef __cplusplus
}
#endif
#endif
