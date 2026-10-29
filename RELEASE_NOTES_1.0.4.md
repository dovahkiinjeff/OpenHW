# OpenHW 1.0.4

Install the original OpenHW base game, followed in order by **1.0.1**, **1.0.2**, **1.0.3**, and **1.0.4**. Close both game executables before applying the patch in the same OpenHW folder.

- In single-player, **Super Salvagers** now prevents collision and kamikaze-impact damage to your salvage corvettes. They retain their existing reduced damage from weapons and explosions. Collisions still affect the other object.
- Ship detail selection uses each model's authored distance bands without display resolution, field of view, frame-rate scaling or previous selection affecting the choice. The texture quality setting remains independent of display resolution.
- Addresses a report of the Ghost Ship appearing white at distance: its Remastered hull uses the same textured native mesh at every distance, with a neutral native texture fallback when a requested color scheme is unavailable.

The patch has been built and checked offline, including its actual Remastered Ghost Ship package at each requested detail level. The reported in-game appearance still needs player verification. Changes in distance thresholds can alter when other ships switch detail levels.

**OpenHW-1.0.4-Source.zip** contains the complete cumulative source through 1.0.4, including the installer, shaders, importers, editors and tests. Historical original comments and older documents remain included as references and may be outdated. Use the current OpenHW guides for modding.

AMD FSR upscaling and AMD FSR frame generation remain recommended; DLSS and Intel upscaling have known issues, and NVIDIA/Intel frame generation is not implemented. Network participants should use the same version; multi-computer gameplay remains in testing.
