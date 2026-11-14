# OpenHW 1.1.2 - Mixed fleet spread, concept panels and room browser

**Update from OpenHW 1.1.1 through Launcher: choose Update now. No reinstall is needed.** New players start with the full 1.1.1 base installer, then use Launcher to install this update before joining multiplayer.

## Any ship combination, all standard formations

Formation Spread now works with mixed ship types and captured races in the same owned formation. Delta, Broad, Delta 3D, Claw, Wall and Sphere are supported. **Custom and Military Parade are excluded.** Changes preserve movement and combat orders and account for hull sizes when tightening spacing.

## Concept art throughout the interface

All 15 supplied concept images are included, with their provenance recorded. Dark translucent blue overlays retain readable controls while revealing the art in Escape/in-game menus, large UI panels, settings, the Fleet Workshop, parade selection, Mod Manager, online panels and Diagnostics. Diagnostics resizes its backdrop with the window.

## Persistent server / match directory

Online Multiplayer now keeps the server's room directory visible both before and after joining a room. It refreshes every three seconds and shows room code, host, map, occupied slots, AI count, capacity and waiting/loading/battle state. Mouse wheel and Previous/Next pages expose all 16 possible rooms; available rooms can be joined directly. Leave your current room before joining another.

Separate header, directory, roster, match-settings and status panels replace the giant opaque blue lobby backdrop. Text adapts to the smaller panels, and Start Match sits below the settings rows.

## Multiplayer compatibility

The matching server worker and directory protocol are deployed with this release. **All players must update to 1.1.2.** Existing 1.1.1 clients cannot join this updated gameplay server. The rollout restarts multiplayer and ends any active match.

## Downloads and validation

Compiled files are hosted only on openhw.tech. GitHub hosts the complete source, source checksum and release notes; its update feed is a metadata fallback pointing to the game server.

Both game modes build with matching simulation identities. Checks cover 12,531 formation assertions, room directory limits/capacity/state/removal, existing lobby behavior, transport validation, Diagnostics rendering, and both packaged utilities. Interactive UI/gameplay review has not been performed by the publishing agent.
