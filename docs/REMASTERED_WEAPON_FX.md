# Kushan Remastered weapon visuals

The first weapon-FX translation pass reads effect definitions and original texture
pixels from the installed `Homeworld2.big`. It builds a separate visual package;
it does not edit the archive or import Remastered damage, targeting, firing rates,
projectile physics, ship statistics, or save-game structures.

Only R1/Kushan ships using the native Remastered hull path are eligible. Classic
Kushan hulls and every other faction keep their existing visuals. Missing or
invalid packages also retain existing effects.

## Import

From the repository root, after staging the native Kushan source files:

```powershell
python tools/import_kushan_weapon_fx.py --archive 'F:\SteamLibrary\steamapps\common\Homeworld\HomeworldRM\Data\Homeworld2.big' --ships out/hwrm-kushan-native/source --work out/hwrm-weapon-fx/source --output out/hwrm-weapon-fx/runtime/RemasteredFX
```

Place the resulting `RemasteredFX/Kushan.rfx` beside the game executable, retaining
the `RemasteredFX` directory. `import-report.json` records every gun mapping,
effect name, limitation, and package hash. The current import contains 61 gun
mappings, 53 effect clips, 779 layers, and 39 textures.

## Runtime behavior

Translated kinetic/plasma projectiles, ion-beam ribbons, muzzle flashes, and
supported impact components use authored textures and size/color curves.
Animated atlas cells retain their source resolution and order. Sustained beams
draw from the current native muzzle joint while preserving the simulation's
collision endpoint. Existing analytic weapon lights remain authoritative.

Transient effects have bounded event and layer budgets. If an event cannot be
spawned, its caller retains the original effect. Reset/load clears transient
events; shutdown releases textures and package memory. No new saved ship or gun
pointers are introduced.

## First-pass limitations

This is not a complete Remastered FX engine. Combo, billboard-ring, beam, and
sampled spray components are supported. Mesh-ring components and trails are not
yet translated; supported siblings in a combo can still render. Non-billboard
ring orientation/spin, beam noise/arc, and all spray-property curves are not yet
reproduced. Spray emission is sampled deterministically and capped at 24 particles
per emitter. Random-bitmap animations currently select the first authored bitmap.

Effect clips exceeding 512 layers retain the original event, including several
large plasma and missile impacts. Missile body/trail rendering is unchanged.
Carrier and mothership muzzle events without a matching source event keep the
original flash. Mothership defensive guns share the source defensive weapon FX
family; this does not remap their gameplay joints.

Projectiles and impacts depth-test against scene geometry. Attached muzzle
components honor the source `UseDepthTest` setting so a flash at the gun surface
is not cut away by its own hull. The pass disables inherited fixed-function fog
and scissor clipping, restoring the previous state afterward. Animated texture
cells use half-texel insets to avoid neighbouring-cell bleed.
Muzzle bursts are attached to the live native muzzle and current gun heading,
without snapshot-velocity extrapolation. Their runtime-only owner ID and creation
stamp prevent stale references after death, docking/hiding, or ID reuse. Missing
native joint queries fall back to the firing point stored in ship-local space.
If the legacy ID lookup misses a live owner, muzzle attachment resolves against
the current outside-ship list using both ID and creation stamp. Hidden/dead ships
remain excluded. This fixes the captured case where 395 of 430 spawned flashes
were immediately discarded despite their owners firing on the render list; it
does not modify save data or the simulation's ID table.

## Verification

Build targets: `homeworld`, `remastered_weapon_fx_data_tests`, and
`remastered_weapon_fx_runtime_tests`. Run the two FX tests with `Kushan.rfx` as
their sole argument. The data test covers malformed packages; the mocked runtime
test covers faction/hull isolation, drawing, state restoration, resets, and
texture lifetime. Run `python -m unittest discover -s tests -p
test_remastered_weapon_fx.py` for importer and integration checks.

These tests do not replace an in-game GPU visual check of turreted ships, beams,
and large engagements.

## Targeted interceptor diagnostics

Launch with `-muzzleFxLog -noRendererLog` using **Homeworld Modern - Muzzle FX
Diagnostics**. This does not enable benchmark mode or change visual settings.
Repeat the comparison between a visible flash and a missing flash, including
stationary/moving fire and camera rotation, then close normally.

The dedicated buffered CSV is `Diagnostics/muzzle-fx-<timestamp>-<pid>.csv`
under the shortcut's working directory. It contains R1 HeavyInterceptor physical
shot events (including render-list rejection), RM spawn/rejection, live ship/gun
resolution failures, joint fallback, clip expiry, and sampled draw submissions.
Draw rows are limited to ten per simulation second per event; all rows are capped
at 100,000 per session. It flushes once per simulation second and on reset/close.
Ship ID, speed, LOD, flags, world origin/direction, camera
distance/view alignment, submitted quads, and positive-color layer counts help
correlate the focused interceptor. No gameplay pointers or save fields are added.

`positive_layers_submitted` means CPU-side nonzero effect geometry was submitted,
not that the final GPU image contains visible pixels. Zero samples or no shot
rows must not be interpreted as proof of GPU occlusion. Without `-muzzleFxLog`,
this diagnostic path creates no file.

Cast-light follow-up diagnostics also include each gun-triggered light's world
position, radius and intensity, plus the gun count, angles and visual scale.
`native_binding_failed` records the precise RM attachment failure (throttled to
one per ship/gun per simulation second). This distinguishes a missing or invalid
native gun pose from a light that exists but is occluded or out of range. These
additional counters do not change visual or lighting behavior.

The cast-light capture confirmed identical positive light range/intensity for
all ten interceptors, but invalid mechanical gun angles rejected nine native
muzzle poses. HW1 only maintains `angle`/`declination` for `GUN_NewGimble` guns.
The native adapter now uses those fields only for that type; other guns retain
their authored mechanical rest pose and keep aiming effects/projectiles with
the live `gunheading`. This also handles unused garbage fields in existing saves
without editing saves or relaxing the subsystem's valid-pose checks.

### RM curve compatibility follow-up (2026-10-08)

Checked [Karos Graveyard FX variables](https://github.com/HWRM/KarosGraveyard/wiki/Variable;-FX)
against the installed retail scripts. Particle_Scale is a birth-time multiplier
based on emitter age; Emitter_ScaleSpeed independently controls speed scaling.
The translator previously applied Particle_Scale and integrated rate curves
only to smoke-named sprays. All sprays now sample scale, speed, deviation and
volume/offset curves at birth and integrate the emission rate, still bounded to
24 particles per spray. Nested particle sizes no longer multiply travel speed
or the parent emitter offset. No global radius multiplier was introduced.

Weapon event delays are retained. Beam muzzle clips are proportionally timed to
the Classic beam firing window, matching the existing beam projectile visual
policy, so the RM charge reaches its peak while the weapon fires. This is a
compatibility timing adaptation, not a change to weapon damage or firing rate.
Other muzzle clips and impacts retain authored seconds. The P1 missile impact's
explicit, user-requested 10x adjustment remains unchanged.

Remaining limitations: the documentation does not define weaponfire hit_clamp
scaling sufficiently to implement it confidently. It is not implemented by this
fix. Gravwell/drag particle dynamics, random radius modifiers, and beam noise
also remain approximate/unsupported; this pass must not be described as a
complete pixel-identical RM FX implementation. Regression checks cover curve
sampling, independent speed/size, event delay, beam expiry and native LOD0-4 FX.
