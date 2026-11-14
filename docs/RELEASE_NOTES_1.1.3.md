# OpenHW 1.1.3 - Offline editors and interface corrections

**Choose Update now in Launcher. No reinstall is required.** Existing 1.1.1 installations apply 1.1.2 and then this patch in order. New players install the full 1.1.1 base and update through Launcher.

## Interface corrections

- The Quit Game confirmation is smaller and centered, with concept art beneath its translucent panel and centered confirmation buttons.
- Drag the top edge of the Selected Ships roster vertically to show more or fewer rows. The height is saved between launches.

## Repair Corvette engine glow

The Kushan/Hiigaran Repair Corvette engine-bulb radius is reduced to half its previous size.

## Editor availability

- **Single-player:** Shift+F11 map editing and Shift+F12 lighting editing are enabled.
- **Local multiplayer / offline skirmish:** live map, lighting and texture editing are enabled.
- **Online multiplayer:** these editors remain blocked. Hotkeys, direct entry points, tool switching and queued map-editor boot paths all share the online restriction. An open map or lighting editor is closed before installing an online match state.

This replaces the blanket editor disablement in the earlier public builds. The restriction now follows whether an online server match is active, rather than treating every multiplayer session as online.

The mixed-fleet formation spread, concept-art panels and persistent room directory from 1.1.2 remain included. The matching multiplayer worker is deployed with this update, so all online players must update to 1.1.3.

Compiled updates are hosted only on openhw.tech. GitHub hosts source and release notes, with metadata fallback pointing to the game server.

## Validation

Both game modes are built with live authoring enabled. The shared policy is checked for offline/local availability, online rejection, return to offline play and the optional disabled-build override. The patch is checked through the real transactional updater with settings and a save preserved. Server and TLS/directory checks accompany deployment. No interactive game/UI testing is performed by the publishing agent.
