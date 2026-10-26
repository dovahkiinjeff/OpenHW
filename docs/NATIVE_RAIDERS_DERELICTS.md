# Native Remastered Raiders and derelicts

The existing five P1 identities load source HODs directly, with component-qualified
full-resolution diffuse, normal, paint and emission textures. No RM gameplay scripts,
ship statistics, abilities, construction identities or save layouts are imported.
Kushan, Taiidan and Kadeshi activation scopes are otherwise unchanged.

## Combat and animation contracts

- Standard corvette: both visible turret meshes are children of Weapon_Gun0/1_Position.
  Latitude joints own the muzzle markers, not barrel geometry. Combined native aiming
  controls Position, preserving actual visible barrel/muzzle alignment. The original
  P1StandardCorvetteTurret(1)/(2) combat rest frames are read from original PEO metadata;
  original combat limits and firing logic remain intact.
- Fighter, missile corvette, ion-array frigate and carrier have fixed visible weapons;
  source muzzle markers are mapped to all 18 existing gameplay gun indices.
- Ion-array frigate retains all four Array_Open/Close and damaged-state MAD events,
  driven by the original game animation clock. No replacement animation starts itself.
- Carrier: the source has three duplicate, self-parented engine export helper names.
  The reader disambiguates exactly these three helpers when P1Mothership is present,
  retaining outer attachment anchors and all source transforms. Other ambiguous or
  cyclic hierarchies still fail closed. The innatess engine effect proxy never becomes
  opaque raster or ray-tracing geometry.
- Existing collision, docking, repair and salvage gameplay metadata remain unchanged.
  Native engine markers are explicitly mapped to existing engine trail indices.

## Derelict coverage

41 existing derelict mesh identities have matching RM HODs, including the intact,
scarred and broken-finger scaffold, Khar-Selim wreck, prison ships, junkyard wrecks,
crate and individual debris pieces. They use rigid, qualified Derelict_<type>
packages; a derelict Carrier cannot activate the Kushan Carrier gun/animation map.
Each original model is independently scale-calibrated against its classic hull.
Scissor coverage uses source TEAM R, never paint or emission. Matte and older Ship
shader spellings retain their actual packed glow semantics.

Eight identities have no matching derelict HOD in the installed RM archive:
IonCannonFrigate, ResourceController, SalCapCorvette, StandardFrigate,
M13PanelA, M13PanelB, M13PanelC and LifeBoat. These retain their original models;
an unrelated intact fleet ship is not silently substituted as a wreck.
Existing planet/background replacements and the invisible hyperspace-gate dummy
are deliberately untouched.

## Local staging and checks

`tools/stage_raiders_derelicts_native.py` stages only these explicit identities under
`out/hwrm-raiders-derelicts-native`; it does not deploy or modify source archives.
Its manifest records original bounds, scale, source hashes, joints, classic markers,
bindings, MAD clips and explicit unmatched fallbacks. Reader DLL is build-only.

NativeShipHodTests `--raiders-derelicts <cache>` validates all 46 packages at LODs 0–4,
all 18 guns, both moving turret axes, eight heading directions, instance isolation,
failed-control rollback and visible ion-array MAD movement/reversal. R1 tests remain
strictly 32 packages / 55 guns. CTest and material/lifecycle regression checks also
run. These are headless checks, not proof of in-game combat or cutscene appearance.

Dust clouds, mission authoring and settings are not changed by this work.

## Corvette visual follow-up

The standard and missile corvette now retain each mesh's highest-detail RM
record at every camera LOD. Material-compatible reduced records still changed
UV/detail visibly, so they are no longer selected on these two hulls. The
ion-array frigate is excluded from this policy and the added FX mappings.
The standard corvette uses a single-axis hinge on both mirrored mounts. Its
source Direction marker is local RM +Y, converted to native HW +X; copying the
classic channel named Declination onto native local Y tipped the pods into the
hull. Heading is now projected onto the actual hinge plane. The mount normal
is invariant and a second rotation channel cannot move either pod.
Other NewGimble visuals follow physical tracking rather than snapped shot aim.

The shared weapon-visual package adds four Raider corvette gun mappings. These
are independent of camera LOD. Authored missile-launch steam is translated from
RM spray/ring data, including the zero-at-ignition rate curve and inherited
emission-time velocity. Steam particles detach; flashes remain gun-attached.
Corvette analytic muzzle lights are moved slightly beyond recessed RM markers
to avoid self-shadowing; projectile and flash origins are not shifted.
Missile-corvette impacts use a compact RM flash/cloud clip with final nested
particle scaling capped, short lifetime and no capital spikes/panel debris.
The final visual size is 10x the first compact revision, as requested.
This visual adaptation does not change missile damage or other ships' impacts.

Special-attack gameplay is explicitly deferred at the user's request. No save
layout, mission settings, dust rendering or ion-frigate FX changes are included.
`deploy_raider_corvette_fx.ps1` backs up and overlays only the executable,
symbols and weapon visual package, and refuses to run while the game is open.
