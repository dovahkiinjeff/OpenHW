# OpenHW 1.0.4

Install the original OpenHW base game, followed in order by **1.0.1**, **1.0.2**, **1.0.3**, and **1.0.4**. Close both game executables before applying the patch in the same OpenHW folder.

- In single-player, **Super Salvagers** now prevents collision and kamikaze-impact damage to your salvage corvettes. They retain their existing reduced damage from weapons and explosions. Collisions still affect the other object.
- Added the **Mission Resource Scaling** slider: 0% uses vanilla fleet-strength scaling, 100% retains all authored starting resources, and intermediate values blend between them. The previous full-resource toggle migrates to 100%. It applies to newly generated missions; existing saves keep their stored resource population.
- Ship detail selection uses each model's authored distance bands, and custom asteroids use fixed world-distance bands. Display resolution, field of view, frame-rate scaling and previous selection do not affect either choice. The texture quality setting remains independent of display resolution.
- Addresses a report of the Ghost Ship appearing white at distance: its Remastered hull uses the same textured native mesh at every distance, with a neutral native texture fallback when a requested color scheme is unavailable.
- Aligns the Kadeshi fuel pod's engine glow with the aft end of its Remastered hull.
- Uses the Remastered Khar-Selim wreck’s salvage points for Mission 2 when its native model is loaded, with the original points retained as a fallback.
- Hardens the Bentusi trade screen’s shared technology-image cache against stale or missing entries. A reported trade crash could not be conclusively traced without a crash log.

The patch has been built and checked offline, including its actual Remastered Ghost Ship package at each requested detail level. The reported in-game appearance still needs player verification. Changes in distance thresholds can alter when other ships switch detail levels.

**OpenHW-1.0.4-Source.zip** contains the complete cumulative source through 1.0.4, including the installer, shaders, importers, editors and tests. Historical original comments and older documents remain included as references and may be outdated. Use the current OpenHW guides for modding.

AMD FSR upscaling and AMD FSR frame generation remain recommended; DLSS and Intel upscaling have known issues, and NVIDIA/Intel frame generation is not implemented. Network participants should use the same version; multi-computer gameplay remains in testing.

A player reported difficulty selecting saves in the diagnostic build; after loading Mission 1 they could load the other saves. Both executables use the same save directory and save format, but the initial selection problem has not been reproduced or diagnosed.
