#ifndef HW_MODERN_WINDOWS_INSTALL_H
#define HW_MODERN_WINDOWS_INSTALL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Resolves a legitimate Homeworld installation, configures the external BIG
   data root, the portable loose-file override root, and the per-user settings
   directory.  Returns zero when the user cancels or validation fails. */
int hwWindowsInstallPrepare(void);
/* Opens the player-approved report window supplied by the portable launcher. */
int hwWindowsDiagnosticsReportOpen(void);
/* Opens the Launcher's authored owned-data browser; no native file dialog. */

#ifdef __cplusplus
}
#endif

#endif
