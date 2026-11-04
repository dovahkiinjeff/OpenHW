# God rays: renderer and quality controls

The 1.0.5 renderer replaces repeated shaft integration inside cloud shading and full-resolution scene composition with one bounded, reduced-resolution shaft pass, followed by depth-aware joint bilateral upsampling (JBU). It is an altered Direct3D 12/HLSL adaptation of [three-good-godrays](https://github.com/Ameobea/three-good-godrays/tree/70ad1c41c78796637064b3bb9246be08e81ee777). Both game executable builds pass, the actual D3D12 pass/helper validation passes, and the six existing cloud/frame-generation regressions pass. Automated checks ran offscreen; individual scene appearance needs player feedback, and no new whole-game performance result is claimed for the god-ray replacement.

The actual D3D12 helper passed 24 repeated draws across three flight slots, all four quality levels and odd output sizes. Neighboring cloud/grid descriptor sentinels remain intact, repeated output is bit-identical, and the debug layer reports no errors. Independent D3D11 raster execution of the same VS/PS bytecode differs by at most `2.38419e-7` illumination. The UI/config harness verifies default/clamping, preset cycling, save/load, cancel, and independence from other effects.

An isolated 64×64, 32³-grid GPU fixture measured the old atmospheric kernel at **0.074696 ms** and the bounded 40-step port at **0.057736 ms**, about **22.7% less time** for that kernel. This excludes both the new reduced-resolution pass structure and removal of native-resolution remarches, and is not a whole-game FPS measurement. Diagnostics now report a separate `godRays` GPU time beside cloud/shadow/composite times.

## Measured problem

The closed Mission 2 run `renderer-20261103-192537-41384.log` contains a nearly stationary comparison at 2560×1440 with 16 cloud volumes and four local lights. OFF uses seven 120-frame windows at frames 15000–15720; ON uses eight windows at 15960–16800. The mixed toggle window at 15840 is excluded.

| Mean recorded GPU time | God rays OFF | God rays ON |
| --- | ---: | ---: |
| Total frame | 3.4496 ms | 5.7607 ms |
| Cloud march | 0.7356 ms | 1.8762 ms |
| Scene composite | 1.1689 ms | 2.3982 ms |
| Sun visibility generation | 0.000 ms | 0.019 ms |

The added cost is primarily repeated shaft integration, rather than generating the visibility field. FSR upscaling remains approximately 0.6 ms in both periods. VSync and FSR frame generation were enabled; these timings describe recorded base-frame GPU work, not generated FPS or a universal speedup prediction.

## Quality presets

Graphics → Post Effects → **God Ray Quality** cycles independently of the existing God Rays switch and mission-authored strength/color. The config key is `godRayQuality`; older profiles without that key use Medium. Values are clamped to 0–3.

| Value | Preset | Integration samples | Width and height relative to output | JBU taps |
| --- | --- | ---: | ---: | ---: |
| 0 | Low | 24 | 1/4 | 4 (2×2) |
| 1 | Medium (default) | 40 | 1/4 | 4 (2×2) |
| 2 | High | 60 | 1/2 | 16 (4×4) |
| 3 | Ultra | 90 | 1/2 | 16 (4×4) |

Samples control shaft integration, not cloud geometry/detail, texture resolution, path-tracing samples or mission lighting.

## Upstream and OpenHW differences

Upstream's [illumination shader](https://github.com/Ameobea/three-good-godrays/blob/70ad1c41c78796637064b3bb9246be08e81ee777/src/godrays.frag) reconstructs world positions from scene depth, bounds the ray segment against the light frustum and accumulates illumination using shadow visibility. Its [compositor](https://github.com/Ameobea/three-good-godrays/blob/70ad1c41c78796637064b3bb9246be08e81ee777/src/compositor.frag) weights reduced-resolution samples by spatial proximity and depth similarity to avoid bleeding across silhouettes.

OpenHW ports that pass structure and reconstruction concept into its existing D3D12 renderer. It uses OpenHW's camera/depth conventions, finite mission support and authored sun visibility/color, rather than importing the Three.js runtime. Positive local visibility contrast drives the shafts, preserving dark regions instead of applying a uniform bright pedestal. The adaptation handles shaft radiance separately so that shadows and scene contrast remain visible. Existing cloud density, opacity and lighting remain independent.

The illumination cap (`maxDensity`) is deliberately changed from upstream's default 0.5 to **0.98** to retain the available range of OpenHW's previous authored shaft strength. It bounds saturated integration; it does not set global cloud density or change the God Rays switch. This is an altered-port choice, not upstream behavior or a visual-parity claim.

## Owner review after local deployment

Compare the same mission save, camera and graphics settings with God Rays ON and OFF, keeping the selected quality preset fixed. Check dark-scene contrast, ship/asteroid silhouettes, shaft intensity/color, camera movement and cloud interiors. Allow loading and shader work to settle before comparing GPU pass timings. A quality or camera change must be recorded separately. The pre-change timings above establish the problem; a new matched run is required before claiming an in-game performance improvement.

See [the pinned upstream provenance and license](../src/ThirdParty/ThreeGoodGodRays/PROVENANCE.md). The OpenHW adaptation is altered software and is not the upstream package.
