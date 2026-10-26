# OpenHW runtime ship definitions

## Weapon presentation fields

Each `GUN` block in a ship or weapon-subsystem document owns its presentation
settings alongside the existing HW1 weapon statistics:

```
GUN 0
{
Type GUN_NewGimble
FireAnimation Weapon_Gun0
WeaponFire @/RemasteredFX/WeaponFire/kinetic_small.wf
DamageLo 40
DamageHi 45
FireTime 2.9
}
```

`FireAnimation` names the owning component's exact event timeline. `WeaponFire`
references the native weaponfire presentation data. They are separate channels:
some RM weapons have both, while others supply their muzzle through weaponfire
without a model-event animation. Neither field changes HW1 projectile physics,
firing cadence, health, armor or damage. Both fields are stripped from the HW1
gameplay-parser input and compiled into the same native asset definition. Do
not author the compiler's internal `weaponEvent`/`weaponFire` rows directly.

The native `.wf` reader consumes literal data assignments for the seven
presentation slots (fire, bullet, hit, anticipate, miss, nopenetrate,
blowthrough), sound names, authored clamp/NLIPS/SM settings, death type and scar
type. No Lua code executes. Original staged files are byte-preserved. Unknown
source fields are exposed as diagnostics, never guessed into aliases. The
reader does not manufacture fire-clamp/fire-NLIPS/fire-SM behavior absent from
the recovered RM loader.

The importer uses exact component markers and normal-activation source weapon
references to disambiguate ordinary firing from special-attack events. Source
speed/range/cadence/damage values are not imported. Ambiguous or unmatched
records remain in the import receipt for review rather than becoming a
first-match or gun-number heuristic.

## Native presentation events (development integration)

`presentation Hull hull_events.owevents` attaches a validated event set to a
ship hull. In a subsystem document, `presentation Hull events.owevents` attaches
events to that subsystem. Composing `StartShipHardPointConfig` changes the owner
to the individual attachment slot and resolves the event file relative to the
subsystem's directory. Copying a turret therefore shares immutable event data
while retaining a distinct attachment identity and playback state.

The importer retains the original `.events` bytes beside the runtime event set
as `*.source.events`, and records source/runtime hashes and unresolved source
references in `*.import.json`. The generated `OWEVENT2` representation contains
named timelines, their parent/loop/duration/threshold values, and each event's
marker list, FX, sound, scale, attachment options and presentation flags. Only
the current generated format is accepted; changed development formats require
regeneration. `gameEv` is retained as
opaque source metadata; it does not call into the HW1 simulation.

Event `animtime` is a normalized position in its timeline (0 through 1).
The scheduler converts that position using the timeline duration and the clock
provided by HW1. The authored `-1` activation sentinel is preserved separately
and excluded from ordinary timed bursts. Simultaneous actions are not merged.
Missing source timeline references are reported rather than assigned invented
durations. Unknown runtime formats, cyclic parents, invalid strings and
non-finite numeric values fail validation before playback.

The event reader, scheduler, asset ownership and local import are implemented.
Each native ship instance owns separate playback players keyed by component and
timeline. Playback takes the existing engine clock explicitly, emits negative
activation events on start and phase-zero events on the first advancing update,
supports explicit restart/stop, and rejects backward clock
steps. Copying a turret or spawning another ship does not share mutable playback
state. The sink receives the owning component, not only an effect name.
Connecting these timelines to every weapon, damage, death and animation cue,
and completing the rendering/sound dispatch, remains in progress. These fields
must not be presented as a completed replacement for the compatibility FX path.

## Version 2: integrated ship and subsystem documents (development)

`OPENHW_SHIP_2` keeps model declarations and the full HW1 gameplay text in one
file. `[Ship]` begins the gameplay section and `[End]` closes it. Do not add a
`gameplay` reference: the engine compiles its simulation view in memory. The
file limit is 1 MiB. This is a declarative format, not a Lua interpreter.

```
OPENHW_SHIP_2
type StandardFrigate
scale 2.83679399
hull hull.hod
mad -
# Include all hull surface and marker declarations here.
StartShipHardPointConfig(NewShipType, "Weapon_Gun0", "Hardpoint_Gun0", "Weapon", "Innate", "Ship", "MyAssaultGun");
weaponSlot Weapon_Gun0 0 0

[Ship]
# All existing ship settings belong here, followed by hull-owned GUN blocks.
# This abbreviated example is not a complete playable ship.
[End]
```

The seven hardpoint arguments are owner (`NewShipType`), unique instance name,
anchor name, role (`Weapon`, `Engine`, or `Utility`), `Innate`, `Ship`, and the
installed subsystem type. An anchor without `_Position` receives that suffix;
an explicitly authored hardpoint uses its exact parent joint instead.

`Subsystems/MyAssaultGun/subsystem.owsubsystem` contains its own configuration:

```
OPENHW_SUBSYSTEM_2
type Subsystem_MyAssaultGun
scale 1
hull model.hod
mad -
surface 0 0 m0
gun 0 Hull Weapon_Gun0_Position Weapon_Gun0_Position - Weapon_Gun0_Muzzle
[Weapons]
GUN 0
{
    Type GUN_NewGimble
    DamageLo 40
    DamageHi 45
    FireTime 2.9
    // Supply the remaining HW1 gun parameters for a complete weapon.
}
[End]
```

Weapons retain HW1 parameter semantics. No external RM `.wepn` file is read.
The existing `SoundType`/effect-table mechanism remains in effect; this patch
does not introduce new per-weapon named FX override fields.

`weaponSlot INSTANCE LOCAL_INDEX SHIP_INDEX` preserves imported weapon
identities. New copied mounts can omit it: their weapons receive unused
indices automatically. Sparse indices after removals are compacted together
in the simulation and visual bindings; legacy `BindGun` references are
remapped as well. Overlapping assignments, unknown subsystem weapons, and
missing joint bindings fail validation. The total limit is 64 weapons.

`hardpoint NAME HULL_JOINT X Y Z RX RY RZ` defines an editable mount. Translation
uses unscaled model units in HW axes, rotations are local degrees. An editor
copy creates a uniquely named mount and subsystem declaration while sharing
the immutable source HOD; it does not overwrite the original RM file.

`joint INSTANCE JOINT PARENT X Y Z RX RY RZ` stores an authored local joint pose
and parent within that component. `-` makes it a component root. It is applied
before combat aiming; source HOD bytes remain unchanged. Unknown joints and
cycles are rejected. Authored poses override animation on those specific
joints, so do not apply them to animated joints unless that is intended.

Version-1 subsystem library entries remain usable as **unarmed visual
templates**. Migrated weapon-bearing variants use version 2 and carry their
actual weapon settings. No subsystem becomes a separately targetable object,
receives health, or changes HW1 damage/repair behavior.

Setup generates 87 ship documents and 58 weapon-configuration variants from
the imported baseline, retaining the original gameplay values. Native HOD,
material, and LOD validation runs after migration. Repeated setup preserves
existing loose definitions. Definition updates check their previous hash and
refuse to replace subsequent local edits. Editor projects remain under
`Modding/Ships`; saving a project does not silently enable it in a campaign.

Coverage includes all shipped R1, R2, P1, P2, P3 and Traders ship gameplay
files. Missiles/mines are projectile definitions, not ship definitions;
derelicts and the default fallback are likewise outside this ship roster.
71 ships use native RM visual packages. The remaining 16 use `visual legacy`
and retain their existing LODFile/MEX visuals, but their entire gameplay and
weapon configuration is embedded in the same v2 format. This does not claim
to provide new RM models for ships that have not received them.

Shipped updates are resolved from `Data/RACE/TYPE/RMNative/ship.owshipdef`
before the archive, with enabled mods and explicit root overrides retaining
priority. The asset checker's `OPENHW_VERIFY_SHIP_ROSTER=1` mode verifies this
production lookup for every shipped ship and compares all compiled gameplay
values with the imported baseline. Set `HW_PORTABLE_ROOT` to the tested root.

The migration also checks actual hull HOD joints for weapons not owned by a
subsystem. It supplies the missing numbered muzzle bindings for the Kushan
drone and assault frigate's two fixed guns. The migration report records each
added binding and any unresolved hull weapons. The Kushan mothership retains
five legacy weapon placements: its RM definition uses six differently named
`Weapon_HullDefense` mounts, so no automatic one-to-one substitution is made.

Multiline hardpoint declarations are normalized for editor import without
changing their meaning. Saving an integrated definition validates its models,
subsystem references and bindings first. Project Save also performs this check
before replacing project files. Individual files have backups, but the project
and definition are not yet a single crash-atomic transaction; an I/O failure
is reported as an incomplete save rather than a successful save.

## Version 1 compatibility reference

The runtime reads `RACE/TYPE/RMNative/ship.owshipdef` before the older
`visual.rmnative` manifest. This is distinct from the editor project
`Modding/Ships/PROJECT/ship.owship`. The project preserves authoring state;
the `.owshipdef` is the definition loaded by the game.

Each file begins with `OPENHW_SHIP_1`. Fields are whitespace separated and
case-sensitive. Blank lines and lines beginning with `#` are ignored. Paths
are relative to the definition's folder, using forward slashes, with no
absolute paths, parent traversal or whitespace. Maximum size is 128 KiB;
maximum line length is 1 KiB. Unknown and duplicate directives fail validation.

Example for a Kushan Assault Frigate (abbreviated surfaces):

```
OPENHW_SHIP_1
type StandardFrigate
scale 2.83679399
hull hull.hod
mad -
gameplay gameplay.shp
attach Weapon_Gun0 Hardpoint_Gun0_Position 1 component1.hod
attach Weapon_Gun1 Hardpoint_Gun1_Position 1 component1.hod
attach Weapon_Gun2 Hardpoint_Gun2_Position 1 component1.hod
attach Weapon_Gun5 Hardpoint_Gun5_Position 1 component1.hod
marker engine_0 Hull EngineMainDrive
gun 0 Weapon_Gun0 Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Muzzle
gun 1 Weapon_Gun1 Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Muzzle
gun 2 Weapon_Gun2 Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Muzzle
gun 5 Weapon_Gun5 Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Position Weapon_Gun0_Muzzle
surface 0 0 c0_m0
```

The complete setup-generated definition includes all visible materials. The
example is not a complete replacement package by itself.

## Model and gameplay

- `type ID`: exact existing game identity. Kushan uses `StandardFrigate`,
  Taiidan uses `Taiidan_StandardFrigate`, Raiders use `P1StandardCorvette`,
  Kadeshi use `P2MultiBeamFrigate`, wrecks use `Derelict_Scaffold`, etc.
- `scale FLOAT`: positive finite scale from RM model units to classic ship
  units. Setup calibrates it against the original hull dimensions.
- `hull PATH`: native HOD; geometry and embedded joints remain native.
- `mad PATH` or `mad -`: optional native animation companion.
- `gameplay PATH`: complete HW1-style gameplay definition. Required for ships,
  omitted for rigid derelicts. The engine loads this member for stats, guns,
  docking rules, navigation lights, trail settings and animation state rules.
  Relative resource callbacks retain the original race directory semantics.

The paired `gameplay.shp` is text, not executed Lua. It uses the existing
documented Classic ship fields and `GUN` blocks. Add a new gun block there,
update its declared gun count, and add a matching `gun` binding below. Damage,
range, firing cadence, projectile kind, tracking limits and effects remain
gameplay fields. The native binding supplies its visual pivots and emitter.

## Attachments and joints

`attach SLOT HULL_ANCHOR ROLE HOD_PATH` mounts a component to a unique joint
in the hull. Roles are 1 turret, 2 engine, 3 other system. Each slot name is
unique even when several mounts share the same HOD bytes. `Hull` is component
zero. A joint belongs to its component namespace, so two gun components may
both contain `Weapon_Gun0_Position` without ambiguity.

An attachment uses the source HOD joint hierarchy. Children inherit movement
and rotation from their parents; a barrel must be parented beneath its actual
pivot. Do not infer game gun numbers from `Hardpoint_GunN` or old
`StandardFrigateGun.N` names. The explicit mapping establishes that relation.

`gun INDEX SLOT AZIMUTH DECLINATION RECOIL MUZZLE` maps one gameplay gun
index (0..63) to its native component and joint names. `-` disables a motion
channel. `MUZZLE` is mandatory. Motion channels referring to the same pivot
are intentionally combined by the existing aiming solver.

- Fixed gun: `gun 0 Hull - - - Weapon_Gun0_Muzzle`.
- One-axis hinge: `gun 0 Hull Weapon_Gun0_Position - Weapon_Gun0_Position Weapon_Gun0_Muzzle`.
- Split turret: azimuth uses Position, elevation/recoil uses Latitude when
  the actual barrel geometry is parented below Latitude.
- Shared turret model: each gameplay gun uses the correct attachment slot,
  while internal joint names can remain identical between attachments.

Bindings are validated against loaded joints before the first instance is
created. Missing joints, duplicate gun indices and partial animation mappings
reject the whole visual package. Version 1 never applies the old compiled
gun/event tables on top of these explicit mappings.

`marker SEMANTIC SLOT JOINT` exposes an animated point through the existing
native marker API. Reviewed engine aliases are `engine_0`, `engine_1`, etc.,
and the Kushan mothership uses `Hangardoor`. Custom marker semantics can be
queried by tools; naming one `dock_0` alone does not replace Classic collision,
navigation or docking routes. Those gameplay routes still come from the
gameplay definition and original MEX data. Editing a visual marker does not
automatically reauthor an entire approach/exit path.

## Shared native subsystems (HW1 attachment semantics)

`subsystem SLOT HULL_ANCHOR ROLE TYPE` declares an installed subsystem at a
named hull hardpoint. This follows the relationship expressed by RM's
`StartShipHardPointConfig`: the ship owns the mount; the subsystem owns its
reusable native model/material definition. It is not an external binding table
and does not execute RM Lua.

```
subsystem ExtraTurret Hardpoint_Gun0_Position 1 kus_assaultgun
mount ExtraTurret 0 0 0 0 45 0
```

The runtime resolves `Subsystems/TYPE/subsystem.owsubsystem`, whose header is
`OPENHW_SUBSYSTEM_1`, identity is `Subsystem_TYPE`, scale is `1`, and model is
given by `hull`. `surface 0 MATERIAL LABEL` declares its local material files.
Source HOD joints and hierarchy remain native. Marker/effect-only innate
components may have no opaque geometry; the editor shows their placement
gizmo rather than inventing a mesh. Unsupported material contracts are excluded
from the generated catalog and reported in `Subsystems/import-report.json`.

`mount SLOT X Y Z RX RY RZ` adds a rigid offset from the hull anchor. Translation
uses model units (the owning ship scale is applied exactly once); rotation uses
degrees around local HW axes. All component children inherit the mount pose.
Missing assets/anchors, invalid transforms and duplicate mount declarations
reject the package. Existing `attach` directives remain backward-compatible.

HW1 subsystem attachments have **no independent health, selection, targeting,
destruction, health bars or repair/rebuilding**. They are part of the owning ship.
Placing a model alone does not invent a new weapon's gameplay values. A firing
weapon still requires a gameplay `GUN` block and an explicit `gun` declaration
in the owning ship definition; the same slot/joint API handles its aiming and
muzzle. Automatic interpretation of RM weapon Lua is not implemented.

In the ship editor, select a mount joint, click **SUBSYSTEM**, choose an available
type, then use the existing move/rotate handles and numerical fields. Copy/paste,
undo/redo and project save/load preserve subsystem identity and transforms.
Save writes the generated declarations into the project `ship.owshipdef`, inside
a marked block; prior authored content is preserved. The ship-file window now
opens the runtime definition first, exposing its joint references rather than
defaulting to the unrelated original `.shp`. Project save is still not live
gameplay installation: publish the definition through a mod and reload.

Initial setup generates the HW1 library from the user's Collection. Hiigaran
and Vaygr subsystem libraries are excluded. The generated retail models and
textures are local installation outputs, not distributable installer payloads.
The packaged 1.0 installer does not support the old development
`--refresh-subsystems` command; successful setup removes its temporary sources.
Use the ship editor and mod workflow for user changes. Rebuilding the library
requires the source tools and original Collection assets.

## Materials and LOD

`surface COMPONENT_INDEX MATERIAL_INDEX LABEL` selects one material namespace.
The folder contains `LABEL.lif` as a legacy bootstrap and full-resolution
`LABEL.dds`, with optional `LABEL_normal.dds`, `LABEL_specular.dds`,
`LABEL_emissive.dds`, `LABEL_paintShade.dds`, `LABEL_teamEffect0.dds` and
`LABEL_teamEffect1.dds`. The exact sidecar names follow the current renderer.
Component indices follow attachment order, beginning with hull zero. A shared
material label may be reused by identical source model/material pairs.

Native HOD LODs are selected by the existing camera-distance system. All
requested LODs 0..4 are validated; existing missing-level fallback is retained.
This format does not turn painted stripes into emission or use the map
normal-map generation policy on RM materials.

## Animation

`event GAME_EVENT CLIP [CLIP ...]` maps an existing gameplay state event to
one through four exact MAD clip names. Example:

```
event HangarDoor_OPEN Open
event HangarDoor_CLOSE Close
event open open1 open2 open3 open4
```

`animation_alias SEMANTIC Hull JOINT` redirects an explicit MAD hull-local
target to an attachment anchor. `animation_clip CLIP` retains the reviewed
four-clip allowlist for the Taiidan Heavy Corvette; it is not a general clip
filter in version 1. Empty/nonplayable source placeholders are not guessed.

## Editor and mod workflow

The floating ship text editor can import/export `.owshipdef` and gameplay
text. Save definitions and referenced assets externally, then install them
under the corresponding `mods/MOD/RACE/TYPE/RMNative` folder and enable MOD in
`mods/load-order.txt`. Restart/reload the game so static ship data is rebuilt.
Editing a file does not mutate an already-running mission or old save layout.

Setup writes editable copies of the generated definitions and gameplay files
to `Modding/RuntimeDefinitions`. These are templates for mods; the base runtime
copy lives in `OpenHW-HW1.obig`. Project `ship.owship` saves are not silently
installed into gameplay.

Version 1 supports modifications of existing archetypes and additional guns
on them. Completely new ship identity/enumeration slots, binary plugins,
automatic authoring-project installation, dynamic build rosters and automatic
conversion of every docking/repair/salvage route are not implemented by this
format. The definition exposes the current runtime contract without claiming
that unfinished systems already work.

Implementation: `RemasteredVisualPackage.cpp` parses/validates the visual rig;
`Universe.c` selects the gameplay member; `Mesh.c` loads the native package;
`RemasteredVisualAsset.cpp` owns bindings and per-instance aiming/animation.
