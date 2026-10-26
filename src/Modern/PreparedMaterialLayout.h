#pragma once
#include "PreparedMaterialCache.h"

namespace hwmodern {
inline unsigned int materialMipCount(unsigned int width, unsigned int height)
{
    unsigned int count = 1;
    for (auto dimension = std::max(width, height); dimension > 1; dimension /= 2) ++count;
    return count;
}

struct PreparedMaterialLayout {
    std::vector<std::uint64_t> offsets;
    std::uint64_t bytes = 0;
    PreparedMaterialLayout(unsigned int width, unsigned int height)
    {
        const auto mips = materialMipCount(width, height);
        for (unsigned int channel = 0; channel < 4; ++channel)
        for (unsigned int mip = 0; mip < mips; ++mip) {
            offsets.push_back(bytes);
            bytes += std::max(1u, width >> mip) * static_cast<std::uint64_t>(std::max(1u, height >> mip)) * 4;
        }
    }
    void appendReadRanges(std::vector<PreparedMaterialCache::Range>& ranges,
        unsigned int subresource, unsigned int width, unsigned int height,
        std::uint64_t destination, std::uint64_t rowPitch) const
    {
        const auto widthBytes = width * 4ull;
        if (rowPitch == widthBytes) ranges.push_back({offsets.at(subresource), widthBytes * height, destination});
        else for (unsigned int y = 0; y < height; ++y)
            ranges.push_back({offsets.at(subresource) + y * widthBytes, widthBytes, destination + y * rowPitch});
    }
};
}
