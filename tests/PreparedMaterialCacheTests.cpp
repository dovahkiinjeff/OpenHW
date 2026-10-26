#include "PreparedMaterialLayout.h"
#include <DirectXTex.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
using hwmodern::PreparedMaterialCache;
using hwmodern::PreparedMaterialLayout;

static void roundTrip(unsigned int width, unsigned int height, bool partial)
{
    DirectX::ScratchImage base;
    CHECK(SUCCEEDED(base.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 4, 1)));
    for (size_t i = 0; i < base.GetPixelsSize(); ++i) base.GetPixels()[i] = static_cast<unsigned char>((i * 173 + i / 31) % 251);
    auto prepared = std::make_shared<DirectX::ScratchImage>();
    const auto mips = hwmodern::materialMipCount(width, height);
    if (mips == 1) *prepared = std::move(base);
    else if ((width & (width - 1)) || (height & (height - 1))) {
        // DirectX's existing non-WIC box filter only accepts power-of-two inputs.
        // Still verify storage/footprints for arbitrary complete mip chains.
        CHECK(SUCCEEDED(prepared->Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 4, mips)));
        for (size_t i = 0; i < prepared->GetPixelsSize(); ++i)
            prepared->GetPixels()[i] = static_cast<unsigned char>((i * 173 + i / 31) % 251);
    } else {
        const auto result = DirectX::GenerateMipMaps(base.GetImages(), base.GetImageCount(), base.GetMetadata(),
            DirectX::TEX_FILTER_FANT | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, *prepared);
        if (FAILED(result)) std::fprintf(stderr, "mip fixture %ux%u: 0x%08lx\n", width, height, result);
        CHECK(SUCCEEDED(result));
    }
    CHECK(prepared->GetMetadata().mipLevels == mips);
    const PreparedMaterialLayout layout(width, height);
    CHECK(layout.bytes == prepared->GetPixelsSize());
    const std::uint64_t key = (static_cast<std::uint64_t>(width) << 32) | height;
    PreparedMaterialCache cache;
    CHECK(cache.queue(7, key, prepared, prepared->GetPixels(), layout.bytes));
    cache.waitPending();
    CHECK(cache.stats().stores == 1);
    std::vector<PreparedMaterialCache::Range> ranges;
    std::vector<unsigned char> expected, restored;
    for (unsigned int channel = 0; channel < 4; ++channel)
    for (unsigned int mip = partial ? mips / 2 : 0; mip < mips; ++mip) {
        const auto* image = prepared->GetImage(mip, channel, 0);
        CHECK(image->pixels == prepared->GetPixels() + layout.offsets[channel * mips + mip]);
        const auto destination = (expected.size() + 511) & ~std::size_t(511);
        const auto pitch = (image->width * 4 + 255) & ~std::size_t(255);
        expected.resize(destination + pitch * image->height, 0xcd);
        for (size_t y = 0; y < image->height; ++y)
            std::memcpy(expected.data() + destination + y * pitch, image->pixels + y * image->rowPitch, image->width * 4);
        layout.appendReadRanges(ranges, channel * mips + mip, static_cast<unsigned int>(image->width),
            static_cast<unsigned int>(image->height), destination, pitch);
    }
    restored.resize(expected.size(), 0xcd);
    // Invalid metadata cannot change any upload byte; caller uses the original generation path.
    CHECK(!cache.read(7, key ^ 1, restored.data(), restored.size(), ranges));
    auto invalid = ranges;
    invalid.push_back({layout.bytes, 1, 0});
    CHECK(!cache.read(7, key, restored.data(), restored.size(), invalid));
    CHECK(std::all_of(restored.begin(), restored.end(), [](auto b) { return b == 0xcd; }));
    std::weak_ptr<DirectX::ScratchImage> lifetime = prepared;
    prepared.reset();
    CHECK(lifetime.expired()); // The cache has no permanently pinned CPU mip copy.
    CHECK(cache.read(7, key, restored.data(), restored.size(), ranges));
    CHECK(restored == expected); // Every channel/mip byte and all GPU padding compared.
    CHECK(cache.stats().hits == 1 && cache.stats().failures == 0);
    cache.clear();
    CHECK(!cache.read(7, key, restored.data(), restored.size(), ranges));
}

int main()
{
    for (auto dimensions : {std::pair<unsigned, unsigned>{1, 1}, {1, 256}, {256, 1}, {73, 139}, {256, 256}, {1024, 512}, {1024, 513}})
        for (bool partial : {false, true}) roundTrip(dimensions.first, dimensions.second, partial);

    auto bytes = std::make_shared<std::vector<unsigned char>>(4096, 0x71);
    PreparedMaterialCache bounded(8192, 4096);
    CHECK(!bounded.queue(1, 1, bytes, bytes->data(), 4097)); // Pending-memory limit.
    CHECK(bounded.queue(1, 1, bytes, bytes->data(), bytes->size()));
    bounded.waitPending();
    CHECK(bounded.queue(1, 1, bytes, bytes->data(), bytes->size())); // Duplicate has no new allocation.
    CHECK(bounded.queue(2, 2, bytes, bytes->data(), bytes->size()));
    bounded.waitPending();
    CHECK(!bounded.queue(3, 3, bytes, bytes->data(), bytes->size())); // Disk-capacity fallback.
    CHECK(bounded.storedBytes() == 8192 && bounded.stats().stores == 2);
    bounded.clear();
    CHECK(bounded.queue(3, 3, bytes, bytes->data(), bytes->size())); // A fresh renderer lifetime.
    bounded.waitPending();

    PreparedMaterialCache concurrent(128ull << 20, 64ull << 20);
    auto large = std::make_shared<std::vector<unsigned char>>(32ull << 20, 0x93);
    CHECK(concurrent.queue(0, 11, bytes, bytes->data(), bytes->size()));
    concurrent.waitPending();
    CHECK(concurrent.queue(1, 12, large, large->data(), large->size()));
    large.reset(); // The worker must retain its source until its last write completes.
    std::vector<unsigned char> restored(4096);
    for (int n = 0; n < 32; ++n) {
        CHECK(concurrent.read(0, 11, restored.data(), restored.size(), {{0, 4096, 0}}));
        CHECK(restored == *bytes); // Independent overlapped offsets while the writer appends.
    }
    concurrent.waitPending();
    CHECK(concurrent.read(1, 12, restored.data(), restored.size(), {{(32ull << 20) - 4096, 4096, 0}}));
    CHECK(std::all_of(restored.begin(), restored.end(), [](auto b) { return b == 0x93; }));
    CHECK(concurrent.stats().failures == 0);
    std::puts("PASS: exact DirectX filtered RGBA material mips, non-square/non-power-of-two sizes, partial restores, GPU padding, invalid metadata fallback, bounded CPU/disk storage, async source lifetime, concurrent reader/writer and renderer reset");
}
