# OpenHW 1.1.9 - Badge display corrections

Choose **Update now** in Launcher. No reinstall or badge reimport is needed.

The badge selector now fits the complete image inside each tile instead of cropping its sides to fill a tall rectangle. The grid also reserves space above the page controls and status line, keeping the last row and its captions clear of the footer.

Ship badge overlays now convert the authored HOD vertical texture coordinate to the decoded image row direction. This corrects upside-down badges in both the fleet preview and gameplay, including raster and ray-traced rendering. The original PNG/DDS files, saved choice and multiplayer badge payloads are unchanged.

Regression checks cover complete square-image containment in tall, wide and square selector areas, exact upright UV orientation in both rendering paths, clipping, image replacement and cleanup. Both game modes build. The transactional upgrade preserves settings, saves, edited maps and the permanent badge library. No interactive visual comparison has been performed by the publishing agent.

Compiled patches are hosted on openhw.tech; GitHub hosts source and source checksums only.
