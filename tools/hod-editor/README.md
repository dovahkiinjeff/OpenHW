# OpenHW HOD Editor

Open **Modding → HOD Editor** in the Fleet Workshop. **Game models** browses the local OpenHW archive. **Open** reads a loose HOD and its neighboring animation file. **New HOD** starts a hierarchy with a root joint. **Import DAE** brings in mesh geometry. Use the hierarchy, inspector, viewport and animation timeline to edit the model.

Use **Save to mod** to choose an existing mod or create a named one. Saving an installed ship uses its original relative path under `mods/<name>/`, enables the mod, and saves the HOD, animation, ship definition and edited material pages together. Restart OpenHW to load the changed ship. The Mod Manager in the game and Launcher also has a **Create Mod** button. Enabled folders override matching game-data paths. Lower entries in the load order take priority. Parade formations remain personal settings.

**Quick Add subsystem** places installed weapons and engines on the selected joint. Select a mount to copy and paste it with **Ctrl+C / Ctrl+V**, or duplicate it with **Ctrl+D**. The gameplay compiler assigns weapon slots and retains each mounted weapon's animation and muzzle-effect bindings. Save to mod also places the referenced event and weapon-effect files in that mod.

After saving, **Ship file**, **Events file**, and **Weapon file** open the ship's referenced files inside the editor. Presentation events are shown as editable JSON containing timelines, markers, effects, sounds and attachment options. Saving rebuilds the native events file. Existing event edits survive later HOD saves.

**Save As** exports a loose HOD and its adjacent ship definition. The editor writes HOD2 and a companion `.mad` for animations. The runtime convention for `hull.hod` is `ship.owshipdef`. Other HOD filenames use a matching basename. Choose a gameplay template when starting from a loose or new model.


This is an OpenHW fork of [HODEditorJS v1.2.17](https://github.com/GuillermoOM/HODEditorJS/releases/tag/v1.2.17), commit `4cb39196f0328096044c9c2b9589c417fc3f0ffb`, by GuillermoOM and contributors. The upstream license is included in [LICENSE](LICENSE). OpenHW embeds the editor as a child of the game window, browses installed assets, keeps archive originals read-only and stages file writes in the output folder.

The normal Windows CMake game build includes the editor. Build dependencies include Node.js with npm and stable Rust for x64 Windows MSVC. The player uses the installed Microsoft Edge WebView2 Runtime. The packaged interface runs locally. To build the editor target alone, run `cmake --build <build-directory> --config Release --target openhw_hod_editor`.
