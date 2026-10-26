#ifndef HW_NATIVE_VISUAL_EVENTS_H
#define HW_NATIVE_VISUAL_EVENTS_H
#include "RemasteredVisualAsset.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit HW1 R1 event contracts, audited against both installed games.
   This configures visual clips only; gameplay timing/states remain in HW1. */
int meshNativeVisualBindClassicEvents(HwRmVisualAsset *asset,
    const char *classicType, char *error, size_t errorSize);
/* Exact audited attachment aliases. Unsupported contracts remain unbound;
   loader must not activate a moving hull with an incomplete gun mapping. */
int meshNativeVisualBindClassicGuns(HwRmVisualAsset *asset,
    const char *classicType, char *error, size_t errorSize);
#ifdef __cplusplus
}
#endif
#endif
