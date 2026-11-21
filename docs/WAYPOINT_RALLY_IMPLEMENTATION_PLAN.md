# HW2 waypoint and production rally system — research and implementation plan

Status: original research plan, 2026-11-16. Subsequently implemented on the owner's instruction; see [current behavior and validation](WAYPOINTS_AND_RALLY.md). The owner superseded the proposed contextual rally behavior below: both rally modes use a military parade around a virtual mothership origin.

## 1. Reference behavior

The original [Relic/Sierra HW2 Gameplay Guide](https://homeworldarchives.com/media/Homeworld-Manual-HW2-ver1.pdf), printed pages 28–31, documents:

- W enters waypoint placement through the movement disc; repeated clicks add connected navigation points.
- Esc finishes plotting and ships traverse the path. A new Move order replaces it.
- A right-click contextual command can terminate the path, including attacking or repairing a target; exit placement to execute it.
- Rally Point sends newly built ships to a position. Rally Object directs them to an object and an applicable action, such as harvesting.

I visually checked pages 28–29: the poorly extracted keyboard symbol is **Esc**.

The [Prima official guide](https://www.homeworldarchives.com/media/Homeworld_2_Prima_Official_eGuide.pdf), printed pages 47 and 50, corroborates multiple movement destinations and places rally settings on each production-capable ship. It describes rallying reinforcements to a strikegroup flagship.

The installed Remastered publisher configuration confirms W / Ctrl+R / Ctrl+P for waypoint / rally point / rally object:

`F:\SteamLibrary\steamapps\common\Homeworld\HomeworldRM\Data\locale\russian\scripts\keybindings.lua`, lines 70–71 and 126.

Its `ui/newui/taskbar/tb_commandpanel.lua`, lines 114–117, defines separate command buttons for all three operations.

### Parity questions still requiring a controlled reference check

The manuals do not establish loop closure/patrol repetition, editing an existing route, a maximum point count, when a producer samples changed rally settings, or every rally-object action and failure case. Do not label proposed choices below as verified HW2 behavior. In particular, the manual's use of “patrol” does not establish how to create a repeating loop. Verify those interactions in HW2/HWRM before claiming complete parity. No reference game was launched during this research.

## 2. Proposed OpenHW experience

These are implementation recommendations, except where explicitly sourced above.

| Operation | Recommended interaction |
| --- | --- |
| Plot waypoints | Select movable ships, press W or choose Waypoints, then place 3D points using the existing disc and altitude controls. |
| Finish | Esc or the Waypoints button commits a nonempty draft; Esc with an empty draft exits placement. Consume this Esc before pause/menu handling. |
| Finish with action | Right-click a valid target to attach the final contextual action; show its icon and target, then finish plotting. |
| Abandon draft | Provide a visible Cancel control; changing selection or entering another exclusive command discards the draft without changing current ship orders. |
| Replace an active route | An explicit Move, Stop, Attack, Dock, special command, or pilot takeover cancels the affected ships' remaining route. |
| Rally to position | Select one or more owned producers, press Ctrl+R or use Rally Point, place a fixed world position. |
| Rally to object | Ctrl+P or Rally Object selects a legal object; show the applicable action for newly produced ships. |
| Clear rally | A named Clear Rally control restores ordinary launch/parade behavior for the selected producers. |

Use rebindable commands. OpenHW already uses P for pause and R for research; Shift controls movement altitude. W also controls pilot flight/navigation, so pilot mode must retain input precedence. Chat, text fields, editors and modal dialogs must not accidentally issue navigation orders.

Show numbered nodes, connected lines, active leg and any terminal action in both normal and Sensors views. Show producer rally markers when the producer is selected. Keep committed paths distinguishable from the current draft, with readable styling over bright nebulae. Do not expose hidden target positions through markers.

Support mixed ship selections and preserve formation, tactics and spread settings. Existing formations, including military parade/custom where movement is already supported, retain their existing movement restrictions; waypoint support must not silently enable spread for those excluded formations. Multiple producers can receive the same rally destination while retaining separate settings.

A production rally route with multiple points, arbitrary sequences of unrelated actions, and route editing are separate features. They must not be implied by implementing a waypoint route plus a single production rally destination. Repeating patrol behavior remains a reference-verification item.

## 3. Existing engine hooks and gaps

| Area | Current code and implication |
| --- | --- |
| Movement data | `src/Game/CommandLayer.h`: `MoveCommand` contains one heading and destination. The command-layer list is active orders, not a player route FIFO. |
| Order replacement | `CommandLayer.c`: `clMoveThese` (~4567) and `clMove` (~4716) reuse/replace current orders. Repeated calls alone do not create a queue. |
| Arrival | `processMoveToDo` (~2715) and `processMoveLeaderToDo` (~2842) also complete on an obstructed destination. Route advancement needs an explicit arrived/blocked distinction. |
| Formations | Formation movement processes the leader and followers together (~8616); keep that behavior and avoid rebuilding the formation at every node. |
| Input and placement | `src/Game/PiePlate.c/.h`, `src/SDL/mainrgn.c`, `src/Game/KeyBindings.c`; reuse the movement disc, clipping, height adjustment and key-binding infrastructure. |
| Dispatch | `src/Game/CommandWrap.c`: `clWrapMove` applies controllability/pause checks and selects local or network dispatch. New commands need equivalent checks. |
| Production | `CommandLayer.c`: `clCreateShip` (~7323) creates the ship inside the actual creator; `ConsMgr.c` (~4955) is a production completion caller. |
| Launch completion | `COMMAND_LAUNCH_SHIP` (~8847) invokes `GroupShip` and the AI launch callback. `GroupShip` (~7934) can join an existing formation, enter parade, or handle research-ship docking. Rally must not be overwritten here. |
| Other launches | `LaunchMgr.c`: `lmReLaunch` and repair/return launches are distinct from newly built ships. Do not redirect every relaunch. |
| Campaign AI | `AIMoves.h` and KAS patrol functions already support scripted team paths. Preserve their ownership and semantics; these are not a player command queue. |
| Online | `src/Game/CommandNetwork.c/.h`, `src/Network/CommandValidation.c`, `DedicatedServer.c`, `server/coordinator.py`; use the existing authenticated, frame-ordered command pipeline. |
| Saves | `SaveGame.c` saves raw `CommandToDo`/ship structures. Directly enlarging them risks compatibility. Existing optional appearance data in `ShipAppearanceSave.inc` provides a useful extension pattern, not a complete solution. |

Line numbers are research anchors in the current working tree and may move.

## 4. Recommended simulation design

### Navigation route state

Add simulation-owned route records associated with the participating command group, with explicit handling for selection splits. Store bounded 3D nodes, active index, optional terminal action, typed target identity, and lifecycle status. Keep UI drafts separate. Start with a proposed maximum of 64 points, subject to packet sizing and usability checks; this is not an HW2 limit.

Execute each leg through normal movement/formation logic. Advance deterministically on actual arrival, using formation-aware tolerances. A blocked leg must not consume all remaining nodes. Retain collision avoidance, and report/retry a blocked route according to an explicit bounded policy. This feature supplies player-authored paths; obstacle avoidance remains the existing navigation system's responsibility.

Intermediate corners should not force the final-arrival braking/orientation routine at every point. Define a shared simulation rule for advancing through intermediate nodes, preserving safe turns and group cohesion. Final arrival uses normal stopping behavior. Do not advance based on render frame rate, camera distance or whichever client happens to be observing.

Passive combat follows existing tactics without losing the route. An explicit player command supersedes it. A split transfers the remaining path only to ships still following the route, with deterministic leader replacement. Death or removal of every member releases the record. Terminal actions are revalidated when reached, using the same capability, ownership and target rules as ordinary orders.

### Producer rally state

Store None / Position / Object per actual production-capable ship, using capability checks suitable for captured and modded producers. Object records carry a type and lifetime-safe identity. A resource and a ship must never be interchangeable IDs.

Tag newly constructed ships with a one-time pending production dispatch. Apply it after launch clearance, at the point where default grouping would otherwise take over. Preserve required research-ship assembly and other mandatory launch behavior. Continue the AI launch notification exactly once.

Recommended policy: held new ships use their producer's current rally setting when they depart. Once dispatched, later rally changes affect future departures. This choice needs reference verification. Auto-launch off remains respected. Explicit orders and ordinary repair/refuel relaunches do not acquire new production rally orders.

A fixed rally location stays fixed when the producer moves. An object rally retains a live target reference. Initially resolve collector-to-resource and reinforcement-to-friendly-ship behavior through normal Harvest/Guard/Follow/formation operations. Verify the reference game's joining behavior before promising identical strikegroup attachment. Expand the target/action matrix only where existing commands support it safely.

If a target disappears, hold safely or fall back to normal grouping when still at the producer; never track an invisible enemy with a saved world position. Capture/transferring a producer clears its former owner's rally. Handle producer destruction with queued and already-launched ships separately. Clear world-space paths at campaign map transitions; same-map save/load must preserve them.

## 5. Online and save requirements

OpenHW's dedicated simulation worker and native clients need identical route/rally progression. Put state transitions in shared simulation code; the Python coordinator transports ordered commands rather than steering individual route legs.

- Introduce bounded, explicitly validated route-commit and rally-set/clear commands. Send one completed route, not a stream of cursor positions.
- Validate packet length, counts, finite/in-map coordinates, authenticated ownership, producer capability, target type and legal actions. Existing validation caps include 32,768-byte packets and 1,024-ship selections; size the combined payload against those limits.
- Define ordering against Stop, new commands, factory capture, target death and paused orders. Reject unsupported clients through protocol/build compatibility checks.
- Include route/rally state in synchronization diagnostics and whichever state checksums cover gameplay. Audit reconnect/resynchronization rather than assuming ordinary saves cover it.
- Save routes, producer settings and pending launch dispatches in a versioned extension with saved-object reference translation. Read older saves as empty route/rally state.
- Inspect optional-tail ordering carefully: appearance loading currently precedes object fixups. New extensions need explicit boundaries, validation and the correct reference-resolution stage.
- Do not promise that older executables can read newly written saves. Decide save-version policy and test migration explicitly.

## 6. Delivery sequence and acceptance gates

1. **Resolve parity details and freeze semantics.** Verify loop/patrol behavior, object-action mapping, cancel behavior and held-ship rally changes against HW2/HWRM. Record a small behavior matrix before coding. Confirm available bindings across all input modes.
2. **Implement shared route execution and serialization.** Distinguish blocked from arrived, preserve formations/spread, handle replacement and member loss; add local command and save support together.
3. **Add placement and visualization.** Movement disc integration, Esc handling, visible cancel/finish controls, terminal target actions, Sensors rendering and rebindable buttons/keys.
4. **Implement producer rally lifecycle.** Position/object modes, per-factory UI, pending new-ship dispatch, launch clearance and mandatory research/AI handling.
5. **Complete network integration and compatibility.** Server validation, identical worker/client behavior, state diagnostics, reconnect behavior, old-save migration and protocol-version gating. Do not release a local-only version as online-ready.

Required acceptance checks:

- Route with horizontal and vertical legs, a final action, zero/one/max nodes, map edges, invalid targets and an obstructed point.
- Mixed fighters/corvettes/capitals; every supported formation; extreme spread; slow members, leader death, selection split and membership changes. Verify no speed loss caused by stopping at each node.
- Route replacement, Stop, passive combat, pause with orders permitted/forbidden, pilot takeover, chat/editor focus and Esc menu interaction.
- Two producers with different rallies; producer moving/dying/captured; target moving/dying/becoming invalid; auto-launch off; changed rally before departure; ordinary refuel/repair return; research-ship assembly.
- Collectors rallying to resources and combat reinforcements to a friendly fleet, with clear handling for incapable ships in mixed production.
- Mid-route save/load, held-new-ship save/load, old saves, campaign transition and scripted AI patrol regression.
- Singleplayer, local multiplayer and dedicated online: two replicas plus the authoritative worker agree after the same input sequence; malformed/foreign-owner packets rejected; test reconnect and several simultaneous matches.
- Normal and Sensors views at multiple resolutions, UI scale settings, bright/dark backgrounds, no hidden-information leaks and no expensive all-ships route redraw.

No code implementation, build, release or reference-game launch was performed for this planning request.
