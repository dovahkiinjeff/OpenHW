# RM binary reference for presentation compatibility

Inspected 2026-10-14: `N:/Wings3D GPT Upgrade/HomeworldRM_Analysis/REPORT.html`
and its function index. The local analysis identifies HomeworldRM.exe SHA-256
`a9c63778c4f3db33d06affa7c23742c1af077c0f376c4690f800c9d1dfbe77f7`.
These are recovered pseudocode and static observations, not original source or
runtime validation. Loose-file snapshots may be modded. Do not distribute the
executable or pseudocode as part of the portable game.

## Evidence affecting implementation

- `005ef0bd`: animation sink receives normalized previous/current progress,
  divided by animation length. This supports our phase-times-duration scheduler.
  Exact loop-boundary behavior still requires further tracing; do not infer
  that from the normalized interface alone.
- Additional tracing: `005f16a6` installs TriggerAnimEvent vtable at 0x8e1ed4.
  Its start entry is `005f1af6`, which calls `005eaf79` with XMM2=-100000 and
  XMM3=-0.00001. These constants were verified from the identified executable
  at 0x905610 and 0x905594. The helper selects events in that negative range.
  Thus authored -1 events dispatch on activation, separately from phase zero.
- `0064e9b9` (also directly inspected): selects **one resolved marker** using
  random modulo marker count. A comma-separated marker field is not a request
  to emit at every marker. Distinct simultaneous event records remain distinct.
- The same dispatcher creates `RenderModelFXUpdate` only when `fx_orphan` is
  absent. Presence matters: the string `off` still bypasses that updater.
  Attached sound also has an update object. Preserve ownership and update the
  marker transform as the ship moves, rather than integrating muzzle velocity.
- `0064fae0` (directly inspected): attached updater parses scale, length,
  secondary entity/marker, transform mode and NLIPS. Transform modes are
  SelfToOther, OtherMarker, OtherToSelf, None, Between. Accepted names are not
  sufficient evidence for the complete transform mathematics.
- Additional assembly check of `0064f880`: the literal `radius` branch returns
  1.0 in this executable. It must not be replaced by our guessed hull bounding
  radius. `entity` delegates to `0064f851`, which calls the render object's
  virtual function at +0x30; its result is not yet established as hull radius.
  The importer retains the source mode rather than baking guessed dimensions.
- `fx_sm` chooses a different spawn wrapper; its exact rendering meaning is
  not established. The runtime flag is named `HW_RM_EVENT_SM` without claiming
  an unverified sensor-visibility meaning.
- `006d527f`: weaponfire has seven independent FX/sound slots: fire, bullet,
  hit, anticipate, miss, nopenetrate, blowthrough. Per-slot clamps/NLIPS/SM
  are not interchangeable with world-space scale. Fire lacks corresponding
  option reads in this loader. Never import RM damage/movement to obtain FX.
- `0060bf9b`: weapon joint bases acquire `_Position`; slave mount lookup has
  additional naming rules. Do not invent duplicate model joint bindings.
- Audio parameter loader `007d8da6` reads `version`, iterates 30 parameter
  entries through `007d8768`, and reads `randSampContainer` separately. The
  parameter reader only writes a value/marks its presence when an authored
  field is found. The caller `007d6f53` first obtains another parameter record
  via `007d70c9`, copies it, then applies the selected Lua file. Therefore a
  missing per-sample field must not be replaced with an importer-invented
  default. Exact directory lookup, inheritance precedence, random-container
  scheduling and DSP behavior still require tracing; function names alone do
  not establish them. The staging inventory retains original authored records
  and their directory/sample scopes instead of pretending to implement these
  remaining playback behaviors.
- Engine lookup `004c5b24`, `004c6a88` and `004c6ab2` uses the explicit
  `EngineNozzle1` through `EngineNozzle10` name table. The reviewed Kushan and
  Taiidan missile HODs each contain a unique `EngineNozzle1`, while their
  identical source event file references absent `MissileEngine1`. Both event
  files were independently re-extracted from `Homeworld2.big`; their exact
  hash is recorded in `OPENHW_RM_SYSTEM.md`. Treating that reference as stale
  source data is an inference from the models and lookup evidence, not a
  claim of a recovered RM marker alias. The importer makes an exact-hash
  correction with a receipt diagnostic; the runtime has no such alias.

## Current gaps exposed by this reference

Additional directly checked FX routines:

- `007f4b13`: fractional spray rate, Force_First overriding the first count,
  ParticleScale separate from ScaleSpeed, moving-origin interpolation and
  staggered initial particle ages. `007f51ea` samples half the authored cone.
- `007ee269`: linear drag over a clamped 0.1-second interval, not exponential
  damping. `007fe912` adds gravwell acceleration toward the owner origin.
- `007f9bac`: trail Length is seconds; the current endpoint is updated and a
  history point committed after 0.1 seconds. Trails are not straight beams.
- `007f18da` and `007f42e3`: ring epsilon factors are sampled per instance,
  RateEpsilon changes effective age, and spin accumulates source spin * 10 *
  degrees-to-radians * effective delta. SpinRandom changes spin direction.
- `007cb180`, `007cb280`, `007cb440`: animated bitmap selection compares
  frameCount * loopCount without treating zero as infinite. Explicit random
  bitmap selection remains separate. `007cb310` retains authored atlas cells.
- `004ba0a1`, confirmed in assembly: incoming positive damage is clamped with
  hit_clamp via `004752c3` and passed as the scale input to `004b9dbb`.
  Non-positive damage selects NOPENETRATE with magnitude one. This is not a
  clamp on each visual layer's radius or a victim hull-size multiplier. The
  broader view-dependent NLIPS/SM behavior remains incomplete.

The revised OWEVENT2 representation retains orphan presence and source text,
radius/entity scaling, numeric/entity/radius/distance length, secondary entity
and marker, and transform modes. There is one reader for the current format;
obsolete generated event files are rejected and must be regenerated, not loaded
through a compatibility branch. The selector helper
chooses one valid resolved marker using caller-supplied randomness without
consuming the gameplay RNG. These data/helper changes do not implement the
complete attached-transform renderer. The timeline dispatcher is not yet
connected to every gameplay state. Gun/MAD, projectile, collision and expiration
playback are connected; general ambient/damage/death state playback remains.
The existing rejection of unsupported event fields prevents silent loss but is
not compatibility. Preserve raw source records and add validated runtime fields,
tests and explicit unsupported diagnostics as each behavior is implemented.

Do not treat the function index as a completed HOD format specification. Use
the per-function files and assembly to resolve specific loader/update questions.
This reference does not replace independent implementation or in-game tests.
