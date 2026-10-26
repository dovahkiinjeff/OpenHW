#include "PagedArray.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    std::abort(); } } while (0)

struct Texel
{
    std::uint32_t surface, emission, normal, material;
};
static_assert(sizeof(Texel) == 16, "Test the renderer's actual texel stride");

static Texel texelAt(std::size_t index)
{
    const auto value = static_cast<std::uint32_t>(index);
    return {value ^ 0xff003315u, value * 2654435761u,
            value ^ 0xffff8080u, value + 0x0000ffffu};
}

struct Tracked
{
    static unsigned int copies, moves, assignments;
    int value = 0;
    Tracked() = default;
    Tracked(const Tracked &other) : value(other.value) { ++copies; }
    Tracked(Tracked &&other) noexcept : value(other.value) { ++moves; }
    Tracked &operator=(const Tracked &other)
    { value = other.value; ++assignments; return *this; }
    Tracked &operator=(Tracked &&other) noexcept
    { value = other.value; ++assignments; return *this; }
};
unsigned int Tracked::copies = 0, Tracked::moves = 0, Tracked::assignments = 0;

struct Throwing
{
    static bool fail;
    int value = 0;
    Throwing &operator=(const Throwing &other)
    {
        if (fail) throw std::runtime_error("intentional assignment failure");
        value = other.value;
        return *this;
    }
};
bool Throwing::fail = false;

int main()
{
    constexpr std::size_t pageElements = 65536;
    hwmodern::PagedArray<Texel> texels(1);
    const Texel zero = {};
    CHECK(texels.size() == 1);
    CHECK(std::memcmp(&texels[0], &zero, sizeof(zero)) == 0);
    CHECK(texels.capacity() == pageElements);
    const Texel *sentinel = &texels[0];
    std::vector<std::size_t> recordOffsets;
    std::vector<const Texel *> recordAddresses;
    // The irregular records straddle multiple page boundaries, just as maps
    // with differently sized material textures do in the renderer.
    const std::size_t recordSizes[] = {17, pageElements - 7,
                                     pageElements * 2 + 3, pageElements + 1};
    for (const std::size_t count : recordSizes)
    {
        const std::size_t offset = texels.size();
        recordOffsets.push_back(offset);
        for (std::size_t i = 0; i < count; ++i)
        {
            const Texel expected = texelAt(offset + i);
            texels.push_back(expected);
        }
        recordAddresses.push_back(&texels[offset]);
        CHECK(&texels[0] == sentinel);
        CHECK(texels.capacity() >= texels.size());
        CHECK(texels.capacity() - texels.size() < pageElements);
    }
    const auto &readOnly = texels;
    CHECK(readOnly.span(readOnly.size(), 1).size == 0);
    CHECK(readOnly.span(0, 0).data == nullptr);
    CHECK(readOnly.span(pageElements - 1, 9).size == 1);
    CHECK(readOnly.span(pageElements, 9).size == 9);
    for (std::size_t record = 0; record < recordOffsets.size(); ++record)
    {
        const std::size_t offset = recordOffsets[record];
        CHECK(&readOnly[offset] == recordAddresses[record]);
        for (std::size_t i = 0; i < recordSizes[record]; ++i)
        {
            const Texel expected = texelAt(offset + i);
            CHECK(std::memcmp(&readOnly[offset + i], &expected, sizeof(Texel)) == 0);
        }
        for (std::size_t i = 0; i < recordSizes[record];)
        {
            const auto span = readOnly.span(offset + i, recordSizes[record] - i);
            CHECK(span.size > 0 && span.size <= recordSizes[record] - i);
            for (std::size_t pixel = 0; pixel < span.size; ++pixel)
            {
                const Texel expected = texelAt(offset + i + pixel);
                CHECK(std::memcmp(&span.data[pixel], &expected, sizeof(Texel)) == 0);
            }
            i += span.size;
        }
    }
    // Copy an existing record while allocating a new page; aliasing remains
    // valid because growing the table never relocates prior pages.
    while (texels.size() < texels.capacity()) texels.push_back(zero);
    const Texel source = texels[recordOffsets[0]];
    texels.push_back(texels[recordOffsets[0]]);
    CHECK(std::memcmp(&texels[texels.size() - 1], &source, sizeof(source)) == 0);

    hwmodern::PagedArray<Tracked, 4> tracked;
    Tracked value;
    std::vector<const Tracked *> addresses;
    for (int i = 0; i < 41; ++i)
    {
        value.value = i;
        tracked.push_back(value);
        addresses.push_back(&tracked[tracked.size() - 1]);
    }
    CHECK(Tracked::copies == 0 && Tracked::moves == 0);
    CHECK(Tracked::assignments == 41);
    for (std::size_t i = 0; i < tracked.size(); ++i)
    {
        CHECK(tracked[i].value == static_cast<int>(i));
        CHECK(&tracked[i] == addresses[i]);
    }
    tracked.clear();
    CHECK(tracked.size() == 0 && tracked.capacity() == 0);
    value.value = 123;
    tracked.push_back(value);
    CHECK(tracked.size() == 1 && tracked[0].value == 123);

    hwmodern::PagedArray<Throwing, 2> throws;
    Throwing item;
    item.value = 71;
    throws.push_back(item);
    throws.push_back(item);
    Throwing::fail = true;
    bool caught = false;
    try { throws.push_back(item); } catch (const std::runtime_error &) { caught = true; }
    CHECK(caught && throws.size() == 2 && throws[0].value == 71 && throws[1].value == 71);
    Throwing::fail = false;
    throws.push_back(item);
    CHECK(throws.size() == 3 && throws[2].value == 71);

    bool overflowRejected = false;
    try
    {
        // Validation must happen before attempting any impossible allocation.
        hwmodern::PagedArray<Texel> excessive(texels.max_size() + 1);
    }
    catch (const std::length_error &) { overflowRejected = true; }
    CHECK(overflowRejected);
    texels.clear();
    CHECK(texels.size() == 0 && texels.capacity() == 0);
    std::puts("PASS: exact 16-byte texels, stable offsets and addresses, bounded page tail, no relocation, clear/reuse, exception safety and overflow");
}
