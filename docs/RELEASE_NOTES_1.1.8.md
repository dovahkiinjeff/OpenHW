# OpenHW 1.1.8 - Fleet badges

Choose **Update now** in Launcher. No reinstall is needed; supported installations receive the patches in order.

Fleet Identity now has a **Badge** tab. Use **Custom Image** to import one or several PNG or DDS images, select a badge, and Apply. Originals are copied permanently to the installation's **Badges** folder, duplicates are reused, and the chosen badge survives restarts. Cancel restores the prior selection while keeping imported images in your library.

OpenHW automatically discovers badge images in an owned Homeworld Remastered Steam installation and installed Steam Workshop packs, including loose images and badge images inside compatible .big archives. Rescan picks up newly installed packs. This does not activate a gameplay mod or redistribute its assets.

Badges use the model's authored secondary badge UVs, retain transparency, and appear in the fleet preview and on supported native Remastered ship models. Models without authored badge areas do not receive invented placement. Online players exchange a small bounded badge image through the dedicated server, so other players do not need the same source file or Workshop pack.

Validation: PNG and DDS import/alpha, duplicate handling and persisted selection across process restarts; discovery against owned Remastered files and three installed Workshop packs; native interceptor authored UV coverage; overlay clipping, material replacement, motion identity and cleanup; and a two-client WebSocket test covering badge exchange and departing-slot cleanup. Both game modes build. No interactive visual comparison has been performed by the publishing agent.

Compiled updates are hosted only on openhw.tech. GitHub hosts source, source checksums and release notes. Retail and Workshop badge images are not included in either release package.
