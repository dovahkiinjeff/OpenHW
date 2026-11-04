# OpenHW 1.0.5 — performance update

Install OpenHW 1.0, then patches **1.0.1 → 1.0.2 → 1.0.3 → 1.0.4 → 1.0.4.5 → 1.0.5**, in order. Close both OpenHW executables before running **OpenHW-Patch-1.0.5.exe** in your existing OpenHW folder. Saves, settings and authored mission customizations are preserved.

The project owner reports a **200–300% increase in frame rate** in their test. This is an owner-reported result, not a controlled measurement across other systems. The gain depends on scene, hardware, settings and frame-generation use.

## Performance changes

- Replaced repeated fleet-wide searches with indexed ship, LOD, owner, animation-pose and instance lookups. Large fleets no longer incur the same growing lookup cost on every query.
- Removed large temporary allocations and unnecessary clearing during ship animation and hierarchy preparation.
- Suppressed redundant rendering-state commands while preserving draw order, shadows, materials and effects.
- Made converted dust-field density reads direct and retained their data in GPU memory, avoiding repeated packed-record decoding and upload-memory reads.
- Frame generation now reuses its completed display-size scene for final presentation, avoiding a second full scene/cloud/effects composition pass. Native UI and brightness/gamma controls are preserved.
- Includes the cloud-binding repair from the local test build: dust fields and screen clouds use their correct bindings, and the cloud compute pipeline exposes the new density buffer.
- Replaces repeated god-ray integration in cloud shading and full-resolution composition with one reduced-resolution pass and depth-aware joint bilateral upsampling. This is an altered D3D12/HLSL port of [three-good-godrays](https://github.com/Ameobea/three-good-godrays), integrated with OpenHW's existing renderer.

These changes retain the existing resolution, LOD thresholds, cloud sampling, material quality, simulation and authored mission settings.

## God-ray quality

**Graphics → Post Effects → God Ray Quality** offers **Low, Medium (default), High and Ultra** independently of the God Rays switch. Existing profiles default to Medium. The pass retains mission-authored god-ray color and strength; it does not change cloud density, opacity or other lighting controls.

Low and Medium render at one-quarter output width and height with 24 and 40 integration samples. High and Ultra render at one-half width and height with 60 and 90 samples. Depth-aware upsampling helps keep shafts from bleeding across ship and asteroid silhouettes. This addition has passed offline validation; no additional whole-game FPS gain is claimed for it, and individual scene appearance still needs player feedback.

## Mission 2 salvage contacts

Corrects the Khar-Selim salvage contacts to match the Remastered wreck. Loading an existing Mission 2 save restores the corrected contacts and updates already-clamped salvage corvette offsets, so a mission restart is not required. An offline check against the actual Remastered HOD verified all five salvage joints in both released native formats. In-game placement still needs player verification.

## Engine and missile trail occlusion

Engine and missile trails now enforce scene depth testing, preventing inherited rendering state from drawing them through foreground ship hulls. Existing glow appearance, visible trail cores and tail transparency are preserved. Actual D3D12 offscreen checks passed with frame-generation guides enabled and disabled, matching the correct-depth baseline across six inherited-state cases. In-game appearance still needs player verification.

## Validation and downloads

Focused offline tests passed for rendering parity, animation and instance lifetimes, density data, frame-generation scene reuse and actual D3D12 cloud bindings/pipeline creation. They complement player testing; they do not establish identical performance on every GPU or scene.

The god-ray D3D12 helper also passed repeated draws across all four quality levels, multiple in-flight frames and odd output sizes, with descriptor sentinels intact and no debug-layer errors. Settings checks cover defaults, quality cycling, save/load and cancel.

- **OpenHW-Patch-1.0.5.exe** — update for an installation already patched through 1.0.4.5.
- **OpenHW-1.0.5-Source.zip** — complete cumulative source through 1.0.5, including the engine, shaders, importers, editors, installer tools and tests.
- **SHA256SUMS-1.0.5.txt** — download verification hashes.

The [optional 1080p cutscenes](https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.0.4.5/OpenHW-Optional-1080p-Cutscenes.zip) remain a separate download. Extract that archive into the OpenHW folder with its `Data/Movies/HD` path. It is not required for this performance patch.

**AMD FSR upscaling and AMD FSR frame generation remain recommended.** This update does not fix DLSS or Intel upscaling; both retain known issues. NVIDIA and Intel frame generation are not implemented. Multiplayer remains in testing; participants should use the same version.

Historical original-source notes remain included and may be outdated. Use the current OpenHW guides when modding.
