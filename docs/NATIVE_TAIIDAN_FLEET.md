# Native Taiidan fleet — 2026-10-08

29 matching existing R2 identities are enabled through the native HOD fleet
loader. This is a deployed test candidate, not a claim of visible gameplay
validation. Gameplay stats, build rosters, collision/MEX routes, save layouts,
missions, and the working R1/P1 imports are unchanged.

## Source and scope

`tools/stage_taiidan_native.py` reads the user's owned `HW1Ships.big` and
`Homeworld2.big`, extracts into the ignored local `out/hwrm-taiidan-native`
cache, audits literal ship/subsystem dependency declarations without executing
Lua, and stages the source hull/component HODs and MAD companions. Native
geometry is composed in memory; it is not exported as GEO. Full-resolution
material sidecars use the established diffuse/normal/specular/emission/team
mask translation, with component-qualified namespaces and source hash checks.

Runtime still loads **loose native packages**, not RM BIG entries directly.
Archive-backed runtime loading remains separate unfinished work. These local
copyrighted assets must not be bundled in the eventual installer/release;
the installer must reproduce the import from the user's owned installations.

The exact 29-name mapping lives in `R2_HULLS`. `CryoTray` and `LightDefender`
retain their original assets because the inspected archives contain no matching
Taiidan RM models. Field-generator segments, missiles/mines, headshot asteroid,
alternate research rigs and other races are not introduced as
new units. Invalid packages retain the existing classic mesh fallback.

## Rigging policies

- Package identities are `Taiidan_<ClassicType>` with `HW_RM_R2_VISUAL_1`.
  A same-named R1 ship cannot accidentally receive R2 gun/event aliases.
- 62 original gameplay gun indices have explicit native muzzle bindings.
  The source component and mesh parenting determine the moving axes: real
  split barrels remain split, rigid Position-owned geometry moves together,
  and fixed hull emitters cannot rotate the hull.
- The heavy corvette and destroyer retain their paired native barrel rigs.
  Cruiser ion slaves 6/8 share their masters' existing RM emitter transforms,
  without duplicate controlled pivots or invented extra turrets.
- Mothership defense emitters 1/3 are deliberately remapped to source
  HullDefense4/HullDefense2 by location; RM numbering differs from HW1.
  Only the five actual original gameplay guns are bound, not all six RM markers.
- Taiidan assault-frigate hardpoints 2/3/4 contain zero-pose self-named export
  helpers beneath real Root-parented anchors. The reader recognizes this narrow
  audited shape, retains the outer mount, and gives only the helper a unique
  internal name. Nonzero or otherwise ambiguous helpers still fail validation.
- Six ships expose 16 original animation events, including damaged corvette
  wings, interceptor docking wings, salvage cockpit swing, sensor array, and
  target-drone panels. The salvage `SwingCockpit_Out` event explicitly maps to
  RM's case-different `SwingCockpit_OUT` clip.
- Heavy-corvette MAD also contains Ambassador conversion tracks referencing
  the removed integrated gun rig. Its package explicitly allows only the four
  HW1 R2 wing/docking clips. Excluded conversion clips cannot be bound. Ordinary
  asset construction retains strict whole-MAD validation, and no source MAD is
  rewritten or missing joint silently fabricated.
- SPIN clips are retained as source data but not automatically played.
- Reviewed engine markers are used where their count/topology matches classic
  trails. Mothership, cloak generator, and research ship retain classic trail
  offsets rather than arbitrarily duplicating their fewer native markers.
- Existing classic docking, salvage, repair paths remain authoritative. The
  source hierarchy/docking metadata is preserved; this does not claim every
  classic service route has been converted to RM semantics.

## Validation

Actual-source checks passed for all 29 runtime packages, LOD requests 0–4,
62 gun bindings against eight headings, visible geometry following each moving
axis, independent ship instances, invalid-pose rollback, 16 animation events
with phase reversal, complete materials, and finite engine marker transforms.
The renderer bridge passed all 29 hull/attachment hierarchies at all five LODs.
The existing R1 32-package/55-gun audit, P1/derelict 46-package/18-gun audit,
18 CTest tests, and targeted Python contract tests passed after the changes.

```powershell
python tools/stage_taiidan_native.py --reader out/build/deploy-x64-clean/Release/remastered_asset_reader.dll --rm-data '<owned HomeworldRM/Data>' --classic-archive '<owned Classic/Data/homeworld.big>' --output out/hwrm-taiidan-native
cmake --build out/build/deploy-x64-clean --config Release --target homeworld native_ship_hod_tests remastered_mesh_bridge_tests --parallel 6
out/build/deploy-x64-clean/Release/native_ship_hod_tests.exe --taiidan out/hwrm-taiidan-native
out/build/deploy-x64-clean/Release/remastered_mesh_bridge_tests.exe --taiidan out/hwrm-taiidan-native
python -m unittest discover -s tests -p test_taiidan_native.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/deploy_taiidan_native.ps1
```

## Deployment record

760 files were copied and SHA-256 verified in `RTX Deployed/Release`: 758 R2
package files plus the executable and matching PDB. No other fleet packages,
mission settings, saves, original archives, or gameplay files were copied.

Executable SHA-256:
`0D61160E86F4074F9EB9A7A99865CD7CD8F67688EE5E7FF48F62CDA8C933A40C`.

Backup/receipt:
`out/deployment-backups/pre-native-taiidan-20261008-151320`.

This executable also includes the previously built but undeployed Sensors
cleanup fix for the Mission 4 hyperspace crash: stale Sensors state is cleared
before screen deletion callbacks, missing-screen cleanup no longer falls back
to deleting an already-invalid region, and a closed viewport is not rendered.
No game launch was performed. Visual appearance, firing, docking/capture, NIS,
and mission transitions still require visible gameplay confirmation.

## Save-load and mirrored assault mounts follow-up

Mission 5 crash dumps `crash-20261008-151553-46632` and
`crash-20261008-151719-37632` faulted in texture recoloring and ship-list
traversal after save loading. A concrete overwrite path was found: embedded
saved MAD binding storage retained the old hierarchy capacity, but
`Fix_madBindings` rebuilt bindings for the larger RM hierarchy. `LoadShip` now
grows that embedded tail before rebinding, preserving animation clocks and
curves. Dynamic static-info restoration also takes the current hierarchy size
along with the current LOD pointer. No saved files are rewritten.

The mirrored Gun2/Gun4 assault-cannon sockets additionally use their authored
Direction/Rest vectors for their mounting frame. The attachment root receives
the correction consistently in direct draws, neutral aiming frames and the
classic renderer bridge. Source HOD bytes and gameplay weapon arcs are unchanged.
The real-package regression checks all four turret outward axes against the
source Direction markers; 29 Taiidan packages/62 guns/16 events, 32 Kushan
packages/55 guns, five Raider packages/18 guns and 41 derelicts passed.
18 CTest checks and 11 lifecycle source-contract checks passed. These tests
do not replace an in-game Mission 5 save-load and turret appearance retest.
