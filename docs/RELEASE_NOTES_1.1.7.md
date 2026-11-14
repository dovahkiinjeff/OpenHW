# OpenHW 1.1.7 - Visible compact rank stars

Choose **Update now** in Launcher. No reinstall is needed; supported installations receive the patches in order.

The compact filled rank stars could inherit back-face culling from world meshes, and their triangle winding was back-facing after conversion from screen coordinates. That could discard the insignia entirely.

The badge renderer now disables culling for the HUD stars, restores the incoming cull state afterward, and emits front-facing triangles. The small pale-gold filled style, tight spacing and rank values are retained. No oversized purple borders or halos return.

Validation captures the actual production drawing function with culling initially on and off, ranks zero through the maximum, and the rank option disabled. It checks front-facing triangles, emitted star counts, color and texture/cull state restoration. Both game modes build; the transactional upgrade preserves settings, a save and an authored map. Matching multiplayer worker and live feed checks accompany publication. No interactive visual comparison has been performed by the publishing agent.

Compiled files are hosted only on openhw.tech. GitHub hosts source, source checksums and release notes.
