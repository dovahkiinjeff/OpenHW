# Military parade editor

Open **Modding > Military Parade Formation Editor > Kushan / Taiidan** from the main menu.

The mothership is the layout origin. Add ship types from the dropdown; each race/type can appear once. The editor renders actual LOD0 ship meshes using the existing engine renderer, without spawning simulation units. Missing model assets cannot be previewed.

- Select a placed type in the left list or click near its center in the viewport.
- Use the XYZ handles to translate. **Move / Rotate** or G/T switches to rotation rings.
- Edit position, rotation in degrees, and growth numerically on the right. Enter commits a field.
- Growth is the offset for each subsequent ship of the same race/type.
- Right drag orbits; holding left+right and dragging zooms, matching the game. Wheel zoom has the same sensitivity and smoothing. Middle drag pans, F frames all entries.
- Ctrl+S saves, Ctrl+Z/Y undo/redo. ESC twice discards unsaved edits when leaving.

Local axes: X right, Y forward, Z up. Rotation order is X, then Y, then Z. Host mothership orientation carries the layout into the world.

## Campaign

Campaign saves are `Parade-Kushan.cfg` and `Parade-Taiidan.cfg` beside the executable. Saving applies the layout to the corresponding mothership's military parade. Unedited types retain vanilla placement. Existing slot/savegame structures are unchanged. Hyperspace-controlled ships retain their existing steering priority.

Loading a campaign save reloads the current external preset. Once cutscenes and hyperspace arrivals finish, authored types already in military parade are placed in the current layout; idle authored types join that parade too. This overrides old saved parade positions without cancelling active combat, player formations, harvesting, salvage, repair, or docking orders. Docked ships remain docked. The current preset applies when they later launch and join parade normally.

## Multiplayer presets — storage only

The Campaign / MP button switches the editor's save destination. Name a preset on the right, then save or load it.

These files live separately in `MultiplayerParadePresets/Parade-<host>-<name>.cfg` beside the executable. They are **not connected to multiplayer gameplay, lobbies, networking, or synchronization**. Loading one only loads an editor draft; saving one does not apply it to campaign gameplay either. Future multiplayer integration is deliberately deferred.

Preset names allow 1–32 letters, numbers, hyphens, or underscores. Replacing a saved file preserves a timestamped backup. Invalid files are rejected without changing the current layout.

## Verification

Production persistence/math tests cover round trips, identity-specific spacing, rotation, invalid input rejection, backups, and multiplayer/campaign isolation. Rendering and interaction require an in-game check; the game is not automatically launched for verification.
