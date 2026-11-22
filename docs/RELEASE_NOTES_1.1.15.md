# OpenHW 1.1.15 — Consolidated full release

**1.1.15 is the new installation and update baseline.** It assembles the current engine, visual changes, integrated HOD editor, launcher, updater and all prior fixes into one full package.

- **New players:** download **OpenHW-Setup.exe** from this GitHub release, run it and select a dedicated game folder. An owned Homeworld Remastered Collection is required for local asset import.
- **Existing players on any earlier release:** choose **Update now** in Launcher, or run the same full installer and select your existing OpenHW folder. The new updater hands off automatically to the complete installer. No intermediate patches or manual updater replacement are required for working supported launchers.
- The installer replaces the program and supplied data files while preserving saves, settings, badges, mods and personal parade configuration. Compatible 1.1-series archives are reused without copying the full archive. Earlier or incompatible game data is rebuilt from your owned Collection; setup requests its location if the saved location is unavailable.
- Launcher and Updater are now utility version **1.1.9**, update protocol **4**. The manual rollback feature remains absent; interrupted file replacement has automatic recovery.
- The previous incremental chain is retired from the active feed. Future patches start from **1.1.15**. Historical releases remain archived.
- Includes the 1.1.14 reflection update, integrated HODEditorJS workshop, named mod creation, ship/weapon/event editing, automatic turret bindings and earlier gameplay/multiplayer fixes.
- Includes the contributor inventory and retained third-party notices. The website has a dedicated [Contributors page](https://openhw.tech/#/contributors).
- The multiplayer simulation identity is unchanged from 1.1.14; the server retains ten-battle capacity.

This reset removes the growing sequence of prerequisite patches and gives new and returning players the same complete baseline. The compiled full installer is hosted on GitHub at the owner's request, with an OpenHW-server fallback. Source is included separately. Retail game archives are not distributed; installation imports them locally from the player's Collection.

Close the game before updating. When running the standalone installer manually, close Launcher and Updater as well. Installation does not launch the game automatically.

Validation: package integrity, current Defender signatures and scans of the exact packages/unpacked contents, replacement/recovery checks and archive-reuse checks. The extended fresh-install benchmark was skipped at the owner's request. No claim is made that a fresh 1.1.15 installation was benchmarked in this release.
