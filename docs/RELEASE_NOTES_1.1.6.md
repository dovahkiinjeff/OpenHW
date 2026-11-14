# OpenHW 1.1.6 - Ghost Ship lighting and compact rank insignia

Choose **Update now** in Launcher. No reinstall is needed; supported installations receive the patches in order.

## Ghost Ship lighting

The Ghost Ship's original translated emission maps are explicitly black and losslessly stored as single-pixel DDS pages. The path-traced material loader required an emission map to match the full diffuse texture dimensions, so it ignored these valid constants and retained a legacy full-hull emissive fallback. That could wash out lighting and self-shadow contrast.

The loader now broadcasts a valid 1x1 authored emission page across the hull texture. Black no-emission maps correctly override the fallback, allowing the hull to receive shaded lighting rather than emitting its own painted color. Full-size maps keep their existing sampling, mismatched nonconstant dimensions are still rejected, and hull cutout coverage remains respected. This uses existing installed assets and also corrects other materials with the same constant-map pattern.

## Rank insignia

Large purple outlined rank icons are replaced with compact filled pale-gold stars, approximately six pixels wide. Four rank stars occupy about 27 pixels instead of 56. Borders, shadows and texture halos are removed. Rank values and gameplay bonuses are unchanged.

## Validation

Both game modes build. Native hierarchy/material tests cover constant black emission across a large hull, full-size sampling, invalid dimensions and cutout/glow coverage. All three installed Ghost Ship emission pages were checked as black 1x1 maps. Upgrade checks preserve settings, a save and an authored map; matching multiplayer worker and live feed checks accompany publication. No interactive visual comparison has been performed by the publishing agent.

Compiled files are hosted only on openhw.tech. GitHub hosts source, source checksums and release notes.
