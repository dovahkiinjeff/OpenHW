# Mission 7 cloud visibility pass

Baseline: F:/OpenHWWIP/Dev/Diagnostics/Release-1.0-20261020-025408.
Continuous windows show roughly 17–20 ms cloud marching and 11–13 ms scene composition, versus around 1 ms DXR in the later Mission 7 views. The scene reaches 255–256 uploaded volumes, with about 80 candidate volumes per coarse screen tile. Earlier windows include loading and graphics-mode changes; these are not controlled A/B measurements. No separate fixed-view benchmark report was available when this note was written.

Changes: screen tile masks increase from 16x9 to 64x36 (72 KiB per frame slot). Four conservative tile-frustum side planes reject rotated cloud/rope support boxes even when those boxes cross the near plane. A camera inside support remains visible in every tile. Two low-resolution-pixel margins preserve reconstruction/jitter coverage. Density evaluation, integration cadence, ordering, lighting, detail and authored mission data remain unchanged. CPU tile planes use reusable storage. The support test uses double precision and an outward tolerance.

Ship LOD: the shipping modern path chooses meshes by projected output-pixel diameter with hysteresis, independently of the legacy noLOD option. Loaded LOD0–4 log messages prove availability, not actual draw distribution. Profile sessions now print [ShipLOD] counts and source mesh triangles for ships actually submitted by the main ship draw path, every 120 frames. Counts exclude shadow-only passes and preview meshes; the triangle count describes complete selected mesh geometry, including any hidden subobjects. It is not a GPU triangle counter.

Validation: native suite and LOD policy tests, randomized projected support points, explicit camera-inside and near-plane off-axis cases. Production shaders are compiled separately before deployment. Actual FPS improvement requires a same-save, same-view before/after capture; no gain is claimed from the synthetic culling tests.

The parade-to-story crash investigation is paused at the user’s request; see work/paused-parade-crash.txt. No parade fix is included here.

## Rejected batch
The player reported much worse performance immediately after deployment. The 64x36 tile/frustum change is rejected and removed from production source. Restore the exact pre-pass executable (SHA256 08A5D32EC9EC1EC6DF8840E54FDBBEE9EDA1B8709628CB9B972077FEFE8A17C1) when the running game exits. Headless correctness checks did not establish performance; the additional per-frame CPU culling work is a suspect, not a measured cause yet. LOD/capture diagnostics remain in source for future investigation.
