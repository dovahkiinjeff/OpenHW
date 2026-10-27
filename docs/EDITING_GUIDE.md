# OpenHW 1.0 authoring guide

This is the current map/environment guide. Original engine notes and older development reports are retained in the source, but are not current instructions. Start with [the documentation index](../documentation/README.md) when references disagree.

## Workflow

1. Load the mission and open Shift+F11. Use a copy of important saves.
2. Pause simulation and KAS scripting when changing scene layout. Their controls are independent.
3. Select the appropriate tab and make changes in the live preview.
4. Save Map for map/cloud changes; save/export in Lighting for lighting changes. These are different documents.
5. Close the editor, resume normal simulation, then reload to confirm persistence.

Retail archives are not rewritten. In the portable release, user map exports are below `RTXExports/Maps`; lighting overrides are in local `RTXLighting`, with exported copies below `RTXExports/Missions`. Global visual exports are below `RTXExports/Global`. Cloud presets use `RTXExports/CloudPresets`. Development loose-data builds may use `Dev/RTXExports` instead. All these locations are within the game directory, not Documents/AppData.

The shipped fallback files are physical `Data/Missions/MissionNN.rtxmap` and `.rtxlight` documents. User overrides can take precedence. The installer verifies the physical files: a copy present only inside an archive is insufficient for these loaders. All 16 map overlays and 15 available authored lighting overrides are included in 1.0; no separate Mission 15 lighting override was invented.

## Map workspace — Shift+F11

| Tab | Use |
| --- | --- |
| Assets | Browse/place available ships, derelicts, resources and supported scene assets |
| Scene | Select and transform original map objects; overlay deletions |
| Objects | Select, transform, duplicate or remove newly placed objects |
| Volumes | Place and style bounded volumetric clouds |
| Ropes | Configure volumetric ropes and original rope visibility/color/opacity presentation |
| RM Dust | Configure supported Remastered dust presentation |
| Fields | Configure clouds derived from connected particle distributions |

The toolbar provides mission selection/loading, Load Paused, independent simulation/KAS pause, view/camera modes, Save Map, Load Map, Lighting, Textures and Done. Keep a saved copy before changing missions. Imported assets are read-only templates; authoring overlays contain your changes.

Assets includes a **Derelicts** dropdown below the RTX dust-volume entry. Select a type, then press **P** to place it; the placed object appears in **Objects**. Wheel adjustments follow the hovered position/rotation/scale row, and double-clicking an editable numeric field allows direct entry. Derelict instance scale changes that instance visually and is saved in its map overlay; shared meshes remain unchanged.

Right-drag aims the editor camera and the wheel moves it forward/back. F focuses selection. Use axis handles to translate/rotate and the inspector for numerical values. Direct-entry fields allow precise typed values instead of relying solely on slider/button increments. Ctrl provides fine adjustments where supported. Delete hides/removes through the overlay. Ctrl+S saves; Ctrl+C/V copies/pastes supported cloud volumes. Read each panel's live labels for the active selection and supported fields.

## Volumes, ropes and fields

OpenHW adapts OpenMW-derived cloud rendering to the project's placed volumes and detected particle fields. Clouds render with path tracing on or off. The renderer's native cloud controls are used within existing placement bounds; changing detail does not require replacing the map's layout.

- Tune boundary shape, position, rotation and size for placed clouds.
- Tune density, spread, softness, coverage and procedural detail using the relevant tab's fields.
- Cloud color is directly authored rather than being forced to inherit the sun or ambient tint. Scene compositing and display processing still affect final pixels.
- Motion is an explicit switch and defaults off. Enable it only when desired, then configure its available movement controls. No swirling is required.
- Particle fields group connected dust distributions into cloud regions; the region count follows map geometry, not a fixed count per mission and not a volume per individual speck.
- All Particle Fields edits the shared map style; individual fields can have their own style. Save the map after tuning either.
- Original dust particles have separate visibility, color and opacity controls. Their appearance can be changed without changing the replacement cloud.
- Original rope meshes have independent color/opacity overrides. This is useful for matching the original visible shell to a new volumetric rope.
- Presets allow reuse of supported volume styles. Keep preset dependencies together when sharing authoring work.

Cloud spread changes the affected region. If a field becomes excessively broad, use precise numerical entry to reduce spread; density and opacity are separate concepts. Excessive density can obscure a whole scene. Preserve a working preset before making large changes.

## Lighting — Shift+F12

Use Shift+F12 directly or click Lighting in the map editor. Pages cover Mission Key, Custom Lights, Mission Tuning and Shadows. Author key azimuth/elevation, intensity/color, ambient strength and automatic/manual ambient color. Add/select/place local lights, aim supported spot lights and solo the key to inspect its contribution.

Mission lighting is consumed by both path-traced and non-path-traced rendering. Tune the corresponding shadow and material/post-processing controls for the intended mode. Global options are distinct from per-map light definitions.

**Non-PT Glow Brightness defaults to 29% globally.** It adjusts hangar and engine mesh glow in non-path-traced mode only. It is not a per-ship brightness slider and does not alter the path-traced renderer's glow.

S saves/exports mission lighting; L reloads it; E exports global visual settings. Use the current panel labels for mouse equivalents. Save Map alone is not a substitute for saving lighting. A missing sky/camera readiness warning means the editor cannot yet place a valid mission key; do not save a broken state as your intended defaults.

## Texture/material workspace

The Textures action opens the live surface/material authoring workspace. Select the relevant object/surface and inspect the available texture and material tuning. Save/export the changes using that workspace's controls. Do not assume that editing a light changes the underlying diffuse, normal, emission or team-color source image; these are distinct material inputs.

## Sharing and modding

Share authored text overlays and original art you have permission to distribute. Imported retail HODs, textures, movies, speech and music are not yours to redistribute merely because setup generated a local package. Include the target mission, required base content and a description of gameplay-affecting object changes.

For ship hierarchy, functional subsystem edits, mod publication, load order and parade layouts, see [Modding](MODDING.md). Build/dependency instructions are in [BUILD_INSTALLER.md](../BUILD_INSTALLER.md), not the historical original-engine notes.
