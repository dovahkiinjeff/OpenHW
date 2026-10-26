# Native interceptor HOD test

Only `R1\HeavyInterceptor\RMTest\HeavyInterceptor.geo` opts into the loader,
and only when `kus_interceptor.hod` exists beside that mesh. The raw locally
extracted HOD is read at runtime. No GEO is generated or read on successful
native loading. Removing the HOD restores the previously tested GEO path.

The first stage reads HOD2 pools, indexed geometry, normals, UV0 and UV1,
material references and the static mesh hierarchy directly into the existing
in-memory ship mesh interface. Badge-material hull faces are retained. The
render mesh preserves 4,680 source vertices and 1,560 triangles. Source collision
geometry is validated, but original HW1 MEX, statistics, weapons, build entry
and save identity remain unchanged. No other ship can opt in.

Limitations: this is a native **geometry** loader, not a complete Remastered
ship shader renderer. It continues using the interceptor's previously adapted
DDS/LIF material sidecars, including team-paint shading and normal maps.
Custom badge imagery, embedded texture uploads and animated ship hierarchy
rendering are not implemented. Nonidentity mesh transforms and unsupported
layouts fail back to GEO rather than silently producing incorrect geometry.

`native_ship_hod_tests` tests malformed/truncated input, indices, badge faces,
axis mapping and memory cleanup. Passing the extracted HOD path to this test
also verifies the actual local model without launching the game. Game logs
contain `[NativeShipHOD]` to distinguish successful native loading from fallback.

No source HOD, extracted shaders or converted copyrighted assets are repository
assets. The installed archives remain untouched.
