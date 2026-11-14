# OpenHW 1.1.1 - Full base release

**Start here. OpenHW 1.1.1 is the complete base installer. No OpenHW 1.1 installation or earlier patch chain is required.**

## New installation

1. Download **[OpenHW-Setup.exe](https://openhw.tech/downloads/v1.1.1/OpenHW-Setup.exe)** ([GitHub fallback](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.1.1/OpenHW-Setup.exe)).
2. Put it in a new writable game folder, run it, and select your owned Homeworld Remastered Collection with Homeworld 1 Classic and the required English archives.
3. Setup imports the required retail files locally and installs OpenHW, Launcher, Updater and Diagnostics together. Start Launcher.exe afterward.

The Collection is read-only during import. Installation, saves, settings, caches and logs remain in your OpenHW folder. This full installer already includes the working update system; **new installations do not need a separate updater replacement**. Future updates use the Launcher's server ticker and **Update now** button. openhw.tech is the primary source, with GitHub as fallback. Automatic checks notify you; installation starts when you choose Update now.

## Why 1.1 was retired

OpenHW 1.1 shipped with a utility packaging problem: a required HTTPS support file was missing, so Launcher and Updater could fail to read the update feeds. Its update workflow also could not complete a Launcher replacement from within the Launcher. Diagnostics needed the saved-name, comment and upload-history workflow completed as well.

To give everyone one complete starting point with these fixes included, **1.1 has been retired from public downloads and replaced by the full 1.1.1 base installer**. The corrected utilities are included directly, rather than requiring new players to install 1.1 and then repair it.

## Already running OpenHW 1.1?

You can keep your existing installation, saves and settings. Close the game and Launcher, download **[Updater.exe](https://openhw.tech/downloads/v1.1.1/Updater.exe)** ([GitHub fallback](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.1.1/Updater.exe)), replace Updater.exe beside OpenHW.exe, and run it once. It installs the verified 1.1.1 recovery and reopens Launcher. Already on 1.1.1? No reinstall is needed.

The full installer is for a new folder; do not run it over an existing OpenHW installation. Back up saves and custom work before migrating folders.

## Included features and corrections

- Dedicated server-hosted multiplayer with host-controlled gameplay rules, AI players, teams and Allied Victory. This remains a multiplayer preview: one active battle on the current service, no running-battle reconnect/save/resume or host migration.
- Launcher configuration, mods, utility maintenance and both normal/diagnostic game modes.
- A footer update ticker and Update now button. Updates download and verify before the helper safely replaces utilities and reopens Launcher.
- Diagnostics opens a separate menu while Launcher remains open. Save your **Homeworld/modding-community alias** and a comment to create a persistent private server folder. **Upload all new logs** sends unuploaded logs and crash dumps in bounded batches; changed logs can upload again and interrupted batches can retry. Normal play does not automatically upload logs. Logs may contain local file paths.
- The missing HTTPS support file and update-check input lock are fixed.
- The complete campaign, authored environments, presentation changes and community corrections from the 1.1 feature set are retained. Game rendering and multiplayer simulation binaries are unchanged by this base repack.
- Live Shift+F11/F12 map, lighting and texture authoring menus remain disabled in public game builds. Authored environments still load.
- Compact conversion releases disposable files during packing. The main generated archive is about 34 GB; allow approximately 80 GB free on NTFS with hard links or 100 GB without them. These are conservative planning estimates, not measurements for every configuration.

AMD FSR upscaling and AMD FSR frame generation remain the recommended options. DLSS has known visual issues; Intel upscaling is not recommended. NVIDIA/Intel frame generation are not implemented in this release.

Complete cumulative source, checksums and an installer audit accompany the release. Retail game data is imported from your owned Collection and is not supplied by this download.
