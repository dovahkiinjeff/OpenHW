Corrects dust clouds and rope clouds across multiplayer maps.

- Rope clouds follow the original rendered rope geometry, including its bends, endpoint offsets and width.
- Removes the fixed source-segment and particle-count cutoffs. Visible rope volumes and particle clouds share the rendering budget so neither can suppress the other entirely.
- Refreshes clouds after particle-position changes and clears old cloud data when switching maps.
- Applies the updated dust appearance: spread 0.63, density 0.34, edge feather 0.99, scattering 0.05, absorption 0, noise scale 0.00046, billow 0.72 and contrast 0.58. Original particles remain visible at approximately 65.88% opacity.
- Includes current approved map settings in the complete installer and existing-player patch.

Normal and diagnostic builds and focused particle/packaging checks passed. In-game visual confirmation remains pending.
