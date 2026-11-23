# Launcher / Updater 1.1.11 — update controls and font handoff repair

The game remains OpenHW 1.1.16. This maintenance update replaces only Launcher and Updater; it does not require another game download for players already on 1.1.16.

- Fixed the main-page update button's `unknown option "-text"` error. The custom Canvas button now updates its own label and callback instead of passing unsupported Button options to Tcl.
- Fixed the three-font `Can't unlink already-existing object: Permission denied` error. The native updater waits for the old Launcher processes before extraction, and each utility now loads fonts from its own versioned runtime rather than overwriting another running utility's fonts.
- Current installations show **You're up to date** and hide the Update button. Checking manually reports **No updates available** when nothing needs installing.
- Refreshed the complete 1.1.15 baseline installer with the corrected utilities. Its game/data payload is unchanged; Launcher still updates the game to 1.1.16.

**Already affected?** In the old Launcher, open **Ship Systems → Check for updates** once. It downloads the corrected updater automatically before handing off; no manual Updater.exe replacement is needed. The old main-page button is repaired when Launcher updates. Close the game before updating.

Validation used real hidden Tk widgets for both available/current states, reproduced the reported three locked-font extraction failures using the released updater, and verified the corrected packaged updater under the same locks. Microsoft Defender passed the utilities and refreshed installer. Player installations were not modified during these checks.
