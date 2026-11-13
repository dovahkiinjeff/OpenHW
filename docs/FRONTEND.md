# OpenHW menus and fleet preview

The Multiplayer landing page keeps its six actions inside the left navy panel. The Modding landing page uses a matching panel on the right, leaving the engineering concept art visible. Their original skirmish, network, replay and editor actions are preserved. Story setup is a centered screen with fleet selection, Player Setup, saved games and Begin New Story.

## Live interceptor preview

Story setup and Player Setup render the selected fleet's interceptor as a 3D model. Changing between Kushan and Taiidan replaces that one preview model. The viewer does not initialize a parade, load a fleet, or create ships in the simulation.

- Right-drag over the model to orbit.
- Use the wheel over the model to zoom.
- Select **Simulate Max Speed** to preview forward travel and engine trails; select it again to stop.
- Hull and stripe changes update the model while the colour picker is open. Engine colour changes update the trail while the simulation is enabled.
- **Apply** saves the six fleet colour channels and returns to the screen that opened Player Setup. **Cancel** discards the pending changes.

The colour channels remain Hull Base, Stripe / Marking, Engine Emission, Nav Lights, Harvest Beam and Hyperspace. The interceptor demonstrates hull paint and engine trails; the other effect channels apply to the appropriate ships and effects during gameplay.

The menu viewer presents the interceptor horizontally. Its display transform is private to the viewer and does not rotate gameplay models.

## Single-ship colours during gameplay

Select one player-owned ship and choose **Colors**, beside Pilot. Each checkbox enables that channel's override for the selected ship. Choose the row or colour swatch to open its picker; confirm that colour, then use the ship panel's **Apply** button. Uncheck a channel to inherit the fleet colour, or use **Reset All** to clear the overrides. Cancel and Escape discard pending changes.

The panel's controls are children of its modal layer so they receive mouse events before the layer blocks input to the game scene. The colour picker remains above the ship panel when HUD ordering is refreshed.

The model viewer follows the gameplay engine-nozzle placement, collision-radius data and trail rendering code. Its trail sampling uses the original simulation interval, independently of menu frame rate. It has private texture palettes and its own clock/viewing position. Leaving the screen releases its model, trail and private palettes. Preview texture preparation is bounded to 1024 pixels (or the player's lower texture setting) and does not change gameplay texture quality.

## Offline validation

`frontend_menu_layout_tests` checks the production layout bounds at 720p, 1080p, 1440p, ultrawide and 4K with four UI scales. `tests/test_frontend_preview_contract.py` compiles and runs the production texture registration, preview trail scope, model framing and texture-quality functions offscreen. The existing texture retirement and surface-paint checks also cover these shared paths.

`tests/test_ship_color_input.py` replays clicks through the production Region dispatcher and ship-colour handlers. It covers all six channels, colour confirmation, Apply, Reset, Cancel, Escape, scene-input blocking and nested-picker ordering. It also reproduces the old same-region modal construction to verify why its clicks were blocked.

These checks do not replace player verification of the final in-game appearance.
