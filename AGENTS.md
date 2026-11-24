# Publication policy

## AI laboratory separation

- `tools/ai-lab/` is source-only administrator research tooling. Its instrumented engine is built from an isolated source copy and must never be packaged in installers, Launcher/Updater payloads, game patches, or installed over the public multiplayer worker.
- Keep filled server configuration, SSH aliases, credentials, private keys, collected logs and private binaries out of public source. Publish only generic templates and source.
- Notify the owner immediately before starting real AI matches. Default experiments are ten concurrent 4v4 Hard matches with reproducible randomized teams, fixed original simulation ticks and a shared 100 GB diagnostic-log ceiling across retained runs.

## Release workflow

When the owner says "publish the update", complete this workflow:

- Publish a versioned update entry and release notes on the website and GitHub.
- Build and publish a fully assembled fresh-install package for every current game release. The website's main download and fresh_install metadata must point to the latest game version, including all patches and current Launcher/Updater, so a new player does not install an old base and then download a patch chain. Replace the promoted full download whenever a release completes; retain older files only for supported upgrade/recovery paths.
- Keep fresh-install downloads separate from existing-player updates. Updating the main full download must not force current players to reinstall or remove any supported incremental update paths.
- GitHub release assets contain source code only, with source checksums if needed. Never attach compiled installers, game executables, Launcher.exe, Updater.exe or binary patches to GitHub.
- Upload compiled files to the OpenHW server under /srv/openhw-downloads/downloads/vVERSION/, verify size and SHA-256 before publishing metadata, then atomically enable the server update feed.
- The Launcher detects newer versions from https://openhw.tech/updates/stable.json. The GitHub updates/stable.json is a metadata fallback only; its download URLs must point to the server.
- Preserve supported existing-installation upgrade paths. Do not use a full installer over an existing game folder.
- Update the website server-download catalog alongside the release entry so downloads remain available even though GitHub hosts no compiled assets.
- Verify the live website, release entry, server assets and both metadata feeds before reporting success.
- Read and write release text explicitly as UTF-8. Set the approved title explicitly when creating AND updating a GitHub release; never inherit an older release's subtitle through template replacement. Reject broken-encoding markers in titles and verify the exact live GitHub/API title. Releases 1.1.17 and 1.1.17a are titled only "OpenHW 1.1.17" and "OpenHW 1.1.17a".
- Source-only publication is the standing default for all future releases unless the owner explicitly changes it.

## Owner-directed 1.1.15 baseline reset

For release 1.1.15, the owner explicitly requests a fully consolidated compiled installer on GitHub. Every earlier release moves directly to that complete package, replacing program files in place while preserving player files. This overrides the source-only GitHub and separate-new-folder rules above for the 1.1.15 baseline package. Keep the full baseline available for later upgrade paths.

## Owner release-check preference

- Do not run antivirus scans during development or release publication unless the owner explicitly requests them. Continue build, checksum, package, update-path and functional checks.
