#ifndef HOMEWORLD_MODERN_WINDOWS_DIAGNOSTICS_H
#define HOMEWORLD_MODERN_WINDOWS_DIAGNOSTICS_H
#include "DiagnosticPolicy.h"

#ifdef __cplusplus
extern "C" {
#endif

void hwWindowsDiagnosticsInstall(void);
#if HW_ENABLE_DIAGNOSTICS
int hwDiagnosticsDisabled(void);
#else
static inline int hwDiagnosticsDisabled(void) { return 1; }
#endif

#ifdef __cplusplus
}
#endif

#endif
