#pragma once

#include <vector>
#include <stdexcept>
#include <cstddef>

// GL names grow for the lifetime of the process; GPU descriptor indices must
// instead follow live storage, and may only be reused after every flight slot
// that could reference the old SRV has retired. Slot zero is permanent white.
class TextureDescriptorSlots
{
    struct Retired { unsigned int slot, pendingFrames; };
    unsigned int capacity_, next_ = 1;
    std::vector<unsigned int> free_;
    std::vector<Retired> retired_;
public:
    explicit TextureDescriptorSlots(unsigned int capacity) : capacity_(capacity) {}
    unsigned int allocate()
    {
        if (!free_.empty())
        {
            auto slot = free_.back(); free_.pop_back(); return slot;
        }
        if (next_ >= capacity_)
            throw std::runtime_error("Native raster texture descriptor heap exhausted");
        return next_++;
    }
    void retire(unsigned int slot)
    {
        if (slot != 0) retired_.push_back({slot, 7u});
    }
    unsigned int replace(unsigned int slot)
    {
        // Allocate first: exhaustion must leave the live binding untouched.
        const auto replacement = allocate();
        retire(slot);
        return replacement;
    }
    void completeFrame(unsigned int frame)
    {
        for (std::size_t i = 0; i < retired_.size();)
        {
            retired_[i].pendingFrames &= ~(1u << frame);
            if (retired_[i].pendingFrames == 0)
            {
                free_.push_back(retired_[i].slot);
                retired_[i] = retired_.back(); retired_.pop_back();
            }
            else ++i;
        }
    }
};
