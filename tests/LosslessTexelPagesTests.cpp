#include "LosslessTexelPages.h"
#include <DirectXTex.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    std::abort(); } } while (0)

struct Texel { std::uint32_t words[4]; };
static_assert(sizeof(Texel) == 16, "Actual production texel stride");
static std::uint32_t randomWord(std::uint32_t &seed)
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}
static bool equal(const Texel &a, const Texel &b)
{ return std::memcmp(&a, &b, sizeof(a)) == 0; }

static void exactnessAndCache()
{
    constexpr std::size_t pageSize = 4096;
    hwmodern::LosslessTexelPages<Texel, pageSize, 2> pages(1);
    std::vector<Texel> reference(1);
    std::uint32_t seed = 1234567;
    for (std::size_t index = 1; index < pageSize * 9 + 17; ++index)
    {
        const Texel value = {{randomWord(seed), 0xff000000, 0xffff8080, 0x0000ffff}};
        reference.push_back(value); pages.push_back(value);
    }
    CHECK(pages.size() == reference.size());
    CHECK(pages.compressedPages() == 9);
    CHECK(pages.storageBytes() < reference.size() * sizeof(Texel));
    for (std::size_t index = 0; index < reference.size(); ++index)
        CHECK(equal(pages[index], reference[index]));
    for (unsigned int i = 0; i < 10000; ++i)
    {
        const std::size_t index = randomWord(seed) % reference.size();
        CHECK(equal(pages[index], reference[index]));
        CHECK(pages.cachedPages() <= 2);
    }
    const std::uint64_t before = pages.decompressions();
    // The production four-plane packer must walk each page only once.
    std::array<std::uint64_t, 4> sums = {};
    for (std::size_t offset = 0; offset < pages.size();)
    {
        const auto span = pages.span(offset, pages.size() - offset);
        CHECK(span.size > 0 && span.size <= pageSize);
        for (std::size_t i = 0; i < span.size; ++i)
            for (unsigned int channel = 0; channel < 4; ++channel)
                sums[channel] += span.data[i].words[channel];
        offset += span.size;
    }
    CHECK(pages.decompressions() - before <= 9);
    std::array<std::uint64_t, 4> expected = {};
    for (const auto &value : reference)
        for (unsigned int channel = 0; channel < 4; ++channel)
            expected[channel] += value.words[channel];
    CHECK(sums == expected);
    CHECK(pages.span(pages.size(), 1).data == nullptr);
    CHECK(pages.span(0, 0).size == 0);
    auto crossing = pages.span(pageSize - 1, 20);
    CHECK(crossing.size == 1 && equal(crossing.data[0], reference[pageSize - 1]));
    pages.clear();
    CHECK(pages.size() == 0 && pages.storageBytes() == 0 && pages.cachedPages() == 0);
    // Exercise a complete clear/repopulate cycle with existing codec handles.
    for (std::size_t index = 0; index < pageSize * 3; ++index)
        pages.push_back(reference[index]);
    // Self-alias while sealing the tail, and read compressed earlier pages.
    const Texel last = pages[pages.size() - 1];
    pages.push_back(pages[pages.size() - 1]);
    CHECK(equal(pages[pageSize * 3], last));
    CHECK(equal(pages[0], reference[0]));
    pages.clear();
    CHECK(pages.storageBytes() == 0);

    hwmodern::LosslessTexelPages<Texel, pageSize, 2> randomPages;
    reference.clear();
    for (std::size_t index = 0; index < pageSize * 4 + 1; ++index)
    {
        Texel value;
        for (auto &word : value.words) word = randomWord(seed);
        reference.push_back(value); randomPages.push_back(value);
    }
    for (std::size_t index = 0; index < reference.size(); ++index)
        CHECK(equal(reference[index], randomPages[index]));
    // Empty and excessive-size inputs cannot overflow page byte calculations.
    hwmodern::LosslessTexelPages<Texel> empty;
    CHECK(empty.span(0, 1).data == nullptr);
    bool overflowRejected = false;
    try { hwmodern::LosslessTexelPages<Texel> impossible(SIZE_MAX); }
    catch (const std::length_error &) { overflowRejected = true; }
    CHECK(overflowRejected);
}

static void realMaterial(const wchar_t *const *paths)
{
    std::array<DirectX::ScratchImage, 4> decoded;
    const DirectX::Image *images[4] = {};
    for (unsigned int channel = 0; channel < 4; ++channel)
    {
        DirectX::ScratchImage source;
        CHECK(SUCCEEDED(DirectX::LoadFromDDSFile(paths[channel], DirectX::DDS_FLAGS_NONE, nullptr, source)));
        const auto *image = source.GetImage(0, 0, 0);
        CHECK(image != nullptr);
        if (DirectX::IsCompressed(image->format))
            CHECK(SUCCEEDED(DirectX::Decompress(*image, DXGI_FORMAT_R8G8B8A8_UNORM, decoded[channel])));
        else if (image->format == DXGI_FORMAT_R8G8B8A8_UNORM)
            CHECK(SUCCEEDED(decoded[channel].InitializeFromImage(*image)));
        else
            CHECK(SUCCEEDED(DirectX::Convert(*image, DXGI_FORMAT_R8G8B8A8_UNORM,
                DirectX::TEX_FILTER_DEFAULT, 0, decoded[channel])));
        images[channel] = decoded[channel].GetImage(0, 0, 0);
        CHECK(images[channel]->width == images[0]->width && images[channel]->height == images[0]->height);
    }
    const auto started = std::chrono::steady_clock::now();
    hwmodern::LosslessTexelPages<Texel> pages;
    for (std::size_t y = 0; y < images[0]->height; ++y)
    for (std::size_t x = 0; x < images[0]->width; ++x)
    {
        Texel value;
        for (unsigned int channel = 0; channel < 4; ++channel)
            std::memcpy(&value.words[channel], images[channel]->pixels + y * images[channel]->rowPitch + x * 4, 4);
        pages.push_back(value);
    }
    const double appendMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    const auto readStarted = std::chrono::steady_clock::now();
    // Compare EVERY byte to the real decoded texture set, across cache eviction.
    for (std::size_t offset = 0; offset < pages.size();)
    {
        const auto span = pages.span(offset, pages.size() - offset);
        for (std::size_t i = 0; i < span.size; ++i)
        {
            const std::size_t pixel = offset + i;
            const std::size_t y = pixel / images[0]->width, x = pixel % images[0]->width;
            for (unsigned int channel = 0; channel < 4; ++channel)
                CHECK(std::memcmp(&span.data[i].words[channel],
                    images[channel]->pixels + y * images[channel]->rowPitch + x * 4, 4) == 0);
        }
        offset += span.size;
    }
    const double readMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStarted).count();
    std::printf("Real material %zux%zu: raw %.2f MiB, resident backing+cache %.2f MiB, compressed pages %zu; append %.2f ms, exact full read %.2f ms\n",
        images[0]->width, images[0]->height, pages.size() * 16.0 / 1048576.0,
        pages.storageBytes() / 1048576.0, pages.compressedPages(), appendMs, readMs);
}

int wmain(int argc, wchar_t **argv)
{
    exactnessAndCache();
    if (argc == 5) realMaterial(argv + 1);
    std::puts("Lossless material pages: exact bits, random access, cache bounds, raw fallback and reload passed.");
    return 0;
}
