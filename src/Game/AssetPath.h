#ifndef HW_ASSET_PATH_H
#define HW_ASSET_PATH_H

#include <stdio.h>

#define FL_Path 256

/* Callers supply the separator as part of the directory, if needed. */
static inline int fileBuildPath(char *path, size_t capacity, const char *directory,
                                const char *name)
{
    if (capacity == 0) return 0;
    int length = snprintf(path, capacity, "%s%s", directory ? directory : "", name);
    if (length < 0 || (size_t)length >= capacity)
    {
        path[0] = '\0';
        return 0;
    }
    return 1;
}

#endif
