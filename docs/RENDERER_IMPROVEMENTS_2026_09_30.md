# Renderer improvement pass — 2026-09-30

The accepted current renderer is the baseline. This pass does not regenerate
textures, restore the reverted metal experiments, or change authored missions.

## Changes

- DXR materials use GPU-resident texture arrays with hardware trilinear mip
  sampling. Generated normal filtering uses a mip footprint instead of nine
  manual bilinear taps. CPU texels remain available for material deduplication
  and emissive-light proxies.
- Cloud reconstruction pairs cloud samples with depth at the same texel center,
  rejects incompatible silhouette samples, and performs a direct pixel resolve
  only when no compatible samples remain. Ray footprints no longer depend on
  derivatives evaluated in a divergent fallback branch.
- Ray-tracing history validates instance identity and object-space geometric
  normal, tracks per-pixel history age, and keeps current-frame depth rather
  than temporally blending depth.
- A conservative 16-by-9 screen-tile bitmask limits the direct cloud marcher to
  potentially intersecting volumes, preserving volume order and density logic.
- Cloud scattering and scene transmission composite in linear light. The
  existing shoulder is retained. The world-cache availability flag now reflects
  actual cache validity, not merely the existence of screen history.

## Costs and limitations

Mip chains are generated once per registered material from its runtime texels;
this is not direct reuse of packaged DDS mips. The mip chain adds approximately
one third to base texture storage (very narrow textures can cost more). Runtime
material preparation and temporary upload storage can increase initial loading
cost. GPU memory now holds materials previously sampled from upload memory.
No speedup or memory reduction is claimed without mission measurements.

Temporal identity uses the existing geometry/material/nearest-transform instance
matcher; it is not an engine ship identifier. Dense identical moving objects
still warrant testing. Linear compositing can alter apparent cloud brightness.

## Verification

`tools/validate_presenter_shaders.ps1` compiles the production presenter vertex,
pixel, cloud-screen, and froxel compute shader entry points.

The optional `renderer_smoke` CMake target runs the production DXR implementation
on synthetic geometry, without launching the game or touching player settings.
It verifies material mip upload, five sequential GPU dispatches, surface-history
convergence, and D3D12 debug-layer errors when the debug layer is installed.
It mirrors the presenter's final motion-resource transition.

These checks do not establish gameplay performance or visual acceptance. Compare
Mission 6 asteroid views and Mission 7 cloud interiors, rotating and zooming near
ship silhouettes, at identical resolution/upscaler/frame-generation settings.
Check initial loading time and VRAM alongside GPU frame time. Preserve authored
cloud and lighting settings during comparison.

## Oversized-volume sampling follow-up

The previous direct march enlarged its cells to the maximum cloud diameter
divided by 128. A 500,000-unit major axis therefore produced approximately
10,156-unit cells even beside the camera. Residual high-frequency density and
boundary noise could alias badly under camera movement. Ellipsoid intersection
also compared an inverse-size-squared coefficient with a fixed epsilon, rejecting
valid rays at sufficiently large scales.

Volumes above a 22,000-unit major dimension now use a geometric distance lattice
with 3.125% cell growth and a 1,024-iteration safety budget. Initial cadence is
capped at the reference 22,000-unit cloud cadence, rather than scaling indefinitely
with cloud size. Partial cells integrate only their actual covered length. Early
optical termination is retained. Smaller volumes retain the existing marcher.
Large-volume noise octaves and boundary noise filter continuously toward their
means as their footprint becomes unresolved; a five-point statistical threshold
filter retains expected low-coverage density instead of thresholding the mean
to zero. This is an approximate far-field representation, not exact integration
of all procedural cells. Key-shadow probes receive footprint filtering too.

Intersection coefficients are normalized before solving, with a cancellation-
resistant quadratic root calculation. The numerical regression script
`tools/test_large_dust_sampling.ps1` covers 503 interval/intersection cases,
including five-billion-unit dimensions and trillion-unit traversal spans.
Those are arithmetic tests, not supported gameplay-scale guarantees. Floating-
point world-position precision, extremely thin shapes, strong local lighting,
many overlapping volumes, and mission GPU timing still require visual testing.
No claim of arbitrary/infinite-scale fidelity or constant frame time is made.
