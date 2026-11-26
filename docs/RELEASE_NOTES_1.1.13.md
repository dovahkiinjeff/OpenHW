# OpenHW 1.1.13 — Installer and updater packaging hardening

Choose **Update now** in Launcher. Existing installations update in place; no reinstall is required.

- Replaced encoded PowerShell archive extraction and its execution-policy override with direct use of the Windows system archive utility in the installer, patch, Launcher and Updater bootstraps.
- Launcher and Updater are now utility version 1.1.7. The manual **Previous Version / Roll Back** feature is removed from this release.
- Refreshed the full base installer with the hardened bootstrap and current Launcher/Updater. It remains the compatible 1.1.1 base and follows the complete patch chain to 1.1.13. Existing players must use updates rather than run the full installer over their game folder.
- Added a Windows release security gate that updates Defender signatures and scans exact packages plus recursively unpacked payloads without executing those payloads. A scan failure or detection blocks the build. The gate records package hashes and logs and does not add antivirus exclusions.
- Game binaries and multiplayer simulation identity are unchanged from 1.1.12. The existing multiplayer service remains compatible and retains its 10-battle capacity. Saves, settings, mods and badges are preserved.

Updated Microsoft Defender scans found no threats in the reviewed public packages or the final replacement packages and their unpacked contents. The reported Bearfoos/Wacatac alerts have not been reproduced or tied to an exact file; these checks do not establish that the user's report was a false positive or guarantee agreement from every scanner.

Validation covers native archive extraction, embedded helper verification, HTTPS feed access, paths containing spaces/apostrophes/non-ASCII characters, updater transactions and preservation of player files. Automatic recovery of interrupted or failed updates remains part of update safety. The manual rollback menu is absent.

Compiled downloads are hosted on openhw.tech. GitHub contains cumulative source and source checksums only.
