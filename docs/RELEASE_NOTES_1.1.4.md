# OpenHW 1.1.4 - Ion-beam contact effects

Choose **Update now** in Launcher. No reinstall is needed. Existing installations receive the supported patches in order.

## Ion-beam impacts

Remastered ion-contact rings and mesh splashes author `UseDepthTest=0`, but OpenHW forced depth testing on every FX layer. That could bury the thin contact effect at the ship's collision surface, making the beam appear to end without an impact.

The renderer now honors each layer's authored depth-test setting. Normal beam/default layers restore depth testing, and translucent effects never write scene depth. Original effect textures, meshes, colors and timing are retained. Damage and gameplay collision are unchanged. The source setting also applies to other FX layers that explicitly request it.

## Validation

Both game builds compile. Renderer regression checks use the original Kushan and Taiidan ion-contact graph layers, exercise the mesh-splash drawing path, and verify that a following beam restores depth testing. The event/lifecycle tests pass. The patch is checked through the transactional updater with settings, a save and an edited map preserved; matching multiplayer worker and live feed checks accompany publication. A battle/visual comparison has not been performed by the publishing agent.

Compiled files are hosted on openhw.tech. GitHub hosts source, source checksums and release notes only.
