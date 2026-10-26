#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace hwmodern
{
// Keep recently seen materials while real GPU headroom exists. A tiny fixed
// allowance causes a camera sweep to evict the view it is about to revisit.
inline std::uint64_t materialRetentionTarget(std::uint64_t required,
    std::uint64_t materialLimit, std::uint64_t adapterBudget, bool sceneKnown)
{
    if (!sceneKnown) return materialLimit;
    constexpr std::uint64_t minimumWarm = 512ull * 1024 * 1024;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto activeAndWarm = required > maximum - minimumWarm
        ? maximum : required + minimumWarm;
    // Half the adapter budget bounds optional retention. The separate live
    // materialLimit already subtracts other GPU users and reserved headroom.
    return std::min(materialLimit, std::max(activeAndWarm, adapterBudget / 2));
}

/* GPU-independent ownership policy. A slot bit is cleared only after the
   presenter's corresponding GPU fence completes. A descriptor/resource is
   replaceable only when no submitted frame can still sample its old value. */
template<unsigned int FlightSlots = 3>
class MaterialTextureResidency
{
    static_assert(FlightSlots > 0 && FlightSlots <= 32, "Invalid flight-slot count");
    struct Entry
    {
        std::uint64_t bytes = 0, lastUse = 0;
        std::uint32_t flightMask = 0;
        bool required = false;
    };
public:
    void resize(std::size_t count)
    {
        if (count < entries_.size())
            throw std::invalid_argument("Residency indices cannot shrink while resources exist");
        entries_.resize(count);
    }
    void reset() noexcept
    {
        entries_.clear();
        residentBytes_ = 0;
        serial_ = 0;
        slot_ = 0;
    }
    void beginFencedSlot(unsigned int slot)
    {
        if (slot >= FlightSlots) throw std::out_of_range("Invalid fenced frame slot");
        slot_ = slot;
        ++serial_;
        for (auto &entry : entries_)
        {
            entry.required = false;
            entry.flightMask &= ~(std::uint32_t(1) << slot);
        }
    }
    // True once for each distinct material in the current scene, regardless
    // of how many ship instances or triangles reference it.
    bool require(std::size_t index)
    {
        auto &entry = entries_.at(index);
        const bool first = !entry.required;
        entry.required = true;
        entry.lastUse = serial_;
        entry.flightMask |= std::uint32_t(1) << slot_;
        return first;
    }
    void resident(std::size_t index, std::uint64_t bytes)
    {
        auto &entry = entries_.at(index);
        const std::uint64_t otherBytes = residentBytes_ - entry.bytes;
        if (bytes > std::numeric_limits<std::uint64_t>::max() - otherBytes)
            throw std::overflow_error("Material residency byte count overflow");
        residentBytes_ = otherBytes + bytes;
        entry.bytes = bytes;
    }
    std::uint64_t residentBytes() const noexcept { return residentBytes_; }
    std::uint64_t requiredBytes() const noexcept
    {
        std::uint64_t total = 0;
        for (const auto &entry : entries_) if (entry.required) total += entry.bytes;
        return total;
    }
    std::size_t residentCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto &entry : entries_) if (entry.bytes) ++count;
        return count;
    }
    bool isResident(std::size_t index) const { return entries_.at(index).bytes != 0; }
    bool canEvict(std::size_t index) const
    {
        const auto &entry = entries_.at(index);
        // One frame of grace prevents beginning a reused back-buffer slot
        // from discarding yesterday's visible material before today's list
        // has been gathered, even if that slot happens to repeat.
        return entry.bytes && !entry.required && entry.flightMask == 0 &&
            serial_ - entry.lastUse > 1;
    }
    bool evict(std::size_t index)
    {
        if (!canEvict(index)) return false;
        auto &entry = entries_[index];
        residentBytes_ -= entry.bytes;
        entry.bytes = 0;
        return true;
    }
    std::vector<std::size_t> evictionCandidates(std::uint64_t targetBytes) const
    {
        std::vector<std::size_t> candidates;
        if (residentBytes_ <= targetBytes) return candidates;
        for (std::size_t i = 0; i < entries_.size(); ++i)
            if (canEvict(i)) candidates.push_back(i);
        std::sort(candidates.begin(), candidates.end(), [this](std::size_t a, std::size_t b)
        {
            if (entries_[a].lastUse != entries_[b].lastUse)
                return entries_[a].lastUse < entries_[b].lastUse;
            return a < b;
        });
        std::uint64_t remaining = residentBytes_;
        std::size_t retained = 0;
        while (retained < candidates.size() && remaining > targetBytes)
            remaining -= entries_[candidates[retained++]].bytes;
        candidates.resize(retained);
        return candidates;
    }
private:
    std::vector<Entry> entries_;
    std::uint64_t residentBytes_ = 0, serial_ = 0;
    unsigned int slot_ = 0;
};

// Missing/fallback materials never access the material descriptor array.
// Engine blends sample both indices, including at their endpoint values.
inline void appendMaterialTextureReferences(std::vector<unsigned int> &indices,
    bool textured, unsigned int active, bool blend, unsigned int idle,
    std::size_t recordCount)
{
    if (!textured || active >= recordCount) return;
    if (std::find(indices.begin(), indices.end(), active) == indices.end())
        indices.push_back(active);
    if (blend && idle < recordCount &&
        std::find(indices.begin(), indices.end(), idle) == indices.end())
        indices.push_back(idle);
}
}
