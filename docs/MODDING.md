# OpenHW 1.0 modding guide

**Read this before using old source notes.** OpenHW combines Remastered presentation assets and original HW1 gameplay. Historical Homeworld format/build documents and old development reports are included for reference and have not all been rewritten to describe OpenHW. They can contain obsolete paths, file formats, unsupported features and superseded limitations. Use this guide, the 1.0 user/editor guides and current runtime format documentation first; verify ambiguous behavior against current code.

## Ship projects

Open **Modding > Ship Viewer / Editor / New Ship Project**. Choose an installed ship as a template. The original HOD stays immutable; the editor saves authoring data and definition overrides, not destructive edits to the retail model.

**Copy/paste, duplicate and position subsystems (including turrets) on ships.** This editor will continue to improve in future releases.

Inspect the hull/joint hierarchy, navigation, docking/launch, salvage/repair, weapons/turrets and engines. Select parts in the hierarchy, edit local numerical transforms or use translation/rotation handles. Hierarchy changes, copying/duplicating chains and undo/redo allow reusable configurations. Right-drag orbits, wheel/both mouse buttons zoom, and middle-drag pans. Read the active viewport legend for mode-specific controls.

SHIP FILE opens the editable ship definition. Native ship/subsystem definitions combine presentation references with the original HW1 gameplay parameters. Complete subsystem placement/rotation/duplication edits synchronize to the project definition. Functional copied weapons require actual weapon configuration and valid model/joint/muzzle bindings; a visual template alone is not a new gun. EVENT FILE displays connected event data read-only and supports inspection/copy, not event timeline authoring.

Projects live under `Modding/Ships/<project>/`. `ship.owship` preserves editor state; the installed `.owshipdef` is the runtime definition. Save/load, Ctrl+S and undo/redo are supported. Saving a project does not activate it in gameplay.

OBJ importing/preview with UVs, normals and material groups is available, along with image assignment for project preview. This is not an OBJ-to-HOD compiler, a general mesh sculptor, an animation authoring suite, or automatic addition of new ship identities. Native HOD geometry is not rewritten. Use external tools for work outside the editor's supported scope.

## Publish an existing native ship modification

1. Save and validate the project. Check the complete weapon and attachment configuration.
2. Choose **SAVE MOD** for a new named folder, or **APPEND TO MOD** for an existing one.
3. Review any replacement confirmation. Required models, bindings, referenced subsystem folders and connected event banks are validated. Backups are kept for replaced files.
4. Enable the mod in **Modding > Mod Manager** and arrange its priority.
5. Restart OpenHW. Test changed weapon counts in a **fresh mission**; existing saves do not migrate their weapon arrays.

Native mod payloads mirror game resource paths, without adding another Data folder:

```text
mods/MyFrigate/R1/StandardFrigate/RMNative/ship.owshipdef
mods/MyFrigate/Subsystems/MyGun/subsystem.owsubsystem
```

The release does not include the author's extra-turret Kushan Assault Frigate gameplay modification. Make your own optional mod if you want such a loadout.

## Load order and boundaries

Enable/disable checkboxes and drag ordering are implemented. `mods/load-order.txt` stores the selected order; **lower/later enabled entries override earlier entries**. Merely creating a folder does not enable a mod. Restart after activation/priority changes; no live simulation/static-asset reload is supported.

Current publication handles native ship packages and referenced subsystems. It is not a general mod archive builder or a native DLL plugin loader. Existing ship identities are modified; adding an entirely new gameplay roster identity is not an editor-only operation. Dependency validation is not a promise that arbitrary conflicting mods will combine correctly. Maintain backups and test a clean mission.

Individual file replacement is atomic, but a multi-file publication is not a single transaction. If publication fails, do not enable the incomplete mod until a successful retry. Only distribute assets you have the right to share; a locally imported Remastered template is not a grant to redistribute its retail models/textures.

## Gameplay definitions and events

See [OPENHW_SHIP_DEFINITIONS.md](OPENHW_SHIP_DEFINITIONS.md) for `OPENHW_SHIP_2`, `OPENHW_SUBSYSTEM_2`, hardpoints, weapon slots and the compatibility formats. Visual subsystem attachments do not become independently damageable subsystems. Original HW1 damage, movement and weapon behavior remain the gameplay foundation. Imported presentation/event coverage has specific limitations documented there; do not assume complete HW2/RM simulation semantics or arbitrary Lua execution.

## Military parade authoring

Open **Modding > Military Parade Formation Editor > Kushan / Taiidan**. The mothership is the origin. Add ship types, move/rotate their positions and configure growth offsets for successive ships of a type. Use numerical input, handles, framing and undo/redo. Save campaign layouts as local `Parade-Kushan.cfg` and `Parade-Taiidan.cfg`.

Campaign presets apply to military-parade placement, respecting active gameplay orders and hyperspace priority. This is separate from the live combat Formation Spread control. Multiplayer preset storage is available for authoring only; it is **not multiplayer gameplay or synchronization**. See [the detailed parade guide](MILITARY_PARADE_EDITOR.md).

## Maps and environments

Use Shift+F11/Shift+F12 for maps, clouds, ropes, original dust appearance and lighting. See [EDITING_GUIDE.md](EDITING_GUIDE.md). The corrected installer supplies authored per-mission defaults at the physical loader paths. User overrides can supersede them. Keep map and lighting exports together when sharing a visual preset.

## Building and future work

[BUILD_INSTALLER.md](../BUILD_INSTALLER.md) builds both game variants and OpenHW-Setup.exe from this complete source snapshot. The source includes tests, conversion tools, shaders and reviewed release content, but not your installed Collection assets or build caches.

NVIDIA/Intel technology completion, player-reported fixes and further performance work are immediate priorities. Cataclysm and multiplayer support are planned later; they are not supported 1.0 modes. Original networking code and multiplayer-related UI/data in the tree are not proof of validated multiplayer compatibility for modern features.

The supplied subsystem library contains HW1 content. Hiigaran/Vaygr factions and subsystem libraries are excluded. VgrBeam and its required shared resources are retained for the Taiidan Defense Fighter. Owner-created asteroid models, textures and every distance LOD install at Data/Asteroids.
