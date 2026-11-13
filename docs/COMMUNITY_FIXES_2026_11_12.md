# Launcher and community fixes - OpenHW 1.0.7

Released in OpenHW 1.0.7. See [the illustrated feature tour](FEATURE_TOUR_1.0.7.md) and [release notes](../RELEASE_NOTES_1.0.7.md).

## Interface and input

- Added the portable Launcher, including updates, normal/diagnostics launch, settings, cache rebuilding, mod activation and load order. See [LAUNCHER.md](LAUNCHER.md).
- Replaced nonfunctional keyboard country adapters with selectable shortcut layouts. Shortcut release uses the layout of the original press, preventing stuck keys when layouts change. Physical number/function/modifier keys remain stable, and text fields retain their normal input path.
- Build-panel wheel input is consumed over the entire docked panel, including headers, buttons and gaps, instead of leaking camera zoom through those areas. Scripted Classic tutorial layout remains unchanged.
- Command, formation and tactics menus opened in Sensors are parented to the Sensors view. Opening a menu no longer replaces and deletes the Sensors screen, which caused the black screen when the menu closed.

## Ships and campaign

- Added a **COLORS** button beside PILOT when one owned ship is selected. Its popup controls hull/base, stripe, engine trail/bulb, navigation-light, resource-beam and hyperspace colors. Each channel has an inheritance switch; Reset All restores inherited appearance. Apply affects only that individual ship. It does not change ownership, fleet paint, authored models or shared texture masks.
- Paint is saved in an optional extension after the existing save blocks, without enlarging `Ship` or changing its binary layout. Both executables use the same extension. Appearance materials are private to edited ships, released on retirement/model unloading, and recreated from unchanged source materials. Unedited ships retain shared materials. This is a local visual customization, not a new synchronized multiplayer command.
- Added **Starting Resource Fleet** to Gameplay and Launcher: mission default, or top up fresh campaign mission arrivals to at least three collectors and two controllers. It preserves larger carried fleets, respects the selected race, preloads the needed ship data and does not inject new ships when loading a save.
- Kushan Support Frigate nav lights use the actual native HOD joints, including scale, basis and animation, while keeping their classic blink/color rules. Explicit authoring marker overrides have precedence.
- Captured frigates approach a holding point near the mothership's actual frigate hangar instead of waiting inside a 6,000-unit radius around the mothership center. Capture stages consistently use the native `Frigate_In` endpoint when available. Bounded docking-point searches replace three loops that could read beyond the list when a named point was absent.
- The original separate incoming/outgoing berth lists and collision avoidance remain active. No blanket docking-speed multiplier or removal of berth reservations was applied. General traffic throughput and the revised capture approach still need player verification in busy scenes.

## Skirmish and textures

- The economy AI can request a carrier after establishing resource collection, if research, unit caps and a cash reserve allow it. Existing queued carrier construction is counted, so it does not repeatedly order duplicates. Carrier support/production uses the existing AI systems.
- Inspection confirms that construction already runs concurrently across different ship types. No global one-ship build restriction was found or removed. RUs, research and configured caps still constrain construction.
- New paint palettes rebuild a loaded texture through the normal pending-texture path. A full palette list falls through to another registry entry rather than writing past its eight slots.
- Fixed the 64-bit texture unregister guard to validate the registry index and palette separately. A valid palette-encoded handle is not an out-of-range registry index.
- Edited ships use their own path-tracing surface variants. Their modern albedo uses the same classic/RM paint masks, hue scalars and paint shading as the regular texture path, preserving alpha and keeping authored glow/normal/material textures intact.

## Validation

- Both normal and diagnostics x64 Release builds.
- Launcher/updater tests for setting preservation and conflicts, mod priority/activation, resource-scaling alias compatibility, local-only cache reset and speech-bank/table pairing; frozen-launcher payload check without a game launch.
- Production ship-appearance tests for shared-material isolation, private material reuse/retirement, all six channels and save/load restoration.
- Production texture-paint checks for classic and native RM masks, paint shading, preserved alpha, absent masks and invalid handles.
- Keyboard layout tests for automatic/explicit mapping, Dvorak, stable numeric/modifier rows, text-input bypass and matching key releases.
- Native HOD tests for navigation marker scale/basis and missing-point handling. Read-only checks against the actual Kushan Support Frigate HOD resolve all seven classic lamps, and the actual mothership HOD supplies its `Frigate_In` endpoint.
- Existing manager input, Sensors cleanup, texture lifecycle and Kushan docking regression checks.

No automated in-game play or camera manipulation was performed. Player testing is still required for the visible result and hangar timing.
