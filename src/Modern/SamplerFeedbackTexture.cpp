#include "SamplerFeedbackTexture.h"
#include <algorithm>
#include <limits>

using Microsoft::WRL::ComPtr;
namespace hwmodern {
namespace {
constexpr std::uint64_t tileBytes = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
D3D12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = type; heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    return heap;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = resource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = before; b.Transition.StateAfter = after;
    return b;
}
unsigned regionDimension(unsigned size) {
    unsigned result = 4;
    while (result < 128 && result * 2 <= size / 2) result *= 2;
    return result;
}
}

bool SamplerFeedbackContext::initialize(ID3D12Device* device,
    ID3D12CommandQueue* directQueue, unsigned descriptorCount) {
    if (device_ || !device || !directQueue || !descriptorCount ||
        directQueue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return false;
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 sf = {};
    D3D12_FEATURE_DATA_SHADER_MODEL model = {D3D_SHADER_MODEL_6_5};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
            &options, sizeof(options))) || options.TiledResourcesTier < D3D12_TILED_RESOURCES_TIER_2 ||
        FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7,
            &sf, sizeof(sf))) || sf.SamplerFeedbackTier == D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED ||
        FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
            &model, sizeof(model))) || model.HighestShaderModel < D3D_SHADER_MODEL_6_5) return false;
    ComPtr<ID3D12Device8> device8;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12DescriptorHeap> clearDescriptors;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorDesc = {};
    descriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorDesc.NumDescriptors = descriptorCount;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device8))) ||
        FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(device->CreateDescriptorHeap(&descriptorDesc, IID_PPV_ARGS(&clearDescriptors)))) return false;
    D3D12_RESOURCE_DESC1 nullDesc = {};
    nullDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    nullDesc.Width = nullDesc.Height = 8;
    nullDesc.DepthOrArraySize = 4; nullDesc.MipLevels = 1; nullDesc.SampleDesc.Count = 1;
    nullDesc.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE;
    nullDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    nullDesc.SamplerFeedbackMipRegion = {4, 4, 1};
    const auto defaultHeap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> nullFeedback;
    if (FAILED(device8->CreateCommittedResource2(&defaultHeap, D3D12_HEAP_FLAG_NONE,
        &nullDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr,
        IID_PPV_ARGS(&nullFeedback)))) return false;
    device_ = device8; queue_ = directQueue; retirementFence_ = fence;
    clearDescriptors_ = clearDescriptors; descriptorCount_ = descriptorCount;
    nullFeedback_ = nullFeedback;
    descriptorStride_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    tier_ = sf.SamplerFeedbackTier;
    return true;
}
D3D12_CPU_DESCRIPTOR_HANDLE SamplerFeedbackContext::clearHandle(unsigned index) const {
    if (!clearDescriptors_ || index >= descriptorCount_) return {};
    auto handle = clearDescriptors_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * descriptorStride_;
    return handle;
}
void SamplerFeedbackContext::createNullViews(D3D12_CPU_DESCRIPTOR_HANDLE start,
    unsigned stride, unsigned count) const {
    if (!device_) return;
    for (unsigned i = 0; i < count; ++i) {
        device_->CreateSamplerFeedbackUnorderedAccessView(nullptr, nullFeedback_.Get(), start);
        start.ptr += stride;
    }
}
void SamplerFeedbackContext::collectRetiredHeaps() {
    if (!retirementFence_) return;
    const auto completed = retirementFence_->GetCompletedValue();
    if (completed == UINT64_MAX) return; // device removed: drained shutdown owns cleanup
    retired_.erase(std::remove_if(retired_.begin(), retired_.end(),
        [completed](const Retirement& item) { return item.fence && item.fence <= completed; }), retired_.end());
}
std::uint64_t SamplerFeedbackContext::pendingRetiredBytes() const {
    std::uint64_t bytes = 0;
    for (const auto& item : retired_) for (const auto& heap : item.heaps)
        bytes += heap->GetDesc().SizeInBytes;
    return bytes;
}
bool SamplerFeedbackContext::retireHeaps(std::vector<ComPtr<ID3D12Heap>> heaps) {
    if (heaps.empty()) return true;
    Retirement item; item.heaps = std::move(heaps);
    const auto value = nextRetirementFence_++;
    const HRESULT result = queue_->Signal(retirementFence_.Get(), value);
    if (SUCCEEDED(result)) item.fence = value;
    // Failure retains the heaps until drained destruction, never use-after-free.
    retired_.push_back(std::move(item));
    return SUCCEEDED(result);
}
bool SamplerFeedbackContext::retireResources(std::vector<ComPtr<ID3D12Resource>> resources) {
    if (resources.empty()) return true;
    Retirement item; item.resources = std::move(resources);
    const auto value = nextRetirementFence_++;
    const HRESULT result = queue_->Signal(retirementFence_.Get(), value);
    if (SUCCEEDED(result)) item.fence = value;
    retired_.push_back(std::move(item));
    return SUCCEEDED(result);
}

bool SamplerFeedbackTexture::create(SamplerFeedbackContext& context,
    const D3D12_RESOURCE_DESC& input, unsigned descriptorIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE gpuView) {
    if (texture_ || !context.supported() || input.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        input.Format != DXGI_FORMAT_R8G8B8A8_UNORM || input.DepthOrArraySize != 4 ||
        input.Width < 512 || input.Height < 512 || input.Width > UINT_MAX ||
        !input.MipLevels || input.SampleDesc.Count != 1 || input.SampleDesc.Quality ||
        input.Flags != D3D12_RESOURCE_FLAG_NONE || !gpuView.ptr || !context.clearHandle(descriptorIndex).ptr) return false;
    D3D12_RESOURCE_DESC desc = input;
    desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    ComPtr<ID3D12Resource> texture, feedback, decoded;
    auto device = context.device();
    if (FAILED(device->CreateReservedResource(&desc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&texture)))) return false;
    UINT totalTiles = 0, subresources = input.MipLevels * 4u;
    D3D12_PACKED_MIP_INFO packed = {};
    D3D12_TILE_SHAPE shape = {};
    std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresources);
    device->GetResourceTiling(texture.Get(), &totalTiles, &packed, &shape,
        &subresources, 0, tilings.data());
    if (subresources != input.MipLevels * 4u || !totalTiles ||
        packed.NumStandardMips + packed.NumPackedMips != input.MipLevels) return false;
    std::vector<MipHeap> groups;
    for (unsigned mip = 0; mip < packed.NumStandardMips; ++mip) {
        const auto& t = tilings[mip];
        if (!t.WidthInTiles || !t.HeightInTiles || !t.DepthInTiles ||
            t.StartTileIndexInOverallResource == D3D12_PACKED_TILE) return false;
        MipHeap group;
        group.firstMip = mip; group.mipCount = 1;
        group.tilesPerSlice = t.WidthInTiles * t.HeightInTiles * t.DepthInTiles;
        group.region.UseBox = TRUE;
        group.region.Width = t.WidthInTiles; group.region.Height = t.HeightInTiles;
        group.region.Depth = t.DepthInTiles; group.region.NumTiles = group.tilesPerSlice;
        groups.push_back(std::move(group));
    }
    if (packed.NumPackedMips) {
        if (!packed.NumTilesForPackedMips) return false;
        MipHeap group;
        group.firstMip = packed.NumStandardMips; group.mipCount = packed.NumPackedMips;
        group.tilesPerSlice = packed.NumTilesForPackedMips;
        group.region.NumTiles = packed.NumTilesForPackedMips; group.region.UseBox = FALSE;
        groups.push_back(std::move(group));
    }
    std::uint64_t countedTiles = 0;
    for (const auto& group : groups) countedTiles += group.tilesPerSlice * 4ull;
    if (countedTiles != totalTiles) return false;

    const unsigned regionWidth = regionDimension(static_cast<unsigned>(desc.Width));
    const unsigned regionHeight = regionDimension(desc.Height);
    D3D12_RESOURCE_DESC1 feedbackDesc = {};
    feedbackDesc.Dimension = desc.Dimension; feedbackDesc.Width = desc.Width;
    feedbackDesc.Height = desc.Height; feedbackDesc.DepthOrArraySize = 4;
    feedbackDesc.MipLevels = desc.MipLevels; feedbackDesc.SampleDesc.Count = 1;
    feedbackDesc.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE;
    feedbackDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    feedbackDesc.SamplerFeedbackMipRegion = {regionWidth, regionHeight, 1};
    const auto defaultHeap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource2(&defaultHeap, D3D12_HEAP_FLAG_NONE,
        &feedbackDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr,
        IID_PPV_ARGS(&feedback)))) return false;
    D3D12_RESOURCE_DESC decodedDesc = {};
    decodedDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    decodedDesc.Width = (desc.Width + regionWidth - 1) / regionWidth;
    decodedDesc.Height = (desc.Height + regionHeight - 1) / regionHeight;
    decodedDesc.DepthOrArraySize = 4; decodedDesc.MipLevels = 1;
    decodedDesc.Format = DXGI_FORMAT_R8_UINT; decodedDesc.SampleDesc.Count = 1;
    if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
        &decodedDesc, D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr, IID_PPV_ARGS(&decoded)))) return false;
    const auto feedbackAllocation = device->GetResourceAllocationInfo2(0, 1, &feedbackDesc, nullptr);
    const auto decodedAllocation = device->GetResourceAllocationInfo(0, 1, &decodedDesc);
    if (!feedbackAllocation.SizeInBytes || feedbackAllocation.SizeInBytes == UINT64_MAX ||
        !decodedAllocation.SizeInBytes || decodedAllocation.SizeInBytes == UINT64_MAX) return false;
    UINT64 readbackBytes = 0;
    device->GetCopyableFootprints(&decodedDesc, 0, 4, 0, feedbackLayouts_, nullptr, nullptr, &readbackBytes);
    clearView_ = context.clearHandle(descriptorIndex);
    device->CreateSamplerFeedbackUnorderedAccessView(texture.Get(), feedback.Get(), clearView_);
    device->CopyDescriptorsSimple(1, gpuView, clearView_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    texture_ = texture; feedback_ = feedback; decoded_ = decoded;
    desc_ = desc; groups_ = std::move(groups); packedFirstMip_ = packed.NumStandardMips;
    feedbackWidth_ = static_cast<unsigned>(decodedDesc.Width); feedbackHeight_ = decodedDesc.Height;
    feedbackReadbackBytes_ = readbackBytes;
    auxiliaryBytes_ = feedbackAllocation.SizeInBytes + decodedAllocation.SizeInBytes;
    return true;
}
bool SamplerFeedbackTexture::fullyMapped() const {
    if (!texture_) return false;
    for (const auto& group : groups_) if (!group.heap) return false;
    return true;
}
bool SamplerFeedbackTexture::readyForSampling() const {
    if (!fullyMapped()) return false;
    for (const auto& group : groups_) if (!group.uploaded) return false;
    return true;
}
std::uint64_t SamplerFeedbackTexture::mappedBytes() const {
    std::uint64_t bytes = 0;
    for (const auto& group : groups_) if (group.heap) bytes += group.heap->GetDesc().SizeInBytes;
    return bytes;
}
void SamplerFeedbackTexture::mapGroup(SamplerFeedbackContext& context,
    const MipHeap& group, ID3D12Heap* heap, D3D12_TILE_RANGE_FLAGS flags) const {
    D3D12_TILED_RESOURCE_COORDINATE coordinates[4] = {};
    D3D12_TILE_REGION_SIZE regions[4];
    D3D12_TILE_RANGE_FLAGS rangeFlags[4];
    UINT offsets[4], counts[4];
    for (unsigned slice = 0; slice < 4; ++slice) {
        coordinates[slice].Subresource = slice * desc_.MipLevels + group.firstMip;
        regions[slice] = group.region; rangeFlags[slice] = flags;
        offsets[slice] = slice * group.tilesPerSlice; counts[slice] = group.tilesPerSlice;
    }
    context.queue()->UpdateTileMappings(texture_.Get(), 4, coordinates, regions,
        heap, 4, rangeFlags, offsets, counts, D3D12_TILE_MAPPING_FLAG_NONE);
}
bool SamplerFeedbackTexture::mapAll(SamplerFeedbackContext& context,
    std::vector<SamplerFeedbackMipRange>& newlyMapped) {
    newlyMapped.clear();
    if (!texture_ || !context.supported()) return false;
    std::vector<ComPtr<ID3D12Heap>> heaps(groups_.size());
    for (size_t i = 0; i < groups_.size(); ++i) if (!groups_[i].heap) {
        D3D12_HEAP_DESC desc = {};
        desc.SizeInBytes = groups_[i].tilesPerSlice * 4ull * tileBytes;
        desc.Properties = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
        if (FAILED(context.device()->CreateHeap(&desc, IID_PPV_ARGS(&heaps[i])))) return false;
    }
    for (size_t i = 0; i < groups_.size(); ++i) if (heaps[i]) {
        groups_[i].heap = std::move(heaps[i]);
        mapGroup(context, groups_[i], groups_[i].heap.Get(), D3D12_TILE_RANGE_FLAG_NONE);
    }
    for (const auto& group : groups_) if (!group.uploaded)
        newlyMapped.push_back({group.firstMip, group.mipCount});
    return true;
}
void SamplerFeedbackTexture::uploadsRecorded(const std::vector<SamplerFeedbackMipRange>& ranges) {
    for (const auto& range : ranges) for (auto& group : groups_)
        if (group.heap && group.firstMip == range.firstMip && group.mipCount == range.mipCount)
            group.uploaded = true;
}
std::uint64_t SamplerFeedbackTexture::trimInactive(SamplerFeedbackContext& context,
    bool noCurrentOrInFlightReferences) {
    if (!texture_ || !feedbackKnown_ || !noCurrentOrInFlightReferences) return 0;
    const unsigned keep = std::min(feedbackMinMip_, packedFirstMip_);
    std::vector<ComPtr<ID3D12Heap>> retired;
    std::uint64_t bytes = 0;
    for (auto& group : groups_) if (group.firstMip < keep && group.heap) {
        mapGroup(context, group, nullptr, D3D12_TILE_RANGE_FLAG_NULL);
        bytes += group.heap->GetDesc().SizeInBytes;
        retired.push_back(std::move(group.heap));
        group.uploaded = false;
    }
    context.retireHeaps(std::move(retired));
    return bytes;
}
bool SamplerFeedbackTexture::feedbackPending(unsigned slot) const {
    return slot < feedbackSlots && (readbacks_[slot].pending || readbacks_[slot].recording);
}
bool SamplerFeedbackTexture::prepareFeedback(SamplerFeedbackContext& context, unsigned slot) {
    if (!texture_ || slot >= feedbackSlots || feedbackPending(slot) || !readyForSampling()) return false;
    auto& readback = readbacks_[slot];
    if (readback.resource) return true;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = feedbackReadbackBytes_;
    desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const auto heap = heapProperties(D3D12_HEAP_TYPE_READBACK);
    const auto allocation = context.device()->GetResourceAllocationInfo(0, 1, &desc);
    if (!allocation.SizeInBytes || allocation.SizeInBytes == UINT64_MAX) return false;
    if (FAILED(context.device()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback.resource)))) return false;
    auxiliaryBytes_ += allocation.SizeInBytes;
    return true;
}
bool SamplerFeedbackTexture::beginFeedback(ID3D12GraphicsCommandList* commands,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuView, unsigned slot) {
    if (!commands || slot >= feedbackSlots || feedbackPending(slot) ||
        !readbacks_[slot].resource || !readyForSampling()) return false;
    const UINT clear[4] = {};
    commands->ClearUnorderedAccessViewUint(gpuView, clearView_, feedback_.Get(), clear, 0, nullptr);
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; b.UAV.pResource = feedback_.Get();
    commands->ResourceBarrier(1, &b);
    readbacks_[slot].recording = true;
    return true;
}
bool SamplerFeedbackTexture::endFeedback(ID3D12GraphicsCommandList1* commands, unsigned slot) {
    if (!commands || slot >= feedbackSlots || !readbacks_[slot].recording) return false;
    auto b = transition(feedback_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    commands->ResourceBarrier(1, &b);
    commands->ResolveSubresourceRegion(decoded_.Get(), UINT_MAX, 0, 0,
        feedback_.Get(), UINT_MAX, nullptr, DXGI_FORMAT_R8_UINT,
        D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK);
    b = transition(decoded_.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commands->ResourceBarrier(1, &b);
    for (unsigned slice = 0; slice < 4; ++slice) {
        D3D12_TEXTURE_COPY_LOCATION source = {}, destination = {};
        source.pResource = decoded_.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = slice;
        destination.pResource = readbacks_[slot].resource.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = feedbackLayouts_[slice];
        commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }
    D3D12_RESOURCE_BARRIER restore[2] = {
        transition(decoded_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST),
        transition(feedback_.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    commands->ResourceBarrier(2, restore);
    readbacks_[slot].recording = false; readbacks_[slot].pending = true;
    return true;
}
void SamplerFeedbackTexture::feedbackSubmitted(unsigned slot, std::uint64_t frameFence) {
    if (slot < feedbackSlots && readbacks_[slot].pending && frameFence)
        readbacks_[slot].fence = frameFence;
}
bool SamplerFeedbackTexture::consumeFeedback(unsigned slot, std::uint64_t completedFrameFence) {
    if (slot >= feedbackSlots || completedFrameFence == UINT64_MAX) return false;
    auto& readback = readbacks_[slot];
    if (!readback.pending || !readback.fence || readback.fence > completedFrameFence) return false;
    void* mapped = nullptr;
    const D3D12_RANGE readRange = {0, static_cast<SIZE_T>(feedbackReadbackBytes_)};
    if (FAILED(readback.resource->Map(0, &readRange, &mapped))) return false;
    unsigned minMip = desc_.MipLevels - 1;
    bool any = false;
    for (unsigned slice = 0; slice < 4; ++slice)
        for (unsigned y = 0; y < feedbackHeight_; ++y) {
            const auto row = static_cast<const unsigned char*>(mapped) + feedbackLayouts_[slice].Offset +
                y * feedbackLayouts_[slice].Footprint.RowPitch;
            for (unsigned x = 0; x < feedbackWidth_; ++x) if (row[x] != 0xff) {
                any = true; minMip = std::min(minMip, static_cast<unsigned>(row[x]));
            }
        }
    const D3D12_RANGE noWrite = {};
    readback.resource->Unmap(0, &noWrite);
    feedbackKnown_ = true;
    // Unsampled maps retain only the packed tail when inactive. Feedback
    // cannot affect active residency, so discarded/stochastic hits are safe.
    feedbackMinMip_ = any ? minMip : packedFirstMip_;
    readback.pending = false; readback.fence = 0;
    return true;
}
void SamplerFeedbackTexture::releaseInactive(SamplerFeedbackContext& context) {
    std::vector<ComPtr<ID3D12Heap>> heaps;
    for (auto& group : groups_) if (group.heap) {
        mapGroup(context, group, nullptr, D3D12_TILE_RANGE_FLAG_NULL);
        heaps.push_back(std::move(group.heap));
    }
    std::vector<ComPtr<ID3D12Resource>> resources;
    if (texture_) resources.push_back(std::move(texture_));
    if (feedback_) resources.push_back(std::move(feedback_));
    if (decoded_) resources.push_back(std::move(decoded_));
    for (auto& slot : readbacks_) {
        if (slot.resource) resources.push_back(std::move(slot.resource));
        slot = {};
    }
    context.retireHeaps(std::move(heaps));
    context.retireResources(std::move(resources));
    groups_.clear(); feedbackKnown_ = false; auxiliaryBytes_ = 0;
}
} // namespace hwmodern
