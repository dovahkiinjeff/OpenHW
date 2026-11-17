# OpenHW 1.1.12 — Campaign fixes and Launcher rollback

Choose **Update now** in Launcher, then restart OpenHW. No reinstall is required. Online multiplayer uses the matching 1.1.12 server build; create a fresh match after updating.

## Launcher

- Ship Systems now includes **Previous Version → Roll Back**. Restore the previous game version from a verified backup retained by a completed update.
- Saves, settings, mods and badges are preserved. Launcher and Updater remain current so you can update again.
- Missing or damaged backups are refused. A failed restore restores the current files. Rollback does not cross the required new-install boundary; newer saves may require the newer game and online play requires the server's matching version.
- Launcher and Updater utilities are version 1.1.6. Both update feeds and the full supported patch chain remain available.

## Waypoints

- Routes now reject legs and corners too small for safe turns, including loop closures. Invalid draft lines flash red; lengthen the legs or widen the turn before finishing.
- Validation accounts for the selected ships' clearance and turning needs and is also enforced when issuing orders. Ordinary stop and rally commands are unchanged.

## Campaign and captured ships

- Removed the same-race restriction for docking captured ships in allied carriers and motherships. Launching remains available across races. Existing stranded captured fighters retry docking during hyperspace roll call without interrupting busy orders.
- Close the launch manager before hyperspace docking starts, preventing stale hangar state from blocking captured-ship processing.
- Captured fuelpods use the small hangar, can retire, and use a closer parade slot around player motherships unless a custom layout overrides it.
- Campaign starts enable fog of war by default. Corrected the Mission 4 retreat warning faction and allowed scripted planet rendering during the destruction cinematic.
- Resource asteroids and dust can be focused with Alt-click in the world and Sensors views.
- Bentusi ion beams no longer use the generic long firing throttle and have a yellow beam tint.
- Movie speech follows presentation timestamps. At normal video end the final frame can remain while queued narration finishes; skipping remains immediate.
- Native Khar-Selim salvage approach uses the actual wreck contacts instead of the legacy distant approach cone, including a saved approach already in progress.
- Taiidan Support Frigate uses native navigation-light positions and suppresses unmatched Classic docking lamps for the verified native hull.

## Validation and distribution

Both normal and diagnostic game builds passed the focused navigation, docking, salvage, presentation, effects and launch-manager tests. Campaign regression harnesses cover the corrected rules. Launcher restore tests cover player-file preservation, corrupt backups and rollback after an interrupted restore. An isolated installation is checked through update, rollback and re-update. These checks do not replace an interactive campaign playthrough.

The dedicated server uses the matching simulation identity and retains capacity for 10 simultaneous battles. Compiled patches, Launcher and Updater are hosted on openhw.tech. GitHub publishes cumulative source and source checksums only. The existing full base installer remains available; existing players use patches.
