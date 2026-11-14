# OpenHW 1.1.5 - Sustained ion-beam contact effects

Choose **Update now** in Launcher. No reinstall is needed; supported installations receive the patches in order.

## Continuous contact

Ion impacts previously waited for an entire effect tree, including its longer-lived smoke, to expire before spawning again. The bright contact layers could disappear while the beam was still hitting.

Continuing beam hits now refresh the original Remastered contact effect at the existing simulation hit cadence. Its authored mesh, textures, color curves and fading bursts overlap during contact instead of being suppressed by old smoke. Duplicate notifications in the same simulation tick update the position without spawning another burst.

The contact follows the clipped beam endpoint between damage ticks. Breaking contact stops attached contact effects; detached particles retain their normal lifetime. Reacquiring a target cannot revive old contact attachments. Projectile deletion and reset still clean up ownership. Damage, hit cadence and gameplay collision remain unchanged. The authored depth settings corrected in 1.1.4 are retained.

## Validation

Both game modes build. A five-second runtime regression checks fresh contact bursts after the initial effect lifetime, endpoint movement without a new damage event, loss/reacquisition before the next update, and projectile cleanup. Existing renderer checks use the original Kushan and Taiidan impact assets. Upgrade validation preserves settings, a save and an authored map; matching multiplayer worker and live feed checks accompany release. An interactive battle/visual comparison has not been performed by the publishing agent.

Compiled files are hosted only on openhw.tech; GitHub hosts source, source checksums and release notes.
