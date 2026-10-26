#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

namespace hwmodern {

// Exact-quality stage: every material referenced by the current scene or an
// in-flight frame remains fully mapped. Hardware feedback only selects which
// whole mips to retain after a material becomes inactive. Reactivation maps and
// uploads every missing mip before DispatchRays. No sampling clamps are used.
class SamplerFeedbackContext {
public:
    bool initialize(ID3D12Device* device, ID3D12CommandQueue* directQueue,
                    unsigned descriptorCount);
    bool supported() const { return device_ != nullptr; }
    D3D12_SAMPLER_FEEDBACK_TIER tier() const { return tier_; }
    ID3D12Device8* device() const { return device_.Get(); }
    ID3D12CommandQueue* queue() const { return queue_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE clearHandle(unsigned index) const;
    // Call once for the added GPU-visible feedback descriptor range. Null
    // feedback writes are specified no-ops, including committed fallback maps.
    void createNullViews(D3D12_CPU_DESCRIPTOR_HANDLE start, unsigned stride,
                         unsigned count) const;
    // Poll only; production never waits. Heaps retire behind a queue fence,
    // because UpdateTileMappings is asynchronous even after all draws finish.
    void collectRetiredHeaps();
    std::uint64_t pendingRetiredBytes() const;
    // All context/resources must outlive their last submitted GPU use.
    // Destroy the context only at the presenter's existing drained lifecycle.
    bool retireHeaps(std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>> heaps);
    bool retireResources(std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resources);
private:
    struct Retirement {
        std::uint64_t fence = 0;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>> heaps;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resources;
    };
    Microsoft::WRL::ComPtr<ID3D12Device8> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> retirementFence_;
    Microsoft::WRL::ComPtr<ID3D12Resource> nullFeedback_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> clearDescriptors_;
    unsigned descriptorCount_ = 0, descriptorStride_ = 0;
    std::uint64_t nextRetirementFence_ = 1;
    D3D12_SAMPLER_FEEDBACK_TIER tier_ = D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED;
    std::vector<Retirement> retired_;
};

struct SamplerFeedbackMipRange { unsigned firstMip = 0, mipCount = 0; };

class SamplerFeedbackTexture {
public:
    static constexpr unsigned feedbackSlots = 3;
    // width/height/format/array size/mips retain the exact production material
    // description. Currently accepts only the four-layer RGBA8 material format.
    // Returns false without mapping anything, so the caller may use its normal
    // committed-resource path. Do not overwrite a still-in-flight descriptor.
    bool create(SamplerFeedbackContext& context, const D3D12_RESOURCE_DESC& desc,
                unsigned descriptorIndex, D3D12_CPU_DESCRIPTOR_HANDLE gpuView);
    ID3D12Resource* resource() const { return texture_.Get(); }
    bool fullyMapped() const;
    bool readyForSampling() const;
    std::uint64_t mappedBytes() const;
    std::uint64_t residentBytes() const { return mappedBytes() + auxiliaryBytes_; }
    unsigned packedFirstMip() const { return packedFirstMip_; }
    unsigned finestWarmMip() const { return feedbackKnown_ ? feedbackMinMip_ : 0; }
    // Allocates all missing physical heaps before issuing any mapping calls.
    // Caller then uploads the returned full mip ranges on this same queue,
    // retaining upload buffers through its normal frame fence. Add an aliasing
    // barrier before first access to a newly mapped range, then CopyTextureRegion
    // under COPY_DEST and restore NON_PIXEL_SHADER_RESOURCE.
    bool mapAll(SamplerFeedbackContext& context,
                std::vector<SamplerFeedbackMipRange>& newlyMapped);
    // Call only after all copies for these ranges have been recorded. Failed
    // upload preparation leaves ranges pending, so a retry cannot mistake a
    // physically mapped, uninitialized mip for usable content. If the command
    // list is abandoned after this call, retain/abandon the whole texture with
    // that frame; do not reuse it as an uploaded texture.
    void uploadsRecorded(const std::vector<SamplerFeedbackMipRange>& ranges);
    // noCurrentOrInFlightReferences must be the presenter's existing material
    // residency pin check, not a guessed frame delay. Never unmaps packed tails.
    std::uint64_t trimInactive(SamplerFeedbackContext& context,
                              bool noCurrentOrInFlightReferences);
    // begin/end are recorded around DispatchRays only on duty-cycled capture
    // frames. prepareFeedback allocates its readback before command recording;
    // a slot cannot be reused before consumeFeedback completes its fence.
    bool prepareFeedback(SamplerFeedbackContext& context, unsigned slot);
    bool beginFeedback(ID3D12GraphicsCommandList* commands,
                       D3D12_GPU_DESCRIPTOR_HANDLE gpuView, unsigned slot);
    bool endFeedback(ID3D12GraphicsCommandList1* commands, unsigned slot);
    void feedbackSubmitted(unsigned slot, std::uint64_t frameFence);
    // frameFence may instead be a monotonically increasing frame token IF the
    // caller invokes consume only after the presenter's actual fence for that
    // exact frame slot has completed. A guessed age/slot index is not proof.
    bool consumeFeedback(unsigned slot, std::uint64_t completedFrameFence);
    bool feedbackPending(unsigned slot) const;
    // For abandonment before any dispatch, or after all material use fences
    // complete. Retains every resource/heap behind a future queue fence, so
    // queued tile mappings cannot outlive their objects. Not an in-flight evict.
    void releaseInactive(SamplerFeedbackContext& context);
    unsigned feedbackWidth() const { return feedbackWidth_; }
    unsigned feedbackHeight() const { return feedbackHeight_; }
private:
    struct MipHeap {
        unsigned firstMip = 0, mipCount = 0, tilesPerSlice = 0;
        D3D12_TILE_REGION_SIZE region = {};
        Microsoft::WRL::ComPtr<ID3D12Heap> heap;
        bool uploaded = false;
    };
    struct Readback {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        std::uint64_t fence = 0;
        bool recording = false, pending = false;
    };
    void mapGroup(SamplerFeedbackContext& context, const MipHeap& group,
                  ID3D12Heap* heap, D3D12_TILE_RANGE_FLAGS flags) const;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_, feedback_, decoded_;
    D3D12_CPU_DESCRIPTOR_HANDLE clearView_ = {};
    D3D12_RESOURCE_DESC desc_ = {};
    std::vector<MipHeap> groups_;
    Readback readbacks_[feedbackSlots];
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT feedbackLayouts_[4] = {};
    std::uint64_t feedbackReadbackBytes_ = 0;
    std::uint64_t auxiliaryBytes_ = 0;
    unsigned feedbackWidth_ = 0, feedbackHeight_ = 0;
    unsigned packedFirstMip_ = 0, feedbackMinMip_ = 0;
    bool feedbackKnown_ = false;
};

} // namespace hwmodern
