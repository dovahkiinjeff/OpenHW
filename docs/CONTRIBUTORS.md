# Contributors and third-party components

[Open the Contributors page](https://openhw.tech/#/contributors).

OpenHW is developed by Viperslayer, building on the work of the original Homeworld teams, community contributors and the upstream projects recorded below. Each component retains its own authorship and license. Source availability is not a blanket redistribution grant.

Reviewed: 2026-11-22 for OpenHW 1.1.14.

## Inventory scope

Project source, bundled native/Python runtimes, Windows editor dependency closure, JavaScript lockfiles (including build and optional platform packages), fonts, and direct server/build tools. Operating-system distribution packages and retail game assets are outside the software dependency inventory.

The catalog is generated from the pinned notices and provenance, vcpkg installed package records, the Windows Cargo dependency closure, both JavaScript lockfiles and Python packaging metadata. Full copied notices are served beside the public catalog. Build and optional-platform dependencies are explicitly labeled.

[Machine-readable inventory](contributors.json) | [Published inventory](https://openhw.tech/data/contributors.json)

## Component credits

| Project | Version | Use | License |
| --- | --- | --- | --- |
| [Homeworld artwork and community captures](https://github.com/dovahkiinjeff/OpenHW/blob/main/assets/UI/ART_PROVENANCE.md) | Supplied material | Fonts & artwork; Original concept artists and rights holders retain ownership. Gallery captions and the provenance inventory identify supplied images and their treatment; retail assets are imported from each player’s owned Collection. | Original artwork/retail rights; not an open-source grant |
| [Rajdhani](https://github.com/google/fonts/tree/main/ofl/rajdhani) | Bundled font | Fonts & artwork; Locally hosted interface typography. | OFL-1.1 |
| [Share Tech Mono](https://github.com/google/fonts/tree/main/ofl/sharetechmono) | Bundled font | Fonts & artwork; Locally hosted interface typography. | OFL-1.1 |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) | 2.3.0 | Game & rendering; FSR reconstruction, native AA and frame-generation integration. | SDK and component license terms |
| [D3D12 Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator) | 3.2.0 | Game & rendering; Pooled Direct3D 12 GPU allocations. | MIT |
| [Homeworld / HomeworldSDL](https://github.com/HomeworldSDL/HomeworldSDL) | Original engine baseline | Game & rendering; Original simulation, campaign, source baseline and SDL portability work; includes Relic CRC and legacy Titan code. | Relic non-commercial source agreement |
| [Intel XeSS](https://github.com/intel/xess) | 3.0.2 | Game & rendering; XeSS reconstruction and native AA integration. | Intel XeSS SDK terms |
| [LZSS / BitIO](https://marknelson.us/) | Legacy source | Game & rendering; Legacy compression code derived from The Data Compression Book and modified for Homeworld. | See source headers and original source terms |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | 1.0 | Game & rendering; Native geometry deduplication; the editor also uses its Rust binding. | MIT |
| [NVIDIA Streamline / DLSS / NGX](https://github.com/NVIDIA-RTX/Streamline) | Streamline 2.12.0 | Game & rendering; Reconstruction integration and selected NVIDIA redistributable runtimes. Individual feature support is documented in the graphics guide. | NVIDIA SDK and runtime terms |
| [OpenMW volumetric clouds](https://github.com/zesterer/openmw-volumetric-clouds) | c8830bcd2de0 | Game & rendering; Cloud shader/noise adaptation and retained upstream reference assets. No explicit license grant was found in the pinned source; the provenance record marks redistribution permission unresolved. | Permission unresolved; see provenance |
| [stb headers](https://github.com/nothings/stb) | Vendored source | Game & rendering; Image and supporting single-header utilities. | MIT OR public domain |
| [three-good-godrays](https://github.com/Ameobea/three-good-godrays) | 70ad1c41c787 | Game & rendering; Altered HLSL/D3D12 adaptation of the volumetric shaft renderer. | Zlib |
| [HODEditorJS](https://github.com/GuillermoOM/HODEditorJS) | 1.2.17 / 4cb39196f032 | HOD editor; Foundation of the integrated HOD editor and native HOD parser. OpenHW adds in-game hosting, mod saves and quick subsystem workflows. | CC-BY-NC-4.0 |
| [@babel/code-frame](https://github.com/babel/babel) | 7.29.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/compat-data](https://github.com/babel/babel) | 7.29.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/core](https://github.com/babel/babel) | 7.29.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/generator](https://github.com/babel/babel) | 7.29.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-compilation-targets](https://github.com/babel/babel) | 7.28.6 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-globals](https://github.com/babel/babel) | 7.28.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-module-imports](https://github.com/babel/babel) | 7.28.6 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-module-transforms](https://github.com/babel/babel) | 7.28.6 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-plugin-utils](https://github.com/babel/babel) | 7.28.6 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-string-parser](https://github.com/babel/babel) | 7.27.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-validator-identifier](https://github.com/babel/babel) | 7.28.5 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helper-validator-option](https://github.com/babel/babel) | 7.27.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/helpers](https://github.com/babel/babel) | 7.29.2 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/parser](https://github.com/babel/babel) | 7.29.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/plugin-transform-react-jsx-self](https://github.com/babel/babel) | 7.27.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/plugin-transform-react-jsx-source](https://github.com/babel/babel) | 7.27.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/template](https://github.com/babel/babel) | 7.28.6 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/traverse](https://github.com/babel/babel) | 7.29.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@babel/types](https://github.com/babel/babel) | 7.29.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@dimforge/rapier3d-compat](https://github.com/dimforge/rapier.js) | 0.12.0 | HOD editor · JavaScript; Build/development dependency | Apache-2.0 |
| [@esbuild/aix-ppc64](https://www.npmjs.com/package/@esbuild/aix-ppc64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/android-arm](https://www.npmjs.com/package/@esbuild/android-arm/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/android-arm64](https://www.npmjs.com/package/@esbuild/android-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/android-x64](https://www.npmjs.com/package/@esbuild/android-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/darwin-arm64](https://www.npmjs.com/package/@esbuild/darwin-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/darwin-x64](https://www.npmjs.com/package/@esbuild/darwin-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/freebsd-arm64](https://www.npmjs.com/package/@esbuild/freebsd-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/freebsd-x64](https://www.npmjs.com/package/@esbuild/freebsd-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-arm](https://www.npmjs.com/package/@esbuild/linux-arm/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-arm64](https://www.npmjs.com/package/@esbuild/linux-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-ia32](https://www.npmjs.com/package/@esbuild/linux-ia32/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-loong64](https://www.npmjs.com/package/@esbuild/linux-loong64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-mips64el](https://www.npmjs.com/package/@esbuild/linux-mips64el/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-ppc64](https://www.npmjs.com/package/@esbuild/linux-ppc64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-riscv64](https://www.npmjs.com/package/@esbuild/linux-riscv64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-s390x](https://www.npmjs.com/package/@esbuild/linux-s390x/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-x64](https://www.npmjs.com/package/@esbuild/linux-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/netbsd-arm64](https://www.npmjs.com/package/@esbuild/netbsd-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/netbsd-x64](https://www.npmjs.com/package/@esbuild/netbsd-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/openbsd-arm64](https://www.npmjs.com/package/@esbuild/openbsd-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/openbsd-x64](https://www.npmjs.com/package/@esbuild/openbsd-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/openharmony-arm64](https://www.npmjs.com/package/@esbuild/openharmony-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/sunos-x64](https://www.npmjs.com/package/@esbuild/sunos-x64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/win32-arm64](https://www.npmjs.com/package/@esbuild/win32-arm64/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/win32-ia32](https://www.npmjs.com/package/@esbuild/win32-ia32/v/0.27.7) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/win32-x64](https://github.com/evanw/esbuild) | 0.27.7 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@jridgewell/gen-mapping](https://github.com/jridgewell/sourcemaps) | 0.3.13 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@jridgewell/remapping](https://github.com/jridgewell/sourcemaps) | 2.3.5 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@jridgewell/resolve-uri](https://github.com/jridgewell/resolve-uri) | 3.1.2 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@jridgewell/sourcemap-codec](https://github.com/jridgewell/sourcemaps) | 1.5.5 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@jridgewell/trace-mapping](https://github.com/jridgewell/sourcemaps) | 0.3.31 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@rolldown/pluginutils](https://github.com/rolldown/rolldown) | 1.0.0-beta.27 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@rollup/rollup-android-arm-eabi](https://www.npmjs.com/package/@rollup/rollup-android-arm-eabi/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-android-arm64](https://www.npmjs.com/package/@rollup/rollup-android-arm64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-darwin-arm64](https://www.npmjs.com/package/@rollup/rollup-darwin-arm64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-darwin-x64](https://www.npmjs.com/package/@rollup/rollup-darwin-x64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-freebsd-arm64](https://www.npmjs.com/package/@rollup/rollup-freebsd-arm64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-freebsd-x64](https://www.npmjs.com/package/@rollup/rollup-freebsd-x64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-arm-gnueabihf](https://www.npmjs.com/package/@rollup/rollup-linux-arm-gnueabihf/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-arm-musleabihf](https://www.npmjs.com/package/@rollup/rollup-linux-arm-musleabihf/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-arm64-gnu](https://www.npmjs.com/package/@rollup/rollup-linux-arm64-gnu/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-arm64-musl](https://www.npmjs.com/package/@rollup/rollup-linux-arm64-musl/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-loong64-gnu](https://www.npmjs.com/package/@rollup/rollup-linux-loong64-gnu/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-loong64-musl](https://www.npmjs.com/package/@rollup/rollup-linux-loong64-musl/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-ppc64-gnu](https://www.npmjs.com/package/@rollup/rollup-linux-ppc64-gnu/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-ppc64-musl](https://www.npmjs.com/package/@rollup/rollup-linux-ppc64-musl/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-riscv64-gnu](https://www.npmjs.com/package/@rollup/rollup-linux-riscv64-gnu/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-riscv64-musl](https://www.npmjs.com/package/@rollup/rollup-linux-riscv64-musl/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-s390x-gnu](https://www.npmjs.com/package/@rollup/rollup-linux-s390x-gnu/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-x64-gnu](https://www.npmjs.com/package/@rollup/rollup-linux-x64-gnu/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-linux-x64-musl](https://www.npmjs.com/package/@rollup/rollup-linux-x64-musl/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-openbsd-x64](https://www.npmjs.com/package/@rollup/rollup-openbsd-x64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-openharmony-arm64](https://www.npmjs.com/package/@rollup/rollup-openharmony-arm64/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-win32-arm64-msvc](https://www.npmjs.com/package/@rollup/rollup-win32-arm64-msvc/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-win32-ia32-msvc](https://www.npmjs.com/package/@rollup/rollup-win32-ia32-msvc/v/4.60.4) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-win32-x64-gnu](https://github.com/rollup/rollup) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@rollup/rollup-win32-x64-msvc](https://github.com/rollup/rollup) | 4.60.4 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@tweenjs/tween.js](https://github.com/tweenjs/tween.js) | 23.1.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/babel__core](https://github.com/DefinitelyTyped/DefinitelyTyped) | 7.20.5 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/babel__generator](https://github.com/DefinitelyTyped/DefinitelyTyped) | 7.27.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/babel__template](https://github.com/DefinitelyTyped/DefinitelyTyped) | 7.4.4 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/babel__traverse](https://github.com/DefinitelyTyped/DefinitelyTyped) | 7.28.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/estree](https://github.com/DefinitelyTyped/DefinitelyTyped) | 1.0.8 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/react](https://github.com/DefinitelyTyped/DefinitelyTyped) | 19.2.15 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/react-dom](https://github.com/DefinitelyTyped/DefinitelyTyped) | 19.2.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/stats.js](https://github.com/DefinitelyTyped/DefinitelyTyped) | 0.17.4 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/three](https://github.com/DefinitelyTyped/DefinitelyTyped) | 0.184.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@types/webxr](https://github.com/DefinitelyTyped/DefinitelyTyped) | 0.5.24 | HOD editor · JavaScript; Build/development dependency | MIT |
| [@vitejs/plugin-react](https://github.com/vitejs/vite-plugin-react) | 4.7.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [baseline-browser-mapping](https://github.com/web-platform-dx/baseline-browser-mapping) | 2.10.32 | HOD editor · JavaScript; Build/development dependency | Apache-2.0 |
| [browserslist](https://www.npmjs.com/package/browserslist/v/4.28.2) | 4.28.2 | HOD editor · JavaScript; Build/development dependency | MIT |
| [caniuse-lite](https://www.npmjs.com/package/caniuse-lite/v/1.0.30001793) | 1.0.30001793 | HOD editor · JavaScript; Build/development dependency | CC-BY-4.0 |
| [clsx](https://www.npmjs.com/package/clsx/v/2.1.1) | 2.1.1 | HOD editor · JavaScript; Bundled interface dependency | MIT |
| [convert-source-map](https://github.com/thlorenz/convert-source-map) | 2.0.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [csstype](https://github.com/frenic/csstype) | 3.2.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [debug](https://github.com/debug-js/debug) | 4.4.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [electron-to-chromium](https://github.com/Kilian/electron-to-chromium) | 1.5.361 | HOD editor · JavaScript; Build/development dependency | ISC |
| [esbuild](https://github.com/evanw/esbuild) | 0.27.7 | HOD editor · JavaScript; Build/development dependency | MIT |
| [escalade](https://www.npmjs.com/package/escalade/v/3.2.0) | 3.2.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [fdir](https://github.com/thecodrr/fdir) | 6.5.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [fflate](https://github.com/101arrowz/fflate) | 0.8.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [fsevents](https://www.npmjs.com/package/fsevents/v/2.3.3) | 2.3.3 | HOD editor · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [gensync](https://github.com/loganfsmyth/gensync) | 1.0.0-beta.2 | HOD editor · JavaScript; Build/development dependency | MIT |
| [js-tokens](https://www.npmjs.com/package/js-tokens/v/4.0.0) | 4.0.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [jsesc](https://github.com/mathiasbynens/jsesc) | 3.1.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [json5](https://github.com/json5/json5) | 2.2.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [lru-cache](https://github.com/isaacs/node-lru-cache) | 5.1.1 | HOD editor · JavaScript; Build/development dependency | ISC |
| [lucide-react](https://github.com/lucide-icons/lucide) | 1.16.0 | HOD editor · JavaScript; Bundled interface dependency | ISC |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | 1.1.1 | HOD editor · JavaScript; Build/development dependency | MIT |
| [ms](https://www.npmjs.com/package/ms/v/2.1.3) | 2.1.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [nanoid](https://www.npmjs.com/package/nanoid/v/3.3.12) | 3.3.12 | HOD editor · JavaScript; Build/development dependency | MIT |
| [node-releases](https://github.com/chicoxyzzy/node-releases) | 2.0.46 | HOD editor · JavaScript; Build/development dependency | MIT |
| [picocolors](https://www.npmjs.com/package/picocolors/v/1.1.1) | 1.1.1 | HOD editor · JavaScript; Build/development dependency | ISC |
| [picomatch](https://www.npmjs.com/package/picomatch/v/4.0.4) | 4.0.4 | HOD editor · JavaScript; Build/development dependency | MIT |
| [postcss](https://www.npmjs.com/package/postcss/v/8.5.15) | 8.5.15 | HOD editor · JavaScript; Build/development dependency | MIT |
| [react](https://github.com/facebook/react) | 19.2.6 | HOD editor · JavaScript; Bundled interface dependency | MIT |
| [react-dom](https://github.com/facebook/react) | 19.2.6 | HOD editor · JavaScript; Bundled interface dependency | MIT |
| [react-refresh](https://github.com/facebook/react) | 0.17.0 | HOD editor · JavaScript; Build/development dependency | MIT |
| [rollup](https://github.com/rollup/rollup) | 4.60.4 | HOD editor · JavaScript; Build/development dependency | MIT |
| [scheduler](https://github.com/facebook/react) | 0.27.0 | HOD editor · JavaScript; Bundled interface dependency | MIT |
| [semver](https://github.com/npm/node-semver) | 6.3.1 | HOD editor · JavaScript; Build/development dependency | ISC |
| [source-map-js](https://www.npmjs.com/package/source-map-js/v/1.2.1) | 1.2.1 | HOD editor · JavaScript; Build/development dependency | BSD-3-Clause |
| [three](https://github.com/mrdoob/three.js) | 0.184.0 | HOD editor · JavaScript; Bundled interface dependency | MIT |
| [tinyglobby](https://github.com/SuperchupuDev/tinyglobby) | 0.2.16 | HOD editor · JavaScript; Build/development dependency | MIT |
| [typescript](https://github.com/microsoft/TypeScript) | 5.8.3 | HOD editor · JavaScript; Build/development dependency | Apache-2.0 |
| [update-browserslist-db](https://www.npmjs.com/package/update-browserslist-db/v/1.2.3) | 1.2.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [vite](https://github.com/vitejs/vite) | 7.3.3 | HOD editor · JavaScript; Build/development dependency | MIT |
| [yallist](https://github.com/isaacs/yallist) | 3.1.1 | HOD editor · JavaScript; Build/development dependency | ISC |
| [adler2](https://github.com/oyvindln/adler2) | 2.0.1 | HOD editor · Rust; A simple clean-room implementation of the Adler-32 checksum | 0BSD OR MIT OR Apache-2.0 |
| [aho-corasick](https://github.com/BurntSushi/aho-corasick) | 1.1.4 | HOD editor · Rust; Fast multiple substring searching. | Unlicense OR MIT |
| [autocfg](https://github.com/cuviper/autocfg) | 1.5.1 | HOD editor · Rust; Automatic cfg for Rust compiler features | Apache-2.0 OR MIT |
| [base64](https://github.com/marshallpierce/rust-base64) | 0.22.1 | HOD editor · Rust; encodes and decodes base64 as bytes or utf8 | MIT OR Apache-2.0 |
| [bitflags](https://github.com/bitflags/bitflags) | 1.3.2 | HOD editor · Rust; A macro to generate structures which behave like bitflags.  | MIT/Apache-2.0 |
| [bitflags](https://github.com/bitflags/bitflags) | 2.11.1 | HOD editor · Rust; A macro to generate structures which behave like bitflags.  | MIT OR Apache-2.0 |
| [bytemuck](https://github.com/Lokathor/bytemuck) | 1.25.0 | HOD editor · Rust; A crate for mucking around with piles of bytes. | Zlib OR Apache-2.0 OR MIT |
| [byteorder](https://github.com/BurntSushi/byteorder) | 1.5.0 | HOD editor · Rust; Library for reading/writing numbers in big-endian and little-endian. | Unlicense OR MIT |
| [bytes](https://github.com/tokio-rs/bytes) | 1.11.1 | HOD editor · Rust; Types and traits for working with bytes | MIT |
| [cc](https://github.com/rust-lang/cc-rs) | 1.2.62 | HOD editor · Rust; A build-time dependency for Cargo build scripts to assist in invoking the native C compiler to compile native C code into a static archive to be linked into Rust code.  | MIT OR Apache-2.0 |
| [cfg-if](https://github.com/rust-lang/cfg-if) | 1.0.4 | HOD editor · Rust; A macro to ergonomically define an item depending on a large number of #[cfg] parameters. Structured like an if-else chain, the first matching branch is the item that gets emitted.  | MIT OR Apache-2.0 |
| [chrono](https://github.com/chronotope/chrono) | 0.4.44 | HOD editor · Rust; Date and time library for Rust | MIT OR Apache-2.0 |
| [color_quant](https://github.com/image-rs/color_quant.git) | 1.1.0 | HOD editor · Rust; Color quantization library to reduce n colors to 256 colors. | MIT |
| [cookie](https://github.com/SergioBenitez/cookie-rs) | 0.18.1 | HOD editor · Rust; HTTP cookie parsing and cookie jar management. Supports signed and private (encrypted, authenticated) jars.  | MIT OR Apache-2.0 |
| [crc32fast](https://github.com/srijs/rust-crc32fast) | 1.5.0 | HOD editor · Rust; Fast, SIMD-accelerated CRC32 (IEEE) checksum computation | MIT OR Apache-2.0 |
| [deranged](https://github.com/jhpratt/deranged) | 0.5.8 | HOD editor · Rust; Ranged integers | MIT OR Apache-2.0 |
| [dpi](https://github.com/rust-windowing/winit) | 0.1.2 | HOD editor · Rust; Types for handling UI scaling | Apache-2.0 AND MIT |
| [dunce](https://gitlab.com/kornelski/dunce) | 1.0.5 | HOD editor · Rust; Normalize Windows paths to the most compatible format, avoiding UNC where possible | CC0-1.0 OR MIT-0 OR Apache-2.0 |
| [fastrand](https://github.com/smol-rs/fastrand) | 2.4.1 | HOD editor · Rust; A simple and fast random number generator | Apache-2.0 OR MIT |
| [fdeflate](https://github.com/image-rs/fdeflate) | 0.3.7 | HOD editor · Rust; Fast specialized deflate implementation | MIT OR Apache-2.0 |
| [find-msvc-tools](https://github.com/rust-lang/cc-rs) | 0.1.9 | HOD editor · Rust; Find windows-specific tools, read MSVC versions from the registry and from COM interfaces | MIT OR Apache-2.0 |
| [flate2](https://github.com/rust-lang/flate2-rs) | 1.1.9 | HOD editor · Rust; DEFLATE compression and decompression exposed as Read/BufRead/Write streams. Supports miniz_oxide and multiple zlib implementations. Supports zlib, gzip, and raw deflate streams.  | MIT OR Apache-2.0 |
| [float-cmp](https://github.com/mikedilger/float-cmp) | 0.10.0 | HOD editor · Rust; Floating point approximate comparison traits | MIT |
| [getrandom](https://github.com/rust-random/getrandom) | 0.4.2 | HOD editor · Rust; A small cross-platform library for retrieving random data from system source | MIT OR Apache-2.0 |
| [http](https://github.com/hyperium/http) | 1.4.0 | HOD editor · Rust; A set of types for representing HTTP requests and responses.  | MIT OR Apache-2.0 |
| [image](https://github.com/image-rs/image) | 0.24.9 | HOD editor · Rust; Imaging library. Provides basic image processing and encoders/decoders for common image formats. | MIT OR Apache-2.0 |
| [itoa](https://github.com/dtolnay/itoa) | 1.0.18 | HOD editor · Rust; Fast integer primitive to string conversion | MIT OR Apache-2.0 |
| [jpeg-decoder](https://github.com/image-rs/jpeg-decoder) | 0.3.2 | HOD editor · Rust; JPEG decoder | MIT OR Apache-2.0 |
| [libm](https://github.com/rust-lang/compiler-builtins) | 0.2.16 | HOD editor · Rust; libm in pure Rust | MIT |
| [log](https://github.com/rust-lang/log) | 0.4.29 | HOD editor · Rust; A lightweight logging facade for Rust  | MIT OR Apache-2.0 |
| [memchr](https://github.com/BurntSushi/memchr) | 2.8.0 | HOD editor · Rust; Provides extremely fast (uses SIMD on x86_64, aarch64 and wasm32) routines for 1, 2 or 3 byte search and single substring search.  | Unlicense OR MIT |
| [meshopt](https://github.com/gwihlidal/meshopt-rs) | 0.6.2 | HOD editor · Rust; Rust ffi bindings and idiomatic wrapper for mesh optimizer | MIT OR Apache-2.0 |
| [miniz_oxide](https://github.com/Frommi/miniz_oxide/tree/master/miniz_oxide) | 0.8.9 | HOD editor · Rust; DEFLATE compression and decompression library rewritten in Rust based on miniz | MIT OR Zlib OR Apache-2.0 |
| [num-conv](https://github.com/jhpratt/num-conv) | 0.2.2 | HOD editor · Rust; `num_conv` is a crate to convert between integer types without using `as` casts. This provides better certainty when refactoring, makes the exact behavior of code more explicit, and allows using turbofish syntax.  | MIT OR Apache-2.0 |
| [num-traits](https://github.com/rust-num/num-traits) | 0.2.19 | HOD editor · Rust; Numeric traits for generic mathematics | MIT OR Apache-2.0 |
| [once_cell](https://github.com/matklad/once_cell) | 1.21.4 | HOD editor · Rust; Single assignment cells and lazy values. | MIT OR Apache-2.0 |
| [png](https://github.com/image-rs/image-png) | 0.17.16 | HOD editor · Rust; PNG decoding and encoding library in pure Rust | MIT OR Apache-2.0 |
| [powerfmt](https://github.com/jhpratt/powerfmt) | 0.2.0 | HOD editor · Rust;     `powerfmt` is a library that provides utilities for formatting values. This crate makes it     significantly easier to support filling to a minimum width with alignment, avoid heap     allocation, and avoid repetitive calculations.  | MIT OR Apache-2.0 |
| [proc-macro2](https://github.com/dtolnay/proc-macro2) | 1.0.106 | HOD editor · Rust; A substitute implementation of the compiler's `proc_macro` API to decouple token-based libraries from the procedural macro use case. | MIT OR Apache-2.0 |
| [quote](https://github.com/dtolnay/quote) | 1.0.45 | HOD editor · Rust; Quasi-quoting macro quote!(...) | MIT OR Apache-2.0 |
| [raw-window-handle](https://github.com/rust-windowing/raw-window-handle) | 0.6.2 | HOD editor · Rust; Interoperability library for Rust Windowing applications. | MIT OR Apache-2.0 OR Zlib |
| [regex](https://github.com/rust-lang/regex) | 1.12.3 | HOD editor · Rust; An implementation of regular expressions for Rust. This implementation uses finite automata and guarantees linear time matching on all inputs.  | MIT OR Apache-2.0 |
| [regex-automata](https://github.com/rust-lang/regex) | 0.4.14 | HOD editor · Rust; Automata construction and matching using regular expressions. | MIT OR Apache-2.0 |
| [regex-syntax](https://github.com/rust-lang/regex) | 0.8.10 | HOD editor · Rust; A regular expression parser. | MIT OR Apache-2.0 |
| [rfd](https://github.com/PolyMeilex/rfd) | 0.14.1 | HOD editor · Rust; Rusty File Dialog | MIT |
| [roxmltree](https://github.com/RazrFalcon/roxmltree) | 0.21.1 | HOD editor · Rust; Represent an XML as a read-only tree. | MIT OR Apache-2.0 |
| [serde](https://github.com/serde-rs/serde) | 1.0.228 | HOD editor · Rust; A generic serialization/deserialization framework | MIT OR Apache-2.0 |
| [serde_core](https://github.com/serde-rs/serde) | 1.0.228 | HOD editor · Rust; Serde traits only, with no support for derive -- use the `serde` crate instead | MIT OR Apache-2.0 |
| [serde_derive](https://github.com/serde-rs/serde) | 1.0.228 | HOD editor · Rust; Macros 1.1 implementation of #[derive(Serialize, Deserialize)] | MIT OR Apache-2.0 |
| [serde_json](https://github.com/serde-rs/json) | 1.0.150 | HOD editor · Rust; A JSON serialization file format | MIT OR Apache-2.0 |
| [shlex](https://github.com/comex/rust-shlex) | 1.3.0 | HOD editor · Rust; Split a string into shell words, like Python's shlex. | MIT OR Apache-2.0 |
| [simd-adler32](https://github.com/mcountryman/simd-adler32) | 0.3.9 | HOD editor · Rust; A SIMD-accelerated Adler-32 hash algorithm implementation. | MIT |
| [syn](https://github.com/dtolnay/syn) | 2.0.117 | HOD editor · Rust; Parser for Rust source code | MIT OR Apache-2.0 |
| [tempfile](https://github.com/Stebalien/tempfile) | 3.27.0 | HOD editor · Rust; A library for managing temporary files and directories. | MIT OR Apache-2.0 |
| [texpresso](https://github.com/jansol/texpresso) | 2.0.2 | HOD editor · Rust; A pure Rust texture compression suite | MIT |
| [thiserror](https://github.com/dtolnay/thiserror) | 2.0.18 | HOD editor · Rust; derive(Error) | MIT OR Apache-2.0 |
| [thiserror-impl](https://github.com/dtolnay/thiserror) | 2.0.18 | HOD editor · Rust; Implementation detail of the `thiserror` crate | MIT OR Apache-2.0 |
| [time](https://github.com/time-rs/time) | 0.3.47 | HOD editor · Rust; Date and time library. Fully interoperable with the standard library. Mostly compatible with #![no_std]. | MIT OR Apache-2.0 |
| [time-core](https://github.com/time-rs/time) | 0.1.8 | HOD editor · Rust; This crate is an implementation detail and should not be relied upon directly. | MIT OR Apache-2.0 |
| [time-macros](https://github.com/time-rs/time) | 0.2.27 | HOD editor · Rust;     Procedural macros for the time crate.     This crate is an implementation detail and should not be relied upon directly.  | MIT OR Apache-2.0 |
| [unicode-ident](https://github.com/dtolnay/unicode-ident) | 1.0.24 | HOD editor · Rust; Determine whether characters have the XID_Start or XID_Continue properties according to Unicode Standard Annex #31 | (MIT OR Apache-2.0) AND Unicode-3.0 |
| [version_check](https://github.com/SergioBenitez/version_check) | 0.9.5 | HOD editor · Rust; Tiny crate to check the version of the installed/running rustc. | MIT/Apache-2.0 |
| [webview2-com](https://github.com/wravery/webview2-rs) | 0.38.2 | HOD editor · Rust; Rust bindings for the WebView2 COM APIs | MIT |
| [webview2-com-macros](https://github.com/wravery/webview2-rs) | 0.8.1 | HOD editor · Rust; Macros which generate callback implementations for WebView2 COM APIs | MIT |
| [webview2-com-sys](https://github.com/wravery/webview2-rs) | 0.38.2 | HOD editor · Rust; Bindings generated with the windows crate for the WebView2 COM APIs | MIT |
| [windows](https://github.com/microsoft/windows-rs) | 0.61.3 | HOD editor · Rust; Rust for Windows | MIT OR Apache-2.0 |
| [windows-collections](https://github.com/microsoft/windows-rs) | 0.2.0 | HOD editor · Rust; Windows collection types | MIT OR Apache-2.0 |
| [windows-core](https://github.com/microsoft/windows-rs) | 0.61.2 | HOD editor · Rust; Core type support for COM and Windows | MIT OR Apache-2.0 |
| [windows-future](https://github.com/microsoft/windows-rs) | 0.2.1 | HOD editor · Rust; Windows async types | MIT OR Apache-2.0 |
| [windows-implement](https://github.com/microsoft/windows-rs) | 0.60.2 | HOD editor · Rust; The implement macro for the Windows crates | MIT OR Apache-2.0 |
| [windows-interface](https://github.com/microsoft/windows-rs) | 0.59.3 | HOD editor · Rust; The interface macro for the Windows crates | MIT OR Apache-2.0 |
| [windows-link](https://github.com/microsoft/windows-rs) | 0.1.3 | HOD editor · Rust; Linking for Windows | MIT OR Apache-2.0 |
| [windows-link](https://github.com/microsoft/windows-rs) | 0.2.1 | HOD editor · Rust; Linking for Windows | MIT OR Apache-2.0 |
| [windows-numerics](https://github.com/microsoft/windows-rs) | 0.2.0 | HOD editor · Rust; Windows numeric types | MIT OR Apache-2.0 |
| [windows-result](https://github.com/microsoft/windows-rs) | 0.3.4 | HOD editor · Rust; Windows error handling | MIT OR Apache-2.0 |
| [windows-strings](https://github.com/microsoft/windows-rs) | 0.4.2 | HOD editor · Rust; Windows string types | MIT OR Apache-2.0 |
| [windows-sys](https://github.com/microsoft/windows-rs) | 0.48.0 | HOD editor · Rust; Rust for Windows | MIT OR Apache-2.0 |
| [windows-sys](https://github.com/microsoft/windows-rs) | 0.61.2 | HOD editor · Rust; Rust for Windows | MIT OR Apache-2.0 |
| [windows-targets](https://github.com/microsoft/windows-rs) | 0.48.5 | HOD editor · Rust; Import libs for Windows | MIT OR Apache-2.0 |
| [windows-threading](https://github.com/microsoft/windows-rs) | 0.1.0 | HOD editor · Rust; Windows threading | MIT OR Apache-2.0 |
| [windows-version](https://github.com/microsoft/windows-rs) | 0.1.7 | HOD editor · Rust; Windows version information | MIT OR Apache-2.0 |
| [windows_x86_64_msvc](https://github.com/microsoft/windows-rs) | 0.48.5 | HOD editor · Rust; Import lib for Windows | MIT OR Apache-2.0 |
| [wry](https://github.com/tauri-apps/wry) | 0.55.1 | HOD editor · Rust; Cross-platform WebView rendering library | Apache-2.0 OR MIT |
| [zmij](https://github.com/dtolnay/zmij) | 1.0.21 | HOD editor · Rust; A double-to-string conversion algorithm based on Schubfach and yy | MIT |
| [altgraph](https://github.com/ronaldoussoren/altgraph) | 0.17.5 | Installer & launcher; Python graph (network) package | MIT |
| [charset-normalizer](https://pypi.org/project/charset-normalizer/3.5.2) | 3.5.2 | Installer & launcher; The Real First Universal Charset Detector. Open, modern and actively maintained alternative to Chardet. | MIT |
| [numpy](https://github.com/numpy/numpy) | 2.3.5 | Installer & launcher; Fundamental package for array computing in Python | BSD-3-Clause and bundled third-party terms |
| [packaging](https://github.com/pypa/packaging) | 26.3 | Installer & launcher; Core utilities for Python packages | Apache-2.0 OR BSD-2-Clause |
| [pefile](https://github.com/erocarrera/pefile) | 2024.8.26 | Installer & launcher; Python PE parsing module | MIT |
| [pillow](https://github.com/python-pillow/Pillow) | 13.0.0 | Installer & launcher; Python Imaging Library (fork) | MIT-CMU |
| [pyinstaller](https://github.com/pyinstaller/pyinstaller) | 6.22.3 | Installer & launcher; PyInstaller bundles a Python application and all its dependencies into a single package. | GPLv2-or-later with a special exception which allows to use PyInstaller to build and distribute non-free programs (including commercial ones) |
| [pyinstaller-hooks-contrib](https://github.com/pyinstaller/pyinstaller-hooks-contrib) | 2026.8 | Installer & launcher; Community maintained hooks for PyInstaller | See bundled license |
| [pywin32-ctypes](https://github.com/enthought/pywin32-ctypes) | 0.2.3 | Installer & launcher; A (partial) reimplementation of pywin32 using ctypes/cffi | BSD-3-Clause |
| [setuptools](https://github.com/pypa/setuptools) | 84.0.0 | Installer & launcher; Most extensible Python build backend with support for C/C++ extension modules | MIT |
| [typing_extensions](https://github.com/python/typing_extensions) | 4.16.0 | Installer & launcher; Backported and Experimental Type Hints for Python 3.9+ | PSF-2.0 |
| [directx-dxc](https://github.com/microsoft/DirectXShaderCompiler) | 2026-05-27 | Native libraries & build tools; DirectX Shader Compiler (LLVM/Clang) | LLVM/Clang and component terms |
| [directxmath](https://github.com/microsoft/DirectXMath) | 2026-06-12 | Native libraries & build tools; DirectXMath SIMD C++ math library | MIT |
| [directxtex](https://github.com/microsoft/DirectXTex) | 2026-05-07 | Native libraries & build tools; DirectXTex texture processing library | MIT |
| [ffmpeg](https://github.com/FFmpeg/FFmpeg) | 9.0 | Native libraries & build tools; A library to decode, encode, transcode, mux, demux, stream, filter and play pretty much anything that humans and machines have created. | LGPL-2.1-or-later; component/build-dependent |
| [ffmpeg-bin2c](https://github.com/FFmpeg/FFmpeg) | 9.0 | Native libraries & build tools; Host tool bin2c for FFmpeg build | See FFmpeg terms |
| [pkgconf](https://github.com/pkgconf/pkgconf) | 3.0.3 | Native libraries & build tools; pkgconf is a program which helps to configure compiler and linker flags for development libraries. It is similar to pkg-config from freedesktop.org. | ISC |
| [sdl2](https://github.com/libsdl-org/SDL) | 2.32.10 | Native libraries & build tools; Simple DirectMedia Layer is a cross-platform development library designed to provide low level access to audio, keyboard, mouse, joystick, and graphics hardware via OpenGL and Direct3D. | Zlib |
| [sdl2-net](https://github.com/libsdl-org/SDL_net) | 2.2.0 | Native libraries & build tools; Networking library for SDL | Zlib |
| [vcpkg-cmake](https://github.com/microsoft/vcpkg) | 2024-04-23 | Native libraries & build tools; Native dependency/build helper used by the vcpkg build. | MIT; see vcpkg notices |
| [vcpkg-cmake-config](https://github.com/microsoft/vcpkg) | 2026-07-21 | Native libraries & build tools; Native dependency/build helper used by the vcpkg build. | MIT; see vcpkg notices |
| [vcpkg-cmake-get-vars](https://github.com/microsoft/vcpkg) | 2025-05-29 | Native libraries & build tools; Native dependency/build helper used by the vcpkg build. | MIT; see vcpkg notices |
| [vcpkg-pkgconfig-get-modules](https://github.com/microsoft/vcpkg) | 2024-04-03 | Native libraries & build tools; Native dependency/build helper used by the vcpkg build. | MIT; see vcpkg notices |
| [vcpkg-tool-meson](https://github.com/mesonbuild/meson) | 1.9.0 | Native libraries & build tools; Meson build system | Apache-2.0 |
| [dragon4](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | MIT |
| [GCC runtime library](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | GPL-3.0-or-later WITH GCC-exception-3.1 |
| [LAPACK](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | BSD-3-Clause-Open-MPI |
| [lapack-lite](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | BSD-3-Clause |
| [libdivide](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | Zlib |
| [Meson](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | Apache 2.0 |
| [spin](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | BSD-3 |
| [tempita](https://github.com/numpy/numpy) | NumPy notices | NumPy · bundled/source components; Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs. | MIT |
| [brotli](https://github.com/google/brotli) | 1.2.1 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [freetype](https://github.com/freetype/freetype) | 2.14.3 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [harfbuzz](https://github.com/harfbuzz/harfbuzz) | 14.5.0 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [lcms2](https://github.com/mm2/Little-CMS) | 2.19.1 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [libavif](https://github.com/AOMediaCodec/libavif) | 1.4.2 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) | 3.2.0 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [libpng](https://github.com/pnggroup/libpng) | 1.6.58 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [libwebp](https://github.com/webmproject/libwebp) | 1.6.0 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [openjpeg](https://github.com/uclouvain/openjpeg) | 2.5.4 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [tiff](https://gitlab.com/libtiff/libtiff) | 4.7.2 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [xz](https://github.com/tukaani-project/xz) | 5.8.4 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [zlib-ng](https://github.com/zlib-ng/zlib-ng) | 2.3.3 | Pillow · bundled components; Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build. | See component terms in Pillow notices |
| [Microsoft platform runtimes](https://developer.microsoft.com/microsoft-edge/webview2/) | Windows prerequisites | Platform prerequisites; Direct3D/Windows SDK, Visual C++ runtime and Edge WebView2 support. These are platform/vendor prerequisites, not claimed as open-source packages. | Applicable Microsoft SDK/runtime terms |
| [bzip2](https://github.com/libarchive/bzip2) | Bundled runtime | Runtime & infrastructure; Python compression support. | bzip2 license |
| [Caddy](https://github.com/caddyserver/caddy) | Server deployment | Runtime & infrastructure; Website HTTPS and reverse proxy. | Apache-2.0 |
| [CMake](https://github.com/Kitware/CMake) | Build tool | Runtime & infrastructure; Native build configuration. | BSD-3-Clause and included notices |
| [CPython](https://github.com/python/cpython) | 3.12.14 | Runtime & infrastructure; Python runtime and standard library for the installer and utilities. | PSF-2.0 and bundled third-party notices |
| [Expat](https://github.com/libexpat/libexpat) | Bundled runtime | Runtime & infrastructure; Python XML parsing support. | MIT |
| [libffi](https://github.com/libffi/libffi) | 8 ABI | Runtime & infrastructure; Foreign-function interface in the Python runtime. | MIT |
| [libmpdec](https://www.bytereef.org/mpdecimal/) | Bundled runtime | Runtime & infrastructure; Python decimal arithmetic. | BSD-2-Clause |
| [Node.js / npm](https://github.com/nodejs/node) | Build tools | Runtime & infrastructure; Builds editor and website assets. | MIT and included third-party terms |
| [OpenBLAS](https://github.com/OpenMathLib/OpenBLAS) | Bundled NumPy wheel | Runtime & infrastructure; Linear algebra runtime supplied with NumPy. | BSD-3-Clause and included notices |
| [OpenSSL](https://github.com/openssl/openssl) | 3.x | Runtime & infrastructure; HTTPS/TLS in the frozen Python runtime. | Apache-2.0 |
| [Rust / Cargo](https://github.com/rust-lang/rust) | Build tools | Runtime & infrastructure; Compiles the embedded editor and parser. | MIT OR Apache-2.0 and component terms |
| [SQLite](https://github.com/sqlite/sqlite) | Bundled runtime | Runtime & infrastructure; Python runtime database library. | Public domain dedication |
| [Tcl / Tk](https://github.com/tcltk/tk) | 8.6 | Runtime & infrastructure; Launcher and installer windows. | Tcl/Tk license terms |
| [vcpkg](https://github.com/microsoft/vcpkg) | Build tool | Runtime & infrastructure; Native dependency resolution and build recipes. | MIT |
| [websockets](https://github.com/python-websockets/websockets) | 17.3 | Runtime & infrastructure; Dedicated multiplayer server transport. | BSD-3-Clause |
| [Wine](https://github.com/wine-mirror/wine) | Server deployment | Runtime & infrastructure; Runs the authoritative Windows simulation worker on the Linux server. | LGPL-2.1-or-later and component terms |
| [XZ / liblzma](https://github.com/tukaani-project/xz) | Bundled runtime | Runtime & infrastructure; Python LZMA compression support. | See component license notices |
| [zlib](https://github.com/madler/zlib) | Bundled runtime | Runtime & infrastructure; Compression in Python and image processing. | Zlib |
| [@esbuild/aix-ppc64](https://www.npmjs.com/package/@esbuild/aix-ppc64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/android-arm](https://www.npmjs.com/package/@esbuild/android-arm/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/android-arm64](https://www.npmjs.com/package/@esbuild/android-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/android-x64](https://www.npmjs.com/package/@esbuild/android-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/darwin-arm64](https://www.npmjs.com/package/@esbuild/darwin-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/darwin-x64](https://www.npmjs.com/package/@esbuild/darwin-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/freebsd-arm64](https://www.npmjs.com/package/@esbuild/freebsd-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/freebsd-x64](https://www.npmjs.com/package/@esbuild/freebsd-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-arm](https://www.npmjs.com/package/@esbuild/linux-arm/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-arm64](https://www.npmjs.com/package/@esbuild/linux-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-ia32](https://www.npmjs.com/package/@esbuild/linux-ia32/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-loong64](https://www.npmjs.com/package/@esbuild/linux-loong64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-mips64el](https://www.npmjs.com/package/@esbuild/linux-mips64el/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-ppc64](https://www.npmjs.com/package/@esbuild/linux-ppc64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-riscv64](https://www.npmjs.com/package/@esbuild/linux-riscv64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-s390x](https://www.npmjs.com/package/@esbuild/linux-s390x/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/linux-x64](https://www.npmjs.com/package/@esbuild/linux-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/netbsd-arm64](https://www.npmjs.com/package/@esbuild/netbsd-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/netbsd-x64](https://www.npmjs.com/package/@esbuild/netbsd-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/openbsd-arm64](https://www.npmjs.com/package/@esbuild/openbsd-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/openbsd-x64](https://www.npmjs.com/package/@esbuild/openbsd-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/openharmony-arm64](https://www.npmjs.com/package/@esbuild/openharmony-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/sunos-x64](https://www.npmjs.com/package/@esbuild/sunos-x64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/win32-arm64](https://www.npmjs.com/package/@esbuild/win32-arm64/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/win32-ia32](https://www.npmjs.com/package/@esbuild/win32-ia32/v/0.28.2) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@esbuild/win32-x64](https://github.com/evanw/esbuild) | 0.28.2 | Website · JavaScript; Build/development dependency; optional/platform-specific locked package | MIT |
| [@types/trusted-types](https://github.com/DefinitelyTyped/DefinitelyTyped) | 2.0.7 | Website · JavaScript; Bundled interface dependency; optional/platform-specific locked package | MIT |
| [dompurify](https://github.com/cure53/DOMPurify) | 3.4.17 | Website · JavaScript; Bundled interface dependency | (MPL-2.0 OR Apache-2.0) |
| [esbuild](https://github.com/evanw/esbuild) | 0.28.2 | Website · JavaScript; Build/development dependency | MIT |
| [marked](https://github.com/markedjs/marked) | 18.2.0 | Website · JavaScript; Bundled interface dependency | MIT |

## Keeping this current

After dependency changes, regenerate the Windows Cargo metadata and run `tools/build_contributors_catalog.py` with the website and installed-game paths. Run the catalog coverage tests before publishing the website. Never infer a license where upstream does not provide one. The OpenMW cloud provenance currently records unresolved permission; this catalog preserves that status.

Original game-source terms: [LICENSE.txt](../LICENSE.txt). Supplied image provenance: [ART_PROVENANCE.md](../assets/UI/ART_PROVENANCE.md).
