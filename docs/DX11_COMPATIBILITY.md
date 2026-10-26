# DX11 compatibility renderer

Launch `HomeworldModern.exe /dx11` (or use `OpenHW - DX11.lnk`). The ordinary
shortcut still uses the existing optimized DX12/RTX renderer. Hardware DX12
device initialization failure also falls back to DX11. Windows 10/11 and a
hardware D3D11 feature-level 11_0 device are required; this does not add DX10.

The DX11 device and swap chain are independent of the DX12 presenter. Both
consume the same GL-free draw recorder, imported HOD hierarchy and textures,
so ship joints, animations, LODs and gameplay are not converted again.

Implemented:

- Per-pixel ship lighting instead of the original CPU vertex-lighting path.
- Existing mission sun direction/color and sky ambient settings.
- RM normal, ORM and emissive textures, mip generation and anisotropic sampling.
- Authored map/mission lights and dynamic FX lights (32 local lights maximum).
- Surface reflectivity, roughness, exposure, ambient and light-strength controls.
- A 2048-square directional shadow map with alpha cutouts and 3x3 filtered sampling.
- Original engine-plume shader, texture blending, fog and hyperspace clip plane.
- Native RM sky and converted Shader Model 5 background materials.
- Resolution changes, menus/UI, VSync and separate native cursor.

Not yet ported: the DX12 volumetric dust/rope compositor and post-processing
pipeline, temporal reconstruction/upscaling, frame generation, bloom, and DXR.
The DX11 graphics menu identifies raster lighting and unavailable path tracing;
DX12 settings are preserved, not overwritten when trying the DX11 shortcut.
This is a compatibility test build, not a claim of complete RTX visual parity.

Validation: `dx11_renderer_smoke` opens a hidden synthetic test window, compiles
the production SM5 ship, sky, shadow and background vertex shaders, submits
recorded geometry, runs the shadow pass, reads back opposite-normal lighting
contrast, and repeats presentation. It does not launch the game or read saves.
Actual gameplay and a genuinely DX11-only GPU still need user testing.

Initial validation on 2026-10-10: Release game and renderer test builds passed;
opposite-normal pixel values were 213 versus 2 in three repeated frames; the
available D3D11 debug queue reported no errors. All 18 existing CTest cases,
three DX11 contract tests, three engine-glow tests and five parade-editor tests
passed. The game was not launched by the agent.

Orientation correction: DX11 renders directly to its swap chain, whereas the
shared DX12 recorder expects a final bottom-up presentation flip. DX11 now
corrects raw clip coordinates, pretransformed pixels, sky/background shaders
and face winding without changing DX12. The GPU test verifies asymmetric red
top/blue bottom UI markers with culling enabled and a pretransformed DrawPixels
marker, in addition to the existing lighting/shadow checks.
