# Particle-driven clouds

Shift+F11 â†’ FIELDS edits volumetric dust generated from actual Asteroid0
positions. Double-click any editable numeric inspector value to type an exact
number. Enter applies, Escape cancels; Ctrl+A selects the whole value and Ctrl+V
pastes a number. Decimal and scientific notation are accepted. Numeric entry
also covers rope/dust settings, volume properties and editable object transforms.
Particle Spread accepts 0 through 4, including values far below 0.25; zero hides
the converted density for that style. Saved particle styles retain float precision.

Clouds are generated from actual Asteroid0
positions. Detection first measures local spacing using eight nearest neighbours.
A sharp distance jump identifies isolated patches; mutual local reach connects
the branches of a larger formation without borrowing the spacing of a distant
cloud. The cloud count is discovered, never fixed to three or selected by mission.
The local graph is only the first pass: neighbouring sampling patches are joined
using their spatial extent and a map-wide spacing estimate before editor field
identities are assigned. Dense compact islands remain separate from a sparse
belt, and nearby tiny fragments inherit the island identity before the joins.
Reach is measured before joining, avoiding progressively expanding merges.

Each formation is then reconstructed using a broader bandwidth based on its
upper-quartile spacing. Particle kernels overlap to form a continuous body;
log(1 + summed contributions) controls local density, compressing bright knots
while retaining density variation. Shrinking Spread no longer multiplies density
by inverse radius cubed. Support is limited by the nearest other formation so
reconstruction preserves the gaps even at increased Spread. Broader kernels
have a finite contribution. Resource region boxes do not define the shape.

Select ALL PARTICLE FIELDS for the mission default, or a numbered connected
formation for an individual style. Particle Spread changes reconstruction
radius; Density Strength scales the resulting sum; Kernel Softness changes the
falloff. Color, scattering, absorption and world-space noise remain editable.
RESET STYLE restores the selected clump to the mission default. Ctrl+S saves
styles to the current map overlay. Actual geometry is rebuilt from the particles
when the map/save loads, so the original particles and gameplay are preserved.

Small sparse grids cache the density calculation. Their borders share the same
kernel samples and are not cloud boundaries. The renderer gives authored clouds
and ropes priority, then uses the remaining shared 256-volume budget for visible
particle-density tiles. Distant fields have no arbitrary 65,000-unit cutoff.

Previous PARTICLE_FIELD_REGION entries are ignored. A previous mission-wide
PARTICLE_FIELD default can be read; old per-region overrides are not assigned to
unrelated particle clumps. Existing manually authored volumes remain present and
can overlap the new density until the author disables or removes them.

Validation: CPU branching-formation separation, map-scale/order invariance,
continuous reconstruction, inter-cloud gaps at maximum Spread, additive-density,
proximity, empty-gap, tile-seam, minor-object
list, styles/persistence and dirty-cache tests; headless production-HLSL GPU
sampling test including packed storage, interpolation, support and linear gain.
These checks do not claim an in-game visual or performance acceptance result.

Offline mission-placement sampling is available through
tools/check_particle_formations.py and the particle_field_tests executable.
Twenty sampled seeds each resolve Mission 1 to three formations and Mission 2
to one. These samples use mission geometry/distributions, not a live RNG dump.

Converted density bricks use exact box support and a fixed sampling cadence
derived from their grid spacing, without the procedural-cloud distance LOD.
The ray marcher composites intersecting bricks in each pixel's ray entry order.
Per-brick normalization peaks do not activate high-density material boosts.
Headless production-HLSL tests cover those bounds, ordering, material scaling,
and grid cadence. Live camera-orbit appearance/performance remains a user check.

Original rope mesh controls: the ROPES inspector provides ORIGINAL ROPE COLOR, ORIGINAL ROPE OPACITY (0–100%, double-click to type), and RESET ROPE COLOR. These affect only the classic nebula mesh; RTX rope settings are independent when the color override is enabled. SAVE MAP persists ORIGINAL_ROPE_STYLE, and older maps default to the original appearance and 100% opacity.
