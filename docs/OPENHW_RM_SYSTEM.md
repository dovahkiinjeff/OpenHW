# OpenHW native RM visuals and system FX

This work preserves HW1 gameplay, saves, ship statistics and mission authoring.
Native Kadeshi visuals cover P2Swarmer, P2AdvanceSwarmer, P2FuelPod,
P2MultiBeamFrigate and P2Mothership. HODs are read by the native asset reader;
they are not converted to Classic GEO. The multibeam weapon mount is an
articulated native attachment, not a guessed fixed muzzle.

The combined `RemasteredFX/Kushan.rfx` filename is retained for compatibility;
its contents now cover Kushan, Taiidan, Raiders and Kadeshi weapon events,
engine trails, ship destruction, harvest/repair beams, missile exhaust and
four resource dust-cloud types. ETG event tables also route general damage,
death and impact effects to RM clips, while retaining their gameplay lifecycle
and lighting. Existing weapon smoke uses detached particles; muzzle flashes
follow the live gun. Native RM normals bypass the generated-normal adjustment.

Ordinary mission dust and nebula ropes remain unchanged. The former editable
harvestable volume system is disabled and its original code/preset retained in
`deferred/harvestable-dust-pre-rm`. Deployment moves the old release preset into
the dated deployment backup, rather than deleting authoring data.

## Reproducible local import

Use legally owned `Homeworld2.big` and Classic `homeworld.big` with
`tools/stage_kadeshi_native.py`. Use `tools/import_kushan_weapon_fx.py` with
`--system-fx --resource-dust --reader` and all four staged fleet source paths.
The importer parses data only, does not execute retail Lua, and rejects unsafe
paths, unexpected identities and malformed packages. `tools/deploy_openhw_rm_system.ps1`
copies only the five P2 visual packages, compiled executable/debug symbols and
combined FX package. Existing settings paths and executable name remain stable.

`audit_local_import_release.py` and `stage_local_import_release.py` exclude
RMNative and RemasteredFX, in addition to the earlier imported assets. They
must not be bundled in a public installer; generate them from owned archives.

## Compatibility adaptations and remaining fidelity limits

This is not RM's renderer transplanted verbatim. RM exhaust textures, widths
and faction palettes drive ribbons along the HW1 trail history. The full RM
engine-burn shader is not reproduced. STYLE_TRAIL follows a translated straight
particle trajectory; bitmap animation selects its first image. Native FX mesh
geometry is supported for neutral-root effect HODs, including audited `None`
and `NULL` parent exports. Data-only AHOD mesh-choice lists select a stable
source mesh per emission; RM source-position lens-flare hotspots are supported.
Generated death/damage
locations absent from the HOD use deterministic native hull-bounds adapters,
not proven identical RM surface locations. Repeated spray particles are capped
while retaining distinct components; event and rendering budgets are bounded.

Unsupported source effects remain explicitly listed in `import-report.json`.
Special-purpose effects without a verified RM counterpart retain Classic
rendering; this includes some cloaking, lightning and special-ability effects.
RM visual transfer does not replace HW1 hyperspace gameplay or cinematics.
These limits must not be described as complete, pixel-identical RM FX parity.

Build/Research/Launch panels are nonmodal outside their own region. B/R/L toggle
the matching open panel or switch from the other panel; Classic tutorial modal
behavior is retained. The window and main menu are branded OpenHW.
# Audio migration work in progress (2026-10-14)

The development runtime now accepts RM PCM WAV music and Relic v1.6 AIFC/FDA
audio. Its decoder uses bounded coefficient reads and local transform state so
asset decoding does not race the old mixer's FFT scratch storage. PCM output is
44.1 kHz stereo; the original 22.05 kHz mixer is interpolated into that output
without changing its clock or the simulation.

The source corpus check decoded 5,056 FDA files. Twenty-two files contained 25
invalid coefficient blocks; the decoder silences and counts those blocks while
preserving duration and continuing with valid following audio. Valid files were
compared across all samples against the existing codec, with a maximum signed-16
PCM difference of one. A separate vgmstream r2117 reference decode confirmed
silence recovery for the examined speech-file error. That tool is an ignored
local verification dependency and is not part of the runtime or installer.

RM music is mapped to 45 existing HW1 track IDs. Local setup stages those files
inside `RemasteredAudio/music` in the generated OBIG input. Credits and the two
old logo track IDs remain explicitly unmapped. Original streams still provide
migration compatibility and speech until the remaining replacements are wired.
All 45 source tracks passed the real runtime WAV conversion/lifecycle check.

SFX, speech and music user gains now apply independently at the mixer. Scripted
fades retain their own gain. PCM stream handles participate in the existing
volume, fade, stop, completion and reset APIs. Music decoding/import is connected;
speech selection, full ambient/lifecycle coverage and the final
asset update are not yet complete. No in-game listening test has been performed
for this migration and this work has not been deployed to the portable install.

The shared PCM loader now serves both music streams and a bounded SFX voice
pool. Effects are precached on the main thread (512 entries, 256 MiB decoded
budget), then played from immutable PCM using up to 64 simultaneous voices.
Firing commands do not decode or open files. The audio callback allocates
nothing, applies the SFX bus gain and stereo pan, and saturates mixed samples.
Reset detaches voices before freeing clips; stale clip/voice handles are not
reused. Tests cover polyphony exhaustion, pan, gain, saturation, completion,
cache reuse and reset. Native model events and weaponfire FIRE sound references
now reach this backend through the game event consumer described below.

The portable audio importer stages all 313 original ETG FDA samples and their
167 companion/container Lua files without recompression. The FDA corpus check
reports zero failures/concealed blocks and 162,224,128 decoded stereo bytes;
the complete source set fits the bounded cache. This staging has been tested
in an isolated output directory, not applied to the installed game.

The RM weaponfire import boundary now parses all seven presentation slots,
their FX/sound references, and the consumed per-slot clamp/NLIPS/SM fields.
All 58 extracted definitions pass. Unknown/misspelled source fields are reported
instead of becoming undocumented aliases. This replaces the old regex extraction
of only bulletfx/hitfx, but the old flattened FX renderer still does not consume
the full set; retaining data is not equivalent to implementing its rendering.

The replacement FX graph index (`OWFX0001`) now preserves typed properties,
curve rows, combo/spray dependencies, mesh/texture names and frame timings
without pre-generating particles. All 1,700 source FX definitions pass typed
parsing. This does not mean every source graph has valid dependencies: unused
source test effects contain missing references. A broad HW1-family event plus
all-weaponfire seed audit resolves 135 of 136 seed expressions; the remaining
source `emp.wf` uses `emp_pulse_combo.lua` where other weaponfire references use
effect identities. No runtime filename alias has been added to hide that.

The engine graph reader checks ownership, bounds, duplicate identities,
dependency cycles/depth, typed curve dimensions and string offsets. Curve
sampling preserves authored radii instead of multiplying in a hull radius.
Named source curve entries are ordered by time during import; raw source hashes
remain in the receipt. Integer-valued BOOL data (including shipped value 99)
is retained, not rewritten. Event FX choice lists preserve duplicate weights,
choose one comma token before lookup, and do not fan out; this follows RM
`0064fa97`/`0064fa5e`. A missing selected name does not choose another effect.
The graph now drives a preallocated live controller and the game event consumer.
Sprays retain fractional counts, source Force_First behavior, separate particle
size/speed and moving emitter history. Free particles do not follow a moving
muzzle; local and gravwell dynamics have separate update paths. BirthSpawn_Fx
children retain their owning FX instance identity. Natural ring completion can
emit Spawn_Fx. Trail Length is a time window with retained movement history,
not beam distance. The old flattened renderer remains disabled for ship combat
FX; no installed-game files have been changed by this migration.

Setup collects FX from the selected native model/event snapshots and their
literal weapon mounts, not every unrelated HW2 weapon in the source archive.
The reviewed fleet selection covers 143 models, 413 FX graph nodes, and 242
original resources (150 TGA images, 54 HOD meshes, 34 animation lists, two mesh
lists and two flare lists). Original image/mesh bytes are copied unchanged.
One internal resource index retains atlas cells/timing, HOD choices and flare
hotspots. It is not another public authoring grammar. The renderer precaches
resources before play; drawing does not read/decode files or load HODs.

Native point-light intensities are tagged separately from legacy ETG lighting
units so the backend does not apply the legacy 0.02 conversion to authored RM
values. Weapon origin/contact lights are not duplicated by old damage-derived
lights when the native presentation owns those effects. Existing upgraded
engine glows/trails are unchanged.

Native weaponfire integration now goes beyond the importer: the C++ reader
loads the original `.wf` bytes into immutable per-gun asset data. `WeaponFire`
and `FireAnimation` are authored in the owning GUN block and follow subsystem
copy/remapping; neither reaches the HW1 balance parser. Asset lifetime tests
verify that instances retain weapon presentation after the loading owner is
released. Current validation: 32 engine tests and all 58 staged source `.wf`
files pass. The event-marker query re-evaluates the current component/turret pose
instead of retaining a fire-time world transform.

The reviewed-mount coverage audit excludes derelicts and currently contains
160 source rig entries: 113 have model-event cues and 40 use native weaponfire
without such a cue. Seven entries remain unclassified (including unused source
barrels, minelayers and the RM prototype light defender). These are source rig
entries, not a promise of 160 simulated weapons or finished renderer coverage.
Rendering, full sound lifecycle coverage and update assembly remain unfinished; no deployment
has been made from this migration.

The native visual registry now dispatches timeline events with their owning
ship, live instance, component and model scale. Playback has one explicit mesh
owner per ship, not one player per rendered LOD. A second LOD cannot duplicate
an active timeline. Stop, ship deletion and mesh release cancel playback;
tests cover independent ships, same-clock deduplication, LOD ownership and
ownership reset. HW1's actual bullet/beam/missile creation paths now notify the
bridge through the first native mesh, with gun pose synchronized, regardless
of camera LOD or render-list membership. The universe update pumps timelines
after dead-owner deletion and ETG reset stops them. Consumers receive native
weaponfire FIRE-slot data and model events independently; replacing/detaching
a consumer cancels old timelines. With no consumer attached the hooks are
inert and do not suppress existing effects or audio. The game sound consumer now
loads the generated `OWSFX001` index, precaches native FDA samples, and handles
model-event sound actions and weaponfire FIRE sound actions. Native gun sounds
suppress their legacy shot/loop counterpart only while that consumer is active.
The same hooks now drive native model-event FX and weaponfire FIRE FX as well.

Sound attachments use stable per-instance tokens and ship identity/creation
stamps, not retained ship pointers. They resolve current component/gun poses on
updates, and stop on owner loss or mesh replacement. Unmarked events use the
component owner frame rather than inventing a `Root` marker. Event joint lookup
uses case-insensitive ASCII matching, as verified in RM's marker hash path
(`004f793f` -> `007922da` -> `00792271`, mode 1); ambiguous matches still fail.
The headless `presentation_runtime` test exercises the actual game consumer,
native bank/controller and PCM mixer together, including moving attachments,
instance replacement, reset, and no file I/O after precache. It is not an
in-game listening test.

The game consumer also snapshots projectile presentation into a runtime-only
side table. No saved Bullet/Missile layout changes or pointers into deleted
firing models are retained. Native BULLET cues follow the actual HW1 projectile
pose; HIT and MISS cues follow HW1 collision/expiration. Beams use the actual
changing HW1 collision length, with one moving contact effect rather than one
new effect per damage tick. The hit magnitude is clamped by the authored
hit_clamp before scaling the complete effect, matching 004ba0a1; individual
layer radii are not clamped or multiplied by victim hull radius. No RM damage,
speed, guidance or collision calculations are imported. Expiration also removes
a pre-existing write through a freed bullet pointer.

Attachment allocation/lookup and projectile snapshot lookup are bounded and
allocation-free during play. Attachment handles encode a slot and generation;
projectile hash deletion back-shifts its cluster rather than accumulating
tombstones during long battles. Tests cover moving shots, deleted source
models, pointer reuse, beam clipping/contact updates, reset, and muted audio.
The native fleet graph playback test produced 365,387 finite draw packets;
the actual drawing path loaded all selected original images/HODs and emitted
50,985 finite vertices with no drawing-time file reads. These are headless
integration checks, not a claim of visually verified RM parity.

Remaining FX work includes secondary event transforms and verified NLIPS/SM,
complete environmental particle dynamics, beam noise/arc and light options,
depth-sorted particles, full flare occlusion/washout, and ambient/utility
state integration. Damage stages now follow HW1 health fraction and stop when
repairs cross their RM thresholds. On gameplay death, the current native hull
and attached subsystem pose are snapshotted, then the RM Death and Deathtrail
timelines continue while the hull is rendered for Death's authored duration.
An exact source Death MAD clip deforms the detached hull when present. This
path has build and automated lifecycle coverage but still needs a real in-game
visual check; full RM destruction parity has not been established. Native
missile HOD drawing is still separate unfinished
work. A playable migration/update has not been assembled or deployed.

Sound staging now validates and inventories all 167 ETG parameter records:
62 directory records and 105 sample records. The import receipt retains each
record's authored fields, envelopes, eight-band equalizers and effect settings,
plus direct sample membership. The 417 membership references overlap because
directory and per-sample metadata may describe the same sample; they are not
417 unique audio clips. Missing fields are not filled with guessed defaults,
and nested sample groups are not flattened into one random pool. Original
FDA/Lua bytes are still copied unchanged. Runtime selection applies inherited
authored fields and per-cue polyphony. Distance gain and stereo pan update with
the listener/owner. Playlist playback retains owner lifetime between entries,
uses the simulation clock for authored silence intervals, and supports ordered
or random entries and finite or looping lists. Tests cover silence, completion,
and rejection of a backwards clock. DSP/equalizer/reverb, frequency/segment-loop
playback and full ambient/damage/death lifecycle integration are still
unfinished; retaining their data is not a claim that those features work.

HW1 MAD updates now dispatch the explicitly bound source clips' presentation
timelines from normalized HW1 animation phase. They do not start a second
movement/animation scheduler. Repeated phase samples do not replay sounds,
activation remains distinct from phase zero, and an endpoint does not wrap a
looping source timeline into a duplicate zero-time burst. The first native mesh
owns these events, independently of camera LOD; gun timelines remain separate.

Missile presentation import is distinct from `.wf` import. The source weapon's
literal presentation family may resolve to `missile/<family>/<family>.miss`.
Only referenced families enter setup's projectile staging. Original HOD/MAD
bytes, compiled events and source snapshots are retained; the `.miss` simulation
assignments are never evaluated or imported. Three reviewed fleet missile
families (`kus_missile`, `tai_missile`, `tur_smallmissile`) pass native HOD/LOD0-4
checks and resolve all six authored event markers after the hash-scoped source
corrections below. Staging/validation does not yet replace the in-game missile
renderer or implement its flight/death event lifecycle.

Two verified archive-source defects are corrected only at import, with original
source bytes retained and diagnostics in the receipt. Hash
`9584d1611bc11ef3dd731b7ce413561887903f2f4e0403564bd26e8d5c9f8912`
is the Turranic missile file missing `events = {`. Hash
`523040ac1d2c50d26d7391ba0ed45086467346e41c2e14586c2f0c3e1ae3c420`
is the shared Kushan/Taiidan event source referencing absent `MissileEngine1`
instead of their unique native `EngineNozzle1`. These are importer corrections,
not alternate runtime grammars or marker aliases. Unknown malformed source is
still rejected. The native HOD reader resolves neutral self-named export-helper
pairs structurally; it still rejects non-neutral ambiguous parents and preserves
the previously audited non-neutral carrier helpers.

Setup checkpoints now carry a native-presentation recipe. Earlier checkpoints
rebuild derived native packages/overlays/definitions/subsystems while preserving
unchanged Classic extraction, canonicalization and Kushan material work. This
prevents resume from silently reusing packages without the new event snapshots.
No installed-game conversion or update package has been run for this change.

