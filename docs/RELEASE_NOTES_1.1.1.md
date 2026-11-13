# OpenHW 1.1.1 - Diagnostics and Launcher fixes

Requires the fresh OpenHW 1.1 installation. This is a small patch over 1.1; no reinstall or old 1.0 patches are required.

Close OpenHW and Launcher, place **OpenHW-Patch-1.1.1.exe** beside OpenHW.exe, and run it. It installs the corrected Launcher automatically. Saves, settings, mods and authored missions are preserved. Game rendering and multiplayer simulation binaries are unchanged.

## Changes

- Diagnostics opens a separate menu with saved names. Use your Homeworld/modding-community alias and add a comment.
- Saving the name creates a persistent private server folder. Later saves update that same folder; this is separate from campaign save files.
- Upload all new logs sends all previously unuploaded logs and crash dumps, including nested log folders. Large files are split into bounded batches.
- Uploads are marked complete only after server confirmation. Changed logs are eligible again; failed batches can be retried without resending confirmed batches.
- Diagnostics opens even while the main Launcher remains open.
- Restores a missing HTTPS support file that caused Check for updates to fail for both the OpenHW and GitHub feeds. Certificate and hostname verification stay enabled.
- Update-check failures now clear the busy state and show a retryable status message, without an error popup trapping Launcher input.

## Updater download

If your existing Updater reports a missing-file error, download the **Updater.exe** attached here and replace the old one with the game and utilities closed. It can then fetch and install 1.1.1. The utility revision is 1.1.2; the game patch version is 1.1.1. GitHub remains the fallback for openhw.tech.

The complete cumulative source is included separately. Diagnostics reports are private; no logs upload automatically during normal play. Logs can contain local file paths.

## Validation

Both packaged utilities reached both live HTTPS feeds. An isolated 1.1-to-1.1.1 transactional installation preserved game binaries, settings and a saved-game fixture. Upload tests cover persistent folders, more than 200 logs, chunks, changed files, authentication and interrupted-batch recovery. Interactive gameplay and the patch installer window remain for player verification.
