# Frame worker ownership

The D3D12 frame uses three persistent CPU workers: ray/path tracing preparation, post-processing command recording, and audio event updates. The main thread retains SDL input, simulation, scene capture, command queue submission and presentation. SDL mixing and stream decoding keep their existing independent threads.

Ray preparation records a dedicated per-flight-slot direct command list while the main thread records native raster replay. The main thread then joins ray preparation before post-processing consumes its scene/depth/motion state. Post-processing records another direct command list on its own worker. GPU submission remains one ordered raster/ray/post batch on the existing queue. No new GPU queue, additional frame of latency, quality reduction, shader change or unconditional GPU fence wait is introduced.

Each worker has one bounded borrowed job. Scope destruction joins that job on all error paths. Frame allocators are reset only after the existing flight-slot fence wait. Post recording completes before queue submission; the original presentation thread is retained. Shutdown drains audio and stops workers before releasing devices. The shared GPU allocator retains its default multithreaded D3D12MA synchronization.

Audio presentation effects remain on the main thread because they can alter native instances and renderer light lists. Native harvest-sound ownership, whose query can lazily create a native model instance, is snapshotted before dispatch. The audio-only event pass may then read the frozen render list/camera and update audio handles while graphics preparation runs. It is joined before window presentation, task yield or resource shutdown. Other callers retain synchronous sound-event updates.

`HW_FRAME_WORKERS=0` is a developer-only serial comparison using the identical command graph and settings. Thread creation failure also falls back to serial recording. No user configuration is changed by this switch.

Validation includes persistent-thread/overlap/lifetime tests, frozen-audio ownership tests, production DXR serial/threaded byte-identical pixel replay and D3D12 validation, and production frame-generation preparation on a post worker with presentation on the original thread. An in-game Mission 14 comparison remains necessary to measure actual performance; thread creation alone is not evidence of an FPS gain.
