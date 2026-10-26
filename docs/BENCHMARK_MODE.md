# Controlled performance captures

Launch `HomeworldModern.exe -benchmark` (the **Homeworld Modern - Benchmark** shortcut does this).
This is opt-in and does not change graphics settings, saves, missions, or simulation behavior.
Routine console/renderer output is suppressed; crash dumps remain enabled. The old optimization
telemetry marker and shaft-image captures are ignored in this mode. Timing uses existing GPU
queries, read after the normal flight-slot fence; there are no added GPU waits or per-frame files.

1. Load the same save for every run. Put the camera at the same position and angle.
2. Close menus/editors, keep the same simulation/pause state, and let loading finish.
3. Press **Ctrl+Shift+F6** once. This chord is consumed, not passed to formation controls.
4. Do not move the camera, change settings, alt-tab, or give orders for **35 seconds**:
   five seconds of warmup, then thirty seconds of capture. The window title shows the stage.
5. A CSV summary is written to `Documents/My Games/Homeworld Modern/Logs/Benchmarks`.
   A redirected `HW_CRASH_DIR` puts `Benchmarks` inside that directory instead.
6. Reload the save before repeating. Capture three repeats per configuration. Change just
   one graphics option between configurations (for example FSR Quality vs native).

Reports contain base-frame intervals, renderer CPU time, task-dispatch CPU time, separate
simulation-update CPU time, GPU pass times, p50/p95/p99/max times, spike counts, and peak
working set/private memory/local VRAM. Memory is sampled once a second, not every frame.
GPU timing covers raster, DXR, dust preparation, cloud shadows, cloud marching, scene
composition, upscaling, final presentation work, and the total recorded command span.
Requested graphics settings, adapter, mission identity, camera basis, and dust counts are included.

The same CSV also retains the **128 largest individual base-frame intervals** in a
`hitch_frame` section. Storage is fixed; recording adds no per-frame file writes,
clock reads, GPU queries, or fence waits. Each row identifies the submitted frame
and the older slot submission whose normal fence was checked, and contains:

- Existing wall-clock CPU phases: renderer, flight-slot fence wait, recording/staging,
  queue execution, Present, signal/bookkeeping, and DXR recording.
- Material upload/eviction/trim deltas and instantaneous resident bytes, plus current-frame
  persistent raster upload bytes and its cache charge. Counter resets are flagged;
  reset rows contain counts since that renderer lifetime began instead of wrapping.
- GPU pass times matched by submitted frame ID, including separate `waited_gpu_`
  columns for the older submission. A missing result is **-1**, including unresolved
  queries at capture end. A zero frame ID means no earlier slot submission exists.

Rows are ordered by decreasing frame interval; ties keep earlier frame IDs. The
first capture frame has no preceding interval and is not ranked. The renderer CPU
columns cover its owning frame, while the frame interval also includes work and
waits between presentations. A long CPU fence wait can be caused by the older GPU
submission, so compare its `waited_gpu_` columns before attributing the delay to the
current GPU row. CPU scopes overlap and should not be added together.

For a hitch investigation, reproducing the camera movement during this opt-in
capture is useful. Its `camera_changed` flag then documents the moving-camera
scenario; it is still not a fixed-view FPS comparison. Cache work on the same row
is correlation, not proof that the cache alone caused the hitch. The 128-row limit
describes the worst retained frames, not the frequency of every possible cause.

Important interpretation:

- Frame intervals include frame caps, VSync, and waits. Renderer CPU time includes Present
  waits; it is not pure computation. Task dispatch includes rendering and simulation.
- Simulation metrics are per update, not per displayed frame. CPU scopes overlap; GPU work
  overlaps CPU work. Do not add all columns together.
- Generated/interpolated frames are excluded. Base FPS is the reciprocal mean frame interval.
- GPU passes have their own sample counts. A few final flight-slot queries may not resolve
  before capture ends; no artificial drain is added. Zero samples means unavailable, not free.
- `camera_changed`, `settings_changed`, or `focus_lost` flag a run that should not be used as
  a fixed-view comparison. Texture quality is restart-latched: keep it identical across runs.
- Closing early writes a `partial` report if recording has started. An abrupt crash may not
  produce a benchmark summary; its normal crash dump is separate.

Start with the slow Mission 7/8 view and the current FSR Quality + frame generation settings.
Collect a baseline before changing quality or implementing optimizations. No performance gain
is claimed until those real captures have been compared.
