#include "GpuMemoryAllocator.h"

#if defined(_WIN32)
#include "../ThirdParty/D3D12MemoryAllocator/D3D12MemAlloc.h"
#include <atomic>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace hwmodern
{
namespace detail
{
struct GpuMemoryAllocatorState
{
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IUnknown> deviceIdentity;
    ComPtr<D3D12MA::Allocator> allocator;
    std::uint64_t id = 0;
    std::atomic<std::uint64_t> placed{0}, committed{0}, budgetFallbacks{0};
};
}
namespace
{
std::mutex registryMutex;
std::unordered_map<IUnknown *, std::weak_ptr<detail::GpuMemoryAllocatorState>> registry;
std::atomic<std::uint64_t> nextAllocatorId{1};

// This key is private: removing/replacing its interface prematurely would
// detach memory ownership from a still-live native D3D12 resource.
const GUID allocationLeaseKey = {0xb92c561f, 0x8b26, 0x4d70,
    {0x88, 0x49, 0xd8, 0x7f, 0x4a, 0xc3, 0x2f, 0x61}};

struct __declspec(uuid("5C2BBD43-2716-4D9B-B7DE-8237E7BB370E")) IAllocationLease : IUnknown
{
    virtual GpuMemoryResourceInfo STDMETHODCALLTYPE info() const noexcept = 0;
};

class AllocationLease final : public IAllocationLease
{
public:
    AllocationLease(std::shared_ptr<detail::GpuMemoryAllocatorState> state,
        D3D12MA::Allocation *allocation) noexcept
        : state_(std::move(state)), allocation_(allocation)
    {
        info_.managed = true;
        info_.placed = allocation->GetHeap() != nullptr;
        info_.allocatorId = state_->id;
        info_.allocationBytes = allocation->GetSize();
        info_.heapOffset = allocation->GetOffset();
        info_.heapIdentity = reinterpret_cast<std::uintptr_t>(allocation->GetHeap());
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IAllocationLease))
        {
            *object = static_cast<IAllocationLease *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    GpuMemoryResourceInfo STDMETHODCALLTYPE info() const noexcept override { return info_; }
private:
    ~AllocationLease()
    {
        // Allocation's resource reference was cleared before attaching this
        // owner. Free its region before releasing the allocator context.
        allocation_.Reset();
        state_.reset();
    }
    std::atomic<ULONG> references_{1};
    std::shared_ptr<detail::GpuMemoryAllocatorState> state_;
    ComPtr<D3D12MA::Allocation> allocation_;
    GpuMemoryResourceInfo info_;
};

bool standardHeap(const D3D12_HEAP_PROPERTIES &heap) noexcept
{
    return (heap.Type == D3D12_HEAP_TYPE_DEFAULT ||
            heap.Type == D3D12_HEAP_TYPE_UPLOAD || heap.Type == D3D12_HEAP_TYPE_READBACK) &&
        heap.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_UNKNOWN &&
        heap.MemoryPoolPreference == D3D12_MEMORY_POOL_UNKNOWN &&
        heap.CreationNodeMask <= 1 && heap.VisibleNodeMask <= 1;
}
}

HRESULT GpuMemoryAllocator::initialize(ID3D12Device *device, IDXGIAdapter *adapter)
{
    if (!device) { reset(); return E_INVALIDARG; }
    ComPtr<IUnknown> identity;
    HRESULT result = device->QueryInterface(IID_PPV_ARGS(&identity));
    if (FAILED(result)) { reset(); return result; }
    if (state_ && state_->deviceIdentity.Get() == identity.Get()) return S_OK;
    reset();
    try
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        for (auto found = registry.begin(); found != registry.end();)
            if (found->second.expired()) found = registry.erase(found);
            else ++found;
        const auto existing = registry.find(identity.Get());
        if (existing != registry.end())
            if (auto shared = existing->second.lock()) { state_ = std::move(shared); return S_OK; }

        auto context = std::make_shared<detail::GpuMemoryAllocatorState>();
        context->device = device;
        context->deviceIdentity = identity;
        context->adapter = adapter;
        if (!context->adapter)
        {
            ComPtr<IDXGIFactory4> factory;
            result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
            if (FAILED(result)) return result;
            result = factory->EnumAdapterByLuid(device->GetAdapterLuid(),
                IID_PPV_ARGS(&context->adapter));
            if (FAILED(result)) return result;
        }
        DXGI_ADAPTER_DESC adapterDescription = {};
        result = context->adapter->GetDesc(&adapterDescription);
        if (FAILED(result)) return result;
        const LUID deviceLuid = device->GetAdapterLuid();
        if (deviceLuid.HighPart != adapterDescription.AdapterLuid.HighPart ||
            deviceLuid.LowPart != adapterDescription.AdapterLuid.LowPart) return E_INVALIDARG;

        D3D12MA::ALLOCATOR_DESC description = {};
        description.pDevice = context->device.Get();
        description.pAdapter = context->adapter.Get();
        description.PreferredBlockSize = 16ull * 1024 * 1024;
        // Preserve legacy GPU-address alignment and committed zero-fill.
        // In particular, do not use DEFAULT_POOLS_NOT_ZEROED from the
        // upstream recommended macro for resources requiring zero content.
        description.Flags = static_cast<D3D12MA::ALLOCATOR_FLAGS>(
            D3D12MA::ALLOCATOR_FLAG_DONT_USE_TIGHT_ALIGNMENT |
            D3D12MA::ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED);
        result = D3D12MA::CreateAllocator(&description, &context->allocator);
        if (FAILED(result)) return result;
        context->id = nextAllocatorId.fetch_add(1);
        registry[identity.Get()] = context;
        state_ = std::move(context);
        return S_OK;
    }
    catch (const std::bad_alloc &) { return E_OUTOFMEMORY; }
}

HRESULT GpuMemoryAllocator::createResource(const D3D12_HEAP_PROPERTIES &heap,
    D3D12_HEAP_FLAGS heapFlags, const D3D12_RESOURCE_DESC &description,
    D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE *optimizedClear,
    ComPtr<ID3D12Resource> &resource, GpuMemoryInitialization initialization) const
{
    resource.Reset();
    const auto context = state_;
    if (!context) return E_UNEXPECTED;
    // Leave custom CPU page/memory pools and multi-node placement to the
    // original device path, whose exact properties this wrapper cannot infer.
    if (!standardHeap(heap)) return E_NOTIMPL;

    D3D12MA::ALLOCATION_DESC allocationDescription = {};
    allocationDescription.HeapType = heap.Type;
    allocationDescription.ExtraHeapFlags = heapFlags;
    const bool poolEligible = initialization == GpuMemoryInitialization::FullyWrittenBeforeRead &&
        heapFlags == D3D12_HEAP_FLAG_NONE && description.SampleDesc.Count <= 1;
    allocationDescription.Flags = poolEligible ? D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET
        : D3D12MA::ALLOCATION_FLAG_COMMITTED;
    ComPtr<D3D12MA::Allocation> allocation;
    ComPtr<ID3D12Resource> created;
    HRESULT result = context->allocator->CreateResource(&allocationDescription, &description,
        initialState, optimizedClear, &allocation, IID_PPV_ARGS(&created));
    if (FAILED(result) && poolEligible && result == E_OUTOFMEMORY)
    {
        // A larger shared block may not fit the current budget while an exact
        // committed allocation still can. Preserve the old rendering path's
        // allocation opportunity instead of dropping a visible resource.
        ++context->budgetFallbacks;
        allocationDescription.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
        result = context->allocator->CreateResource(&allocationDescription, &description,
            initialState, optimizedClear, &allocation, IID_PPV_ARGS(&created));
    }
    if (FAILED(result)) return result;

    auto *owner = new (std::nothrow) AllocationLease(context, allocation.Get());
    if (!owner)
    {
        // The allocation still owns a resource reference at this point.
        created.Reset();
        allocation.Reset();
        return E_OUTOFMEMORY;
    }
    // CreateResource returned a separate resource reference. Detach only the
    // allocation's reference so resource -> private owner -> allocation has
    // no strong reference back to the resource itself.
    allocation->SetResource(nullptr);
    result = created->SetPrivateDataInterface(allocationLeaseKey, owner);
    if (FAILED(result))
    {
        // Keep its memory alive until the native resource is destroyed.
        created.Reset();
        owner->Release();
        allocation.Reset();
        return result;
    }
    const bool placed = allocation->GetHeap() != nullptr;
    if (placed) ++context->placed;
    else ++context->committed;
    owner->Release(); // The native resource now owns the sole private lease.
    allocation.Reset();
    resource = std::move(created);
    return S_OK;
}

GpuMemoryAllocatorStatistics GpuMemoryAllocator::statistics() const
{
    GpuMemoryAllocatorStatistics result;
    const auto context = state_;
    if (!context) return result;
    D3D12MA::TotalStatistics stats = {};
    context->allocator->CalculateStatistics(&stats);
    D3D12MA::Budget local = {};
    context->allocator->GetBudget(&local, nullptr);
    result.allocatorId = context->id;
    result.allocationCount = stats.Total.Stats.AllocationCount;
    result.allocationBytes = stats.Total.Stats.AllocationBytes;
    result.blockCount = stats.Total.Stats.BlockCount;
    result.blockBytes = stats.Total.Stats.BlockBytes;
    result.localUsageBytes = local.UsageBytes;
    result.localBudgetBytes = local.BudgetBytes;
    result.placedResourcesCreated = context->placed;
    result.committedResourcesCreated = context->committed;
    result.budgetFallbacks = context->budgetFallbacks;
    return result;
}

void GpuMemoryAllocator::setCurrentFrameIndex(unsigned int frameIndex) const
{
    if (state_) state_->allocator->SetCurrentFrameIndex(frameIndex);
}

GpuMemoryResourceInfo gpuMemoryResourceInfo(ID3D12Resource *resource) noexcept
{
    GpuMemoryResourceInfo result;
    if (!resource) return result;
    IUnknown *unknown = nullptr;
    UINT bytes = sizeof(unknown);
    if (FAILED(resource->GetPrivateData(allocationLeaseKey, &bytes, &unknown)) || !unknown)
        return result;
    ComPtr<IUnknown> owner;
    owner.Attach(unknown);
    ComPtr<IAllocationLease> allocationInfo;
    if (SUCCEEDED(owner.As(&allocationInfo))) result = allocationInfo->info();
    return result;
}
}
#endif
