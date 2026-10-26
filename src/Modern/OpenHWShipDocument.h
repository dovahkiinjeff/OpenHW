#ifndef OPENHW_SHIP_DOCUMENT_H
#define OPENHW_SHIP_DOCUMENT_H
#include "RemasteredVisualPackage.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Compile an editable ship and its referenced subsystem definitions in memory.
   outputGameplay selects the legacy simulation view; otherwise the native rig
   view is returned. Both views have the same authoritative source document. */
int openhwShipDocumentCompile(const void *source,size_t size,
    HwRmPackageReadFile read,HwRmPackageReleaseFile release,void *user,
    int outputGameplay,void **output,size_t *outputSize,char *error,size_t errorSize);
void openhwShipDocumentFree(void *output);
/* Editor save: materialize every subsystem weapon assignment and reject unarmed weapon slots. */
int openhwShipDocumentComplete(const void *source,size_t size,
    HwRmPackageReadFile read,HwRmPackageReleaseFile release,void *user,
    void **output,size_t *outputSize,char *error,size_t errorSize);
int openhwShipDocumentLegacyVisual(const void *source,size_t size);
/* Normalize multiline declarations for editor import without resolving assets. */
int openhwShipDocumentNormalize(const void *source,size_t size,void **output,size_t *outputSize,char *error,size_t errorSize);
#ifdef __cplusplus
}
#endif
#endif
