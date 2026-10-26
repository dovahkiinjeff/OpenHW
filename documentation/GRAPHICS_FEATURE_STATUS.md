# OpenHW 1.0 graphics status

This replaces the outdated pre-1.0 status table. For release recommendations use the table below, not older renderer experiments or the mere presence of an SDK.

| Feature | 1.0 status |
| --- | --- |
| Windows x64 / Direct3D 12 | Current release renderer |
| Hardware ray/path tracing | Optional; compatible hardware required |
| Non-path-traced lighting | Mission key/ambient/local lighting, shadows and emissive materials |
| Non-PT glow brightness | Global control; release default 29% |
| AMD FSR upscaling | Fully working; recommended |
| AMD FSR frame generation | Fully working; recommended |
| NVIDIA DLSS | Known issues; not recommended |
| Intel upscaling | Not recommended |
| NVIDIA / Intel frame generation | Not implemented for this release |
| Remastered HOD presentation | Imported locally, with native materials/rigs/subsystems and supported LODs |
| Volumetric clouds / ropes / particle fields | OpenMW-derived rendering adapted to placed bounds; works with PT off |
| Live authoring | Map, lighting, material, ship and parade workspaces; see guides |
| Diagnostics | Separate full-logging executable; normal executable logging compiled out |

Native presentation uses modern GPU resources while retaining substantial original HW1 simulation and draw-call vocabulary. Do not describe the whole engine as a new fully multithreaded simulation. The frame worker ownership document describes the specific parallel work.

See [README](../README.md), [user guide](../docs/USER_GUIDE.md), [editing](../docs/EDITING_GUIDE.md), [frame worker ownership](../docs/frame-worker-ownership.md), and [limitations](../docs/LIMITATIONS.md). Original source notes may not describe this renderer.
