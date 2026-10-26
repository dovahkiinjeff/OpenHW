#include "GpuMemoryAllocator.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace hwmodern;
using Microsoft::WRL::ComPtr;
static void require(bool condition, const char *message)
{ if (!condition) throw std::runtime_error(message); }
static D3D12_RESOURCE_DESC bufferDescription(UINT64 bytes)
{
    D3D12_RESOURCE_DESC result = {};
    result.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    result.Width = bytes;
    result.Height = result.DepthOrArraySize = result.MipLevels = 1;
    result.SampleDesc.Count = 1;
    result.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return result;
}
static D3D12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES result = {};
    result.Type = type;
    result.CreationNodeMask = result.VisibleNodeMask = 1;
    return result;
}
static void transition(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commands->ResourceBarrier(1, &barrier);
}
static void activate(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource)
{
    if (!gpuMemoryResourceInfo(resource).placed) return;
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    barrier.Aliasing.pResourceAfter = resource;
    commands->ResourceBarrier(1, &barrier);
}

int main() try
{
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIFactory6> factory;
    require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))), "DXGI factory");
    ComPtr<IDXGIAdapter1> selectedAdapter;
    for (UINT index = 0; ; ++index)
    {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 description = {};
        if (!candidate || FAILED(candidate->GetDesc1(&description)) ||
            (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
        if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&device)))) { selectedAdapter = candidate; break; }
    }
    require(!!device && !!selectedAdapter, "D3D12 hardware device creation");
    DXGI_ADAPTER_DESC1 selectedDescription = {};
    require(SUCCEEDED(selectedAdapter->GetDesc1(&selectedDescription)), "adapter description");
    std::printf("Hardware adapter: %ls\n", selectedDescription.Description);
    ComPtr<ID3D12InfoQueue> validation;
    device.As(&validation);
    GpuMemoryAllocator allocator, second;
    require(SUCCEEDED(allocator.initialize(device.Get(), selectedAdapter.Get())), "allocator creation");
    ComPtr<ID3D12Device5> device5;
    require(SUCCEEDED(device.As(&device5)), "modern device interface");
    require(SUCCEEDED(second.initialize(device5.Get())), "shared allocator creation");
    const auto allocatorId = allocator.statistics().allocatorId;
    require(allocatorId && second.statistics().allocatorId == allocatorId,
        "canonical IUnknown identity shares base/device5 allocator");
    const auto defaultHeap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    const auto uploadHeap = heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const auto readbackHeap = heapProperties(D3D12_HEAP_TYPE_READBACK);
    constexpr UINT64 bufferBytes = 128 * 1024;
    std::vector<ComPtr<ID3D12Resource>> buffers(48), textures(24);
    std::unordered_set<std::uint64_t> placedHeaps;
    for (auto &resource : buffers)
    {
        require(SUCCEEDED(allocator.createResource(defaultHeap, D3D12_HEAP_FLAG_NONE,
            bufferDescription(bufferBytes), D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            resource, GpuMemoryInitialization::FullyWrittenBeforeRead)), "pooled buffer");
        const auto info = gpuMemoryResourceInfo(resource.Get());
        require(info.managed && info.allocatorId == allocatorId, "buffer allocation lease");
        require(resource->GetGPUVirtualAddress() % 256 == 0, "RTAS-compatible address alignment");
        if (info.placed) placedHeaps.insert(info.heapIdentity);
    }
    D3D12_RESOURCE_DESC texture = {};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = texture.Height = 32;
    texture.DepthOrArraySize = texture.MipLevels = 1;
    texture.SampleDesc.Count = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for (auto &resource : textures)
    {
        require(SUCCEEDED(allocator.createResource(defaultHeap, D3D12_HEAP_FLAG_NONE,
            texture, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, resource,
            GpuMemoryInitialization::FullyWrittenBeforeRead)), "pooled texture");
        const auto info = gpuMemoryResourceInfo(resource.Get());
        require(info.managed && info.allocationBytes > 0, "texture allocation lease");
        if (info.placed) placedHeaps.insert(info.heapIdentity);
    }
    const auto pooled = allocator.statistics();
    require(pooled.allocationCount == buffers.size() + textures.size(), "all resources counted");
    require(pooled.placedResourcesCreated > 2 && placedHeaps.size() < pooled.allocationCount,
        "multiple resources share actual placed heaps");
    require(pooled.blockCount < pooled.allocationCount, "pool reduces native memory block count");
    require(pooled.blockBytes < 32ull * 1024 * 1024, "moderate blocks avoid large allocation slack");
    std::printf("Pooled GPU memory: %llu allocations, %llu blocks, %.3f MiB occupied / %.3f MiB blocks\n",
        static_cast<unsigned long long>(pooled.allocationCount),
        static_cast<unsigned long long>(pooled.blockCount),
        pooled.allocationBytes / 1048576.0, pooled.blockBytes / 1048576.0);

    // Existing ComPtr copies and alternate resource interfaces retain the
    // private allocation lease until the final native resource reference ends.
    ComPtr<ID3D12Resource1> alternate;
    const auto retiredAliasInfo = gpuMemoryResourceInfo(buffers[1].Get());
    require(SUCCEEDED(buffers[1].As(&alternate)), "alternate resource interface");
    const auto beforeAliasRelease = allocator.statistics().allocationCount;
    buffers[1].Reset();
    require(allocator.statistics().allocationCount == beforeAliasRelease, "QI reference retains region");
    alternate.Reset();
    require(allocator.statistics().allocationCount == beforeAliasRelease - 1, "final QI release frees region");

    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDescription = {};
    require(SUCCEEDED(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue))), "queue");
    ComPtr<ID3D12CommandAllocator> commandsAllocator;
    require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandsAllocator))), "command allocator");
    ComPtr<ID3D12GraphicsCommandList> commands;
    require(SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandsAllocator.Get(), nullptr, IID_PPV_ARGS(&commands))), "commands");
    ComPtr<ID3D12Fence> fence;
    require(SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "fence");
    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    require(event != nullptr, "fence event");
    UINT64 serial = 0;
    auto submit = [&]()
    {
        require(SUCCEEDED(commands->Close()), "close commands");
        ID3D12CommandList *lists[] = {commands.Get()};
        queue->ExecuteCommandLists(1, lists);
        require(SUCCEEDED(queue->Signal(fence.Get(), ++serial)), "signal fence");
        require(SUCCEEDED(fence->SetEventOnCompletion(serial, event)), "fence event completion");
        require(WaitForSingleObject(event, 30000) == WAIT_OBJECT_0, "GPU completion");
        require(SUCCEEDED(device->GetDeviceRemovedReason()), "device remains healthy");
        require(SUCCEEDED(commandsAllocator->Reset()), "reset allocator");
        require(SUCCEEDED(commands->Reset(commandsAllocator.Get(), nullptr)), "reset commands");
    };
    ComPtr<ID3D12Resource> upload, readback, zero;
    require(SUCCEEDED(allocator.createResource(uploadHeap, D3D12_HEAP_FLAG_NONE,
        bufferDescription(bufferBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, upload)), "upload");
    require(SUCCEEDED(allocator.createResource(readbackHeap, D3D12_HEAP_FLAG_NONE,
        bufferDescription(bufferBytes), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, readback)), "readback");
    require(SUCCEEDED(allocator.createResource(defaultHeap, D3D12_HEAP_FLAG_NONE,
        bufferDescription(bufferBytes), D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, zero)), "zero buffer");
    require(!gpuMemoryResourceInfo(zero.Get()).placed, "zero-fill preservation stays committed");

    std::vector<unsigned char> pattern(bufferBytes);
    for (std::size_t i = 0; i < pattern.size(); ++i) pattern[i] = static_cast<unsigned char>(i * 19 + 37);
    void *mapped = nullptr;
    D3D12_RANGE noRead = {0, 0};
    require(SUCCEEDED(upload->Map(0, &noRead, &mapped)), "upload map");
    std::memcpy(mapped, pattern.data(), pattern.size());
    upload->Unmap(0, nullptr);

    // Reset both public wrappers before using the resource. Resource leases
    // keep the allocator, heap and device context alive without a cycle.
    allocator.reset(); second.reset();
    require(!allocator.active() && !second.active(), "public wrappers reset");
    require(gpuMemoryResourceInfo(buffers[0].Get()).allocatorId == allocatorId,
        "resource retains allocator after wrapper reset");
    GpuMemoryAllocator observer;
    require(SUCCEEDED(observer.initialize(device.Get())), "observe resource-owned context");
    require(observer.statistics().allocatorId == allocatorId, "weak registry finds surviving lease context");

    // The renderer's existing flight vector remains sufficient to retain both
    // a resource and its allocation while queued copies are in flight.
    std::vector<ComPtr<ID3D12Resource>> flight;
    flight.push_back(std::move(buffers[0]));
    const auto retiredFlightInfo = gpuMemoryResourceInfo(flight[0].Get());
    const auto inFlightCount = observer.statistics().allocationCount;
    activate(commands.Get(), flight[0].Get());
    commands->CopyBufferRegion(flight[0].Get(), 0, upload.Get(), 0, bufferBytes);
    transition(commands.Get(), flight[0].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commands->CopyBufferRegion(readback.Get(), 0, flight[0].Get(), 0, bufferBytes);
    submit();
    require(observer.statistics().allocationCount == inFlightCount, "flight vector retains allocation");
    require(SUCCEEDED(readback->Map(0, nullptr, &mapped)), "readback map");
    require(std::memcmp(mapped, pattern.data(), pattern.size()) == 0, "GPU buffer bytes unchanged");
    readback->Unmap(0, nullptr);
    flight.clear();
    require(observer.statistics().allocationCount == inFlightCount - 1, "fenced retirement frees allocation");

    // Refill the freed ranges and initialize the new resource after an
    // activation barrier. No old contents or old resource pointers survive.
    std::vector<ComPtr<ID3D12Resource>> replacements(2);
    bool reusedRetiredRange = false;
    for (auto &replacement : replacements)
    {
        require(SUCCEEDED(observer.createResource(defaultHeap, D3D12_HEAP_FLAG_NONE,
            bufferDescription(bufferBytes), D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            replacement, GpuMemoryInitialization::FullyWrittenBeforeRead)), "range reuse buffer");
        const auto info = gpuMemoryResourceInfo(replacement.Get());
        for (const auto &retired : {retiredAliasInfo, retiredFlightInfo})
            reusedRetiredRange |= info.placed && retired.placed && info.heapIdentity == retired.heapIdentity &&
                info.heapOffset < retired.heapOffset + retired.allocationBytes &&
                retired.heapOffset < info.heapOffset + info.allocationBytes;
        activate(commands.Get(), replacement.Get());
        commands->CopyBufferRegion(replacement.Get(), 0, upload.Get(), 0, bufferBytes);
        transition(commands.Get(), replacement.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands->CopyBufferRegion(readback.Get(), 0, replacement.Get(), 0, bufferBytes);
        submit();
        require(SUCCEEDED(readback->Map(0, nullptr, &mapped)), "reused range readback");
        require(std::memcmp(mapped, pattern.data(), pattern.size()) == 0, "reused GPU range bytes unchanged");
        readback->Unmap(0, nullptr);
    }
    require(reusedRetiredRange, "actual freed placed range was recycled");
    replacements.clear();

    commands->CopyBufferRegion(readback.Get(), 0, zero.Get(), 0, bufferBytes);
    submit();
    require(SUCCEEDED(readback->Map(0, nullptr, &mapped)), "zero readback map");
    const auto *bytes = static_cast<const unsigned char *>(mapped);
    require(std::all_of(bytes, bytes + bufferBytes, [](unsigned char value) { return value == 0; }),
        "preserved committed allocation reads as zero");
    readback->Unmap(0, nullptr);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64 textureBytes = 0;
    device->GetCopyableFootprints(&texture, 0, 1, 0, &footprint, nullptr, nullptr, &textureBytes);
    ComPtr<ID3D12Resource> textureUpload, textureReadback;
    require(SUCCEEDED(observer.createResource(uploadHeap, D3D12_HEAP_FLAG_NONE,
        bufferDescription(textureBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, textureUpload)), "texture upload");
    require(SUCCEEDED(observer.createResource(readbackHeap, D3D12_HEAP_FLAG_NONE,
        bufferDescription(textureBytes), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, textureReadback)), "texture readback");
    require(SUCCEEDED(textureUpload->Map(0, &noRead, &mapped)), "texture upload map");
    for (UINT y = 0; y < 32; ++y)
        for (UINT x = 0; x < 128; ++x)
            static_cast<unsigned char *>(mapped)[y * footprint.Footprint.RowPitch + x] =
                static_cast<unsigned char>(y * 11 + x);
    textureUpload->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION textureDestination = {}, textureSource = {};
    textureDestination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    textureDestination.pResource = textures[0].Get();
    textureSource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    textureSource.pResource = textureUpload.Get();
    textureSource.PlacedFootprint = footprint;
    activate(commands.Get(), textures[0].Get());
    commands->CopyTextureRegion(&textureDestination, 0, 0, 0, &textureSource, nullptr);
    transition(commands.Get(), textures[0].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    textureSource = textureDestination;
    textureDestination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    textureDestination.pResource = textureReadback.Get();
    textureDestination.PlacedFootprint = footprint;
    commands->CopyTextureRegion(&textureDestination, 0, 0, 0, &textureSource, nullptr);
    submit();
    require(SUCCEEDED(textureReadback->Map(0, nullptr, &mapped)), "texture readback map");
    for (UINT y = 0; y < 32; ++y)
        for (UINT x = 0; x < 128; ++x)
            require(static_cast<unsigned char *>(mapped)[y * footprint.Footprint.RowPitch + x] ==
                static_cast<unsigned char>(y * 11 + x), "GPU texture pixels unchanged");
    textureReadback->Unmap(0, nullptr);

    buffers.clear(); textures.clear();
    upload.Reset(); readback.Reset(); zero.Reset(); textureUpload.Reset(); textureReadback.Reset();
    require(observer.statistics().allocationCount == 0, "final resources free every allocation without cycles");
    observer.reset();
    require(SUCCEEDED(allocator.initialize(device.Get())), "allocator recreates after all owners end");
    require(allocator.statistics().allocatorId != allocatorId && allocator.statistics().allocationCount == 0,
        "weak registry does not leak or permanently retain allocator context");
    allocator.reset();
    if (validation)
        for (UINT64 i = 0; i < validation->GetNumStoredMessages(); ++i)
        {
            SIZE_T messageBytes = 0;
            validation->GetMessage(i, nullptr, &messageBytes);
            std::vector<unsigned char> storage(messageBytes);
            auto message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
            validation->GetMessage(i, message, &messageBytes);
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
            { std::fprintf(stderr, "%s\n", message->pDescription); require(false, "D3D12 validation error"); }
        }
    commands->Close();
    CloseHandle(event);
    std::puts("PASS: pooled buffers/textures, byte-exact GPU copies, committed zero-fill, canonical device sharing, resource/QI/flight retirement lifetime, allocator reset and zero live allocations");
    if (validation) std::puts("PASS: D3D12 debug-layer validation reported no errors");
    return 0;
}
catch (const std::exception &error)
{ std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
