# OpenHW Launcher

The 1.0.7 patch installs `Launcher.exe` automatically. It belongs beside `OpenHW.exe` in a completed OpenHW installation. It always manages that folder. It does not search other drives, move your game, install a service, or create an AppData profile. Both the game and the diagnostics game must be closed before changing settings, applying updates or resetting the cache.

The Fleet Command shell uses the supplied hangar concept art, an amber/cyan interface, a custom application icon and bundled Rajdhani / Share Tech Mono fonts. Fonts are private to the launcher process, not installed into Windows. The header replaces the Windows caption and border: drag it to move the window; the down-chevron minimizes, corner brackets maximize/restore, and the power symbol closes. Double-clicking the header also maximizes/restores. The edges resize the window. Alt+F4 uses the same unsaved-change/update checks as the Close control.

The left rail opens **Deploy**, **Configuration**, **Mod Loadout** and **Ship Systems**. Dialogs, confirmation prompts and the RGB/hex color picker share the same theme. Folder selection uses the Windows file picker. Ctrl+S saves configuration.

## Play and updates

**Play OpenHW** starts the normal executable; **Launch with diagnostics** starts `OpenHW-Daignostics.exe`. Pending launcher settings and mod choices are saved first. The game starts with the installation folder as its working directory.

**Check for updates**, under Ship Systems, uses the existing official, version-aware update feed. It selects the required patch sequence, checks download sizes and SHA-256 checksums, and uses the updater's backup/rollback process. Saved games, authored mission settings and mods are preserved. It does not guess future patch names. An update requiring a newer protocol stops with an explanation; use the separately maintained `Updater.exe` to obtain the required updater/launcher update.

If an update replaces Launcher.exe itself, close it and run the standalone Updater.exe. Replacement of the running launcher is refused before any game file changes. The launcher does not update the game automatically on every start. Downloads, update journals and backups stay inside `Development`.

## Settings

The searchable catalogue comes from the game's persisted settings table. It includes display, rendering, post-processing, audio, gameplay, keyboard shortcuts, player colors and skirmish settings. Color buttons open a color picker. Display changes take effect on the next game start. Borderless/windowed mode uses the refresh rate of the Windows desktop; the selected refresh rate applies to supported exclusive fullscreen modes.

Shift+F11 map/ship authoring, placed clouds, mission lighting files and other editor documents are not edited by the launcher. Use the existing in-game editors for them. Internal installation paths, device identifiers and save-format bookkeeping are not player settings.

The launcher preserves unknown settings, legacy text encoding and comments in `Homeworld.cfg`. It backs up changed files and refuses to overwrite a configuration changed since it was opened. When changing settings in another program, close and reopen the launcher first.

AMD FSR reconstruction and frame generation remain the recommended options. NVIDIA/Intel choices retain their existing experimental/unimplemented labels and do not become supported merely because they appear in the catalogue.

## Keyboard and language

**Keyboard layout** offers Windows/automatic, physical US QWERTY, French AZERTY, German QWERTZ, Spanish, Italian and US Dvorak shortcuts. The in-game Keyboard page has the same selector. Number rows, control groups, modifiers and function keys remain stable. The game does not load/activate Windows keyboard layouts. Text entry continues to use the Windows layout.

Original text/subtitle resources are selectable in English, French, German, Spanish and Italian. Newly written OpenHW interface labels remain English. Missing classic translations fall back to English instead of failing the load.

The Steam Remastered Collection lists English full audio, with the other listed languages providing interface/subtitle support: [official language table](https://store.steampowered.com/app/244160/Homeworld_Remastered_Collection/). Do not assume another Steam text-language selection provides a voice dub.

**Import owned classic voice pack** accepts a localized Homeworld 1 Classic `Data` directory containing `Homeworld.big` and `HW_Comp.vce`. Specify the actual language of those files. Only the two speech lookup tables and the voice bank are imported; the whole game is not extracted. The import checks that all three files share a checksum. Complete installed packs appear in the voice-language list. No voice assets are downloaded or included with Launcher.exe. Incomplete/mismatched banks are not offered and the game safely retains its original audio.

## Mods

The **Mod Loadout** page uses the same `mods/load-order.txt` as the in-game Mod Manager. Double-click or use **Enable / disable** to toggle a mod. **Move up** and **Move down** change priority. **Later enabled entries override earlier entries.** Save or launch the game to apply your choices.

Newly discovered folders start disabled. Folders use the existing letters/digits/underscore/hyphen naming rules. The launcher does not modify mod payloads. Restart OpenHW after changing activation or order; live simulation/asset replacement is not supported. Conflicting mods are resolved file by file using the existing loader rules, not merged automatically.

## Shader cache

**Rebuild cache**, under Ship Systems, moves the game's local `Development/ShaderCache` into `Development/Backups/ShaderCache/<timestamp>`. The next game startup rebuilds it and can take longer. No GPU-driver or Windows cache is deleted. The launcher does not compile shaders while another game instance is running.

## Build

Use the project's supported Windows x64 toolchain:

The prepared art, icon, fonts and font licenses are included under `assets/Launcher`. To regenerate the crops and icon, run `python tools/build_launcher_theme.py` with Pillow installed. Runtime display uses Tk's image loader; users do not need Pillow, font installation or an external theme download.

```powershell
python tools/generate_launcher_settings.py
cmake -S tools/updater -B out/updater -A x64
cmake --build out/updater --config Release --target OpenHWLauncherBootstrap
python tools/package_updater.py --launcher --source . --installed F:\OpenHW1 --bootstrap out/updater/Release/OpenHWLauncherBootstrap.exe --pyinstaller-modules out/importer-build-deps --output F:\OpenHW1\Launcher.exe --work out/launcher-package
```

Packaging reuses the completed installer's frozen Python support. Launcher.exe prepares its own private runtime under `Development/Launcher/Runtime/1.0.1`, so it does not need a system Python installation and does not rely on a future patch preserving the original installer runtime.

## Named Diagnostics folders

Open **Diagnostics** from the game main menu or **Ship Systems > Report an issue** in the Launcher. The main Launcher can remain open. Select an existing diagnostics name or choose **New diagnostics folder**. Enter your Homeworld/modding-community alias and a comment, then click **Save name + comment**. This creates a persistent private folder on openhw.tech; later saves update that same folder. The name is separate from campaign save files.

Click **Upload all new logs** to upload every log/crash dump under Development/Logs that has not already been acknowledged. There is no newest-file limit or manual file selection. Nested folders and duplicate basenames are supported. Large files are split into 32 MiB chunks and at most 60 chunks are sent per bounded batch. Changed files are eligible again; previously acknowledged chunks are skipped. If a batch fails, retry the upload: successful earlier batches remain recorded.

The folder identity, private upload credential, saved comment and content hashes are stored only in Development/Reports/profiles.json. Keep that file when moving an installation to preserve saved folders and upload history. No upload runs in the background during normal gameplay. Logs can contain local paths; reports are private to the OpenHW team.

## Launcher updates after the 1.1.1 recovery

The 1.1 base needs a one-time replacement of Updater.exe. Close Launcher and the game, replace that file in the installation root, and run it. The recovery is bundled and verified, installs 1.1.1, and reopens Launcher. Later runs use the live update channel rather than forcing the bundled old patch over a newer installation.

The footer checks the server at startup and every five minutes without installing anything. When an update is available, Update now appears. Launcher downloads verified updates before automatically handing installation to the themed helper and reopening itself. The helper waits for the old Launcher and its bootstrap to exit; it never kills those processes. Failed downloads leave Launcher open. Failed installation offers Retry and Return to Launcher. Transactional patch recovery and file backups remain active.
