# Separate Windows cursor

The normal Windows pointer now uses SDL's Windows color-cursor backend, not a
textured quad in the scene. Windows updates its position independently of game
presentation, upscaling, and frame generation. Command cursor selection, click
processing, hover text, selection boxes, and camera motion still follow game
updates; this is not a separate simulation/input thread.

The existing HD PNG mapping, top-left hotspot, automatic UI scale, and user
cursor-size percentage are retained. Cursor images are alpha-aware resized and
cached per command type and size. Missing PNGs use the installed legacy artwork;
an OS cursor creation failure falls back to the default Windows pointer.

Hidden and camera-rotation/zoom cursors remain hidden. Relative/clamped controls,
including color-picker drags, temporarily retain the software cursor because
SDL suppresses system cursors in relative mode. Non-Windows builds retain their
existing software presentation. Focus restoration refreshes visibility rather
than unconditionally hiding the pointer. Reset/shutdown selects a default handle
before freeing cached handles.

The native cursor test exercises alpha resampling and actual Windows cursor
creation, swapping, visibility, and destruction without creating a window or
launching the game. In-game artwork, drag, focus, and low-FPS behavior still need
a normal user-launch visual check.
