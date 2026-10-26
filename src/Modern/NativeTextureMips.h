#pragma once
#include <DirectXTex.h>
#include <cstddef>

namespace hwmodern {
// Generate once at upload, with the authored base level unchanged. Scratch
// storage is released after upload; do not retain another full CPU texture.
inline bool buildNativeTextureMips(const unsigned char *pixels, size_t bytes,
    unsigned width, unsigned height, DirectX::ScratchImage &result) {
    if (!pixels || !width || !height ||
        size_t(width) > SIZE_MAX / 4u / height || bytes < size_t(width) * height * 4u) return false;
    DirectX::Image base = {};
    base.width = width; base.height = height;
    base.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    base.rowPitch = size_t(width) * 4u; base.slicePitch = base.rowPitch * height;
    base.pixels = const_cast<unsigned char *>(pixels);
    if (width == 1 && height == 1) return SUCCEEDED(result.InitializeFromImage(base));
    // Use the native CPU filter so this upload does not depend on a COM/WIC
    // apartment having been initialized on the render thread.
    const bool powerOfTwo = (width & (width - 1u)) == 0u && (height & (height - 1u)) == 0u;
    const auto filter = powerOfTwo && width > 1 && height > 1
        ? DirectX::TEX_FILTER_BOX : DirectX::TEX_FILTER_LINEAR;
    return SUCCEEDED(DirectX::GenerateMipMaps(base,
        DirectX::TEX_FILTER_FLAGS(filter | DirectX::TEX_FILTER_FORCE_NON_WIC), 0, result));
}
}
