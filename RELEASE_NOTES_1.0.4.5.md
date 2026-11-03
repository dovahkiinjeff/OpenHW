# OpenHW 1.0.4.5

Install OpenHW 1.0 and patches **1.0.1, 1.0.2, 1.0.3 and 1.0.4**, in that order, before applying this patch. Close both OpenHW executables, run **OpenHW-Patch-1.0.4.5.exe**, and choose your existing OpenHW installation if prompted. Your saves and settings are kept.

## Changes

- Corrects the startup splash artwork lookup for the released folder layout.
- Removes the black bars added over in-engine cinematic scenes and corrects a blue screen during a fully black cinematic fade.
- Allows scripted cinematic environments to select their own backdrop instead of always forcing the mission backdrop. Timing and individual scene appearance still need player verification.
- Moves initial graphics preparation into mission loading and addresses an excessively long initial Mission 1 fade after its introductory movie. These changes are intended to improve the transition when a movie ends or is skipped, including the reported first-load black background. Player confirmation across missions is still needed; some preparation can take place while the loading screen is visible.
- The ship editor camera can zoom much closer for detailed placement and inspection.
- EVE-style Command control is available for corvettes as well as previously supported larger ships, including its multiplayer control path. Fighter control remains unchanged.
- Both the normal and diagnostics executables are included. The complete cumulative source is provided separately.

## Optional 1080p classic cutscenes

Download **OpenHW-Optional-1080p-Cutscenes.zip** only if you want the optional videos. Extract it into the OpenHW installation folder, preserving `Data/Movies/HD`. The game prefers those videos and falls back to the original videos when they are unavailable. Remove the MP4 files from that folder to go back to the originals.

The 17 classic videos are resized to 1080p, with the baked-in letterboxing removed and a small crop at the sides. Their original 15 fps timing is preserved. This version does not use AI-generated detail or temporal cleanup.

## Verification and known limitations

The patch payload and optional video archive were checked offline, and focused startup, loading, ship editor and ship-control tests passed. These checks do not replace a playthrough: first-load cinematic transitions, sky timing and the startup splash window need player verification. The reported Bentusi trade crash and diagnostic save-selection problem have not been conclusively diagnosed. Historical source notes may be outdated.

AMD FSR upscaling and AMD FSR frame generation remain the recommended options. DLSS and Intel upscaling have known issues; NVIDIA and Intel frame generation are not implemented. Multiplayer remains in testing; all players should use the same version.

Use **SHA256SUMS-1.0.4.5.txt** to verify downloads. The optional videos are a separate download and are not required to install or run the patch.
