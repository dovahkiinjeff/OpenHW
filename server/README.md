# OpenHW dedicated server

The dedicated server runs the existing OpenHW CPU simulation and AI, validates player commands, and broadcasts canonical ordered frames. Clients mirror those frames to render the battle. The Ubuntu deployment runs the same Windows x64 build under Wine; it is not a separate native Linux game engine or a peer-to-peer relay.

## Components and compatibility

`coordinator.py` accepts TLS WebSocket clients through Caddy, manages rooms and the loading barrier, and supervises one worker process per battle. `lobby.py` owns slots, readiness, AI, teams and host permissions. `protocol.py` matches `src/Network/OnlineProtocol.h`. The game executable's `--dedicated-worker` entry starts before graphics, audio, splash screens and user settings.

The source build generates `server-fingerprint.txt` and the client's embedded build fingerprint. Install a matching worker and fingerprint whenever the simulation changes. Normal and diagnostic builds use the same compatibility identity. Before tick zero, the service also compares a pointer-free signature of the loaded initial world and core ship gameplay parameters. This is not a complete hash of every mod file; arbitrary mod synchronization is outside this preview.

The worker advances the original 16 Hz simulation in pairs of ticks and produces eight canonical command frames per second. AI updates run on the authority only. Human commands are bounded and checked against the connection's assigned player and owned ships. The public gateway never accepts worker inspection requests, arbitrary process commands or filesystem paths from clients.

## Private Ubuntu deployment

The current layout is:

| Location | Purpose |
| --- | --- |
| `/srv/openhw-game/app` | Gateway source, service definition and build fingerprint |
| `/srv/openhw-game/game` | Private owned game content, worker EXE and runtime DLLs |
| `/srv/openhw-game/state` | Private JSON configuration and Wine prefix |
| `/srv/openhw-game/logs` | Per-battle worker logs |
| `/srv/openhw-game/venv` | Python environment with `requirements.txt` installed |

The service runs as the unprivileged `openhw-game` user. Its writable paths are limited to state and logs. The game data directory must remain outside the website's document root. Never attach the private content bundle, Wine prefix, credentials or player logs to a public release.

Dependencies are Python 3.12 or newer, the pinned `websockets` package, Wine64, Caddy, and the runtime DLLs used by the matching Windows build. Build private content from an owned, matching OpenHW installation:

```powershell
python server/build_private_content.py F:/OpenHW1 F:/OpenHW1/Development/Multiplayer/ServerContent
```

Copy the worker EXE as `OpenHW-Server.exe`, its required runtime DLLs and the generated content archive to the private game directory. The content builder excludes presentation media and DDS textures while preserving simulation definitions, geometry, collisions, hardpoints and animation data. Original game archives are not supplied by this repository.

With the service account, directories and Python environment prepared, list the worker's installed maps using that account and Wine prefix:

```sh
cd /srv/openhw-game/game
sudo -u openhw-game env WINEPREFIX=/srv/openhw-game/state/wine WINEDEBUG=-all \
  WINEDLLOVERRIDES='mscoree,mshtml=' /usr/lib/wine/wine64 ./OpenHW-Server.exe \
  --dedicated-probe --list-maps > /srv/openhw-game/state/maps.log
```

Place `server-fingerprint.txt` and the files from this directory in `app`, install `requirements.txt` into `venv`, and run `configure_host.py` as administrator on the intended Ubuntu host. That script targets this exact directory layout, generates the private configuration from the validated map list, installs the systemd unit, and adds a Caddy route while preserving the existing website handlers. Review it before using it on a different host. It enables the service; after replacing an already-running worker or changing its fingerprint, restart the service explicitly.

The public route is `wss://openhw.tech/multiplayer/v1`. Caddy forwards only that path to loopback port 8767:

```caddyfile
handle /multiplayer/v1 {
    reverse_proxy 127.0.0.1:8767
}
```

Keep this handler before the website's fallback handler. The gateway itself listens only on `127.0.0.1`; no separate public game port is needed. `OPENHW_SERVER_CONFIG` selects the administrator-owned JSON configuration. `max_matches` currently defaults to one. Raising it requires measuring the CPU and memory budget for simultaneous battles.

## Operate the service

```sh
systemctl status openhw-multiplayer
journalctl -u openhw-multiplayer -n 50 --no-pager
systemctl restart openhw-multiplayer
```

Restarting ends any running battle. Worker logs are written below the private logs directory. The diagnostic EXE and matching PDB give more useful crash details. Wine can create operating-system state in its private prefix even though the game worker bypasses user settings and rendering.

## Developer checks

Build `homeworld` in the diagnostic and no-diagnostic Release configurations. Keep `server-fingerprint.txt` with the corresponding candidate; exact fingerprints must agree between both client variants and the deployed worker.

```powershell
python -m unittest discover -s server -p test_lobby.py -v
python -m unittest discover -s server -p test_coordinator.py -v
python server/test_loading_gate.py PATH/TO/OpenHW-Server.exe PATH/TO/PRIVATE-CONTENT --logs PATH/TO/TEST-LOGS
python server/test_chat_overlay.py PATH/TO/OpenHW-Server.exe PATH/TO/PRIVATE-CONTENT --logs PATH/TO/TEST-LOGS
python server/probe_worker.py PATH/TO/OpenHW-Server.exe PATH/TO/PRIVATE-CONTENT --steps 512 --log PATH/TO/worker.log
```

These checks cover room permissions, match-capacity cleanup after cancellation, the actual frontend loading gate with no legacy peer progress packets, and the multiplayer chat overlay's 64-bit callback address through its first draw and reuse. The chat check dispatches the real frontend callback with scene drawing disabled and creates no game window. The worker probe runs a real native simulation with AI, records its frames, and replays them through a separate headless replica.

Dedicated clients finish local loading before sending their loaded-world signature. They do not wait inside the original peer-to-peer progress exchange. The coordinator then waits for all loaded-world signatures; its loading deadline defaults to 900 seconds to accommodate first-use shader preparation and can be set with `load_timeout_seconds` (180 to 1800 seconds). Each client completion and the server's barrier transition are recorded in the service log.

For live testing, install `requirements.txt` into a local test environment and use the matching running service:

```powershell
python server/test_live.py PATH/TO/DIAGNOSTIC.exe PATH/TO/PRIVATE-CONTENT PATH/TO/server-fingerprint.txt --second-exe PATH/TO/NORMAL.exe --controls --frames 192 --logs PATH/TO/TEST-LOGS
```

This creates two human connections against easy and advanced AI on separate teams. It compares the two player perspectives, replays VPS frames on Windows, verifies movement/color/formation orders and ownership rejection, and checks host pause authority. `--controls` also exercises direct piloting, EVE navigation and X formation wing-leader control. The private inspection pipe exposes ship positions and control membership for these assertions; it is not reachable through the public gateway.

`OpenHW-Multiplayer-Preview.exe --online-probe` checks the actual native WinHTTP TLS connection, room creation and leave path without opening a game window. `OPENHW_MULTIPLAYER_URL` can select a development HTTPS endpoint; plain HTTP is allowed only for loopback testing.

Interactive rendering and a complete played battle remain separate acceptance checks. Read [the preview guide](../docs/MULTIPLAYER_PREVIEW.md) for the current player-facing limits.

## Private diagnostics reports

`diagnostics.py` is a separate standard-library HTTP service. The menu saves named upload profiles using a community alias and comment, creates a persistent private server folder at save time, then explicitly uploads all previously unuploaded log data. It does not collect configuration files, passwords or installed game data. Logs can contain player information and local paths; reports are private and must not be published as release assets.

The versioned HTTPS route is `POST /diagnostics/v1/reports` with `Content-Type: application/zip` and one `Content-Length`. Success returns HTTP 201 and `{"report_id":"..."}`. There is no public retrieval endpoint. The service binds only to `127.0.0.1:8768`; Caddy replaces `X-Forwarded-For` with its actual connecting address for rate limiting. Other headers cannot name output files or destinations.

Each ZIP contains `report.json`, flat `logs/<basename>` entries with `.log`, `.txt`, `.json` or `.dmp` extensions, and optionally one flat `saves/<basename>` entry. Extensionless saves and `.sav`, `.save`, `.game` are accepted. Metadata is UTF-8 JSON:

```json
{
  "schema": 1,
  "alias": "Community alias",
  "comment": "What happened and how to reproduce it",
  "created_utc": "2026-11-13T12:00:00Z",
  "game_version": "1.1",
  "files": [{"path": "logs/renderer.log", "size": 123}]
}
```

`created_utc` and `game_version` are optional. Alias is required, nonempty and at most 80 characters; comment is required and at most 4000 characters; the version string is at most 32. The inventory must exactly match each attachment's archived path and uncompressed size. Original absolute source paths do not belong in metadata. Unknown schemas and metadata fields are rejected so a future format requires a deliberate server change.

The compressed body is limited to 64 MiB, expanded content to 256 MiB, metadata to 64 KiB, and ZIP membership to 64 entries, with at most 60 logs and one save. Names, file types, duplicate paths, symlinks, sizes and every member's CRC are checked without extracting anything. Repetitive logs with high compression ratios are accepted within the expanded limit. Headers have a 15-second absolute deadline, body upload a 90-second deadline and validation a 30-second deadline. At most four request connections and two uploads run concurrently. Default intake limits are 120 attempts per address and 600 globally per ten minutes, 10 GiB total stored reports, and 1 GiB free space retained. Full storage rejects new reports; it never silently deletes old reports.

The current menu first sends `POST /diagnostics/v1/profiles` with JSON `{schema:1,name,comment}`. Saving creates `/srv/openhw-reports/<sanitized-alias>-<random>/profile.json` and returns `profile_id` plus a private random upload token. Only the token's SHA-256 is stored server-side. Updating the same profile sends its ID and `Authorization: Bearer <token>`; no public profile listing or retrieval exists. Names are at most 80 characters; comments at most 4000. The client stores the credential under its portable Development/Reports folder, never in game logs.

New ZIP reports include `profile_id` and the same Authorization header. Validated archives and JSON sidecars are stored flat inside that persistent profile folder using unique report IDs. The optional per-file fields `source`, `offset` and `sha256` identify bounded chunks from a relative Development/Logs path; the server checks the actual streamed SHA-256. At inspection time, combine a file's recorded offsets, selecting the newest overlapping chunks, to reconstruct its snapshot. The client remembers acknowledged chunk hashes globally across saved profiles, includes changed chunks again, and persists history after each batch receipt. Legacy clients without profile IDs remain supported: their reports retain `/srv/openhw-reports/<sanitized-alias>-<UTC>-<random>/report.zip` and `report.json`. The directories are private to `openhw-game`. The alias never becomes an arbitrary path, and uploaded content is never executed. An administrator should review, archive or remove reports according to the project's retention needs.

Copy `diagnostics.py`, `configure_diagnostics.py` and `openhw-diagnostics.service` into `/srv/openhw-game/app`, then run this on the intended VPS:

```sh
sudo python3 /srv/openhw-game/app/configure_diagnostics.py
systemctl status openhw-diagnostics
journalctl -u openhw-diagnostics -n 30 --no-pager
```

The helper validates and backs up Caddy configuration, preserves website/download/multiplayer handlers, prepares private storage and enables the restricted service. It refuses an unexpected route layout. After later code replacements, restart `openhw-diagnostics` explicitly. This service is independent of the game worker and does not require stopping matches. Its regression suite runs without the live server or game:

```powershell
python -m unittest discover -s server -p test_diagnostics.py -v
```

The separate `openhw-update-mirror.timer` refreshes the verified GitHub download mirror every 15 minutes after deployment. `mirror_updates.py` uses the updater verification code installed beside it; mirrored public downloads stay in `/srv/openhw-downloads`, entirely separate from private diagnostics reports and owned game content.


## Real-time simulation pacing

The modern physics rate is 64 ticks per second. Each worker/client SYNC advances
two ticks, so the gateway sends 32 command frames per second using monotonic
deadlines that include worker/transport processing time. The original 8 Hz
packet cadence advanced only one quarter of real time with this engine build.
The server-only 1.1 timing hotfix corrects that cadence; players keep their
existing 1.1 game files and start a new match after deployment.

`test_coordinator_clock.py` exercises the actual match loop with a virtual wall
clock and validates the engine-rate contract. Add `--realtime --frames 192` to
`test_live.py` to measure game-time versus wall-time after warmup while replaying
both Windows clients. The live fix measured 4.969 simulated seconds in 4.969 wall
seconds, with matching checksums, AI/teams, controls and host pause. This checks
simulation speed; it does not prove that every rendered movement artifact is gone.
