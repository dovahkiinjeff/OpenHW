#pragma once

#if defined(_WIN32)
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>

namespace hwmodern
{
namespace detail { struct GpuMemoryAllocatorState; }

enum class GpuMemoryInitialization
{
    // Preserve the old committed-resource zero-fill contract. Required for
    // histories/outputs that can be read before a complete clear or copy.
    PreserveZeroInitialization,
    // The caller guarantees all subsequently readable bytes/texels are
    // initialized by a copy, clear or GPU build before their first read.
    FullyWrittenBeforeRead
};

struct GpuMemoryAllocatorStatistics
{
    std::uint64_t allocatorId = 0;
    std::uint64_t allocationCount = 0, allocationBytes = 0;
    std::uint64_t blockCount = 0, blockBytes = 0;
    std::uint64_t localUsageBytes = 0, localBudgetBytes = 0;
    std::uint64_t placedResourcesCreated = 0, committedResourcesCreated = 0;
    std::uint64_t budgetFallbacks = 0;
};

struct GpuMemoryResourceInfo
{
    bool managed = false, placed = false;
    std::uint64_t allocatorId = 0;
    std::uint64_t allocationBytes = 0, heapOffset = 0, heapIdentity = 0;
};

/* Existing ComPtr<ID3D12Resource> ownership stays authoritative. Each created
   resource holds a private allocation lease; no separate allocation handle
   has to be copied into frame-retirement vectors. Releasing a caller's
   allocator wrapper is safe while its resources remain alive. It never waits
   for GPU work: existing frame fences must still protect resource retirement.
   Before first GPU use of a pooled DEFAULT resource, callers issue an ALIASING
   barrier (nullptr -> new resource), then completely initialize readable
   contents. Normal state-transition barriers remain the caller's responsibility. */
class GpuMemoryAllocator
{
public:
    HRESULT initialize(ID3D12Device *device, IDXGIAdapter *adapter = nullptr);
    void reset() noexcept { state_.reset(); }
    bool active() const noexcept { return !!state_; }

    HRESULT createResource(const D3D12_HEAP_PROPERTIES &heap,
        D3D12_HEAP_FLAGS heapFlags, const D3D12_RESOURCE_DESC &description,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE *optimizedClear,
        Microsoft::WRL::ComPtr<ID3D12Resource> &resource,
        GpuMemoryInitialization initialization =
            GpuMemoryInitialization::PreserveZeroInitialization) const;

    // Explicit diagnostic operation; not intended for a per-draw hot path.
    GpuMemoryAllocatorStatistics statistics() const;
    void setCurrentFrameIndex(unsigned int frameIndex) const;

private:
    std::shared_ptr<detail::GpuMemoryAllocatorState> state_;
};

GpuMemoryResourceInfo gpuMemoryResourceInfo(ID3D12Resource *resource) noexcept;
}
#endif
