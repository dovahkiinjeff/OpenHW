# Macrostutter pass, 20 October 2026

This update targets repeated CPU work and cache churn without changing rendering quality, ship LOD selection, authored missions, textures, clouds, lighting, or player settings.

## Evidence and scope

The completed E363 session `renderer-20261020-065430-11156.log` reports 1,808 material uploads, 1,735 evictions and 24,901 MiB of fine mip heaps retired over the session. During its later world views, residency repeatedly stays near 76 materials / 4,072.5 MiB while uploads and evictions continue increasing. The geometry cache repeatedly approaches its 256 MiB admission limit. These are aggregate counters, not proof that every reported hitch has the same cause.

The subsequent CE9 process was launched with `/noDiagnostics /logOff`. It produced no comparable renderer log. This pass therefore does not treat the older log as a measurement of that build or claim an end-to-end FPS improvement.

## Changes

- Texture trimming now checks the actual target before discarding fine mips. Below-budget inactive textures retain their full detail. Known scenes retain their required materials plus the existing 512 MiB warm cache, subject to the existing DXGI budget and headroom. Pre-gather slot retirement enforces hardware pressure rather than a falsely empty scene target. Completed retired heaps are collected, and pending heaps are counted as owned memory when deriving the budget. Eligible LRU fine mips are trimmed before whole resources; active and unfinished GPU references remain pinned. Sensors retention and return grace remain intact.
- Persistent raster geometry retains its conservative admission reservation through conversion and partial uploads. Temporary upload allocations have separate frame-fenced ownership and release their cache charge only on retirement. After successful upload and CPU-copy disposal, an entry is charged for its actual GPU allocations. The 256 MiB cap remains; this frees capacity previously charged forever for discarded CPU and upload copies.
- Native raster PSO variants reuse the identical compiled vertex and pixel shader blobs for the renderer lifetime. Different blend, depth and cull states still create their own PSOs, but no longer compile the same shader pair repeatedly.
- Opt-in benchmark captures retain a bounded set of the 128 longest frame intervals with CPU phase timings, texture activity, geometry uploads/cache charge, submitted/waited frame identities and matching existing GPU timestamps. Reports mark unavailable GPU tails explicitly. No additional GPU wait or per-frame file output is introduced. Moving-camera captures are useful for hitch attribution; fixed-view comparison flags retain their original meaning.

## Validation

Validation results and the exact deployed executable/symbol hashes are recorded in `Dev/MacrostutterCandidate/manifest.json` in the live installation. Automated checks use offscreen fixtures and CPU tests without launching or controlling the game. Real Mission 7 camera-motion results remain a user gameplay check; synthetic parity is not a mission benchmark.
