# OpenHW 1.0.3

Install the original OpenHW base game, followed by **1.0.1**, **1.0.2**, and then **1.0.3**. Close both game executables before running the patch and select the same OpenHW installation folder. This patch is not a full installer.

## Gameplay and interface

- Restores ordinary double-click docking and support actions. **Shift+Ctrl+double-click** adds the visible ships of the clicked type to your selection.
- Corrects support-frigate latch positions using the Remastered model's docking markers, with the original positions as a fallback. Docking now starts the corvette fold animation on all approach routes and preserves the latest animation pose as visual detail levels appear.
- Prevents a crash caused by requesting the build manager again while it was already open. The existing manager is reused and can focus the selected eligible factory.

## Effects

- Moves non-capital engine glow and trail origins closer to the hull, while keeping the trail joined to its glow and preserving a small gap.
- Orients hull-impact effects outward and allows a sustained ion-beam hit to restart its authored effect after the previous burst ends.

## Downloads and known issues

- **OpenHW-Patch-1.0.3.exe** is the patch installer. **OpenHW-1.0.3-Source.zip** contains the complete cumulative working source from 1.0 through 1.0.3, including the installer, shaders, importers, editing tools and tests. The source is also available under this release's tag.
- A reported stray Turanic mothership in Mission 16 remains under investigation. A reported unusually white research-ship engine panel has not yet been attributed to a confirmed cause.
- AMD FSR upscaling and AMD FSR frame generation remain the recommended options. DLSS and Intel upscaling have known issues and are not recommended; NVIDIA and Intel frame generation are not implemented.
- Historical original source comments and older documentation remain included as reference material and may be outdated. Consult the current OpenHW guides when modding.

The patch builds passed offline checks, including build-manager reuse, docking and native animation tests. Full multiplayer and every in-game scenario have not been certified by those checks. All network participants should use the same version.
