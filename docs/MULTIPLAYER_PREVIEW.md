# OpenHW dedicated multiplayer preview

This preview connects OpenHW to the gameplay server at `openhw.tech`. The server runs the game simulation and AI. Players create a room, share its six-character code, choose teams and rules, and receive the same ordered battle frames. No player needs to forward ports.

## Start a preview match

1. Install OpenHW 1.1 into a new writable folder using the fresh installer and your owned Collection. No earlier patch chain is required.
2. Start `OpenHW.exe`, or `OpenHW-Daignostics.exe` for logging. Both standard 1.1 executables include the multiplayer preview; separate preview EXEs are unnecessary. Launcher.exe can start either variant.
3. Open **Multiplayer > Online Multiplayer** and choose **Create Match**. To join another room, type its six-character code and choose **Join Match**.
4. The room host selects the map, player races, AI difficulties, teams and match rules. **Add AI Player** adds a computer opponent; click or scroll its race, difficulty and team to change it. Team **FFA** means no shared alliance; matching numbered teams are allies. Enable **Allied Victory** for a team win when only allied players remain.
5. Everyone selects **Ready** after reviewing the settings. Changes clear readiness. The host selects **Start Match**. The battle begins when all humans and the server finish loading matching gameplay data.

The host can test alone against an AI. A second human needs the same 1.1 build and matching installed gameplay data. Older 1.0.x EXEs cannot join this server. Use unmodified gameplay data for the first test; enabling arbitrary ship mods is not supported by this preview's compatibility checks.

## Host controls

The lobby's **Gameplay**, **Map / Match** and **Advanced** tabs contain:

- Unlimited fuel, orders while paused, weapon recoil, resource yield multiplier, ranks and starting launch behavior.
- Harvestable resource population, unlimited RUs, fog of war, a larger starting resource fleet, Super Salvagers, Capture / Build All and unit-cap overrides.
- Map, starting fleet/resources, bounty, harvesting, research, fuel burn, unit caps, hyperspace and victory rules.
- Resource injections and lump sums with their timing and amounts, crates, carrier-as-mothership, and slot hull/stripe/engine colors.

These rules apply to the whole match, including AI players. They are locked after starting and do not replace each player's saved single-player preferences. Camera, cursor, FPS display, graphics and audio remain personal settings. During a battle, only the host can pause or resume with **P**.

RTS orders, formation spacing, direct piloting, EVE-style Command control, eligible X/Claw wing-leader control and per-ship appearance orders use the canonical server command stream. Existing ship eligibility and control keys still apply.

## Preview limits and verification

The VPS currently permits **one running battle at a time**. Map capacities still govern the number of humans and AI, up to eight total slots. Rooms are private by invite code; accounts, a public room browser and matchmaking are not included.

A human disconnect ends the battle. Rejoining a running battle, saving/resuming an online match, and host migration during battle are not implemented. Lobby ownership can transfer before a match starts. A loaded-world mismatch or detected simulation divergence stops the match instead of allowing the players to continue with different states.

Automated testing uses the real VPS over TLS, the Windows simulation under Wine on the server, and Windows replicas with different local players. It checks load agreement, canonical frame replay, AI/team setup, accepted player orders and rejected ownership violations. Rendered menus, camera behavior and a full interactive match still need player testing. The preview is included in 1.1; it does not claim unrestricted production multiplayer support.

The loading handoff no longer waits for obsolete peer progress packets. The
multiplayer chat overlay also preserves its full 64-bit frontend atom address;
an old 32-bit cast caused a crash on the first HUD draws after loading. The
headless chat regression exercises the actual HUD callback and its reuse for
another match without opening a game window.

## Source and hosting

The implementation lives in `src/Network`, with the lobby drawn by `src/Game/ModernOnlineLobby.inc`. Server setup and developer test commands are in [the server guide](../server/README.md). Retail game content belongs only in private installed/server data directories; it is not part of the server source package.


## Server timing correction

The November 13, 2026 server-side hotfix restores normal match speed: 32 server
command frames per second supply the engine's 64 physics ticks per second.
Previously, the old 8 Hz command cadence made matches advance at quarter speed,
with large gaps between movement updates despite high rendering FPS. No ship
speed values or client executables changed. Existing 1.1 players need only start
a new match; there is no additional game patch to download for this correction.
