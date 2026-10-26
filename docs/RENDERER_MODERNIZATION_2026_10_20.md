# Renderer modernization, 20 October 2026

This pass adds pooled D3D12 allocation, persistent remastered ship raster geometry, and hardware sampler feedback for the inactive material cache. It preserves the previously deployed memory fixes, authored maps, texture resolution, lighting, clouds, geometry detail, and player settings.

## Allocation and geometry

D3D12 Memory Allocator v3.2.0 is pinned to upstream commit `1d86c1130f61453634b1df85782e1fecfd59a525`. The wrapper shares allocator ownership across wrappers for the same canonical device, caps default pool blocks at 16 MiB, and keeps each allocation alive through the resource's COM ownership. Existing frame fences remain responsible for submitted work. Only fully initialized resources use pooled placement; temporal histories retain their committed zero-initialization contract. Budget or optional allocator failures preserve the original allocation path.

Immutable DXR vertices and indices now upload once to DEFAULT memory, with temporary upload resources retired by the existing frame-slot fences. BLAS/TLAS buffers use pooled allocations with explicit alias activation and initialization. An allocation failure cannot release a destination already referenced by a recorded copy.

Eligible native remastered ship hierarchies register immutable raster geometry once. Per-material geometry is deduplicated with meshoptimizer v1.0, pinned to `73583c335e541c139821d0de2bf5f12960a04941`, and retained in GPU buffers. The old CPU lighting inputs become per-draw constants; the GPU evaluates the existing two-light and dock-tint expression with its original color conversion. Triangle and corner order, UVs, winding and material order remain unchanged. There is no simplification or quantization. Classic, mutable, morphing, specular, unsupported and cache-admission cases retain the established renderer.

The raster cache has a conservative 256 MiB admission bound that includes staging, fallback storage and entries still held by submitted frames. Busy entries cannot be evicted. Mesh deletion and known UV mutations invalidate identities. GPU-ready entries drop their compact CPU copies. Native raster telemetry reports persistent draws, triangles, upload bytes and cache charge.

## Texture residency

Sampler Feedback requires SM 6.5, tiled resources tier 2 and supported sampler-feedback hardware. Small textures and unsupported devices use the exact existing MA/committed path. Active and in-flight materials always retain every mip. This implementation uses whole-mip groups across all four texture channels; it does not claim fine-tile active streaming.

One capture frame in 60 records the actual minimum sampled LOD for each material, including idle engine textures. A separate compute pass writes hardware sampler feedback, which is decoded and read only after the existing presenter fence proves completion. Inactive materials can then release finer physical mip heaps, retaining a small warm tail. Reactivation maps and uploads every missing original mip before sampling. No LOD clamp or lower-quality texture substitution is introduced, and no blocking wait is added to the production frame loop.

An initial implementation writing feedback directly from DXR hit shaders caused a driver hang on the RTX 5070 Ti during the headless integration test. That implementation was rejected before deployment. The final SM 6.3 capture library uses an ordinary UAV minimum operation; the SM 6.5 compute shader alone executes the hardware feedback instruction. Non-capture frames use the original shader library with no capture operation.

## Validation

The Release game target and 48 automated tests pass. Tests run without launching or controlling the game.

- The production raster recorder and shaders match the unchanged CPU `Shader.c` reference in 20 GPU image cases: flat/smooth normals, zero/one/two lights, dock tint, clamping, negative inputs, alpha blending, two-texture engine blending, UV sampling and clipping. The fixture uploads 688 geometry bytes initially and zero on repeated frames.
- GPU allocator tests cover pooled buffers/textures, exact copies, zero initialization, shared device ownership, final-reference retirement, wrapper reset and allocation release, with no D3D12 debug errors.
- The standalone four-layer 1024-square feedback fixture drops from 22,544,384 mapped bytes to 524,288 warm bytes after fence retirement. Fractional LOD and wrap samples match bit for bit after complete restoration. These numbers describe the fixture, not the game's overall memory use.
- The combined production DXR test forces capture, physically trims inactive mip heaps, restores all channel/mip bytes exactly, and passes clipping, cloud sunlight, shafts, temporal history and D3D12 validation. The same suite passes with the optional streaming context disabled.

An additional production DXR A/B test ran four warm and twelve measured alternating pairs at each of 1280x720 and 1920x1080 on the RTX 5070 Ti. All 32 full-frame pairs matched byte for byte, and all 32 capture frames proved nonempty material minimum requests and decoded hardware feedback. At 1080p the GPU median was 0.1905 ms baseline and 0.5143 ms with capture; the median paired increase was 0.3226 ms on a capture frame (0.0054 ms amortized at 1/60). At 720p the paired increase was 0.1603 ms. Timing includes production feedback clear/decode/readback operations and excludes the diagnostic image/request copies. This single-triangle fixture checks capture cost and parity; it does not establish real mission performance, and cost can scale with scene workload.

Mission 7 FPS, actual persistent-cache coverage, and whole-process RAM/VRAM after this pass require a normal user gameplay run. Synthetic correctness and timing results are not an end-to-end speedup measurement.

## Upstream references

- https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/quick_start.html
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine
- https://github.com/zeux/meshoptimizer
- https://github.com/microsoft/DirectX-Specs/blob/master/d3d/SamplerFeedback.md

The implementation adopts the persistent default-buffer upload pattern; it does not import or replace the engine with MiniEngine. Vendored source hashes and MIT licenses are recorded under `src/ThirdParty/D3D12MemoryAllocator` and `src/ThirdParty/meshoptimizer`.
