# Portable automatic updater

Install the OpenHW base game first. Download [Updater.exe](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.0.5/Updater.exe), place it beside `OpenHW.exe`, close both game executables, and run it. No Python installation, administrator prompt or separate installation path is required. The updater only changes the OpenHW installation in its own folder; it does not search for or patch other copies on the computer.

It reads the installed version, fetches the official stable update feed and installs the required patches in dependency order. A new base installation currently receives 1.0.1, 1.0.2, 1.0.3, 1.0.4, 1.0.4.5 and 1.0.5 automatically. An installation partway through that chain receives only its missing steps. An installation already current, or newer than the stable feed, is left alone.

Downloads are checked against their published SHA-256 and size. Embedded patch payloads and individual files are verified too. Saves, player options, mods and authored map/lighting files are protected. Backups, downloads, logs, the private updater runtime and temporary files stay under `Development` in the same OpenHW folder. Windows/driver-owned system caches remain under their components' control.

Interrupted downloads resume when the server supports byte ranges. A failed file installation rolls back the active patch; patches already completed remain installed. An interrupted file transaction is recovered on the next run. If missing mission skies require regeneration and the original Collection has moved, the updater asks for its new location; it reads that installation without changing it. Successfully regenerated sky files may remain after a later failure and can be reused on retry.

The updater checks that the game is closed. It never starts the game or adds a scheduled task, startup entry, service, Defender exclusion or registry installation. Cancellation waits for the active install/maintenance step to reach a safe boundary. Logs are in `Development/Logs/updater.log` and `updater-bootstrap.log`; per-patch backups and journals are under `Development/Updater/Transactions`.

## Future releases

The EXE does not contain a hard-coded game patch list. It reads [`updates/stable.json`](../updates/stable.json) from the official repository. The release workflow refreshes that file whenever a release is published or edited. It ignores drafts and prereleases and requires a published asset hash. Ordinary future patches using the same container format automatically extend the chain.

For a cumulative patch, nonstandard prerequisites or extra maintenance, attach `OpenHW-Update.json` to that release before publishing:

```json
{
  "from": ["1.0.5", "1.0.6"],
  "format": "openhw-exe-zip-v1",
  "minimum_protocol": 1,
  "post_actions": [
    {"kind": "repair-mission-skies-v1"}
  ]
}
```

Without that metadata, a patch depends on the preceding published game patch. `verified-helper-v1` is also supported: its `path` must name an EXE in the patch's verified file manifest; `args` is an array, `success_codes` defaults to `[0]`, and `timeout_seconds` defaults to 7200. Helpers must be noninteractive and operate on the same installation. They run with that folder as their working directory. New installer formats or actions require a new updater protocol.

Publish a newer updater EXE with `OpenHW-Updater.json` containing its `version`, `protocol`, official release `url`, `size` and `sha256`. The feed selects the newest updater version. Existing updaters download and verify it, finish their current process safely, replace themselves and restart automatically. The updater uses a private runtime so game patches can replace the game's installer dependencies without replacing DLLs loaded by the running updater.

The workflow needs GitHub Actions enabled and `contents: write`. If a feed refresh fails, the old feed remains available; inspect the workflow failure and rerun it. A release is not automatically available to the updater until its feed refresh succeeds. The generator can also be run with `python tools/publish_update_manifest.py --publish` using an authorized `GITHUB_TOKEN`.

## Build

On Windows with Visual Studio C++ tools, CMake, Python 3.12 and PyInstaller 6.22.3 available:

```powershell
cmake -S tools/updater -B out/updater -A x64
cmake --build out/updater --config Release
python tools/package_updater.py --source . --installed F:\OpenHW1 --bootstrap out/updater/Release/OpenHWUpdaterBootstrap.exe --pyinstaller-modules out/importer-build-deps --output Updater.exe --work out/updater/package
```

The installed game supplies the frozen installer modules and runtime support; its game data is not packaged into the updater. `--pyinstaller-modules` points to a directory containing the specified PyInstaller package. Increment the updater version in `tools/openhw_updater.py`, `tools/UpdaterVersion.h` and the packager together when publishing an updater change.

Tests: `python -m unittest discover -s tests -p test_portable_updater.py` and `python -m unittest discover -s tests -p test_update_feed.py`. The packaged native launcher/private Python runtime and a real 1.0.5 patch were also checked in an isolated installation fixture. The live game was not updated or started by those checks.
