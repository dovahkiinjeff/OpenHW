# OpenHW 1.0 â€” installation and user guide

## Installation and disk space

Download `OpenHW-Setup.exe` into the **writable final game folder** and run it there. Select your own installed Homeworld Remastered Collection (Collection root, HomeworldRM, or its Data folder). Setup also needs the Collection's Homeworld 1 Classic data and required English archives. It installs beside the installer and does not change the original Collection. No separate Classic executable selection or manual music/voice copying is required.

**Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.

Allow setup to finish extraction, asset conversion and validation. Its logs live in `Development/Logs`. After success, setup removes `Development/SetupSources` and `Development/SetupWork`; if cleanup fails it reports retained files. Failed imports retain working data for recovery. Do not delete working data during installation. Setup does not launch the game.

Start `OpenHW.exe` for normal play. `OpenHW-Daignostics.exe` is the full-logging variant (filename spelling intentional). Application-owned diagnostic output is below `Development`. Save games, settings, mods and exports also stay in the game folder; no application-owned AppData install is required. Windows and driver caches are external system behavior.

First boot detects resolution and refresh rate rather than imposing the author's monitor mode. Other options use the authored release defaults. Change video/gameplay options to suit your system. Keep backups of `SavedGames`, `Homeworld.cfg`, `RTXExports`, `RTXLighting`, `Modding`, `mods` and parade presets before installing updates. Never overwrite the only copy of your authoring work.

## Recommended graphics settings

| Feature | OpenHW 1.0 status |
| --- | --- |
| AMD FSR upscaling | Fully working; recommended |
| AMD FSR frame generation | Fully working; recommended |
| NVIDIA DLSS upscaling | Known visual issues; not recommended |
| Intel upscaling | Not recommended |
| NVIDIA frame generation | Not implemented for this release |
| Intel frame generation | Not implemented for this release |

**Stick to AMD technology for 1.0**, including on supported non-AMD GPUs. Select FSR explicitly rather than assuming an automatic vendor selection is the recommended path. Upscaling and frame generation are separate options. Generated frames improve presentation smoothness; they do not increase simulation update speed or replace a healthy underlying frame rate.

Path tracing is optional and requires compatible hardware. Non-path-traced mode uses the mission's authored lighting and shadows. The global **Non-PT Glow Brightness** control defaults to **29%** and affects hangar/engine mesh emission only in that mode. Find lighting through Shift+F12 or the Shift+F11 editor's Lighting button. Adjusting this value does not change the path-traced glow setting.

Brightness/gamma and post-processing controls are available alongside display and image-quality settings. Internal high-dynamic-range lighting is not the same thing as enabling HDR on a Windows display. UI scale is adjustable. Use Gameplay > Show FPS for the yellow, black-outlined counter at the upper right.

## Fleet and formation controls

Double-click a ship to select visible ships of that type. Shift-double-click another type to add its visible peers. Press B with a build-capable carrier/mothership selected to open that ship's Build menu. Launch Manager lists eligible holds; drone frigates are not treated as carriers. Captured ships keep their identity and appropriate build/docking behavior. Player-controlled strike craft and corvettes can explicitly dock with the Karos Graveyard mission relay.

Select ships of **one type in a formation** to display Formation Spread. It changes spacing immediately, without requiring another move order. Center is the default layout, the tight end brings ships close, and the wide end doubles the default spread. Mixed-type selections do not show the slider. Main-view move orders no longer use the former short-range clamp, although the game still has a finite playable world.

Gameplay options include camera sensitivity, cursor scale, unlimited strike-craft fuel, orders while paused, ship weapon recoil, resource multiplier, ranks, mission-start launch behavior, FPS display, Disable SP Resource Scaling and Unlimited RUs. Resource/fuel cheats are single-player options. Disable SP Resource Scaling keeps the authored maximum resource population instead of reducing it according to adaptive campaign scaling; it is separate from resource yield and unlimited currency.

Fleet Identity offers hull base/stripe color, engine-emission override and player navigation-light controls. Fleet Control also contains Capture / Build All, Super Salvagers and Disable Unit Caps. Use these optional gameplay changes deliberately when playing the campaign.

## Ship control

This is primarily a fun feature and a foundation for future unusual game modes. Select one eligible player ship and click **COMMAND** or **PILOT**. Eligibility and available cockpit controls depend on ship class. Normal fleet command resumes on Esc.

### EVE Online-style command controls

COMMAND supplies target-based flight and weapon control:

| Input | Action |
| --- | --- |
| Ctrl+click | Lock a target |
| Q / W / E | Approach / orbit / keep range |
| Ctrl+Space | Stop |
| Double-click space | Fly in that direction |
| Right drag / wheel | Orbit camera / zoom |
| Ctrl+wheel; range buttons | Change requested range |
| Speed bar; + / - | Set speed limit |
| F1â€“F12 | Toggle weapons in the selected bank |
| Page Up / Page Down | Change weapon bank |
| Unlock / Cease Fire | Clear target / disable firing |
| Esc | Exit ship control |

### Direct piloting

PILOT supplies direct throttle and flight controls. The on-screen key legend changes with the camera mode and is the authority for the active mode.

| Input | External pilot action |
| --- | --- |
| W / S | Increase / decrease power |
| A / D | Turn left / right |
| Q / E | Strafe left / right |
| R / F | Ascend / descend |
| Space / left mouse | Fire |
| Right mouse | Recenter |
| Wheel; 1 / 2 | Zoom |
| C, when offered | Toggle fighter cockpit |
| Esc | Exit |

In cockpit mode the mouse steers, A/D roll, W accelerates, S brakes/reverses, right mouse provides free look and the wheel changes power. Health, speed and weapon readiness are shown in the pilot HUD.

### Wingleader control

**X and Claw formations support wing control through their wingleader.** Click/select the leader, then enter ship control. The WINGLEADER HUD indicates the wing and speed limit. This leads the formation; it is separate from adjusting its formation-spread slider.

## Editors and mods

Shift+F11 opens map/cloud authoring; Shift+F12 opens lighting. The main-menu Modding section contains ship editing, Mod Manager and military parade authoring. See [Editing Guide](EDITING_GUIDE.md) and [Modding Guide](MODDING.md). Ship-layout changes may require a fresh mission, not an existing save. Restart after changing active mods or priority.

## Reporting and upcoming work

Completing NVIDIA DLSS and Intel support and addressing player-reported bugs/glitches are immediate priorities. Until then use AMD FSR for 1.0. Include the mission, reproducible steps, GPU/driver, all reconstruction/frame-generation settings, screenshots/video and relevant diagnostic output. Do not share retail game assets. [Known limitations](LIMITATIONS.md) distinguishes supported behavior from unfinished integrations.

## OpenHW 1.0.1 patch

Close both game executables and run OpenHW-Patch-1.0.1.exe against an existing installation. It restores dust settings on startup/save loading and regenerates missing mission lighting skies from the original Collection. Existing user settings and authored maps are preserved. See [patch notes](../RELEASE_NOTES_1.0.1.md).
