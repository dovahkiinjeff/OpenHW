# Local skirmish and multiplayer menus — patch 1.0.2

Technical implementation and offline validation notes for OpenHW patch 1.0.2.

The 27 October crash dump resolves to `dbgFatalf -> fileSizeGet ->
fileLoadAlloc -> hrInitBackground`. The optional screenshot selector returned
an empty name on the compact installation. The old existence check accepted
the installation directory and the required-file loader aborted.

Loading artwork now uses a bounded, optional reader, with a packaged hangar
illustration as fallback. It never sends an empty name to the filesystem,
closes every file handle, and accepts short reads. Screenshot selection uses
absolute enumeration paths without changing the process working directory.

The multiplayer interface now provides six connection destinations, including
saved skirmishes and recorded battles. Skirmish has four horizontal tabs with
separate Back and Start Battle actions. Map selection, fleet colours, saved
games, lobbies, connection settings and battle rule variants share the modern
panel layout. Decorative frames and cutouts no longer shrink or cut through
the controls. Fleet colour previews retain their aspect ratio.

The revised local 1.0.2 build replaces CPU-player triangles and the black
counter with a compact minus/value/plus control. Preset rows share a width and
sliders have visible grips. Local skirmishes start with at least one AI fleet.

All loading modes use the detailed SP panel, with real phase/item progress,
timing and recent activity. Network games additionally show each player's
reported readiness without changing loading packets or synchronisation.

Multiplayer maps select remastered backgrounds from their BTG environment
assignment, including the default EZ05 composition. HDR lighting metadata uses
the matching environment, while editable overrides use a separate stable key
for each multiplayer map. Campaign lighting files are not reused as MP edits.
The background default is resolved before modern rendering so an earlier map's
cached environment cannot be inherited accidentally.

Engine ribbon heads and historical samples now use the existing glow bulb's
physical origin, including authored sprite offsets. Glow appearance is retained.

Assets now has a Derelicts dropdown immediately below the RTX dust-volume
entry. Its full catalog does not depend on a retail BIG table of contents.
Selecting a type and pressing P loads missing static data and places it through
the editor's existing cursor placement. New placements appear under Objects;
Scene remains the view of original map objects, and Volumes remains for clouds.
Object transforms, duplication, deletion and ADD records use the existing map
serializer. Multiplayer map overlays have separate stable per-map filenames.

Object inspector adjustments now resolve the displayed row directly: XYZ,
pitch/yaw/roll and instance scale. Wheel input uses the hovered row; identity
and other read-only rows cannot accidentally move the object. Derelict visual
scales are per instance, captured in the normal raster/RT transform and saved
through RTXMAP records. They do not change shared meshes or save-game layouts.
The scale cache uses object identity, clears on universe reset, and retires on
deletion so a reused address cannot inherit another object's scale.

Validation: both game executables compile; optional-artwork regression tests
pass; layout tests exercise 32 actual local FIB pages at five resolutions and
four UI scales. No game or installer was launched for these checks. Gameplay
and the final appearance require the user's test. This does not implement or certify online matchmaking or a complete
multi-computer compatibility test; pilot synchronisation is described below.

Additional checks cover all loading modes, trail-origin offsets, map/lighting
identity and the production HDR metadata lifecycle for campaign and MP maps.

The cumulative patch also retains 1.0.1's authored dust/cloud initialisation
and mission lighting sky repair. Existing mission authoring, saves and options
are preserved. Program backups and patch logs stay under Development.

The 1.0.2 follow-up also removes the story-only entry gate for direct
piloting, command navigation and X/Claw wing-leader control. Local skirmish
uses the same controller as campaign. Network pilot input travels through the
captain's ordered command stream before changing steering, weapons or squad
state. Each player has a separate ownership-checked session; camera position,
focus, mouse input and selection stay local. Death, capture, external orders,
mission reset and replay cleanup retire sessions. Every participating client
must use this rebuilt 1.0.2 patch. Network weapon masks support 1,024 guns per
piloted hull; higher-count modded hulls remain controllable through normal RTS
orders. The offline production-controller checks cover command gating, remote
input isolation, session ownership and X/Claw wing followers. Live network
latency and an actual multi-computer session still require player testing.

Gameplay Options now includes RESTORE FOG OF WAR. It restores the original
Homeworld render-list visibility ranges for ships, derelicts and minor objects
rather than drawing distant objects without a range limit. The native Sensor
Manager logic remains in place; this option does not reset exploration or
modify shared simulation state. It applies on the next render-list rebuild,
is saved locally, and Cancel restores the previous value. It defaults OFF to
preserve the current presentation in existing installations.
