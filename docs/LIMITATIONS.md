# OpenHW 1.0 — limitations and reports

## Recommended rendering path

AMD FSR upscaling and AMD FSR frame generation are fully working and recommended for 1.0. DLSS has known visual issues and is not recommended. Intel upscaling is not recommended. NVIDIA and Intel frame generation are not implemented for this release. Vendor libraries and older source experiments do not change this supported-feature statement. Completing NVIDIA and Intel support is planned.

## Platform, performance and setup

Windows x64 is the supported release platform. Path tracing requires compatible DXR hardware. Performance varies with the mission, fleet size, graphics settings and hardware; no universal FPS or VRAM claim is made. Frame generation does not increase simulation rate.

**Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.

## Editing and compatibility

- Restart after changing mod activation/order. Static simulation assets do not hot-reload.
- Existing saves do not migrate modified weapon arrays; test weapon-count changes in fresh missions.
- Ship editing does not implement arbitrary HOD geometry writing, a full animation suite, automatic new ship identities, or an OBJ-to-HOD compiler.
- Event data viewing is read-only; presentation-event coverage is not a claim of complete Remastered simulation parity.
- Multi-file project/mod publication is not one crash-atomic operation. Preserve backups and resolve incomplete saves before enabling a mod.
- Historical reports of opening Sensors during a cutscene locking interaction and returning from parade preview into a mission causing trouble should be reported if reproduced; do not assume an earlier deferred fix was released. Avoid switching into Sensors during scripted camera sequences, and restart after editor use if a transition behaves incorrectly.
- Patch 1.0.2 repairs local skirmish and reconnects pilot/command/wing control through shared network input. Every peer must use this rebuilt patch. Full multi-computer gameplay, latency and online services still need testing; this is not a claim of fully supported online multiplayer. Cataclysm remains planned. Multiplayer-named parade presets are storage only.
- Legacy source comments, format notes and development documents are included as historical references. They may describe different renderers, paths and features. Start with [the current documentation index](../documentation/README.md).

## Reporting

Player reports are important to ongoing fixes. Include the mission/save context, precise reproduction steps, GPU/driver, resolution, PT on/off, upscaler and framegen settings, and a screenshot/video if useful. Run `OpenHW-Daignostics.exe` for diagnostics and share relevant output under `Development`, not your entire game folder. Review files for personal information. Do not upload retail archives, extracted models/textures, music, speech or movies.

The immediate focus is fixing reported bugs and glitches, continuing performance improvements, and completing NVIDIA/Intel support. Planned Cataclysm and multiplayer work follows as the foundation improves; no date is promised.
