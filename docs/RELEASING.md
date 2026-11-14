# Publication policy

When the owner says "publish the update", complete this workflow:

- Publish a versioned update entry and release notes on the website and GitHub.
- GitHub release assets contain source code only, with source checksums if needed. Never attach compiled installers, game executables, Launcher.exe, Updater.exe or binary patches to GitHub.
- Upload compiled files to the OpenHW server under /srv/openhw-downloads/downloads/vVERSION/, verify size and SHA-256 before publishing metadata, then atomically enable the server update feed.
- The Launcher detects newer versions from https://openhw.tech/updates/stable.json. The GitHub updates/stable.json is a metadata fallback only; its download URLs must point to the server.
- Preserve supported existing-installation upgrade paths. Do not use a full installer over an existing game folder.
- Update the website server-download catalog alongside the release entry so downloads remain available even though GitHub hosts no compiled assets.
- Verify the live website, release entry, server assets and both metadata feeds before reporting success.
- Source-only publication is the standing default for all future releases unless the owner explicitly changes it.
