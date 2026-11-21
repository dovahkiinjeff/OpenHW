# OpenHW 1.1.14 — Reflections and integrated HOD modding

Choose **Update now** in Launcher. Existing installations update in place; no reinstall is required.

- Restored native Remastered material reflections in path tracing and the D3D12/D3D11 raster renderers. Cockpits, polished panels and other reflective areas now follow their authored reflection masks and gloss maps, independently of hull paint brightness.
- Added the integrated HODEditorJS-based workshop inside the game, including model, hierarchy, material and animation editing. Quick Add, copy/paste and duplicate subsystem workflows remain available.
- Added **Create Mod** in the game and Launcher Mod Manager and in the editor's **Save to mod** chooser. Named folders under `mods` override matching game-data paths when enabled, in load order. Restart the game after changing a mod.
- Saving a ship to a mod writes its HOD, ship definition and regenerated runtime material maps. Adding an armed turret generates weapon slots and hardpoints, and carries its weapon-fire and event references into the mod so muzzle flashes and other configured effects remain connected.
- Added **Ship file**, **Events file** and **Weapon file** buttons inside the HOD editor. Event files have an editable JSON representation; saving writes the game's binary event format.
- Shared subsystem models now resolve to their existing native archive assets. No additional retail model copies are distributed. Personal parade formations remain separate from mods.
- Launcher and Updater advance to utility version **1.1.8**. The manual rollback feature remains absent.
- Updated the dedicated server to the matching multiplayer identity. Capacity remains ten simultaneous battles.

The patch regenerates native reflection-control pages from each player's installed HOD archive, without copying the whole archive or staging a second complete material set. Existing custom loose material overrides are preserved. This maintenance step can take longer than a normal executable-only patch.

Validation includes native renderer reflection checks, a complete interceptor/heavy-corvette-turret mod save accepted by the game's loader, event/weapon round trips, editor subsystem copy/paste, mod creation, and installer/updater transaction checks. A complete interactive multiplayer match remains a separate player test.

Compiled updates are hosted on openhw.tech. GitHub provides cumulative source and source checksums only.
