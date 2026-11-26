# Waypoints and rally parades

Implemented in the local development build, 2026-11-16. This is not a published update.

## Right-click commands

With ships selected, open the right-click menu and choose **Waypoints**. With a production-capable ship selected, the same menu also offers **Rally Point**, **Rally Object** and **Clear Rally**. There is one entry for each operation.

## Waypoints

- Choose Waypoints, or press W with the default English bindings.
- Click successive positions using the existing movement disc. Its normal height controls still work, including Shift for altitude.
- Press Esc, Enter, or the Waypoints command again to finish and issue the route. An empty draft issues no order.
- Backspace removes the last plotted point. Delete cancels the entire draft.
- After placing at least two points, click the first point marker to close and immediately start a repeating loop. The closing leg returns to point 1 and the same track repeats until replaced or stopped. No duplicate final point is added.
- A right-click on a target after plotting points ends the route with a contextual action. Enemy ships become attack targets, friendly ships become guard targets, and resource targets become harvest orders for capable ships. Holding D selects docking for a friendly ship; holding Z selects the existing special operation, including applicable repair/salvage behavior.
- An explicit new movement/combat/docking/Stop order replaces the affected ships' remaining route. Other members retain it when only part of the group receives a replacement order.

Routes support up to 64 points and the engine's 1,000-ship selection capacity. Formation movement uses the formation leader's arrival and keeps existing formation/spread behavior. Intermediate points request cruising speed instead of distance-based braking, and moving ships steer into the next leg without stopping to align first. Only an open route's final point requests a stop; every point on a loop is a pass-through point. Unformed ships and separate formations advance independently, so faster ships do not wait at a point for unrelated slower units. Real formations continue advancing together through their leader. Collision avoidance, tactics and ship speed limits still apply. An obstructed leg retries without consuming the next point. This does not add a new obstacle pathfinder.

W is rebindable under Waypoints. Existing saved or localized bindings take precedence during migration, so an installation already using W for another action may leave Waypoints unbound until it is assigned a free key. The menu remains available.

## Rally Point and Rally Object

Both operations assemble newly produced ships in a **military parade around a virtual mothership origin**. No additional mothership is created, rendered, registered as a unit, or given a collision body. The existing mothership parade slot layout determines ship positions around that origin.

- **Rally Point / Ctrl+R:** place the virtual origin at a fixed 3D position.
- **Rally Object / Ctrl+P:** choose an owned ship, resource or selectable derelict; its position supplies the moving virtual origin. The parade retains the orientation captured from its producer when the rally is set.
- **Clear Rally:** future production uses ordinary launch/grouping behavior. Ships already in the virtual parade keep the last anchor position.

A rally object is an anchor, not an instruction to harvest, attack, dock or guard it. Rally settings belong to individual production ships. Selecting several producers can assign the same destination to all of them, with separate parade groups.

Ships follow the producer's current rally setting when they finish leaving its launch bay. Ships held inside remain held until launched. Subsequent repair/refuel returns do not acquire a new production rally order. Mandatory research-ship assembly retains its existing behavior.

Changing a producer's active rally moves that producer's existing virtual parade as well as directing future departures. A rally object that dies or changes to another owner leaves the parade at its last valid position. Losing the producer stops its dependent parade through the normal command cleanup path.

## Saves and multiplayer

Routes, current legs, rally settings, virtual parade associations and pending new-ship departures are stored in a versioned optional save extension. Older saves load without that state. New save behavior is supported by the new build; older executables do not understand the navigation extension.

Committed commands use the existing authenticated multiplayer command stream. Route progression and parade steering run in the shared native simulation, including the dedicated worker. The server validates bounds, ownership, selection capacity, duplicate ship IDs, producer capability and typed targets. Updated clients and the worker must share the same simulation build identity.

## Validation

The native `navigation_orders` test executes the production route/rally state machine and authoritative packet validator with a recording command layer. It covers blocked legs, 3D progression, split selections, formation-leader arrival, one-time production dispatch, virtual anchors, moving/lost/captured targets, research exceptions, save/load, pause policy and rejected malformed/foreign-owner packets. Additional regressions exercise continuous pass-through speed requests at several approach distances, non-stopping corner steering, five repeated laps, loop save/load, independent ship cursors, formation cohorts, previous-build route migration and loop packet validation.

The native `navigation_ui` test executes the production placement/menu callbacks without opening a game window. It covers plotting, finish/cancel/undo, contextual targets, rally modes, first-marker loop closure and Sensors object picking. These tests do not substitute for visual playtesting, a full collision/launch-bay scenario or a live multi-client match.

## Route size and turn clearance

Waypoint legs must leave room for the selected ships to turn at cruising speed. Invalid preview legs flash red; clicking an invalid point or loop closure does not issue a route. Move the point farther away or widen the turn. Backspace removes points and Delete cancels the draft. Enter/Esc cannot commit invalid geometry.

The baseline uses the 800-unit frigate parade spacing: each leg needs at least three intervals (2400 units), making a regular six-sided track at least six intervals across (4800 units). Larger hulls or slower turning ships increase that requirement. Sharp corners need longer adjoining legs; both corners sharing a leg must fit. Checks use full 3D distances and include the closing leg and the first/last corners of loops. Two-point reversing loops are not allowed; use at least three well-spaced points. Rally commands and ordinary single-point movement retain their existing behavior.
