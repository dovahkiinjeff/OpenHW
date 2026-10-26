# OpenHW 1.0

**The best of both worlds: high-fidelity Homeworld Remastered assets with original Homeworld 1 gameplay.** Formerly Homeworld RTX, OpenHW combines RM and HW1 assets, modern rendering, a revised interface and extensive in-game authoring/modding tools.

> [!IMPORTANT]
> **For 1.0, use AMD FSR upscaling and AMD FSR frame generation. Both fully work and are the recommended options.** DLSS currently has issues and is not recommended. Intel upscaling is also not recommended. **NVIDIA and Intel frame generation are not implemented for this release.**

> [!WARNING]
> **Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.

## Modern rendering and environments

- Direct3D 12 with optional ray/path tracing and a non-path-traced lighting path using authored mission sun/key, ambient and local lighting, shadows and emission.
- AMD FSR reconstruction and frame generation are the recommended release combination. Native rendering is also available.
- Imported Remastered ships, native HOD rigs/subsystems, materials, animations/presentation data and supported distance LODs, while retaining original HW1 gameplay.
- Remastered backgrounds, creator-made replacement asteroids, upgraded Classic textures, filtered/mipmapped materials, glow maps and updated weapon/engine/missile presentation.
- OpenMW-derived volumetric clouds for placed volumes, ropes and connected particle fields, working with PT off. Motion is optional and defaults off. Direct cloud color control, original dust visibility/color/opacity, and original rope color/opacity overrides are available.
- Authored maps/clouds for all 16 missions and the 15 authored lighting overrides present in the working game, installed at the physical loader paths. No separate Mission 15 lighting override was fabricated.
- Global **Non-PT Glow Brightness: 29%** by default, plus brightness/gamma, texture quality, effect budgets, PT settings, bloom/god rays/lens flare, motion blur, chromatic aberration and film grain.
- Improved caching, resource reuse and parallel frame preparation; clearer startup/mission-loading status and display detection.

## Gameplay, formations and ship control

- Same-type formation selections get a **real-time Formation Spread** slider: tight, default at center, and double spread at the right. Mixed-type groups do not show it.
- Shift-double-click adds visible ships of another type to a selection. Main-view movement no longer has the former short-range command clamp.
- Select a build-capable carrier/mothership and press B for its Build menu. Launch Manager improves hold selection and excludes drone frigates from carrier-style holds.
- Captured fleet identity and production rosters, mission-specific colors, and the Karos relay docking exception for player fighters/corvettes.
- Fleet color/engine/nav-light customization, Capture / Build All, Super Salvagers, Disable Unit Caps, Unlimited RUs, Disable SP Resource Scaling, resource multiplier, ranks, unlimited strike-craft fuel, paused orders and mission-start launch controls.
- FRAPS-style top-right FPS counter, scalable modern menus/Build/Launch/Research panels and subdued contextual artwork.
- **Ship control is implemented for fun and as a foundation for future game modes:** EVE Online-style approach/orbit/keep-range command control, or direct piloting with throttle, steering and firing. Eligible fighters offer cockpit control.
- **Select the wingleader of an X or Claw formation to control that wing.** This is distinct from the spacing slider.

## In-game authoring and modding

- **Shift+F11:** Assets, Scene, Volumes, Ropes, RM Dust and Fields; placement/transforms, precise numerical editing, independent sim/KAS pause, cloud presets and map save/load.
- **Shift+F12:** mission sun/key direction/color/intensity, ambient control, custom lights, tuning/shadows and save/export. Map editor also links lighting and texture/material workspaces.
- **Ship Viewer / Editor / New Ship Project:** hierarchy/rig preview, editable definitions, transforms, undo/redo and **copy/paste, duplication and placement of subsystems—including turrets—on ships**. Supported native changes can be saved into mods. The editor will continue improving in future releases.
- **SAVE MOD / APPEND TO MOD**, dependency/binding validation, and **Mod Manager** activation and drag ordering. Restart after changing mods; test changed weapon counts in a fresh mission.
- Military Parade Formation Editor for Kushan/Taiidan parade position, rotation and repeated-ship growth offsets. Multiplayer preset storage is authoring-only, not network integration.
- Full source, shaders, importers, tests and installer-building tools. Original-source notes are retained, clearly distinguished from current 1.0 instructions. **Old notes are not updated OpenHW modding instructions; use the documentation index to avoid confusion.**

## Watch the project

[![OpenHW project showcase](https://img.youtube.com/vi/R4PvuIpeAaA/maxresdefault.jpg)](https://www.youtube.com/watch?v=R4PvuIpeAaA)

The video shows the earlier Homeworld RTX development; this release's instructions supersede its older installation advice.

## Package contents

- **OpenHW-Setup.exe** — the single-file installer; choose your own Remastered Collection.
- **OpenHW-1.0-Source.zip** — the complete working source and installer factory, excluding generated dependency/build caches and retail game data.
- **SHA256SUMS-1.0.txt** — download verification hashes.
- **OpenHW-1.0-Documentation.zip** — current README, release notes, installation, editing, modding, limitations and build guides for offline reading.

Setup produces **OpenHW.exe** (diagnostic logging compiled out) and **OpenHW-Daignostics.exe** (full diagnostics, spelling intentional). Diagnostic/setup output is under **Development** in the game folder. The experimental extra-turret Kushan Assault Frigate modification is not a release gameplay override.

## Verification

- Both source-built game variants passed 68/68 compiled tests in the release build.
- Release packaging tests and earlier offline installer self-checks passed. The final installer received a content/hash audit; installation and gameplay testing are performed by the project owner.
- Installer audit checks the two executables' logging separation, first-boot display detection, global 29% non-PT glow default, content inventory, disabled developer mods and authored mission hashes.
- All 16 maps and 15 available lighting overrides are verified against the effective working-build authoring and physical loader paths.
- These are build/offline checks, not a claim that every GPU, mission interaction or mod combination is bug-free.

## Installation

1. Install your legally owned **Homeworld Remastered Collection**, including Homeworld 1 Classic and the required English archives.
2. Place **OpenHW-Setup.exe** in the writable folder where you want OpenHW to live, such as `D:\Games\OpenHW`.
3. Run it and select the Collection root, HomeworldRM or its Data directory. Setup also locates Classic data. **The destination is the folder containing the installer.**
4. Wait for extraction, conversion, package construction and validation to complete. No manual music/voice copying is required. Original Collection files are read-only inputs.
5. Launch **OpenHW.exe**. Resolution and refresh rate are detected at first boot, not fixed to the author's monitor.

All application-owned settings, saves, mods, exports and working files remain inside the installation folder. `Development` holds diagnostics and setup/cache data. Successful setup removes its temporary conversion copies; failed setup retains work for recovery. Windows/driver system caches remain controlled by those components. Setup does not auto-launch the game.

**Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.

## Documentation

Start with [the current documentation index](https://github.com/dovahkiinjeff/OpenHW/blob/main/documentation/README.md), [user guide](https://github.com/dovahkiinjeff/OpenHW/blob/main/docs/USER_GUIDE.md), [editing guide](https://github.com/dovahkiinjeff/OpenHW/blob/main/docs/EDITING_GUIDE.md), [modding guide](https://github.com/dovahkiinjeff/OpenHW/blob/main/docs/MODDING.md), and [source build guide](https://github.com/dovahkiinjeff/OpenHW/blob/main/BUILD_INSTALLER.md). Historical original-source notes remain included and may not describe current behavior.

## Immediate future updates and player reports

Priorities are **completing NVIDIA DLSS and Intel technologies**, fixing bugs/visual glitches reported by players, improving the ship editor and continuing performance work. **Cataclysm and multiplayer support are planned** as more issues are resolved and performance improves; they are not supported 1.0 modes and have no announced release date.

Reports from people playing the mod are important. Tell the author what happened, the mission, exact steps, GPU/driver, resolution, PT state, upscaler and frame-generation settings; screenshots/video and relevant diagnostics help. Use [GitHub Issues](https://github.com/dovahkiinjeff/OpenHW/issues). Never attach retail archives, extracted game assets, voice/music or movies.

## Ownership

OpenHW is free and unofficial. Original engine code retains the Relic agreement and its non-commercial terms; third-party components retain their own notices. Approved upscaled Classic art and supplied concept art are included, alongside creator-made asteroids. This does not make game-derived art public domain. Remastered models/materials and original archives/audio/movies are imported from the user's own Collection. Read the source's LICENSE.txt, THIRD_PARTY_NOTICES.md and provenance notes.
