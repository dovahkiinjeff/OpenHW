#include "MaterialTextureResidency.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <random>
#include <utility>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
using Policy = hwmodern::MaterialTextureResidency<3>;

static void fenceAll(Policy &policy)
{
    policy.beginFencedSlot(0);
    policy.beginFencedSlot(1);
    policy.beginFencedSlot(2);
}

int main()
{
    constexpr std::uint64_t MiB = 1024ull * 1024;
    CHECK(hwmodern::materialRetentionTarget(1024 * MiB, 12ull << 30, 16ull << 30, true) == 8ull << 30);
    CHECK(hwmodern::materialRetentionTarget(1024 * MiB, 1400 * MiB, 16ull << 30, true) == 1400 * MiB);
    CHECK(hwmodern::materialRetentionTarget(1024 * MiB, 12ull << 30, 0, true) == 1536 * MiB);
    CHECK(hwmodern::materialRetentionTarget(UINT64_MAX, UINT64_MAX, 0, true) == UINT64_MAX);
    CHECK(hwmodern::materialRetentionTarget(0, 123, 0, false) == 123);
    // Alternate distant camera views larger than the old 512 MiB warm cache.
    // With headroom, return sweeps keep exact textures and need no reupload.
    Policy sweep;
    sweep.resize(16);
    unsigned int uploads = 0;
    for (unsigned int frame = 0; frame < 60; ++frame) {
        sweep.beginFencedSlot(frame % 3);
        const auto first = (frame % 2) * 8;
        for (unsigned int i = first; i < first + 8; ++i) {
            sweep.require(i);
            if (!sweep.isResident(i)) { sweep.resident(i, 128 * MiB); ++uploads; }
        }
        const auto target = hwmodern::materialRetentionTarget(sweep.requiredBytes(), 12ull << 30, 16ull << 30, true);
        for (auto i : sweep.evictionCandidates(target)) CHECK(sweep.evict(i));
    }
    CHECK(uploads == 16 && sweep.residentBytes() == 2048 * MiB);
    fenceAll(sweep);
    for (unsigned int i = 0; i < 8; ++i) sweep.require(i);
    const auto pressured = hwmodern::materialRetentionTarget(sweep.requiredBytes(), 1024 * MiB, 16ull << 30, true);
    for (auto i : sweep.evictionCandidates(pressured)) CHECK(sweep.evict(i));
    CHECK(sweep.residentBytes() == 1024 * MiB && sweep.requiredBytes() == 1024 * MiB);

    std::vector<unsigned int> references;
    hwmodern::appendMaterialTextureReferences(references, false, 0, true, 1, 1000);
    hwmodern::appendMaterialTextureReferences(references, true, 1000, true, 1, 1000);
    CHECK(references.empty());
    hwmodern::appendMaterialTextureReferences(references, true, 4, false, 0, 1000);
    hwmodern::appendMaterialTextureReferences(references, true, 9, true, 3, 1000);
    hwmodern::appendMaterialTextureReferences(references, true, 9, true, 3, 1000);
    std::sort(references.begin(), references.end());
    references.erase(std::unique(references.begin(), references.end()), references.end());
    CHECK((references == std::vector<unsigned int>{3, 4, 9}));

    Policy policy;
    policy.resize(1000); // Registered fleet assets, most never rendered.
    std::vector<std::unique_ptr<unsigned int>> resources(1000);
    policy.beginFencedSlot(0);
    for (const unsigned int index : references)
    {
        CHECK(policy.require(index));
        CHECK(!policy.require(index));
        resources[index] = std::make_unique<unsigned int>(index);
        policy.resident(index, 100);
    }
    CHECK(policy.residentCount() == 3 && policy.residentBytes() == 300);
    CHECK(!resources[0] && !resources[999]); // Stable holes, no eager uploads.
    CHECK(policy.evictionCandidates(0).empty()); // Required now.
    policy.beginFencedSlot(1);
    CHECK(!policy.canEvict(3)); // Frame slot 0 has not completed.
    policy.beginFencedSlot(2);
    CHECK(!policy.canEvict(3));
    CHECK(!policy.evict(3)); // A cache limit cannot override a GPU lifetime.
    policy.beginFencedSlot(0); // Frame slot 0's old submission finally retired.
    CHECK(policy.canEvict(3));
    const auto evicted = policy.evictionCandidates(150);
    CHECK((evicted == std::vector<std::size_t>{3, 4}));
    for (const auto index : evicted)
    {
        CHECK(policy.evict(index));
        resources[index].reset(); // Descriptor is deliberately not rewritten.
    }
    CHECK(policy.residentBytes() == 100 && resources[9] && !resources[3]);
    CHECK(!policy.isResident(3));
    CHECK(policy.require(3));
    resources[3] = std::make_unique<unsigned int>(3003); // Same descriptor index.
    policy.resident(3, 100);
    CHECK(*resources[3] == 3003 && policy.isResident(3));

    // A texture sampled by all flight slots cannot be replaced when only
    // one slot's fence has completed. It must outlive all three submissions.
    Policy shared;
    shared.resize(1);
    for (unsigned int slot = 0; slot < 3; ++slot)
    {
        shared.beginFencedSlot(slot);
        shared.require(0);
        if (!slot) shared.resident(0, 8192);
    }
    shared.beginFencedSlot(0);
    CHECK(!shared.canEvict(0));
    shared.beginFencedSlot(1);
    CHECK(!shared.canEvict(0));
    shared.beginFencedSlot(2);
    CHECK(shared.canEvict(0));
    shared.require(0); // Reused before trim: keep the exact old resource.
    CHECK(shared.evictionCandidates(0).empty());

    // Repeated use of one back buffer is safe and does not reupload every
    // frame. Once a menu stops referencing it, the grace period expires.
    Policy repeated;
    repeated.resize(1);
    repeated.beginFencedSlot(0);
    repeated.require(0);
    repeated.resident(0, 1024);
    for (int frame = 0; frame < 12; ++frame)
    {
        repeated.beginFencedSlot(0);
        CHECK(!repeated.canEvict(0));
        repeated.require(0);
    }
    repeated.beginFencedSlot(0);
    CHECK(!repeated.canEvict(0));
    repeated.beginFencedSlot(0);
    CHECK(repeated.canEvict(0));

    // Least recently used cold entries leave first. Current active working
    // sets remain complete even when they exceed the optional cache budget.
    Policy lru;
    lru.resize(3);
    lru.beginFencedSlot(0);
    lru.require(2); lru.resident(2, 200);
    lru.beginFencedSlot(1);
    lru.require(0); lru.resident(0, 200);
    fenceAll(lru);
    lru.require(1); lru.resident(1, 10000);
    auto cold = lru.evictionCandidates(10000);
    CHECK((cold == std::vector<std::size_t>{2, 0}));
    for (const auto index : cold) CHECK(lru.evict(index));
    CHECK(lru.requiredBytes() == 10000 && lru.residentBytes() == 10000);
    CHECK(lru.evictionCandidates(0).empty());
    lru.resize(20);
    CHECK(lru.isResident(1) && lru.residentCount() == 1);
    lru.reset();
    CHECK(lru.residentCount() == 0 && lru.residentBytes() == 0);

    // Independently model submitted descriptor generations. Fences may retire
    // in any back-buffer-slot order; cache eviction must never change or
    // destroy a descriptor generation captured by an unfinished submission.
    Policy randomized;
    constexpr unsigned int records = 160;
    constexpr std::uint64_t resourceBytes = 65536;
    randomized.resize(records);
    std::array<unsigned int, records> generations{}, generationSerials{};
    std::array<std::vector<std::pair<unsigned int, unsigned int>>, 3> pending;
    std::mt19937 random(712659);
    for (unsigned int frame = 0; frame < 4000; ++frame)
    {
        const unsigned int slot = random() % 3;
        pending[slot].clear(); // Actual fence completion for this slot only.
        randomized.beginFencedSlot(slot);
        const unsigned int needed = 1 + random() % 7;
        for (unsigned int n = 0; n < needed; ++n)
        {
            const unsigned int index = random() % records;
            if (!randomized.require(index)) continue;
            if (!generations[index])
            {
                for (const auto &flight : pending)
                    for (const auto &reference : flight) CHECK(reference.first != index);
                generations[index] = ++generationSerials[index];
                randomized.resident(index, resourceBytes);
            }
            pending[slot].push_back({index, generations[index]});
        }
        const auto target = randomized.requiredBytes() + 2 * resourceBytes;
        for (const auto index : randomized.evictionCandidates(target))
        {
            for (const auto &flight : pending)
                for (const auto &reference : flight) CHECK(reference.first != index);
            CHECK(randomized.evict(index));
            generations[index] = 0;
        }
        for (const auto &flight : pending)
            for (const auto &reference : flight)
                CHECK(generations[reference.first] == reference.second);
    }
    fenceAll(randomized);
    for (const auto index : randomized.evictionCandidates(0)) CHECK(randomized.evict(index));
    CHECK(randomized.residentBytes() == 0);

    std::puts("PASS: active-only references including idle blends, stable descriptor holes, all-flight-slot fence lifetime, no early descriptor reuse, repeated slots, bounded LRU, pinned active working sets and 4000 randomized descriptor-generation timelines");
}
