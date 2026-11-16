# Update path recovery maintenance — OpenHW 1.1.11

The game version remains **1.1.11**. Launcher and Updater utility revision **1.1.5** corrects upgrade checks and installation guidance; multiplayer game files are unchanged.

- Every published 1.1 / 1.1.1-or-newer starting version has a patch route to 1.1.11.
- OpenHW 1.0 through 1.0.7 requires the refreshed 1.1.1 base in a separate empty folder, followed by updates in that new folder. These older data files cannot safely be upgraded by copying the current game patch over them.
- All eight original 1.0-series patches are preserved on the OpenHW download server with their original sizes and SHA-256 checksums. All feed download links now use the server, including legacy patches.
- The Launcher now shows **New installation required** and a **Get new installer** button for legacy folders, rather than a generic missing-path error. The 1.1 recovery route remains supported.
- Release validation checks every recorded starting version, including explicit new-install recovery boundaries, and rejects broken routes in the supported patch chain.

If an old Launcher cannot update itself because it stops before handing off, [download the current Launcher](https://openhw.tech/downloads/v1.1.11/Launcher.exe) and [Updater](https://openhw.tech/downloads/v1.1.11/Updater.exe) into that installation after closing both utilities. A 1.0-series installation will then explain the required separate install. Your existing installation, saves and settings are retained.

Normal current installations receive the utility maintenance through **Update now**. No game reinstall or multiplayer server restart is required for 1.1-series players.
