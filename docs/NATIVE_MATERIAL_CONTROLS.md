# Native ship materials

RM HODs remain unmodified. Material import retains diffuse, authored normals,
emission, paint masks, and one `_material.dds` control page per surface and
engine state. This page is linear RGBA: original reflection sharpness, roughness,
dielectric specular strength, and the encoded environment mask. Alpha 128 marks
bays. Values 129 through 253 encode the source mask from zero through one.
Older pages have alpha 255 and fall back to their roughness and specular data. The internal renderer registration explicitly tags this material
layout; ordinary ORM materials retain their separate renderer meaning.

The importer follows `sob_ship.frag` and `sob_share_materials.imp` default
semantics: GLOW red controls gloss, green controls emission, and max(blue-green,0)
controls specular strength. TEAM changes painted-surface gloss and specular
strength. RGB-glow shaders use their separate SPEC page and the source shader's
G=1 policy. Gloss is converted to roughness using sqrt(2/(gloss+2)). This is a
translation into OpenHW shading, not an assertion of pixel-identical RM lighting.

DXR, D3D12 raster and DX11 consume the same controls. RM normals and roughness are authored
data, not generated paint relief. The denoiser does not infer metalness from
yellow paint or the global reflection slider.

Shift+F11 > TEXTURES opens the global control panel. It exposes specular gain,
un-authored roughness, generated legacy normal strength, and texture resolution.
The panel labels the two legacy-only controls explicitly. Resolution takes effect
after restart; the other adjustments apply live. SAVE GLOBAL and closing the panel
persist through the ordinary portable game settings. Material controls are no
longer captured, applied, or written by mission lighting overrides (reserved
mission tuning indices 9–11). No mission-specific texture interpretations exist.

`stage_installed_material_controls.py` stages a targeted existing-install update
from declared native surface mappings, including mod overlays. It does not rewrite
gameplay, models, weapons, texture artwork, or existing mod definitions. The normal
material staging pipeline emits the same controls for new installs.

Ship reflections use the mission sky, the surface normal, and the viewing angle.
The renderer blends broad and sharp environment samples using GLOW red. Direct
highlights and reflected sky retain their own colour on dark hull paint. DXR
stores reflected radiance separately from diffuse lighting until compositing.
Bay materials receive direct highlights and suppress environment reflections.

For packed installs, `tools/stage_archive_reflections.py` reads the original
ship HODs from the install archive and stages updated material controls. It
preserves the existing green and blue channels.
