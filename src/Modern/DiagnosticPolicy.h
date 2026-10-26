#ifndef HW_DIAGNOSTIC_POLICY_H
#define HW_DIAGNOSTIC_POLICY_H
/* A product build choice, independent of optimization and assertions.
   Disabled calls do not evaluate their arguments or retain format strings. */
#ifndef HW_ENABLE_DIAGNOSTICS
#define HW_ENABLE_DIAGNOSTICS 1
#endif
#if HW_ENABLE_DIAGNOSTICS
#include <stdio.h>
#include <wchar.h>
#define HW_DIAG_FPRINTF(...) fprintf(__VA_ARGS__)
#define HW_DIAG_FWPRINTF(...) fwprintf(__VA_ARGS__)
#define HW_DIAG_PRINTF(...) printf(__VA_ARGS__)
#define HW_DIAG_WPRINTF(...) wprintf(__VA_ARGS__)
#define HW_DIAG_PUTS(...) puts(__VA_ARGS__)
#define HW_DIAG_DEBUGSTRING(...) OutputDebugString(__VA_ARGS__)
#define HW_DIAG_DEBUGSTRINGA(...) OutputDebugStringA(__VA_ARGS__)
#define HW_DIAG_DEBUGSTRINGW(...) OutputDebugStringW(__VA_ARGS__)
#else
#define HW_DIAG_FPRINTF(...) ((void)0)
#define HW_DIAG_FWPRINTF(...) ((void)0)
#define HW_DIAG_PRINTF(...) ((void)0)
#define HW_DIAG_WPRINTF(...) ((void)0)
#define HW_DIAG_PUTS(...) ((void)0)
#define HW_DIAG_DEBUGSTRING(...) ((void)0)
#define HW_DIAG_DEBUGSTRINGA(...) ((void)0)
#define HW_DIAG_DEBUGSTRINGW(...) ((void)0)
#endif
#endif
