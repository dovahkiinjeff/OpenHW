# Rendering cost reduction, November 3, 2026

This is a local performance test build based on 1.0.4.5. It is not a published
patch or a claim of doubled gameplay FPS. No resolution, LOD thresholds, cloud
samples, material quality, simulation behavior or authored mission settings were
reduced.

## Evidence and scope

The latest existing gameplay trace, `renderer-20261030-022347-24524.log`, has
approximately 0.37 ms raster, 0.75 ms DXR, 3.13 ms clouds, 4.58 ms scene
composition and 0.70 ms upscaling in steady Mission 1 windows. VSync and FSR
frame generation were enabled; Present waited about 12.5 ms per frame. These
data explain why PT/non-PT displayed FPS alone cannot establish a 2x opportunity.

An older large Mission 14 battle captured 25.297 ms renderer CPU, 1.573 ms
simulation and 15.049 ms GPU (`Dev/Mission14FinalPerformance/evidence.json`).
It predates some current fixes and is not a controlled baseline for this build.
Both shared CPU fleet preparation and shared GPU composition warrant work.

## Changes

- Native ship/LOD instance, owner, pose and instance-token lookups use fixed
  intrusive hash indexes. Ordered lists remain authoritative for traversal and
  cleanup; allocation-failure behavior and instance lifetimes are retained.
- Ship hierarchy pose updates use compact bounded per-call scratch instead of
  allocating/zeroing about 306 KB of mesh-construction metadata per update.
  The native hierarchy visitor avoids vector growth for the gameplay limit of
  255 nodes. Validation still completes before publishing any binding changes.
- Raster replay omits redundant D3D12 state bindings within a command list.
  Custom sky/background/clear operations invalidate the cache. Draw order,
  uploads, snapshots, shadows and guidance are retained. Set
  `HW_RASTER_STATE_CACHE=0` to use the original binding behavior for comparisons.
- Particle cloud density reads use a scalar SRV rather than loading a packed
  176-byte volume record and selecting one of 44 lanes for every scalar tap.
  The same grid values are kept in local GPU memory per fenced frame slot and
  copied when packed grid content/order changes. If the optional local resource
  cannot be allocated, the scalar view reads the original upload buffer.
- Frame generation reuses its finished display-size SDR scene for the final
  display pass. The old path recomputed scene/cloud composition and display
  effects twice. Native UI detection, alpha and display controls are retained;
  setup failure falls back to the original final pass.

## Isolated measurements and verification

- At 24,576 live ship/LOD instances, 100,000 acquire/token/find triplets took
  7,120.398 ms with the old registry and 3.214 ms with the index. This is an
  intentionally large lookup stress case, not whole-frame performance.
- On installed HODs, pose preparation improved from 10.664 to 3.494 microseconds
  for a heavy cruiser, 7.662 to 1.946 for a Taiidan frigate and 13.156 to 4.612
  for a mothership. Three alternating runs, 20,000 calls per model.
- A 512-hull offscreen replay improved from about 0.45–0.46 ms CPU to
  0.405–0.407 ms, with identical image hashes. GPU time was unchanged.
- 2,515,456 scalar density loads took median 0.23728 ms via packed-record
  decoding and 0.014496 ms via the direct local-grid view. Every grid value
  and 64-tap accumulation matched bit-for-bit. This isolates grid reads, not
  complete cloud rendering.
- Eighteen production presenter cases matched every final RGBA byte between
  the original and reused FG display path. The actual D3D12 helper also rendered
  correctly under the debug layer without validation errors.
- Registry tests include ship and mesh address reuse, stale tokens, multiple
  LODs, lifecycle cleanup and failed allocations/instance creation. Bridge tests
  include 255-node bounds, overflow rejection and failure atomicity. Existing
  installed hull/attachment/animation checks passed.

A same-save, same-view gameplay comparison is still required to measure total
FPS and frame-time gains. A VSync/frame cap can hide reduced rendering cost.
These changes do not establish that all remaining bottlenecks have been found.

## Cloud binding repair after player test

The first local optimized build hid clouds: legacy history initialization
overwrote the new scalar grid view, the final screen-field write targeted the
preceding descriptor, and the compute root did not expose the scalar grid.
The isolated density/readback tests did not exercise these production bindings.
The repair uses a shared explicit screen-field binding helper, retains the grid
descriptor at initialization, and extends the compute root through the scalar
grid slot. New real descriptor-table and production compute-pipeline tests cover
these failures. The other performance changes are retained. Gameplay gains must
be measured with clouds visible.
