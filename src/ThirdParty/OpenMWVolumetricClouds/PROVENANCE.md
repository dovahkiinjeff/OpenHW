# OpenMW volumetric clouds reference and HLSL adaptation

Upstream author: **zesterer**, repository [openmw-volumetric-clouds](https://github.com/zesterer/openmw-volumetric-clouds).
Pinned source revision: `c8830bcd2de0f6e7355f91880308426203eded3c`, retrieved 2026-10-21.
The unmodified upstream shader, README, and noise PNGs are retained in this directory. `manifest.json` records the exact source URLs and SHA-256 hashes.

## Licensing status

No explicit license file or grant for adapting and redistributing source code was found in the pinned repository. The README welcomes mod-pack inclusion and requests attribution/contact, keeping the mod current, reporting bugs, and contributing improvements. That text is retained verbatim in the upstream README; it does not establish a named software license. **This adaptation is an internal prototype. Redistribution permission remains unresolved.** No MIT, GPL, or other license is inferred or attached, and no author contact has been made by this work.

## Adapted implementation

`../../Modern/OpenMWCloudKernel.hlsl` ports the source shader's modern-GLSL integer hash, the two linearly filtered 2D lookups interpolated across 15 Z slices, the separate billow noise, two large samples, the detail hierarchy, coverage threshold, billow erosion, power-five shaping, transmission formula, and logarithmic ray coordinates.

`../../Modern/OpenMWCloudNoise.h` embeds the original red channels as a 128-by-128 RG8 texture: noise2 red in R, noise3 red in G. The decoder does not rescale, sharpen, reinterpret as sRGB, or otherwise alter either red channel. Identical original PNGs are also under `assets/Clouds/OpenMW/`.

Homeworld retains its arbitrary finite map placement, grid/rope support, editable shape envelopes, density, authored cloud RGB, and compositing. OpenMW weather, altitude bands, water-height dependence, sky replacement, sun/atmosphere coloring, light halos, and close-camera fades are intentionally excluded because they do not define finite space dust fields or the user's requested direct cloud color.

Motion is disabled by a zero speed. Optional motion translates the entire noise domain rigidly along a fixed axis; the upstream opposing octave drift and mist swirling are excluded. There is no swirl implementation or temporal noise/dither. Footprint filtering gradually suppresses unresolved billow detail; it never changes the phase or geometry placement.

## Host contract

- Bind `Texture2D<float2>` at `t6` as linear RG8 UNORM and a linear WRAP sampler at `s1`.
- Call `OpenMWCloudPattern(p, coverage, detail, billowStrength, simulationTime, motionSpeed, footprint)` with a stable world/local position times the desired base noise scale. Never derive `p` from camera/view coordinates.
- `coverage` is normalized 0..1, `detail` selects source levels 0..3, `billowStrength` 0 disables erosion and 1 matches the source, and `footprint` is a spatial step/pixel footprint in the same normalized coordinates as `p`.
- At `detail >= 2`, the large scale uses two source samples at Z offsets +/-0.01. Detail levels 1 and 3 use billow frequencies 500000/120000 and 500000/30000, with weights 1 and 0.25.
- The return is the raw nonnegative power-five cloud factor, not a normalized 0..1 mask. Multiply by an independently authored extinction density and finite envelope. It may exceed 1; bounding or normalizing that modulation is a host policy and should not be misrepresented as the upstream formula.
- For converted particle-density fields, retain the grid as linear support/density. Use independently authored field coverage (default 0.80); kernel softness belongs only to the formation bake. OpenMW billow and contrast drive the appearance, with smooth bounded modulation `2*p/(1+p)` and no residual uniform-fog baseline. Never nonlinearly transform a brick's normalized density: neighboring bricks may have different normalization peaks.
- Coverage near 0.5 is intentionally sparse because the source subtracts 1 before adding coverage. Values around 0.8..1 provide useful dense-field modulation. Existing coverage domains such as 0..2 need an explicit host mapping, not silent reuse as normalized coverage.
- `OpenMWCloudTransmission` reproduces `pow(0.9999, segmentLength * density * 10000)` with `exp`. `OpenMWCloudSegmentBoundary` distributes a bounded segment count logarithmically over a finite entry/exit interval, supporting entry=0 and exact exit coverage. The host must not use the original atmosphere's large first-sample opacity hack.
- Optional motion speed 1 is 0.003 base-noise units per simulation second. Speed zero does not read time in the coordinate calculation and must remain the default.

This cloud port does not itself provide or claim correct frame-generation motion vectors. Frame-generation input/output and transparent media need their own validation.
