# RM dust, move UI and hyperspace presentation

Shift+F11 now has a reachable **RM DUST** tab. It replaces the retired dust
inspector with per-mission controls for the actual imported sprite effect:
opacity, brightness, emitter size, normalized effect age, and RGB tint
multipliers. Effect age samples the RM size/color/offset curves at a stable
point in their lifetime; it does not alter resource simulation. Defaults
preserve the prior presentation. Opacity zero hides it; reset restores RM
defaults. Regular map volumes, ropes, and old authored presets are untouched.

Ctrl+S / SAVE MAP writes `RMHarvestableDust-MissionNN.cfg` beside the executable.
L / LOAD MAP reloads the current mission's file. Mission changes automatically
load their own appearance. The old `RMHarvestableDust.cfg` remains an untouched
fallback for missions without a saved override; reset restores RM defaults for
the current mission only. Changes preview immediately. Ctrl gives
fine adjustments; Shift gives coarse adjustments. Brightness applies to RGB,
opacity to alpha, and to RGB coverage for ONE/ONE additive layers.

The move UI is captured after the world, so the presenter treats it as an
overlay rather than fogged/upscaled scene content. Circle tessellation has a
128-segment floor. Shift vertical positioning inverts the current projection
at fixed world X/Y rather than repeatedly warping the system cursor, using
stale projected endpoints, and multiplying absolute height by 2.75. Physical
cursor movement is not constrained; vertical destination tracks screen Y.
World movement limits and actual command simulation remain unchanged.

DXR instances now receive the compatibility rasterizer's eye-space clip plane.
Clipped instances force any-hit even for opaque materials; it rejects hidden
geometry before sampling diffuse/normal/emission, for both primary and shadow
rays. Unclipped opaque instances retain the optimized opaque path.

B/L/R presses with an active docked manager are routed once before legacy
FIB key regions can consume them. The existing own-panel toggle and
close-other/open-requested logic is reused; repeat is suppressed until release.

Automated checks cover projection math and integration contracts. Shader/build
and GPU smoke checks are necessary but are not a substitute for in-game visual
verification of clouds, Shift placement, hyperspace and all nine menu pairs.
No game is automatically launched.
