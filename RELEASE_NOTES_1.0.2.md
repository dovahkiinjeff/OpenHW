# OpenHW 1.0.2 — skirmish, multiplayer menus and gameplay fixes

Download **OpenHW-Patch-1.0.2.exe** below (approximately 40 MB). This is a patch, not a full game installer.

## Required installation order

1. Install OpenHW using the original [OpenHW-Setup.exe](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.0.0/OpenHW-Setup.exe) and your own installed Homeworld Remastered Collection.
2. Apply [OpenHW-Patch-1.0.1.exe](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.0.1/OpenHW-Patch-1.0.1.exe).
3. Close **OpenHW.exe** and **OpenHW-Daignostics.exe**, then apply **OpenHW-Patch-1.0.2.exe** to the same OpenHW installation.
4. Wait for **Patch complete**, then start OpenHW.exe.

**Install the original first, then 1.0.1, then 1.0.2.** Put each patch beside OpenHW.exe or select the installed game folder when prompted. Keep the original Collection available in case missing sky/lighting resources need to be regenerated. Saves, options, mods and authored map/cloud/lighting settings are preserved. Backups and logs stay inside the game folder under **Development/Patches/1.0.2/Backup** and **Development/Logs/patch-1.0.2.log**.

## Skirmish and multiplayer interface

- Fixed the local AI-skirmish startup crash caused by missing or empty optional loading artwork. Loading now falls back safely to installed artwork.
- Reworked multiplayer connection screens and their submenus, including skirmish setup, map selection, fleet colours, saved battles, lobbies and rule options.
- Added clear Battle, Fleet Rules, Resources and Fleet Colours tabs, with separate Back and Start Battle controls.
- Replaced the CPU-player triangles/black counter with minus, count and plus controls; aligned game-type buttons and added visible slider grips. Local skirmish defaults to at least one AI fleet.
- Skirmish and multiplayer loading now use the detailed story-style loading panel, with current work, actual reported phase/item progress, timing and recent activity. Network loading also shows players' reported readiness.
- Multiplayer maps use the corresponding Remastered sky/environment and lighting metadata. Each map has its own lighting/map-edit identity; campaign overrides and previously cached skies are not reused accidentally.

## Ship controls and gameplay

- Reconnected direct PILOT controls, EVE-style COMMAND navigation and eligible fighter cockpit controls in local skirmish and multiplayer.
- Reconnected X/Claw formation control through the selected wing leader.
- Network pilot input uses the existing ordered command stream, with separate ownership-checked player sessions. Steering, weapon toggles and wing control are shared; cameras and local input remain local. Sessions retire on death, capture, other orders or universe reset.
- Added **Gameplay → Restore Fog of War**. ON restores the original Homeworld visibility ranges for ships, derelicts and minor objects instead of unlimited distant rendering. It applies immediately and saves locally; the default remains OFF. Existing Sensor Manager exploration logic is unchanged.
- Corrected engine-trail origins so the ribbon and its history begin at the existing glow bulb, respecting authored offsets. The glow's appearance is retained.

## Map editor

- Added a **Derelicts** dropdown beneath the RTX dust-volume entry in Assets. Select a type and press **P** to place it.
- Added an **Objects** tab for newly placed objects. Scene lists original map objects; Volumes remains for clouds.
- Fixed the inspector wheel always changing position X. Position XYZ, rotation XYZ and instance scale now adjust their own displayed rows; read-only rows cannot move an object.
- Added per-instance derelict visual scaling, including saved map-overlay transforms, without changing shared meshes or save-game layouts.
- Retains 1.0.1's authored dust/cloud initialization, save-load restoration and missing sky/lighting-resource repair.

## Source, validation and compatibility

- **OpenHW-1.0.2-Source.zip** contains the updated engine, shaders, tests, editing/import tools and installer/patch source. The same source is committed under this release tag.
- **SHA256SUMS-1.0.2.txt** contains download checksums.
- Both normal and diagnostic executables built successfully. Ten offline regression checks and the production visibility-list test passed. Full multi-computer gameplay, latency and online services are not certified by these checks; please report issues.
- **Every participant in a network game must use this same rebuilt 1.0.2 patch.** Native network weapon masks support up to 1,024 guns per piloted hull; higher-count modded hulls retain normal RTS control.
- **AMD FSR upscaling and AMD FSR frame generation remain recommended.** DLSS and Intel upscaling have known issues and are not recommended; NVIDIA/Intel frame generation are not implemented.

The full base installer is unchanged. This release does not bundle replacement retail archives, HODs, music or voice data. Original source comments and historical documentation remain included as references; use the current guides when modding.
