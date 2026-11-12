# OpenHW 1.0.6 - texture loading, startup and refresh settings

Install **OpenHW 1.0.5 first**, then close both game executables and run **OpenHW-Patch-1.0.6.exe** in your existing OpenHW folder. This is a patch, not the base installer. [Updater.exe](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.0.5/Updater.exe) downloads and installs the missing patches in order. Manual installation order remains **1.0 -> 1.0.1 -> 1.0.2 -> 1.0.3 -> 1.0.4 -> 1.0.4.5 -> 1.0.5 -> 1.0.6**.

Saves, settings, mods and authored maps are preserved. Both the normal executable and the full diagnostics executable are included. All patch work and backups remain inside the installation's Development folder.

## Repeated texture loading

Community GTX 1080 Ti logs exposed substantial texture preparation: the newer run decoded about 20 GiB of image data cumulatively, taking 14.1 seconds of CPU time across the run. This is cumulative work, not 20 GiB of resident RAM or VRAM.

The decoded DDS cache previously handed an image to one later consumer and then discarded it. It now retains a pristine bounded copy for subsequent requests. Each caller receives its own writable allocation, and exact encoded-content checks invalidate images changed by a live mod edit. The existing 128 MiB cache limit now includes both retained image bytes and encoded-content checks; no texture resolution, LOD distances, lighting or effects are reduced.

In an isolated developer-machine test of 32 requests for the same 1024x1024 DDS, actual decompressions fell from **16 to 1**, and elapsed time fell from **185 ms to 28 ms**. This measures repeated texture loading only. **It is not a measured gameplay FPS increase on the 1080 Ti.** Player testing on that hardware is still required, and this does not eliminate the GPU cost of path tracing, clouds or frame generation.

## Startup shader compatibility

Fixes a graphics-startup failure reproduced on an RTX 5070 Ti: the driver rejected the optimized cloud-composition pipeline with `E_OUTOFMEMORY` even with memory available. Both ordinary and frame-generation cloud variants now retain flow control while keeping full compiler optimization. Separate cache identities prevent loading the incompatible variant again.

Both corrected D3D12 pipelines passed offscreen creation checks on the affected machine. Eight cloud-output cases matched the original shader exactly, across 32,768 tested color/opacity components. This fix is not restricted to GTX cards.

## Refresh-rate persistence

The saved exclusive-fullscreen refresh request is now separate from the observed active refresh. Borderless/windowed startup and an exclusive-mode fallback no longer overwrite a saved request such as 360 Hz with the desktop's 60 Hz. Video options report the active timing, and diagnostics record requested versus applied timing.

**Borderless and windowed modes use Windows' desktop refresh.** Set the desired rate in Windows Advanced display settings for those modes. For exclusive fullscreen, choose the requested rate in OpenHW, apply, then restart. The monitor, connection and driver must offer that mode; this patch does not force an unsupported timing or repair an unrelated Windows/driver reset.

## Validation and downloads

- **OpenHW-Patch-1.0.6.exe** - patch for an installation updated through 1.0.5.
- **OpenHW-1.0.6-Source.zip** - complete cumulative source, including these fixes, tests and packaging tools.
- **SHA256SUMS-1.0.6.txt** - verification hashes.

Focused checks cover repeated texture reuse, writable-buffer isolation, changed source content, case/path normalization, memory limits, eviction, oversized images, invalid input, texture quality, texture lifetime, and refresh persistence across borderless/windowed/exclusive/fallback/Auto/error cases. Both x64 game configurations build successfully. The game was not driven automatically for verification.

The supplied community logs include different maps, settings changes and VSync/frame-generation pacing, so they are not a controlled before/after FPS benchmark. AMD FSR remains the recommended upscaler. DLSS and Intel upscaling retain known issues; NVIDIA and Intel frame generation remain unimplemented. On older GPUs, compare frame generation on and off rather than assuming it always improves performance.
