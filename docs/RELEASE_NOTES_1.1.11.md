# OpenHW 1.1.11 — Waypoints, rally parades and multiplayer compatibility

Update through **Update now** in Launcher, then restart the game. No reinstall is needed. Multiplayer clients and the dedicated worker must use this matching build; create a fresh match after updating.

- Added **Waypoints**, **Rally Point**, **Rally Object** and **Clear Rally** to the ship context menu. Press **W** to start a route, click its points, and finish with Enter or Esc. Backspace removes the last point; Delete cancels placement.
- Ships maintain cruising speed through intermediate waypoint turns. Open routes stop at their final point.
- Click the first marker after placing at least two points to close a repeating loop. Loop turns remain pass-through points.
- Unformed ships and separate formation groups advance independently; ships within a formation stay together.
- Producers can rally new ships into military parade slots around a fixed point or an object-following virtual origin. No extra mothership is spawned. Repair/refuel relaunches retain their existing orders.
- Route, loop and rally state is saved with the game. Older saves remain supported.
- Deployed the matching dedicated multiplayer worker and compatibility identity. The server retains its limit of **10 simultaneous battles**.

Existing installations apply the supported patch chain from the 1.1.1 base. New installations use the existing refreshed base installer, then update in Launcher. Compiled patches are hosted on openhw.tech; GitHub provides source and release notes only.

Validation: both game variants built successfully; native navigation, route placement, loop persistence and packet validation checks passed. An isolated Launcher/updater transaction verifies the 1.1.10 upgrade and preservation of saves, settings, maps, mods and badges. Graphical multiplayer playtesting remains the next player test.
