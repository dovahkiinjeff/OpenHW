# OpenHW — architecture and future work

OpenHW combines original Homeworld 1 gameplay with high-fidelity Remastered presentation assets, imported from the user's own Collection. Direct3D 12, optional ray/path tracing and non-path-traced lighting form the current graphics foundation. The original simulation remains distinct from rendering and generated presentation frames.

Parallel frame preparation exists for specific rendering/audio tasks; it is not a claim that all game simulation runs on separate threads. See [frame worker ownership](../docs/frame-worker-ownership.md). Native resource import, HOD/material/LOD handling and modern UI sit alongside retained legacy engine systems.

## Immediate priorities

- Complete and stabilize NVIDIA DLSS and Intel technologies. Use AMD FSR upscaling and FSR frame generation for 1.0; NVIDIA/Intel frame generation is not implemented for the release.
- Fix bugs and visual glitches reported by players, using reproducible mission/hardware/settings information.
- Continue performance improvements as expensive scenes and regressions are identified.

## Planned expansion

Cataclysm and multiplayer support are planned as more issues are resolved and performance improves. Neither is a supported 1.0 mode, and no delivery date is announced. Existing networking code or stored multiplayer parade presets do not establish compatibility for the current renderer, mods or gameplay changes.

The earlier prototype roadmap has been replaced here because its status claims were obsolete. Use [the documentation index](README.md) to distinguish current guidance from original-engine reference notes.
