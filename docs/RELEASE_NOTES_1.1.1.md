# OpenHW 1.1.1 - Required updater recovery and Diagnostics

**Required for OpenHW 1.1. Available now.** This utility update needs a one-time manual replacement of Updater.exe. No full game reinstall is required.

## Install once

1. Close OpenHW and Launcher.exe.
2. Download **[Updater.exe](https://openhw.tech/downloads/v1.1.1/Updater.exe)** ([GitHub fallback](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.1.1/Updater.exe)).
3. Put it beside OpenHW.exe, replacing your existing Updater.exe.
4. **Run Updater.exe once.** It installs the bundled, verified 1.1.1 repair and opens the updated Launcher automatically.

Requires the fresh OpenHW 1.1 base. Saves, settings and game files are preserved. Do not use the old 1.0 patch chain.

## Future updates inside Launcher

The bottom ticker checks for updates on startup and every five minutes. **Update now** appears when a game or Launcher update is available. Clicking it downloads and verifies the update, safely closes the Launcher, installs through the update window, and reopens Launcher afterward. Launcher and Updater can update themselves.

**openhw.tech is the primary update source; GitHub is the fallback.** Automatic checks notify you; installation starts when you choose Update now.

## Diagnostics

- Diagnostics opens a separate menu while Launcher remains open.
- Save a name using your **Homeworld or modding-community alias**, and add a comment. Saving creates your persistent private server folder.
- **Upload all new logs** sends previously unuploaded logs and crash dumps in bounded batches. Changed logs are eligible again, and interrupted batches can be retried.
- Upload history is recorded only after server confirmation. Normal play does not automatically upload anything. Logs can contain local file paths.

## Reliability

Fixes the missing HTTPS support file that caused update-feed connection errors and prevents update-check failures from locking Launcher input. After the one-time repair, future updates no longer require manually leaving Launcher to run Updater.exe.

The patch changes utilities; rendering and multiplayer game binaries remain unchanged. The recovery includes verified patch/Launcher files rather than a full game extraction. Complete cumulative source and SHA-256 checksums are provided separately.
