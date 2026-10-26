# OpenHW loose-data WIP layout

`F:\OpenHWWIP` is a separate private development install. It reads unpacked
resources directly from `Data` and does not mount or contain an OBIG archive.
The `openhw.loose` marker selects this startup mode; remove the marker only
after building and installing a compatible OBIG package.

- `Data`: authoritative game assets, including converted RM ships, subsystems,
  presentation events/FX, audio, UI, cursors, planets, asteroids and missions.
- `Dev`: source-import receipt/data, setup tools, documentation, logs, crash
  diagnostics, benchmarks, shader cache and visual exports.
- `Modding`, `mods`, `SavedGames`: user work and saves, kept separate from the
  game-data tree.

The base loose files and setup sources are NTFS hard links to the original
converter output under `F:\OpenHW`. They remain usable if the original path is
removed, but **editing a linked file in place also changes that converter-cache
file**. To make an independent edit, first copy the file to a temporary name
within WIP and replace the original path with the copy. The presentation
overlay, existing loose overrides, editor assets, and subsystem catalog are
independent copies.

`F:\OpenHW` is not changed by WIP staging. The WIP build has passed automated
tests and asset-inventory checks, but gameplay startup and rendering still
require an in-game smoke test.
