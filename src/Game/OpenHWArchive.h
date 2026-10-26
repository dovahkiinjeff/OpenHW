#ifndef OPENHW_ARCHIVE_H
#define OPENHW_ARCHIVE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Immutable memory-mapped OBIG v1. Individual resources remain <2 GiB;
   archive offsets and total size are 64-bit. Views live until process exit. */
int openhwArchiveMount(const wchar_t *path, char *error, size_t capacity);
/* Optional locally built presentation update. Its resources precede the base
   archive; existing loose files and enabled mods retain their priority.
   Count/Name continue to enumerate the immutable base import. */
int openhwArchiveMountPresentation(const wchar_t *path,char *error,size_t capacity);
/* Shutdown/test teardown only, after every borrowed asset view is released. */
void openhwArchiveUnmount(void);
int openhwArchiveMounted(void);
void openhwArchiveSetLooseRoot(const char *root);
const void *openhwArchiveView(const char *relative, int *size);
unsigned int openhwArchiveCount(void);
const char *openhwArchiveName(unsigned int index);
int openhwModOverride(const char *relative, char *path, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
