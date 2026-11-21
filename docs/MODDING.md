# OpenHW modding guide

Open **Modding → HOD Editor** in the Fleet Workshop. The editor runs in the game window and edits ship, derelict and other HOD models. Choose **Game models** to edit a copy of an installed model or **New HOD** to start a new one.

## Ship subsystems

Select a hull joint and choose **Quick Add subsystem** to place a shared weapon or engine. Select its mount and use **Ctrl+C / Ctrl+V** to copy and paste, or **Ctrl+D** to duplicate. To copy an existing fitted component, use the mounted list in Quick Add. Position or rotate the new mount with the viewport handles. **Save HOD** writes the model and its gameplay definition in one operation. A loose or new hull can use an installed ship as its gameplay template.

## Publish an existing native ship modification

1. Choose **Save to mod**, select a named mod or use **Create Mod**, then save. The editor writes the HOD, ship definition, runtime material maps and referenced weapon/event documents at the matching relative paths.
2. Use **Ship file**, **Events file** or **Weapon file** to edit the associated documents without leaving the editor. Events are editable JSON saved back to the native binary format.
3. The selected mod is enabled automatically. Arrange its priority in Mod Manager.
4. Restart OpenHW and test changed weapon counts in a **fresh mission**; existing saves do not migrate their weapon arrays.

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

Online multiplayer uses the dedicated OpenHW server. HOD edits and arbitrary mod payloads are local content and are not automatically distributed to other players. See the current multiplayer guide for supported match rules and validation limits.
