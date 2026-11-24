Fixes host map selection in online multiplayer lobbies.

- Uses the server's installed map catalogue and excludes invalid empty map entries.
- Cycles through actual maps, skipping maps that cannot hold the current lobby's human and AI players.
- Adds clearer previous/next map controls and explains when only the current map fits.
- Corrects map enumeration and loading to use the populated map count rather than allocation capacity.
- Map changes remain host-only and are locked after starting. Changes are broadcast to guests and reset readiness.

Normal and diagnostic builds, map-selection regressions, and real local lobby-transport checks passed. No active online match was used for testing.
