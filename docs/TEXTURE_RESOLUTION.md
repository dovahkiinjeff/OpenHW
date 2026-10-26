# Model texture resolution

Graphics / Image Pipeline now exposes Low (512), Medium (1024), High (2048),
and Ultra (full source). Ultra is the default when an old settings file has no
`textureResolution` entry. Values 0..3 are saved by the existing settings writer;
Cancel restores the previous selection. Changes require restarting the game.

The renderer latches the cap at first texture load. Both raster replacements
and DXR diffuse, normal, and emission DDS maps use the same aspect-preserving
long-edge policy. Smaller textures are not enlarged. UI and sky rendering are
not changed by the model DDS decoder. CPU reductions use DirectXTex area
filtering rather than nearest-neighbor decimation. Team masks and paint shading
use the same selected dimensions as diffuse, instead of bootstrap dimensions.

The native Kushan mothership package contains source maps up to 4096x2048 and
2048x4096. Its LIF files are deliberately small compatibility bootstraps, not
the desired modern rendering texture. Previously the direct non-team raster
upload accepted only BC7, causing RGBA DDS to fall back to that bootstrap.
The direct uploader now accepts the imported RGBA8 format too. Reduced textures go through
the CPU decode path instead of the full-size direct upload.

Source/package dimensions alone do not prove the dimensions seen in a user's
screenshot. Mothership raster replacement messages now include actual upload
dimensions in the ordinary renderer log, allowing a normal user launch to
verify this without a hidden test launcher.
