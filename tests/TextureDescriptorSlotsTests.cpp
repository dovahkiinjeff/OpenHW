#include "TextureDescriptorSlots.h"
#include <cstdio>
#include <set>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main()
{
    TextureDescriptorSlots slots(64);
    std::set<unsigned int> persistent;
    for (int i = 0; i < 20; ++i) persistent.insert(slots.allocate());
    auto retired = slots.allocate();
    slots.retire(retired);
    slots.completeFrame(0); slots.completeFrame(1);
    CHECK(slots.allocate() != retired);
    slots.completeFrame(2);
    CHECK(slots.allocate() == retired);

    // More than a million monotonically increasing GL names, with only a
    // handful of live transient UI textures. None may alias ship descriptors.
    for (unsigned int frame = 0; frame < 300000; ++frame)
    {
        slots.completeFrame(frame % 3);
        std::set<unsigned int> current;
        for (int draw = 0; draw < 4; ++draw)
        {
            auto slot = slots.allocate();
            CHECK(slot != 0 && slot < 64);
            CHECK(persistent.count(slot) == 0);
            CHECK(current.insert(slot).second);
            slots.retire(slot);
        }
    }
    TextureDescriptorSlots full(2);
    CHECK(full.allocate() == 1);
    bool rejected = false;
    try { full.allocate(); } catch (const std::runtime_error &) { rejected = true; }
    CHECK(rejected); // Never silently clamp to another texture's descriptor.
    {
        TextureDescriptorSlots resized(8);
        const auto original = resized.allocate();
        const auto replacement = resized.replace(original);
        CHECK(original != replacement);
        resized.completeFrame(0); resized.completeFrame(0);
        resized.completeFrame(2);
        CHECK(resized.allocate() != original);
        resized.completeFrame(1);
        CHECK(resized.allocate() == original);
        // An exhausted replacement must not retire the original binding.
        TextureDescriptorSlots exhausted(2);
        const auto live = exhausted.allocate();
        rejected = false;
        try { exhausted.replace(live); } catch (const std::runtime_error &) { rejected = true; }
        CHECK(rejected);
        exhausted.completeFrame(0); exhausted.completeFrame(1); exhausted.completeFrame(2);
        rejected = false;
        try { exhausted.allocate(); } catch (const std::runtime_error &) { rejected = true; }
        CHECK(rejected);
    }
    std::puts("Texture descriptors: 1.2M transient allocations, no ship aliasing; fenced retirement and exhaustion passed.");
    return 0;
}
