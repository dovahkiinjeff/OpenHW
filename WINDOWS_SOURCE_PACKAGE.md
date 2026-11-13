# OpenHW 1.1 Windows source package

This is the full cumulative working source, including all 1.0 updates, 1.1
community fixes, dedicated multiplayer preview, launcher/updater and installer.
It is not a patch against another checkout. Follow
[BUILD_INSTALLER.md](BUILD_INSTALLER.md) to build both game variants,
Launcher.exe, Updater.exe and the fresh-install OpenHW-Setup.exe.

Owned retail archives/models/audio/movies are imported from the user's
Collection. The reviewed profile, inventory, curated documentation images and
source manifest are included. Server code is public; private configuration,
credentials, Wine state, logs and game data are excluded.

Public builds disable Shift+F11/F12 live map/lighting editors while retaining
authored mission loading. Developer builds can enable HW_ENABLE_LIVE_MAP_AUTHORING.
Original source notes are preserved; begin with
[the current documentation index](documentation/README.md).
