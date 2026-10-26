# GPU work reuse: first optimization batch

Historical captures in `Documents/My Games/Homeworld Modern/Logs/Benchmarks`
show native Mission 1 simulation averaging ~0.04 ms, raster ~0.42 ms,
cloud marching ~6.2 ms, and final composition ~9.6 ms. The captures include
camera movement; several are partial. They are diagnostic evidence, not a
valid before/after comparison, and they predate the latest assets/FX rollback.

Changes:

- Clear-air pixels reuse a current-frame atmosphere-only bilinear footprint
  already evaluated by the quarter-resolution dust pass. All four contributing
  samples must have exactly unit transmittance and matching geometry class;
  geometry depth must agree within 0.1%. Otherwise the original native
  atmosphere integration remains. Cloud-intersecting pixels and native edge
  repair are not routed through this cache. No temporal history is introduced.
- Atmospheric samples with a zero envelope or fully blocked visibility skip
  four reference lookups. Both are algebraically zero contributions.
- Cloud samples with zero direct sunlight skip the solid-sun visibility lookup.
- DXR hit shading solves the triangle UV footprint once and shares that exact
  mip value between diffuse, emission, ORM, and normal channels. The normal
  map retains its additional projected-footprint filter and native RM policy.

No ray sample/bounce count, authored density, mission light, texture detail,
engine glow, simulation, LOD range, or persistent setting is reduced.
Atmosphere-only spatial reuse is filtered reconstruction, not mathematically
identical to full-resolution integration; compare small shafts and silhouettes.

Validation: production shader compilation, native tests, Python contracts,
and the asset-free GPU renderer smoke covering DXR, material mips, sun columns,
asteroid exemptions, actual cloud lighting and shaft integration. These are
correctness checks, not proof of a game FPS improvement.

## Player A/B capture

Use the two new shortcuts beside the deployed executable:

1. `OpenHW - Benchmark Reference` sets `HW_OPT_ATMOS_CACHE=0` for its process.
2. `OpenHW - Benchmark Optimized` sets it to `1` (the ordinary-launch default).

Both use the same executable and `-benchmark`, preserving graphics settings.
Load the same save/view, wait for loading, pause simulation if appropriate,
then press Ctrl+Shift+F6. Keep the camera/settings unchanged for 35 seconds.
Close between launches. Prefer three repeats per mode. Reports include
`clear_atmosphere_cache` so the modes cannot be confused. Compare GPU presenter
/scene-composite and total, frame-time percentiles, and image quality.
The reference disables the spatial cache only; exact-zero skips and mip reuse
remain enabled in both. Compare an older backup build to isolate the whole batch.

A substantial real-game gain is not claimed until those captures exist.
Further cloud/DXR changes should be guided by the resulting dominant pass.
