# Native engine material states

Native RM thruster surfaces retain both authored texture states. The normal
surface label uses Off. Its `_engineOn` sibling contains the On diffuse,
bootstrap LIF and companion maps (including emission and normal).
All native package staging paths preserve both states; other shader materials
do not receive engine variants. Source HODs remain unmodified.

The renderer preloads both materials and interpolates between them per ship.
The linear-movement flag targets active; dead, disabled, clamped and
trail-suppressed ships target idle. A full transition takes 0.75 simulation
seconds, reverses from its current value, and pauses with simulation time.
Repeated render passes do not advance it. This is an OpenHW transition duration,
not a claim of reproducing an RM timing constant. Propulsion/combat are unchanged.
The draw context resets after each ship, including sensor and offscreen shadow
draws. Preview models and wrecks remain idle.

Raster and ray tracing interpolate the diffuse/glow maps on the GPU. Ray-traced
surface variants reserve bit 0x100 for paired engine materials; the continuously
varying blend is instance data, never a new cache key. Emissive light proxies
interpolate their sampled colors with the same amount. There are no per-frame
texture uploads, intermediate texture sets, or geometry rebuilds. Both material
registrations are released with the owning mesh.

For the loose-data development install, `deploy_idle_thruster_textures.py
--active` stages the verified On siblings using cache manifests matched by
source HOD hash. It leaves idle textures untouched, and requires backups before
overwriting a locally modified active sidecar. It can run without `--apply`
for a read-only preview.
