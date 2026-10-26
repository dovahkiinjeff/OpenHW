# Build OpenHW 1.0 and its installer

This is the current complete engine/build source, not a patch against another
checkout. It includes the C/C++ engine, HLSL shaders, mission code, third-party
code and notices, tests, import/conversion code, and the reviewed release profile
under `assets/Release`. The profile contains the approved defaults, map/cloud
authoring and native ship/subsystem definitions from the current working game.
The extra-turret Assault Frigate mod is not part of it.

Authored mission defaults use the WIP's actual runtime priority: maps from
`Dev/RTXExports/Maps` when loose mode is enabled, otherwise `RTXExports/Maps`,
then `Data/Missions`; lighting from `RTXLighting`, then `Data/Missions`.
`assets/Release/mission-authoring.json` records the selected files and hashes.
Setup writes these files to the physical `Data/Missions` directory used by
the runtime's archive-bypassing loaders and verifies them before reporting
installation complete. Root `missions` or archive-only copies are insufficient.

## Prerequisites

- Windows x64, Visual Studio 2022 or 2026 with Desktop development with C++ and a
  Windows SDK; Git; CMake 3.25 or newer; Python 3.12 x64 with Tk and venv.
- Internet access for the pinned public build dependencies. The build script
  creates its Python environment, downloads/caches and output below this tree.
- Use a source path without spaces for FFmpeg/vcpkg, such as `F:\OpenHW1\Source`.
  This build does not require the developer's WIP directory or installed game.
- Running the finished installer requires the user's own HWRM Collection,
  including Homeworld 1 Classic and the English archives listed by
  `tools/portable_setup.py`. The selected installation is read-only.

From this source folder in PowerShell:

```powershell
.\Windows\build-release.ps1 -Python python
```

The script builds both Release variants, runs their tests, builds the native
import tools, freezes the setup engine, stages only inventoried release content,
checks the nested runtime archive, and creates:

`out\distribution\OpenHW-Setup.exe`

That single executable is the download. The adjacent `.payload.zip` and `.json`
are build/audit artifacts, not additional files users need to download. To choose
another output folder, pass `-OutputDirectory`.

Existing verified SDK/vcpkg caches can be reused with `-SkipBootstrap`,
`-VcpkgRoot`, `-VcpkgInstalledRoot`, and `-SdkRoot`. These are optional build
parameters; no machine-specific cache path is required by the source.

## Installed layout

- `OpenHW.exe`: diagnostics/logging compiled out.
- `OpenHW-Daignostics.exe`: full diagnostics (intentional requested spelling).
- `Development`: setup components, logs, captures, symbols, cache and receipts.
- Settings, saves, imported retail resources and all other application-owned
  files stay in the folder containing the installer.
- Defaults use the captured current options, including global Non-PT glow 29%.
  Resolution, refresh and adapter identity are detected on first boot.
- Setup does not launch the game.

## Content boundary and source snapshot

`assets/Release/content-inventory.json` enumerates every shipped presentation
file and its SHA-256. The project owner explicitly approved supplied concept
art and the upscaled Classic texture set. Those exceptions are identified;
they are not described as public domain or as containing no game-derived art.
The owner confirmed that the replacement asteroid models/textures are their
own work; these are included and identified separately. Unused Kharak textures
are excluded as requested.
Unchanged Classic LIFs, retail archives, HOD/MAD meshes, original audio, and
converted Remastered material files are excluded. Setup imports/builds them
from the selected installation, including compiled event data.

`SOURCE_MANIFEST.json` lists the snapshot's files and hashes. Build/dependency
caches, retail data, personal saves/logs, and the originating Git worktree
pointer/history are not source inputs and are excluded. Original repository
source and license notices are preserved; this is not a license reclassification.

The existing cloud adaptation has an explicit upstream permission-status note
in `src/ThirdParty/OpenMWVolumetricClouds/PROVENANCE.md`. That note remains
unresolved by this packaging work. No permission or public-domain status is
inferred merely from a repository being publicly accessible. Publication does not resolve that upstream permission-status note.

## Installation disk budget

**Disk space:** allow approximately **120 GB free during conversion** as a conservative planning estimate; the exact peak of this compact installer has not been measured. The download is about 473 MB and the main generated game archive is about 34 GB, plus loose resources and user data. Setup reads the original Collection archives directly, stages selected HW1 resources and removes intermediate conversion caches. The original Collection and source-build dependencies occupy separate space.

The compact importer reads the original Collection archives without copying them. It keeps a single main conversion tree, removes each faction material/package cache after merging, and converts backgrounds one mission at a time. Original HOD bytes are preserved; immutable scratch assets use hardlinks when supported, while editable definitions remain independent. The installer rejects Hiigaran/Vaygr faction assets except the approved shared Taiidan VgrBeam dependency.
