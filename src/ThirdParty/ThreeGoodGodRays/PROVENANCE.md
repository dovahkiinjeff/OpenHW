# three-good-godrays provenance

- Project: https://github.com/Ameobea/three-good-godrays
- Inspected revision: `70ad1c41c78796637064b3bb9246be08e81ee777`
- Original implementation acknowledged upstream: https://github.com/N8python/goodGodRays
- Copyright: Casey Primozic, 2022; see the complete unmodified [LICENSE](LICENSE).
- Upstream license: https://github.com/Ameobea/three-good-godrays/blob/70ad1c41c78796637064b3bb9246be08e81ee777/LICENSE

The candidate OpenHW renderer is an **altered Direct3D 12/HLSL adaptation**, not an unmodified copy of the Three.js package. It adapts the reduced-resolution shadow-visibility integration and joint bilateral upsampling structure described by `src/godrays.frag` and `src/compositor.frag` at the revision above. OpenHW retains its own finite mission bounds, camera/depth conventions, sun visibility and authoring controls. The Three.js runtime and demo assets are not incorporated.

The adaptation is under development. It must preserve dark-scene contrast and treat shaft radiance separately, rather than applying a whole-scene haze. No performance improvement or visual parity is asserted before production-path validation. See [the development notes](../../../docs/GOD_RAYS.md).
