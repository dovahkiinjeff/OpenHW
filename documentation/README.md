# OpenHW 1.0 documentation index

**Important for modders: original Homeworld source-code notes are included and have not been updated to OpenHW.** Historical format descriptions, comments, old build instructions and development reports may be useful for understanding the original engine, but can be misleading about current paths, rendering, supported features and mod formats. Do not follow them as 1.0 instructions without checking the current implementation.

## Current entry points

- [Project and complete feature overview](../README.md)
- [Installation, settings, fleet/formation controls and ship piloting](../docs/USER_GUIDE.md)
- [Map, cloud, rope, dust, lighting and material authoring](../docs/EDITING_GUIDE.md)
- [Ship editing, subsystem publication and Mod Manager](../docs/MODDING.md)
- [Military parade editing](../docs/MILITARY_PARADE_EDITOR.md)
- [Current graphics support](GRAPHICS_FEATURE_STATUS.md)
- [Future work](MODERN_RENDERER_ROADMAP.md)
- [Limitations and issue reporting](../docs/LIMITATIONS.md)
- [Build the complete game and installer](../BUILD_INSTALLER.md)

## Technical references

The current runtime ship-definition guide is [OPENHW_SHIP_DEFINITIONS.md](../docs/OPENHW_SHIP_DEFINITIONS.md); it explicitly distinguishes current and compatibility formats. [OPENHW_RM_SYSTEM.md](../docs/OPENHW_RM_SYSTEM.md) and [OPENHW_FORMAT_POLICY.md](../docs/OPENHW_FORMAT_POLICY.md) describe integration details. Read their scope/limitations alongside the release guides; a supported source-code structure does not imply a completed user-facing feature.

## Historical and original-source references

`File Formats/` contains the original BIG, GEO, LiF, MAD and MEX notes. They describe historical Homeworld data, not the complete OpenHW OBIG, Remastered HOD/material or integrated ship/subsystem pipelines. Their original body text is preserved and marked as reference material.

[ORIGINAL_SOURCE_BASELINE.md](ORIGINAL_SOURCE_BASELINE.md) records original source provenance, not a current build recipe. Other historical development reports in `docs/`, old release notes, and comments in inherited source remain included. They may contain retired paths, incomplete-stage plans and obsolete recommendations. Original ownership/license text is not rebranded or rewritten.

When documents disagree, begin with the current release guides above, then confirm the exact behavior in current source and tests. Report contradictions instead of guessing that an old instruction is still correct.
