# OpenHW 1.0.1 — dust initialization, saved games and mission lighting

This is a **standalone patch installer for an existing OpenHW installation**, approximately 40 MB. Download **OpenHW-Patch-1.0.1.exe** below.

## Fixes

- Authored dust color, opacity and cloud styles initialize during ordinary mission rendering, including cinematic camera frames; pressing Shift+F11 is no longer needed to trigger initialization.
- Loading a save invalidates the previous cloud initialization state, including a save from the same mission, so saved mission appearances are restored rather than reverting to oversized default clouds.
- Missing physical mission sky textures and HDR lighting metadata are regenerated from the owner's original Collection. This restores the resources required by mission sun/key controls and god rays. Missing sky resources are now reported separately from a retail local-light import failure.

## Apply the patch

1. Close **OpenHW.exe** and **OpenHW-Daignostics.exe**.
2. Run the patch beside OpenHW.exe, or select the installed OpenHW folder when prompted.
3. Keep the original Homeworld Remastered Collection available. The patch uses the recorded source location and asks for the Collection folder if it moved.
4. Let mission lighting conversion finish. It may take a few minutes; wait for **Patch complete**.
5. Start OpenHW.exe.

Saves, options, mods and authored map/cloud/lighting settings are preserved. Original Collection archives are read-only inputs. Previous program files are backed up under **Development/Patches/1.0.1/Backup**. Patch logs are in **Development/Logs/patch-1.0.1.log**. All application-owned files remain in the game folder.

**New users:** install the [OpenHW 1.0 base installer](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.0.0/OpenHW-Setup.exe), then apply this patch before playing.

## Downloads and source

- **OpenHW-Patch-1.0.1.exe** — updates both normal and diagnostic game executables and repairs missing lighting resources.
- **OpenHW-1.0.1-Source.zip** — complete updated engine and installer/patch source.
- **OpenHW-1.0.1-Documentation.zip** — current offline documentation.
- **SHA256SUMS-1.0.1.txt** — download checksums.

Both game variants and the patch launcher compiled successfully. Gameplay testing is performed by the project owner; this patch was not applied or run in-game automatically.

**AMD FSR upscaling and AMD FSR frame generation remain the recommended options.** DLSS and Intel upscaling are not recommended in this release; NVIDIA and Intel frame generation are not implemented.
