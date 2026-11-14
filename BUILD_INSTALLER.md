# Build OpenHW 1.1.2 and its fresh installer

This is the complete cumulative source: Windows x64 engine, shaders, missions,
dedicated multiplayer server, tests, importer, launcher/updater and reviewed
release profile. No 1.0 patch chain is needed. The extra-turret Kushan Assault
Frigate mod is excluded.

## Build from source

Use Windows x64, Visual Studio 2022 or 2026 with Desktop development with C++ and
a Windows SDK, Git, CMake 3.25 or newer and Python 3.12 x64 with Tk and venv.
Python 3.9 is insufficient. Internet access is needed for pinned dependencies.
Use paths without spaces for FFmpeg/vcpkg.

From the source folder:

~~~powershell
.\Windows\build-release.ps1 -Python python -ReleaseVersion 1.1.2
~~~

This builds/tests both game variants and utility bootstraps, freezes the current
importer, packages Launcher.exe/Updater.exe, stages reviewed content, exports
clean cumulative source and audits the nested runtime.

Output: out/distribution/OpenHW-Setup.exe. Source defaults to the sibling
OpenHW-Source-1.1.2 directory, which must be empty or absent. Override with
-OutputDirectory and -SourceDirectory. The source build needs no installed game:
utilities reuse the newly frozen setup dependency image. Reuse verified caches
with -SkipBootstrap, -VcpkgRoot, -VcpkgInstalledRoot and -SdkRoot.

Both public variants explicitly use HW_ENABLE_LIVE_MAP_AUTHORING=OFF.
Shift+F11/F12 live map/lighting menus are unavailable; authored mission settings
still load. Developers can configure a separate CMake build with
-DHW_ENABLE_LIVE_MAP_AUTHORING=ON. The release script disables it.

## Assemble already-built current inputs

Maintainers can reuse a trusted installed setup dependency image while replacing
application modules with current source. The helper must include current
release_setup, conversion/import modules and SQLite support. Never copy an
older frozen importer unchanged. The normal build freezes current source
directly and needs no installed game.

With current game builds, utility EXEs, importer tools and a current frozen setup:

~~~powershell
python tools/stage_release.py --repo . --profile assets/Release --output out/release-stage-1.1.2 --normal-build out/build/release-no-diagnostics/Release --diagnostic-build out/build/deploy-x64-clean/Release --tools-dir out/import-tools --version 1.1.2 --launcher out/release-1.1.2/Launcher.exe --updater out/release-1.1.2/Updater.exe

python tools/export_release_source.py --repo . --output F:/OpenHW1/Source-1.1.2 --version 1.1.2

python tools/package_release_installer.py --stage out/release-stage-1.1.2 --frozen out/release-installer-freeze/OpenHW-Setup --bootstrap out/build/deploy-x64-clean/Release/OpenHWSetupBootstrap.exe --output out/distribution/OpenHW-Setup.exe

python tools/audit_release_bundle.py --installer out/distribution/OpenHW-Setup.exe --source F:/OpenHW1/Source-1.1.2 --output out/distribution/Release-Audit.json

python -m zipfile -c F:/OpenHW1/OpenHW-Source-1.1.zip F:/OpenHW1/Source-1.1.2
~~~

Use Python 3.12. Staging/source destinations must be new. Audit the exported
snapshot, not the developer's whole worktree. The manifest excludes itself from
its own hashes. Adjacent .payload.zip/.json and Release-Audit.json are build/audit
artifacts, not extra required downloads.

## Fresh installation and portable layout

Put setup in a new writable final game folder and select an owned Remastered
Collection. Required Classic/English archives are listed in portable_setup.py.
The Collection stays read-only. Setup rejects old installations even without a
completion receipt. A matching interrupted 1.1.2 setup can resume using its
recorded payload checksum; this is not a 1.0.x upgrade installer.

- OpenHW.exe: logging compiled out; multiplayer preview included.
- OpenHW-Daignostics.exe: full diagnostics (intentional spelling).
- Launcher.exe and Updater.exe: installed automatically.
- Development: setup support, logs, captures, symbols, cache and receipts.
- Settings, saves, mods and application-owned data remain in this folder.

Setup never launches the game. Display size/refresh detect on first boot; other
defaults come from reviewed presets, including global Non-PT Glow at 29%.

The main generated archive is about 33.61 GB before loose files, runtime,
shader caches and user content. Conversion needs additional working space.
The compact importer selects required HW1 entries instead of duplicating all
retail archives. Consuming final packing and early cleanup reduce overlapping
temporary copies. Exact full installation peak remains unmeasured. Until
measured, allow approximately 80 GB free on NTFS with hard links, or 100 GB when
hard links are unavailable. These are conservative planning estimates, not a
measured requirement or guaranteed maximum. Old 200-250 GB advice concerns
the superseded broad-extraction installer. Source builds need separate space.

## Content and source boundary

assets/Release/content-inventory.json records shipped presentation files and
hashes. Supplied concept art, approved upscaled Classic textures and the owner's
asteroids are identified exceptions, not public-domain claims. Retail archives,
HOD/MAD models, unchanged textures, original speech/music/movies and converted
Remastered materials are imported/generated locally. Unused Kharak textures are
excluded.

Mission files follow effective WIP priority: loose exports when enabled, then
RTXExports/Maps, then Data/Missions; lighting uses RTXLighting, then Data/Missions.
mission-authoring.json records selected hashes. Setup publishes/verifies the
physical Data/Missions loader path; archive-only copies are insufficient.

The source exporter includes server Python, service/timer units, requirements
and guides. Private server state/configuration, credentials, owned game data,
Wine prefixes and logs stay out. Curated docs/media illustrations, launcher
art/fonts and notices are included. SOURCE_MANIFEST.json lists files and hashes.
Build caches, personal saves/logs and Git worktree history/pointers are excluded.
Original notes/licenses remain; begin with current documentation.

The cloud adaptation keeps its upstream permission-status note in
src/ThirdParty/OpenMWVolumetricClouds/PROVENANCE.md. Packaging does not resolve
that note or change upstream licensing.

## One-time 1.1.1 updater recovery package

Build the utilities and the utility-only 1.1.1 patch first. Generate the offline recovery inputs with `python tools/create_repair_bundle.py --patch <OpenHW-Patch-1.1.1.exe> --launcher <Launcher.exe> --output <bundle-folder>`. Then run the existing `tools/package_updater.py` packaging command with `--bundle <bundle-folder>` to produce the one-time replacement Updater.exe. This bundles only the verified patch and Launcher component, never an entire retail extraction. The resulting replacement also supports future server-driven updates; bundled recovery is selected only for installations that still need it. The utility's internal revision is distinct from the game patch version.
