# OpenHW 1.0

**The best of both worlds: high-fidelity Homeworld Remastered assets and original Homeworld 1 gameplay.** OpenHW combines RM and HW1 assets with modern rendering and in-game creation tools to build the ultimate classic Homeworld experience. Formerly Homeworld RTX.

[![OpenHW](assets/UI/startup_splash.png)](https://github.com/dovahkiinjeff/OpenHW/releases/latest)

[Download OpenHW 1.0](https://github.com/dovahkiinjeff/OpenHW/releases/latest) [Installation and controls](docs/USER_GUIDE.md)  [Editors](docs/EDITING_GUIDE.md)  [Modding](docs/MODDING.md)  [Build the installer](BUILD_INSTALLER.md)

> [!IMPORTANT]
> **Use AMD technology for 1.0. AMD FSR upscaling and AMD FSR frame generation are the recommended, working options.** DLSS currently has known visual issues and is not recommended. Intel upscaling is also not recommended. **NVIDIA and Intel frame generation are not implemented for this release.** Bundled vendor libraries or development code do not imply supported release features.

> [!WARNING]
> **Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.

## Installation

1. Install your legally owned **Homeworld Remastered Collection**, including Homeworld 1 Classic and the required English data archives.
2. Download **OpenHW-Setup.exe** from Releases. Put it in the writable folder where you want OpenHW installed, for example `D:\Games\OpenHW`.
3. Run it and select the Collection installation. The Collection root, `HomeworldRM`, or its `Data` directory can be selected; setup also locates the Collection's Classic data.
4. Let extraction, conversion, package building and validation finish. Setup installs **beside itself** and leaves the original Collection untouched. There is no manual music/voice-copy step.
5. Start **OpenHW.exe** after setup completes. Setup does not automatically start the game. The first launch detects your display resolution and refresh rate.

All application-owned settings, saves, mods, exports and working files stay inside the OpenHW folder. `Development` contains setup work, diagnostics, logs, caches and receipts. Successful setup removes its temporary `SetupSources` and `SetupWork` conversion copies; a failed setup keeps work for recovery. Windows and graphics-driver system caches remain under those components' control.

Two game executables are supplied: **OpenHW.exe** has diagnostic logging compiled out; **OpenHW-Daignostics.exe** enables full diagnostics and writes under **Development**. The spelling of the diagnostic filename is intentional. Source is a separate download for developers, not required to play.

## Watch the project

[![OpenHW project showcase](https://img.youtube.com/vi/R4PvuIpeAaA/maxresdefault.jpg)](https://www.youtube.com/watch?v=R4PvuIpeAaA)

This showcase documents the project's earlier Homeworld RTX development; the instructions on this page describe OpenHW 1.0.

## Modern rendering and environments

- Native Windows x64 rendering with Direct3D 12 and optional hardware ray/path tracing; a non-path-traced mode is also available.
- Non-path-traced rendering shares authored mission sun/key, ambient and local-light settings, with shadows and emissive materials. **Non-PT Glow Brightness defaults globally to 29%** and adjusts hangar/engine glow textures independently of path-traced rendering.
- AMD FSR reconstruction and frame generation are the recommended 1.0 presentation path. Native rendering remains available. The interface is presented separately from reconstructed scene detail.
- Locally imported Remastered HOD ships, rigs, subsystems, textures, animation/presentation data and distance LODs, including campaign-specific ships and stations. Classic gameplay remains the simulation foundation.
- Fleet colors, mission-specific liveries and captured-ship identity; filtered, mipmapped materials, normal maps, emission and team-color masks.
- Replacement asteroid models/materials, authored LODs, upgraded Classic texture assets, Remastered backgrounds and improved weapon, engine and missile effects.
- OpenMW-derived volumetric cloud rendering adapted to finite, placed map volumes, ropes and detected particle fields. Clouds work without path tracing. Motion is optional and defaults off; there is no swirling requirement.
- Authored cloud color, density, shape and detail control; original dust and rope presentation can be tuned independently. Converted fields follow connected particle distributions instead of requiring a hand-placed volume on every particle.
- Per-mission maps, clouds and lighting shipped as defaults. All 16 map overlays are included, alongside the 15 authored lighting overrides that exist; Mission 15 uses its fallback lighting where no separate override was authored.
- Brightness/gamma, bloom, god rays, lens flare, motion blur, chromatic aberration and film grain; adjustable texture resolution, effect density/budget, PT bounce/sample settings and frame limit; adjustable UI scale and display settings. Internal HDR lighting does not require Windows HDR output.
- More informative startup and mission-loading status, shader/cache work, optimized asset reuse and parallel frame-preparation work. Performance depends on hardware, settings and scene complexity; generated FPS is not simulation FPS.

## Fleet identity, capture and production

The Fleet Identity menu provides hull base/stripe colors, an independent engine-emission color override and player navigation-light appearance controls. Fleet Control includes Capture / Build All, Super Salvagers and Disable Unit Caps options. Capture foreign ships and retain their fleet identity. Captured build-capable ships use their appropriate production rosters. Carrier/mothership production and docking rules account for ship identity, with mission-required exceptions such as the Karos Graveyard communications relay accepting player-controlled fighters and corvettes. Select a build-capable carrier or mothership and press **B** to open its build menu.

## Formations, fleet controls and gameplay

- **Live Formation Spread:** select a same-type group in a formation to adjust its spacing immediately. The center is the normal layout, the left is tight spacing, and the right is twice the default spread. Mixed-type selections do not display this control.
- Double-click a ship to select its visible peers. **Shift-double-click** another type to add its visible peers to the selection.
- Main-view move placement no longer has the former short-range command clamp; use it like the Sensors Manager within the playable world.
- Revised Launch Manager with improved hold selection, spacing and access to available holds. Drone frigates are excluded from carrier-style hold selection.
- Camera-inside-ship handling hides the obstructing hull instead of drawing the expensive wireframe view.
- Gameplay controls include unlimited strike-craft fuel, orders while paused, weapon recoil, resource multiplier, rank system, mission-start launch behavior, **Unlimited RUs**, and **Disable SP Resource Scaling**. The latter retains the full authored single-player resource population instead of reducing it to the campaign's adaptive amount.
- **Show FPS** adds a classic yellow, black-outlined FRAPS-style counter at the top right.
- Redesigned Build, Launch, Research, ship information, menus and loading panels, with subdued background art and scalable text/controls.

## Ship control: command or pilot

Ship control is implemented, primarily for fun and as a foundation for future unusual game modes. Select an eligible ship and use **COMMAND** for EVE Online-style approach, orbit, keep-range, target locking and weapon-bank control, or **PILOT** for direct flight. Direct piloting includes throttle, turning, strafing, vertical movement and firing; eligible fighters also have a cockpit option.

**X and Claw formations can be controlled through their wingleader.** Select the formation's wingleader and enter ship control to lead the wing. The on-screen WINGLEADER status identifies the controlled formation. This is distinct from the Formation Spread slider.

See the [ship-control reference](docs/USER_GUIDE.md#ship-control) for keys and the differences between command, external pilot and cockpit controls.

## In-game creation and modding

### Maps, clouds, ropes and particle fields

**Shift+F11** opens the live map workspace: Assets, Scene, Volumes, Ropes, RM Dust and Fields. Browse assets, place and transform objects, edit existing scene objects, configure cloud volumes, tune rope shapes and work with automatically grouped particle fields. Simulation and mission scripting can be paused independently during authoring.

Cloud color and opacity/density, procedural detail, size, placement and optional motion can be authored. Original dust particles have separate visibility, color and opacity controls; original rope meshes also have color/opacity overrides. Numeric editor fields support direct entry where exposed, including fine particle-spread values. Save/load map overlays and reusable cloud presets without rewriting the retail archives.

### Lighting, shadows and materials

**Shift+F12**, also reachable through the map editor's Lighting button, exposes mission sun/key direction and color, intensity, ambient mode/color, custom lights, tuning and shadows. Place or aim lights, solo the key, adjust visual response, and save/export per-mission authoring. The map editor's texture/material workspace exposes the corresponding live surface controls. Global Non-PT Glow Brightness affects only the non-path-traced renderer.

### Ship editor and reusable subsystems

Open **Modding > Ship Viewer / Editor / New Ship Project**. Preview installed ships, inspect hull/joint and attachment hierarchies, move and rotate parts, copy or duplicate subsystem chains, and edit ship/weapon definitions. Native subsystem edits synchronize to the editable ship definition; copied functional weapon mounts retain their own weapon-slot identities and settings.

**Copy/paste, duplicate and place subsystems—including turrets—on ships**, then save supported configurations as mods. The ship editor will continue improving in future releases.

The workspace includes save/load, undo/redo, numeric transforms, axis handles, a ship-definition text editor and a read-only view of connected event data. OBJ preview/import and image assignment are available for authoring projects; this is not an arbitrary OBJ-to-HOD compiler or a full replacement for external modeling software.

**SAVE MOD** publishes a new native ship mod; **APPEND TO MOD** adds to an existing one. Validation checks native dependencies and bindings. Enable the result in **Mod Manager**, arrange priority, and restart. Modified weapon counts should be tested in a fresh mission: existing saves do not migrate their weapon arrays. The experimental extra-turret Kushan Assault Frigate modification is not enabled or included as a release gameplay override.

### Mod Manager and parade editor

Enable/disable installed mods and drag them into load order. Lower enabled entries override higher ones. Files remain in local `mods` folders; original imported assets remain the baseline. There is no general DLL plugin loader or live simulation-asset hot reload.

The **Military Parade Formation Editor** lets you author Kushan/Taiidan mothership parade layouts using real ship previews, position/rotation handles, numeric edits and growth offsets for repeated ship types. Campaign presets apply to ships joining military parade. Multiplayer-named preset files are editor storage only: multiplayer integration and synchronization are not implemented by that feature.

Read the [editing guide](docs/EDITING_GUIDE.md), [modding guide](docs/MODDING.md), [ship definition format](docs/OPENHW_SHIP_DEFINITIONS.md), and [parade editor guide](docs/MILITARY_PARADE_EDITOR.md).

## Requirements

- Windows 10/11 x64; a writable destination folder.
- Your own installed Homeworld Remastered Collection with the required Classic and English data.
- A supported Direct3D 12 GPU and current graphics driver; hardware DXR capability for path tracing.
**Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.
- AMD FSR upscaling/frame generation are the recommended technologies regardless of GPU brand, where supported by the adapter/runtime. Do not select NVIDIA or Intel alternatives expecting 1.0 parity.

## Essential controls

| Control | Action |
| --- | --- |
| Shift+F11 | Live map/cloud editor |
| Shift+F12 | Lighting and shadow editor |
| B with a build-capable ship selected | That ship's build menu |
| Double-click / Shift-double-click | Select visible type / add visible type |
| Formation Spread slider | Immediately change same-type formation spacing |
| COMMAND / PILOT | Ship command control / direct piloting |
| Esc during ship control | Return to normal fleet control |
| Modding menu | Ship projects, Mod Manager and parade editor |

## Documentation and source

- [Install, settings and gameplay controls](docs/USER_GUIDE.md)
- [Map, cloud, lighting and texture authoring](docs/EDITING_GUIDE.md)
- [Ship editing, mod publication and load order](docs/MODDING.md)
- [Ship control and formation reference](docs/USER_GUIDE.md#ship-control)
- [Military parade layouts](docs/MILITARY_PARADE_EDITOR.md)
- [Known limitations and reporting](docs/LIMITATIONS.md)
- [1.0 release notes](RELEASE_NOTES_1.0.md)
- [Build OpenHW-Setup.exe from the complete source](BUILD_INSTALLER.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

Download **OpenHW-1.0-Source.zip** for the full working source, shaders, importers, tools, tests, reviewed release content and installer factory. Generated build caches and your retail data are not part of the source download. The build guide specifies the compiler, SDK and pinned dependencies.

## Immediate future updates

The next priorities are **fully implementing and stabilizing NVIDIA DLSS and Intel technologies**, and **fixing bugs and visual glitches reported by people playing OpenHW**. For 1.0, stick to AMD FSR upscaling and AMD FSR frame generation.

**Cataclysm and multiplayer support are planned** as more issues are ironed out and performance continues to improve. They are future work, not supported 1.0 game modes, and have no announced delivery date.

Player reports matter: they reveal mission, hardware and gameplay combinations that development testing can miss. Please report problems to the project author through [GitHub Issues](https://github.com/dovahkiinjeff/OpenHW/issues), with reproducible steps and useful evidence.

## Reporting problems

Include the mission/save context, what you did, expected versus actual behavior, GPU and driver, resolution, path-tracing state, upscaling mode and frame-generation state. Screenshots/video are helpful. Reproduce using `OpenHW-Daignostics.exe` when logs are needed and include the relevant files from `Development`; review captures for personal information before sharing. Do not upload retail archives, extracted game assets, music, voice or movies. See [known limitations](docs/LIMITATIONS.md).

## License and ownership

OpenHW is a free, unofficial project, not affiliated with or endorsed by the original developers/publishers or GPU vendors. The original engine source is subject to the Relic agreement in [LICENSE.txt](LICENSE.txt), including its non-commercial terms. Third-party code and art retain their own notices and ownership; this is not a blanket public-domain or permissive-license release.

The package includes the approved upscaled Classic texture set, supplied concept artwork and creator-made asteroid replacements. Remastered models/materials, retail archives and original audio/movies are imported locally from the user's Collection, not supplied as an installed game download. See [content provenance and build details](BUILD_INSTALLER.md#content-boundary-and-source-snapshot) and [third-party notices](THIRD_PARTY_NOTICES.md).
