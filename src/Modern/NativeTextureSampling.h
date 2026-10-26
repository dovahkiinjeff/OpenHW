#pragma once
#include <d3d12.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <cmath>

namespace hwmodern {
inline bool textureUsesMips(GLint filter) {
    return filter == GL_NEAREST_MIPMAP_NEAREST || filter == GL_LINEAR_MIPMAP_NEAREST ||
        filter == GL_NEAREST_MIPMAP_LINEAR || filter == GL_LINEAR_MIPMAP_LINEAR;
}
constexpr unsigned nativeTextureSamplerCount = 64;
inline unsigned nativeTextureSamplerIndex(GLint minFilter, GLint magFilter,
    GLint wrapS, GLint wrapT, float anisotropy) {
    const bool minLinear = minFilter == GL_LINEAR || minFilter == GL_LINEAR_MIPMAP_NEAREST ||
        minFilter == GL_LINEAR_MIPMAP_LINEAR;
    const bool magLinear = magFilter != GL_NEAREST;
    unsigned filter;
    if (!textureUsesMips(minFilter)) filter = 12u + (minLinear ? 2u : 0u) + (magLinear ? 1u : 0u);
    else if (minLinear && magLinear && std::isfinite(anisotropy) && anisotropy > 1.0f) {
        filter = anisotropy <= 2.0f ? 8u : anisotropy <= 4.0f ? 9u : anisotropy <= 8.0f ? 10u : 11u;
    } else {
        const bool mipLinear = minFilter == GL_NEAREST_MIPMAP_LINEAR || minFilter == GL_LINEAR_MIPMAP_LINEAR;
        filter = (minLinear ? 4u : 0u) | (magLinear ? 2u : 0u) | (mipLinear ? 1u : 0u);
    }
    return filter * 4u + (wrapS == GL_REPEAT ? 0u : 1u) + (wrapT == GL_REPEAT ? 0u : 2u);
}
inline D3D12_SAMPLER_DESC nativeTextureSamplerDescription(unsigned index) {
    D3D12_SAMPLER_DESC d = {};
    const unsigned filter = index / 4u;
    d.AddressU = (index & 1u) ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d.AddressV = (index & 2u) ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    d.MaxLOD = filter >= 12u ? 0.0f : D3D12_FLOAT32_MAX;
    d.MaxAnisotropy = 1;
    if (filter >= 8u && filter < 12u) {
        d.Filter = D3D12_FILTER_ANISOTROPIC;
        d.MaxAnisotropy = 2u << (filter - 8u);
    } else {
        const unsigned bits = filter >= 12u ? ((filter - 12u) << 1u) : filter;
        d.Filter = D3D12_ENCODE_BASIC_FILTER(
            (bits & 4u) ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
            (bits & 2u) ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
            (bits & 1u) ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
            D3D12_FILTER_REDUCTION_TYPE_STANDARD);
    }
    return d;
}
}
