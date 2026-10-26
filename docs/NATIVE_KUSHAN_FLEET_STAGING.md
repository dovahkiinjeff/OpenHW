# Native Kushan fleet staging

Status: **all 32 existing R1 hull identities enabled in a deployed native-HOD
test build on 2026-10-06; visible gameplay validation remains pending.** Readers,
native animation sampling, visual subsystem composition and the
hierarchy-preserving renderer bridge are implemented. No gameplay stats, build rosters, save
identities or Taiidan assets are changed by this work.

## Implemented

- A native HOD2 fleet scene reader preserves source mesh/LOD records, indexed
  parts, material identity, UV0/UV1, hierarchy and joint rest poses. It does not
  flatten moving children into the hull or apply the interceptor's scale.
  Material parameter names, texture references/float4 values, image formats and
  mip spans are retained. Texture-pool decompression is explicit and lazy; it
  leaves the source archive unchanged.
- A native RM MAD reader preserves every clip, explicit track-to-curve indices,
  double-precision keys, tangents, infinity modes and loop ranges. Duplicate
  curve names are valid; duplicate clip names, ambiguous animated joints,
  nonfinite keys and invalid indices are rejected. Empty placeholder clips are
  retained as non-playable data. Its sampler evaluates indexed unweighted Hermite
  curves using XY tangent slopes and source seconds/radians, with exact clip
  endpoints. Missing channels preserve the source joint defaults. All audited
  Kushan curves use constant infinity; unfamiliar modes/tangents are rejected.
  Explicit HW1 event aliases now preserve the existing state clock and duration.
  This does not implicitly play unmatched clips or SPIN.
- A visual subsystem layer defines borrowed immutable source models and creates
  independent per-ship instances. Hull attachment anchors must be unambiguous.
  Shared turret models retain separate pose callbacks and visibility. A first
  instance freezes the definition; release is deferred while instances exist.
- Composition converts RM coordinates to HW1 coordinates once, retains source
  pivots and hierarchy, chooses available source LODs and emits material-qualified
  draw records. Pose callbacks accept HW1-local matrices for later gun/MAD
  adapters. No invalid pose submits a partial ship. The owning ship's world
  transform remains outside this layer.
- The local dependency planner parses audited Lua data calls without executing
  Lua or importing stats. Its explicit mapping covers 32 existing R1 hull
  identities. Attached models are parts of those ships, not new unit types.
- Native samples feed independent component poses; the renderer bridge copies
  them into each ship's `mhlocalbinding` matrices, not the shared static mesh.
  Reversal uses explicit phase, with no implicit SPIN playback. Bad topology or
  existing legacy gun/MAD callbacks reject the update before any matrix changes.
- `RemasteredMeshBridge.c` builds ordinary nonpacked in-memory `meshdata` with
  empty pivot/marker nodes and separate indexed geometry children. Material and
  classic binding-name resolution are explicit callbacks. Object limits, path
  lengths, finite scaled positions and allocation sizes are checked. Source
  scene pointers are not retained by the finished mesh. Texture registration
  remains in the existing engine path; this is not GEO serialization.
- The interceptor uses that bridge with a strict compatibility guard. Differential tests match its
  previous geometry, scale, UVs, badge-to-hull and engine material mapping. This
  old isolated prototype files remain intact; the deployed LOD redirect now uses
  canonical fleet paths. Fleet behavior has not yet been visually verified in-game.

- Owned visual assets retain hulls, animation data and shared dependencies until
  their last instance is released. Runtime-only mesh/ship registries hook mesh
  deletion, ship deletion, capture and save-load reconstruction without adding
  handles to serialized Ship identities.
- Existing MAD start/update/end and load paths sample native events. Native
  bindings bypass classic geometry-space curve callbacks. Tests cover collective
  support-frigate arms, pause/reversal phase and independent per-ship poses.
- Explicit gun-index/component aliases drive 34 moving turret controls plus 21
  fixed muzzle bindings. Tests require each moving axis to affect visible
  geometry, and fixed barrels to remain rigid. Cruiser slave muzzles retain their
  paired-model mapping. Projectile/missile origins use the native posed muzzle
  while preserving gameplay heading, damage, speed and classic MEX fallback.
- Reviewed engine markers drive trail offsets independently of camera LOD.
  Mothership retains classic four-trail offsets because its source hull contains
  only two nozzle markers; no fake four-to-two mapping is made. Its `Hangardoor`
  semantic explicitly maps to animated native `moshipdoor` for docking queries.
- Strict loose visual packages load only existing original R1 GEO/PEO hull paths. They
  contain source HOD/MAD, component-qualified material sidecars and explicit
  semantic bindings, never executable Lua or imported stats. Invalid packages
  fall back to the original mesh; source archives are unchanged. The ordinary
  renderer consumes copied in-memory meshdata, not a serialized GEO export.

Source archive reads are unchanged originals. Extracted data and audit outputs
remain in the ignored local `out/hwrm-kushan-native` cache, not repository assets.
Ship hulls come from HW1Ships.big; Kushan turret/engine subsystem dependencies
also occur in Homeworld2.big. Shared-model descriptors must resolve recursively.

## Findings / activation gates

1. Heavy corvette's `Make_Ambassador` MAD references `Weapon_Gun*_Position`,
   absent from the hull HOD. The guard detects this. Do not auto-run the clip or
   map its missing joints by substring. Actual turret models are dependencies.
2. Mothership contains two different `NavLight_Body04` leaf markers. Preserve
   their positions. Duplicate unused marker names are allowed, but references
   to an ambiguous parent, render anchor or animated joint are rejected.
3. Mothership door alias: RM `moshipdoor` versus HW1 `Hangardoor`; docking clips
   are RM `Open/Close` versus HW1 `HangarDoor_OPEN/HangarDoor_CLOSE`. NIS and
   door-state transitions require explicit adapters, including timing.
4. Support frigate's generic open/close clips are single poses and its
   `damageclose` placeholder is empty. Native individual `open1..4/close1..4`
   clips need collective mapping to the existing HW1 arm-state controls.
5. Resource controller has 120 indexed curves, but only 24 unique channel
   names. A dictionary keyed only by name discards authored animation data.
6. Several source hulls omit visible turrets and engine pieces, instead
   attaching subsystem HODs. Body-only activation is incomplete. Invisible
   HW1_ResearchModule metadata must not make a new visible or playable object.
7. Non-render SCAR/collision data is a bounded opaque pool tail. It is not
   another visual LOD and must not replace the engine's gameplay MEX collision.
8. Some unused source tangent frames contain NaNs on degenerate UV triangles.
   They are bounded/skipped, never published as shader inputs. Published
   positions/normals/UVs remain finite. A future tangent renderer must regenerate
   or validate tangent data rather than exposing these opaque source fields.
9. Heavy cruiser's source descriptor repeats `Weapon_Turret_Ion1` at two distinct
   anchors. Preserve that source weapon identity but qualify the independent
   visual instances by anchor. Never collapse them or fabricate another gameplay
   weapon. Repeated identical slot/anchor pairs remain an error.

## Remaining visible validation and adapters

- Verify each translated source material through the visible raster/DXR path,
  including matte cutout shadow coverage, paint brightness and emission.
- The bomber deliberately uses the rigid RM hull and fixed native muzzle
  markers, with gameplay aiming/firing unchanged and no invented pod animation.
  Heavy-corvette animation aliases and gun controls pass composition tests.
- Remap remaining salvage, repair and docking points with reviewed semantics,
  distances and directions. Classic collision/MEX contracts remain the fallback;
  the new door query alone does not claim complete docking-path conversion.
- Validate initial MAD states, NIS visibility and mission/save transitions in
  visible gameplay; do not substitute source-parser tests for this validation.
- Validate the deployed test candidate in normal visible gameplay before treating
  it as a release-ready replacement fleet.

## Material and multi-part animation continuation

`tools/native_hod_surfaces.py` uses the actual engine reader through a build-only
DLL. `tools/stage_kushan_native_materials.py` staged 108 material records across
48 unique mapped R1 hull/attachment models under the ignored local
`out/hwrm-kushan-native/material-cache/v2` directory. Selected surfaces are copied
into the deployed visual packages. On/Off thruster variants remain separate;
full-resolution diffuse, normal, packed specular, emission, paint shade and team
masks retain independent names, plus an RGBA LIF bootstrap with per-pixel masks.

Source shader checks established that ordinary `ship` glow uses packed G,
whereas `shipglow`/`badgeglow` use RGB glow with a separate specular texture.
`matteScissor` uses TEAM R >= 0.5 for coverage, not diffuse alpha or team paint.
Its translation has zero recoloring masks. Badge body surfaces remain solid;
badge decal projection is still a separate integration task. The untextured
`default` material on Light Defender retains its authored opaque grayscale
vertex tint through a neutral ramp, without paint or emission. Other unsupported
untextured colors/alpha are rejected. `innateSS` UI-effect proxies are suppressed
from surface draws, with their hierarchy joints retained. These are tested local
policies, not proof of complete shader parity or deployed fleet behavior.

The existing modern texture registration path now accepts `_emissive.dds`
sidecars. Valid matching-size sidecars override palette-inferred glow, preserve
RGB color independently of hull paint, and cannot emit through a diffuse cutout.
Existing assets without these sidecars retain their previous behavior. This is
build-tested but not yet visually verified on the replacement fleet.

Native subsystem instances can compose multiple explicitly selected clips
atomically. Four actual support-frigate arm clips pass open/close phase and
reversal sampling together. Conflicting overlapping tracks and invalid later
clips leave the old pose untouched. Callers still own HW1 event/state timing;
this does not auto-start SPIN or reinterpret old MAD/NIS events.

Exact joint-to-ship marker transforms are now exposed through the same animated
hierarchy, including subsystem anchors and model scale exactly once. Missing or
ambiguous names fail without writing output. This is the lookup foundation for
weapon and engine/door adapters. Remaining docking/repair/salvage consumers still
require explicit integration rather than assuming old and new joint names match.

`tools/stage_kushan_native_packages.py` calibrates each hull's scale against the
same longest classic hull axis, using actual native runtime hierarchy bounds.
It stages 32 source-HOD packages under `out/hwrm-kushan-native/package-cache/v2`.
The package audit loads all 32 through the runtime package reader. None is copied to deployment
by the staging tool. Thruster On/Off files exist in the material cache; current
package surfaces use On, not a completed runtime thruster-state adapter.

Version 2 shares identical source-model files and material sidecars between
repeated attachments while retaining independent component poses. Compared with
version 1 this removes 136 duplicate files (128,681,394 bytes), without resampling
textures or reducing geometry quality.

## Test deployment record

On 2026-10-06, all 796 version-2 package files (32 native manifests) and the built
executable were copied to the existing `RTX Deployed/Release` installation.
Every copied package file and the executable passed SHA-256 comparison against
the build. Executable SHA-256:
`5AF91B87669F8220741F4E2391ABACA8D69A4F7F57D42CC9D1A9DF5594D0532C`.

The previous executable and interceptor LOD file were backed up under
`out/deployment-backups/pre-native-kushan-fleet-20261006`. Only the executable,
new R1 native-package directories, and five interceptor mesh path redirects were
deployed. Original archives, other fleets, mission settings and gameplay data
were not changed. Canonical `.peo` paths are supported for mothership and heavy
cruiser activation as well as `.geo` paths. Invalid packages retain classic mesh
fallback. Classic docking, repair and salvage routes remain in use except for
the explicitly mapped native door query. The game was not launched by deployment.

The heavy-corvette MAD's two `Weapon_Gun*_Position` tracks are explicitly aliased
to hull `Hardpoint_Gun*_Position` anchors. Their starting transforms match to
float precision; the channels are hull-local, not attachment-local. Real-source
tests verify both muzzle positions move once through the attached gun hierarchy,
return to the starting pose, and remain independent between ships. Its two
classic gun indices now use independently controlled native gun components.
Unaliased asset creation still rejects the missing joints. The mapped sampler
can route tracks across components atomically, but no unreviewed frame conversion
or fuzzy joint renaming is enabled.

The scene reader now retains bounded HOD2 docking metadata: source path names,
parent, compatibility labels, link metadata, flags and full point records.
`tools/audit_kushan_native_docking.py` compares 93 authored paths across 32 R1
identities to classic MEX docking points without changing gameplay routes.
Support-frigate fighter port numbering is reversed between versions; carrier
fighter/corvette entries also differ. Nearest-point results are audit data only,
not automatic bindings. Truncated records, oversized counts and trailing data
are rejected without publishing a partial scene.

## Validation commands

### Native turret aiming correction (2026-10-07)

Combat follow-up: the original `BindGun` names also supply NewGimble gameplay
rest matrices. Native mesh replacement removed those matching hierarchy parts,
leaving zero matrices in zero-initialized GunStatic and preventing tracking/fire.
The native BindGun path now reads only original LOD0 GEO/PEO hierarchy metadata,
composes the original parent transforms, and preserves the original combat rest
frame and multi-level flag. Repeated child/recoil bindings do not overwrite the
first parent aiming frame. Rendering still uses native RM gun controls, not
classic callbacks. Construction/save binding reconstruction rebuilds current
NewGimble orientations from those restored frames and existing angles, repairing
affected loaded saves without rewriting them. Fixed/heading-only guns ignore
unused mechanical bindings. No damage, ranges, turret limits or AI changes.

Native gun poses now use the maintained gameplay `gunheading` in HW1 ship
space, rather than applying classic mechanical Euler angles to RM rest frames.
The adapter obtains the current animated native mount frames without existing
gun controls, solves the two-axis pose, and commits the complete gun batch
atomically. Recoil, projectile damage, firing arcs, cooldowns, and combat AI are
unchanged. Fixed hull barrels remain fixed. No ship/save layouts changed.

Source geometry ownership matters: corvette/support/assault/cruiser turret meshes
are rigid children of Position, so both rotations must move that visible mesh;
their Latitude marker alone is not an independently rigged barrel. The destroyer
ion assemblies and defender rigs retain their actual split-joint mapping.
Do not universally redirect elevation to a Latitude marker based on its name.

The actual-source audit exercises all moving mappings among 55 guns/32 R1
identities against eight headings, including rearward and axis-aligned headings,
and checks muzzle direction, changed visible geometry direction, ship isolation,
and invalid-batch rollback. Heavy-corvette Make_Ambassador samples also verify
that animated hull attachment frames retain their gameplay aim. This does not
prove heavy corvettes are firing in gameplay: opt-in muzzle diagnostics now
record heavy-corvette range/readiness/alignment and live attack/fuel/MAD states.


### Lighting correction and intact scaffold follow-up (2026-10-06)

The first native fleet gameplay run reached DXR geometry/shadow submission, but
the explicit emission sidecars carried opaque hull coverage in alpha, including
black non-emissive texels. The presenter uses this alpha to protect luminous
surfaces from lighting/shadow modulation, so ordinary hull texels were incorrectly
protected too. Runtime sidecar registration now sets protection coverage to zero
for black emission RGB, retains authored alpha for real glow, and clamps it by
hull cutout coverage. The surface staging tool emits the corrected mask for future
packages. Existing fleet packages do not need texture resampling or restaging.

The intact scaffold is a derelict, not one of the 32 R1 ship identities. It now has
a separate strict `HW_RM_SCAFFOLD_VISUAL_1` package contract and exact activation
at `Derelicts/Scaffold/Rl0/LOD0..3/Scaffold.geo` or `.peo` only. Its source is
`Homeworld2.big`'s `ship/der_scaffold/der_scaffold.hod`, calibrated against the
classic intact scaffold at scale 2.86361985. The package is rigid and rejects MAD,
attachments and semantic markers; no gameplay/NIS/collision/navlight data is
imported. Scarred scaffolds and other derelicts remain untouched. The scaffold's
native hierarchy reaches the same raster/DXR bridge as the fleet.

The corrected executable, matching PDB and 26 scaffold package files were deployed
and hash-verified. Executable SHA-256:
`CC10E0E846E807084034E88695414CD898E1019ABA4549F105468E04C448C7AA`.
Previous executable/PDB backup:
`out/deployment-backups/pre-hod-lighting-scaffold-20261006`.
The stopped save-loading correction was not included. No game launch was made.
Real scaffold package/bridge checks, all 32 fleet package checks, 13 CTest tests
and emission-mask Python/C++ checks passed; visible gameplay confirmation remains
pending.

```powershell
cmake --build out/build/deploy-x64-clean --config Release --target native_ship_hod_tests native_ship_mad_tests remastered_mesh_bridge_tests homeworld
ctest --test-dir out/build/deploy-x64-clean -C Release --output-on-failure
out/build/deploy-x64-clean/Release/native_ship_mad_tests.exe out/hwrm-kushan-native/source/ship
out/build/deploy-x64-clean/Release/native_ship_hod_tests.exe out/hwrm-interceptor-test/source/ship/kus_interceptor/kus_interceptor.hod out/hwrm-kushan-native/source/ship out/hwrm-kushan-native/hierarchy-audit.tsv out/hwrm-kushan-native/package-cache/v2
python -m unittest discover -s tests -p test_kushan_hod_bindings.py
python -m unittest discover -s tests -p test_kushan_native_materials.py
python -m unittest discover -s tests -p test_kushan_native_packages.py
python -m unittest discover -s tests -p test_native_visual_lifecycle.py
python -m unittest discover -s tests -p test_kushan_native_docking.py
python tools/audit_kushan_native_docking.py --reader out/build/deploy-x64-clean/Release/remastered_asset_reader.dll --source out/hwrm-kushan-native/source/ship --classic out/hwrm-kushan-native/classic-bindings.json --packages out/hwrm-kushan-native/package-cache/v2/manifest.json --output out/hwrm-kushan-native/docking-audit.json
python tools/plan_kushan_hod_bindings.py --root out/hwrm-kushan-native/source --output out/hwrm-kushan-native/binding-plan.json --hierarchy-audit out/hwrm-kushan-native/hierarchy-audit.tsv
out/build/deploy-x64-clean/Release/remastered_mesh_bridge_tests.exe out/hwrm-kushan-native/source/ship out/hwrm-kushan-native/hierarchy-audit.tsv
```

Source validation also passed for all 16 visible Kushan subsystem HODs extracted
from Homeworld2.big (50 HOD files including hull variations, plus 13 MAD files).
Passing the reader tests is not evidence of deployed fleet rendering/animation.
The headless bridge additionally passed all 32 existing R1 hull identities with
their visible attached models at LOD requests 0 through 4. Tests cover parent
links, copied-data lifetime, allocation canaries, shared-model independence,
hidden metadata, and actual mothership door samples reaching per-ship bindings.
All 13 MAD companions passed source sampling; valid hull animations also passed
composition and reversal checks, including explicit heavy-corvette hull-anchor
aliases. These tests are not a substitute for visible gameplay.
