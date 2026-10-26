// Exercise the production DXR implementation on the GPU without opening the
// game, modifying player settings, or requiring proprietary game assets.
#include "../src/Modern/ModernRaytracing.cpp"
#include "../src/Modern/FrameWorker.h"
#include "DustSunVisibilityShader.h"
#include <d3d12sdklayers.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

static void validateDustSunPipeline(ID3D12Device *device,
    ComPtr<ID3D12RootSignature> &root, ComPtr<ID3D12PipelineState> &sunPipeline) {
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 3; ranges[0].BaseShaderRegister = 9;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[5] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &ranges[0]};
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable = {1, &ranges[1]};
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[3].Descriptor.ShaderRegister = 18;
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[4].Descriptor.ShaderRegister = 1;
    D3D12_ROOT_SIGNATURE_DESC description = {};
    description.NumParameters = 5; description.pParameters = parameters;
    ComPtr<ID3DBlob> bytes, errors;
    require(SUCCEEDED(D3D12SerializeRootSignature(&description,
        D3D_ROOT_SIGNATURE_VERSION_1, &bytes, &errors)), "dust sun root serialization");
    require(SUCCEEDED(device->CreateRootSignature(0, bytes->GetBufferPointer(),
        bytes->GetBufferSize(), IID_PPV_ARGS(&root))), "dust sun root creation");
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options = {};
    require(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
        &options, sizeof(options))), "DXR feature query");
    if (options.RaytracingTier < D3D12_RAYTRACING_TIER_1_1) {
        std::cout << "SKIP: dust sun ray queries require DXR 1.1\n";
        return;
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = root.Get();
    pipeline.CS = {g_hwDustSunVisibilityShader, sizeof(g_hwDustSunVisibilityShader)};
    require(SUCCEEDED(device->CreateComputePipelineState(&pipeline,
        IID_PPV_ARGS(&sunPipeline))), "dust sun compute PSO");
    std::cout << "PASS: dust sunlight DXIL/root signature accepted by GPU\n";
}

int main(int argc, char **argv) try {
    const bool feedbackPerformance = argc > 1 && std::strcmp(argv[1], "feedback-perf") == 0;
    const bool disableFeedbackCapture = argc > 1 && std::strcmp(argv[1], "no-capture") == 0;
    const bool disableStreaming = argc > 1 && std::strcmp(argv[1], "no-streaming") == 0;
    using namespace hwmodern;
    {
        // The fallback receives exactly the path tracer's resolved light bytes,
        // even when there is no initialized DXR device or acceleration structure.
        float view[16]={};view[0]=view[5]=view[10]=view[15]=1;view[12]=-100;
        float direction[3]={0,1,0},key[3]={.8f,.2f,.1f},ambient[3]={.03f,.04f,.05f};
        raytracingSetMissionSkyLighting(1,direction,key,ambient);
        raytracingSetSkyLightingStrengths(1.2f,.4f);
        HWModernMissionLightEmitter authored={};authored.enabled=1;authored.type=HW_MODERN_MISSION_LIGHT_POINT;
        authored.position[0]=25;authored.color[1]=1;authored.intensity=2;authored.radius=500;
        raytracingSetMissionAuthoringLights(&authored,1);
        HWModernMapLightEmitter map={};map.type=1;map.renderPosition[0]=40;map.color[2]=1;map.intensity=3;
        HWModernDynamicLightEmitter dynamic={};dynamic.source=HW_MODERN_LIGHT_ION_BEAM;dynamic.shape=HW_MODERN_LIGHT_LINE;
        dynamic.position[0]=10;dynamic.endPosition[0]=20;dynamic.direction[2]=1;dynamic.color[0]=1;dynamic.intensity=2;dynamic.radius=100;
        std::memcpy(state.viewMatrix,view,sizeof(view));
        std::vector<GpuLight> traced;buildLights(&map,1,&dynamic,1,traced);
        std::vector<ResolvedSceneLight> raster;resolveRasterSceneLights(view,&map,1,&dynamic,1,raster);
        require(traced.size()==5 && raster.size()==traced.size() && !std::memcmp(traced.data(),raster.data(),raster.size()*sizeof(ResolvedSceneLight)),"canonical raster/DXR lighting bytes");
        require(std::fabs(raster[0].intensity-1.62f)<1e-5f && raster[0].direction[1]==1 && raster[0].color[0]==.8f,"shared sun intensity/color/direction");
        require(std::fabs(raster[1].intensity-.34f)<1e-5f && raster[2].position[0]==-75 && raster[3].position[0]==-60 && raster[4].endPosition[0]==-80,"shared ambient and local/line view transforms");
        raytracingSetMissionAuthoringReplacesMapLights(true);resolveRasterSceneLights(view,&map,1,&dynamic,1,raster);
        require(raster.size()==4,"authored replacement suppresses original map lights");
        raytracingSetMissionAuthoringLights(nullptr,0);raytracingSetMissionAuthoringReplacesMapLights(false);
        float originalDirection[3]={0,0,-1},originalColor[3]={1,1,1};
        raytracingSetMissionSkyLighting(0,originalDirection,originalColor,nullptr);
        raytracingSetSkyLightingStrengths(1,1);
        std::cout<<"PASS: raster and DXR share exact light records, authoring priorities, strengths and coordinate conversion without DXR initialization\n";
    }
    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings))))
    {
        dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIFactory6> factory;
    require(SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))), "factory");
    ComPtr<IDXGIAdapter1> adapter;
    require(SUCCEEDED(factory->EnumAdapterByGpuPreference(0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))), "high-performance adapter");
    DXGI_ADAPTER_DESC1 adapterDescription = {};
    adapter->GetDesc1(&adapterDescription);
    std::wcerr << L"GPU: " << adapterDescription.Description << L'\n';
    require(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&device))), "device creation");
    ComPtr<ID3D12InfoQueue> messages;
    device.As(&messages);
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    require(SUCCEEDED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))), "queue");
    ComPtr<ID3D12RootSignature> sunRoot;
    ComPtr<ID3D12PipelineState> sunPipeline;
    validateDustSunPipeline(device.Get(), sunRoot, sunPipeline);
    ComPtr<ID3D12DescriptorHeap> descriptors;
    D3D12_DESCRIPTOR_HEAP_DESC heap = {};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.NumDescriptors = materialFeedbackDescriptorBase + materialDescriptorCount + 1u;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    require(SUCCEEDED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&descriptors))), "descriptors");
    require(raytracingInitialize(device.Get(), descriptors.Get(),
        device->GetDescriptorHandleIncrementSize(heap.Type), 0, 4, 43, 40,
        disableStreaming ? nullptr : queue.Get()), "DXR initialization");
    require(raytracingCreateOutput(64, 64), "DXR output creation");
    ComPtr<ID3D12CommandAllocator> allocator;
    require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator))), "allocator");
    ComPtr<ID3D12GraphicsCommandList> commands;
    require(SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(), nullptr, IID_PPV_ARGS(&commands))), "commands");
    ComPtr<ID3D12Fence> fence;
    require(SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "fence");
    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    UINT64 serial = 0;
    auto submit = [&]() {
        require(SUCCEEDED(commands->Close()), "close command list");
        ID3D12CommandList *lists[] = {commands.Get()};
        queue->ExecuteCommandLists(1, lists);
        require(SUCCEEDED(queue->Signal(fence.Get(), ++serial)), "signal");
        require(SUCCEEDED(fence->SetEventOnCompletion(serial, event)), "fence event");
        require(WaitForSingleObject(event, 30000) == WAIT_OBJECT_0, "GPU timeout");
        const HRESULT removed = device->GetDeviceRemovedReason();
        if (FAILED(removed))
        {
            std::cerr << "Device removed HRESULT=0x" << std::hex << removed << std::dec << '\n';
            if (messages)
                for (UINT64 messageIndex = 0; messageIndex < messages->GetNumStoredMessages(); ++messageIndex)
                {
                    SIZE_T size = 0;
                    messages->GetMessage(messageIndex, nullptr, &size);
                    std::vector<unsigned char> storage(size);
                    auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
                    if (SUCCEEDED(messages->GetMessage(messageIndex, message, &size)))
                        std::cerr << message->pDescription << '\n';
                }
            ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
            if (SUCCEEDED(device.As(&dred)))
            {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
                if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)))
                    for (auto *node = breadcrumbs.pHeadAutoBreadcrumbNode; node; node = node->pNext)
                    {
                        const UINT last = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
                        std::cerr << "DRED breadcrumbs: " << last << '/' << node->BreadcrumbCount << '\n';
                        for (UINT operation = last > 5 ? last - 5 : 0;
                             operation < std::min(node->BreadcrumbCount, last + 6); ++operation)
                            std::cerr << "  operation " << operation << " = " << node->pCommandHistory[operation] << '\n';
                    }
                D3D12_DRED_PAGE_FAULT_OUTPUT1 fault = {};
                if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&fault)))
                    std::cerr << "DRED page-fault VA=0x" << std::hex << fault.PageFaultVA << std::dec << '\n';
            }
        }
        require(SUCCEEDED(removed), "device removed");
        allocator->Reset(); commands->Reset(allocator.Get(), nullptr);
    };
    ComPtr<ID3D12Resource> sky;
    D3D12_HEAP_PROPERTIES properties = {}; properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC texture = {};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = texture.Height = 1; texture.DepthOrArraySize = texture.MipLevels = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM; texture.SampleDesc.Count = 1;
    texture.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    require(SUCCEEDED(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE,
        &texture, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&sky))), "sky");
    ComPtr<ID3D12DescriptorHeap> rtvs;
    heap = {}; heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap.NumDescriptors = 1;
    require(SUCCEEDED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtvs))), "RTV heap");
    device->CreateRenderTargetView(sky.Get(), nullptr, rtvs->GetCPUDescriptorHandleForHeapStart());
    float skyColor[4] = {0.25f, 0.25f, 0.25f, 1};
    commands->ClearRenderTargetView(rtvs->GetCPUDescriptorHandleForHeapStart(), skyColor, 0, nullptr);
    transitionResource(commands.Get(), sky.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    submit();
    // Cross the real 1 MiB archive page boundary, rather than testing only
    // the uncompressed tail. All GPU mips will be compared after eviction.
    constexpr unsigned int materialSize = 512;
    std::vector<unsigned int> color(materialSize * materialSize);
    std::vector<unsigned int> emission(color.size(), 0), normals(color.size(), 0xffff8080);
    for (unsigned int i = 0; i < color.size(); ++i)
        color[i] = (i & 1) ? 0xff4080ff : 0xff804020;
    int material = 0, unusedMaterial = 0, mesh = 0;
    raytracingRegisterSurfaceTexture(&material, materialSize, materialSize,
        color.data(), emission.data(), normals.data(), nullptr);
    raytracingRegisterSurfaceTexture(&unusedMaterial, materialSize, materialSize,
        color.data(), emission.data(), normals.data(),
        nullptr, "smoke/unused-material");
    require(surfaceTexels.compressedPages() > 0, "real lossless CPU archive pages used");
    const auto usedTextureIndex = surfaceTextures.at(&material).textureIndex;
    const auto unusedTextureIndex = surfaceTextures.at(&unusedMaterial).textureIndex;
    float vertices[] = {-1,-1,0, 1,-1,0, 0,1,0};
    unsigned short indices[] = {0,1,2};
    HWModernTriangleSurface surface = {};
    surface.materialIdentity = &material;
    surface.opacity = 1;
    surface.baseColor[0] = surface.baseColor[1] = surface.baseColor[2] = 1;
    surface.uv[2] = 1; surface.uv[4] = 0.5f; surface.uv[5] = 1;
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float transform[16]; std::memcpy(transform, identity, sizeof(transform)); transform[14] = -3;
    float baseColor[3] = {1,1,1};
    raytracingSetPathSettings(1, 6, 1);
    require(requestedMaximumBounces == 6, "six-bounce option reaches the renderer");
    raytracingSetPathSettings(1, 99, 1);
    require(requestedMaximumBounces == 6, "bounce upper bound is six");
    raytracingSetPathSettings(1, 0, 1);
    require(requestedMaximumBounces == 1, "bounce lower bound is one");
    raytracingSetPathSettings(1, 6, 1);
    if (feedbackPerformance) {
        // Optional isolated A/B experiment. It changes only this executable's
        // synthetic scene/state and never opens a window or writes settings.
        require(state.samplerFeedback.supported() && state.feedbackStateObject,
            "feedback-perf requires the supported production capture pipeline");
        UINT64 timestampFrequency = 0;
        require(SUCCEEDED(queue->GetTimestampFrequency(&timestampFrequency)) && timestampFrequency,
            "GPU timestamp frequency");
        D3D12_QUERY_HEAP_DESC queryDescription = {};
        queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryDescription.Count = 2;
        ComPtr<ID3D12QueryHeap> queries;
        require(SUCCEEDED(device->CreateQueryHeap(&queryDescription, IID_PPV_ARGS(&queries))),
            "performance timestamp heap");
        ComPtr<ID3D12Resource> timestampReadback, requestReadback;
        require(createBuffer(2 * sizeof(UINT64), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST, timestampReadback), "timestamp readback");
        require(createBuffer(2 * sizeof(unsigned), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST, requestReadback), "mip request proof readback");
        constexpr unsigned warmPairs = 4, measuredPairs = 12;
        unsigned frameNumber = 0, captureProofs = 0, exactPairs = 0;
        auto percentile = [](std::vector<double> samples, double p) {
            std::sort(samples.begin(), samples.end());
            return samples[std::min(samples.size() - 1,
                static_cast<size_t>(std::ceil(p * samples.size()) - 1))];
        };
        for (const auto dimensions : {std::pair<unsigned,unsigned>{1280,720}, {1920,1080}}) {
            const unsigned width = dimensions.first, height = dimensions.second;
            require(raytracingCreateOutput(width, height), "performance output creation");
            const auto outputDescription = state.output->GetDesc();
            require(outputDescription.Format == DXGI_FORMAT_R16G16B16A16_FLOAT,
                "performance output byte comparison format");
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT outputLayout = {};
            UINT64 outputBytes = 0;
            device->GetCopyableFootprints(&outputDescription, 0, 1, 0,
                &outputLayout, nullptr, nullptr, &outputBytes);
            ComPtr<ID3D12Resource> outputReadback;
            require(createBuffer(outputBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COPY_DEST, outputReadback), "performance output readback");
            std::vector<double> baselineTimes, captureTimes, pairedDeltas;
            std::vector<double> baselineCpuTimes, captureCpuTimes;
            unsigned resolutionProofs = 0;
            for (unsigned pair = 0; pair < warmPairs + measuredPairs; ++pair) {
                std::vector<unsigned char> pixels[2];
                double gpuMilliseconds[2] = {}, cpuMilliseconds[2] = {};
                // Alternate AB / BA order to reduce clock/thermal order bias.
                for (unsigned order = 0; order < 2; ++order) {
                    const bool capture = ((order + pair) & 1u) != 0;
                    const unsigned variant = capture ? 1u : 0u;
                    // The usual frame-slot fence has completed in submit().
                    // Keep tokens monotonic, selecting only the existing
                    // production modulo-60 capture decision; no shader fork.
                    const auto nextSerial = ((state.materialFrameSerial / 60u) + 1u) * 60u +
                        (capture ? 0u : 1u);
                    state.materialFrameSerial = nextSerial - 1u;
                    raytracingSetFrameSlot(frameNumber++ % dxrFrameResourceCount);
                    raytracingBeginFrame();
                    raytracingSetJitter(0, 0);
                    raytracingBeginScene(60, float(width) / float(height), identity);
                    raytracingSubmitGeometry(&mesh, vertices, 3, 12, indices, 1, 6, 0,
                        &surface, sizeof(surface), transform, baseColor, 3);
                    raytracingEndScene();
                    // Both paths use the same seed, camera, settings and reset
                    // history, so neither consumes the other path's image.
                    state.frameSeed = 0x1230u + pair;
                    state.previousSceneHash = ~buildSceneHash();
                    state.accumulationFrame = 0;
                    const auto capturesBefore = state.feedbackCaptures;
                    commands->EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
                    const auto cpuStart = std::chrono::steady_clock::now();
                    require(raytracingRecord(commands.Get(), sky.Get(), nullptr, 0, nullptr, 0),
                        "performance production ray dispatch");
                    cpuMilliseconds[variant] = std::chrono::duration<double,std::milli>(
                        std::chrono::steady_clock::now() - cpuStart).count();
                    commands->EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
                    commands->ResolveQueryData(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                        0, 2, timestampReadback.Get(), 0);
                    require(state.feedbackCaptures == capturesBefore + (capture ? 1u : 0u),
                        "performance frame selected wrong production PSO");
                    // All diagnostic copies are deliberately outside timing.
                    transitionResource(commands.Get(), state.output.Get(),
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
                    D3D12_TEXTURE_COPY_LOCATION source = {}, destination = {};
                    source.pResource = state.output.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    destination.pResource = outputReadback.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    destination.PlacedFootprint = outputLayout;
                    commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
                    transitionResource(commands.Get(), state.output.Get(),
                        D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    if (capture) {
                        transitionResource(commands.Get(), state.materialMipRequests.Get(),
                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
                        commands->CopyBufferRegion(requestReadback.Get(), 0, state.materialMipRequests.Get(),
                            usedTextureIndex * sizeof(unsigned), sizeof(unsigned));
                        commands->CopyBufferRegion(requestReadback.Get(), sizeof(unsigned), state.materialMipRequests.Get(),
                            unusedTextureIndex * sizeof(unsigned), sizeof(unsigned));
                        transitionResource(commands.Get(), state.materialMipRequests.Get(),
                            D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    }
                    transitionResource(commands.Get(), state.motion.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    submit();
                    void *mapped = nullptr;
                    require(SUCCEEDED(timestampReadback->Map(0, nullptr, &mapped)), "performance timestamps mapped");
                    const auto ticks = static_cast<const UINT64 *>(mapped);
                    require(ticks[1] >= ticks[0], "performance timestamps ordered");
                    gpuMilliseconds[variant] = double(ticks[1] - ticks[0]) * 1000.0 / double(timestampFrequency);
                    timestampReadback->Unmap(0, nullptr);
                    require(SUCCEEDED(outputReadback->Map(0, nullptr, &mapped)), "performance output mapped");
                    pixels[variant].resize(size_t(width) * height * 8u);
                    for (unsigned y = 0; y < height; ++y)
                        std::memcpy(pixels[variant].data() + size_t(y) * width * 8u,
                            static_cast<const unsigned char *>(mapped) + outputLayout.Offset +
                            size_t(y) * outputLayout.Footprint.RowPitch, size_t(width) * 8u);
                    outputReadback->Unmap(0, nullptr);
                    if (capture) {
                        require(SUCCEEDED(requestReadback->Map(0, nullptr, &mapped)), "performance mip proof mapped");
                        unsigned requests[2]; std::memcpy(requests, mapped, sizeof(requests));
                        requestReadback->Unmap(0, nullptr);
                        float requestedMip; std::memcpy(&requestedMip, requests, sizeof(float));
                        require(requests[0] != UINT_MAX && std::isfinite(requestedMip) && requestedMip >= 0 &&
                            requestedMip < 3 && requests[1] == UINT_MAX, "nonempty material min-LOD atomics proof");
                        auto &frame = state.frames[state.activeFrameSlot];
                        auto texture = state.streamingMaterials[usedTextureIndex];
                        require(texture && !frame.feedbackTextures.empty() && texture->feedbackPending(state.activeFrameSlot),
                            "production hardware feedback decode pending");
                        require(texture->consumeFeedback(state.activeFrameSlot, frame.feedbackSerial) &&
                            !texture->feedbackPending(state.activeFrameSlot) && texture->packedFirstMip() > 0 &&
                            texture->finestWarmMip() < texture->packedFirstMip(), "nonempty decoded hardware feedback proof");
                        ++captureProofs; ++resolutionProofs;
                    }
                }
                require(pixels[0] == pixels[1], "feedback capture changed production output pixels");
                ++exactPairs;
                if (pair >= warmPairs) {
                    baselineTimes.push_back(gpuMilliseconds[0]); captureTimes.push_back(gpuMilliseconds[1]);
                    pairedDeltas.push_back(gpuMilliseconds[1] - gpuMilliseconds[0]);
                    baselineCpuTimes.push_back(cpuMilliseconds[0]); captureCpuTimes.push_back(cpuMilliseconds[1]);
                }
            }
            const double baselineMedian = percentile(baselineTimes, .5), captureMedian = percentile(captureTimes, .5);
            const double pairedMedian = percentile(pairedDeltas, .5);
            std::cout << std::fixed << std::setprecision(4) << "FEEDBACK_PERF " << width << 'x' << height
                << " pairs=" << measuredPairs << " warm_pairs=" << warmPairs
                << " baseline_gpu_median_ms=" << baselineMedian << " baseline_gpu_p95_ms=" << percentile(baselineTimes, .95)
                << " capture_gpu_median_ms=" << captureMedian << " capture_gpu_p95_ms=" << percentile(captureTimes, .95)
                << " paired_delta_median_ms=" << pairedMedian
                << " capture_delta_pct=" << (captureMedian / baselineMedian - 1.0) * 100.0
                << " amortized_1_in_60_delta_ms=" << pairedMedian / 60.0
                << " baseline_cpu_record_median_ms=" << percentile(baselineCpuTimes, .5)
                << " capture_cpu_record_median_ms=" << percentile(captureCpuTimes, .5)
                << " exact_pairs=" << warmPairs + measuredPairs << " nonempty_capture_proofs=" << resolutionProofs << '\n';
        }
        if (messages) for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
            SIZE_T size = 0; messages->GetMessage(i, nullptr, &size);
            std::vector<unsigned char> storage(size); auto message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
            messages->GetMessage(i, message, &size);
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                std::cerr << message->pDescription << '\n'; throw std::runtime_error("performance D3D12 validation error");
            }
        }
        std::cout << "PASS: optional feedback performance/parity mode; " << exactPairs
            << " byte-identical full-frame pairs and " << captureProofs
            << " proven nonempty atomics/hardware feedback captures. GPU timestamps exclude diagnostic copies; "
               "CPU recording includes the debug layer when available. Single-triangle fixture, not game FPS.\n";
        commands->Close(); CloseHandle(event); raytracingShutdown(); return 0;
    }
    // Exercise the optional production feedback PSO immediately; subsequent
    // frames return to the baseline PSO and rebuild its cached shader table.
    state.materialFrameSerial = disableFeedbackCapture ? 0 : 59;
    for (unsigned int frame = 0; frame < 5; ++frame) {
        raytracingSetFrameSlot(frame % 3);
        raytracingBeginFrame();
        raytracingBeginScene(60, 1, identity);
        raytracingSubmitGeometry(&mesh, vertices, 3, 12, indices, 1, 6, 0,
            &surface, sizeof(surface), transform, baseColor, 3);
        raytracingEndScene();
        require(!state.instances.empty(), "submitted geometry");
        require(ensureMaterialTextures(commands.Get(), state.frames[state.activeFrameSlot]), "material upload");
        require(raytracingRecord(commands.Get(), sky.Get(), nullptr, 0, nullptr, 0), "ray dispatch");
        if (frame == 0 && !disableFeedbackCapture && state.samplerFeedback.supported())
        {
            require(!!state.feedbackStateObject && state.feedbackCaptures == 1,
                "supported device executes the feedback capture pipeline");
            require(!state.frames[state.activeFrameSlot].feedbackTextures.empty(),
                "feedback capture decodes a real material texture");
        }
        float sunView[12] = {};
        require(raytracingSunVisibilityScene(sunView) != 0 && sunView[0] == 1.0f &&
            sunView[5] == 1.0f && sunView[10] == 1.0f, "dust sun scene transform");
        // Match the presenter's final motion-vector sampling transition.
        transitionResource(commands.Get(), state.motion.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        submit();
    }
    if (argc>1 && std::strcmp(argv[1],"threaded")==0) {
#include "../tests/ThreadedDxrReplay.inc"
    }
    require(state.materialTextures.size() == 2, "registered material indices stay stable");
    require(state.materialTextures[usedTextureIndex] && !state.materialTextures[unusedTextureIndex],
        "unused registered texture remains an unallocated GPU hole");
    require(state.materialTextures[usedTextureIndex]->GetDesc().MipLevels == 10, "complete mip chain");
    auto readMaterialPixels = [&](unsigned int index) {
        auto resource = state.materialTextures[index];
        require(!!resource, "resident material readback");
        const auto desc = resource->GetDesc();
        const UINT subresources = desc.MipLevels * desc.DepthOrArraySize;
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresources);
        UINT64 size = 0;
        device->GetCopyableFootprints(&desc, 0, subresources, 0,
            layouts.data(), nullptr, nullptr, &size);
        ComPtr<ID3D12Resource> readback;
        require(createBuffer(size, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST, readback), "material readback allocation");
        transitionResource(commands.Get(), resource.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
        for (UINT subresource = 0; subresource < subresources; ++subresource) {
            D3D12_TEXTURE_COPY_LOCATION destination = {};
            destination.pResource = readback.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            destination.PlacedFootprint = layouts[subresource];
            D3D12_TEXTURE_COPY_LOCATION source = {};
            source.pResource = resource.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source.SubresourceIndex = subresource;
            commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
        transitionResource(commands.Get(), resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        submit();
        void *mapped = nullptr;
        require(SUCCEEDED(readback->Map(0, nullptr, &mapped)), "material pixels mapped");
        std::vector<unsigned char> pixels;
        for (const auto &layout : layouts)
            for (UINT y = 0; y < layout.Footprint.Height; ++y) {
                const auto row = static_cast<const unsigned char *>(mapped) +
                    layout.Offset + y * layout.Footprint.RowPitch;
                pixels.insert(pixels.end(), row, row + layout.Footprint.Width * 4);
            }
        readback->Unmap(0, nullptr);
        return pixels;
    };
    const auto originalMaterialPixels = readMaterialPixels(usedTextureIndex);
    auto savedBudgetAdapter = state.memoryBudgetAdapter;
    const auto savedBudget = state.materialBudgetLimitBytes;
    state.memoryBudgetAdapter.Reset();
    state.materialBudgetLimitBytes = ~0ull;
    auto drawMaterial = [&](unsigned int frame, bool replacement) {
        raytracingSetFrameSlot(frame % 3);
        raytracingBeginFrame();
        raytracingBeginScene(60, 1, identity);
        auto selected = surface;
        if (replacement) selected.materialIdentity = &unusedMaterial;
        raytracingSubmitGeometry(&mesh, vertices, 3, 12, indices, 1, 6,
            replacement ? 1 : 0, &selected, sizeof(selected), transform, baseColor, 3);
        raytracingEndScene();
        require(raytracingRecord(commands.Get(), sky.Get(), nullptr, 0, nullptr, 0),
            "residency-switch ray dispatch");
        transitionResource(commands.Get(), state.motion.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        submit();
    };
    unsigned int switchFrame = 5;
    {
        const auto beforeSensors = state.materialTextures[usedTextureIndex];
        const auto uploadsBeforeSensors = state.materialUploads;
        const auto streamed = state.streamingMaterials[usedTextureIndex];
        const auto bytesBeforeSensors = streamed ? streamed->mappedBytes() : 0;
        raytracingSetSensorViewActive(true);
        // Many completed presenter slots with no world dispatch: the Sensors
        // view must not discard the fleet that will reappear on exit.
        for (unsigned slot = 0; slot < 180; ++slot)
        {
            raytracingSetFrameSlot(slot % 3);
            require(state.materialTextures[usedTextureIndex].Get() == beforeSensors.Get(),
                "Sensors preserves the previous world material");
            if (streamed)
                require(streamed->readyForSampling() && streamed->mappedBytes() == bytesBeforeSensors,
                    "Sensors retains full mip residency through every completed slot");
        }
        raytracingSetSensorViewActive(false);
        drawMaterial(5, false);
        require(state.materialUploads == uploadsBeforeSensors &&
            state.materialTextures[usedTextureIndex].Get() == beforeSensors.Get(),
            "return from Sensors reuses material without regeneration or upload");
        std::cout << "PASS: Sensors residency survives 180 no-world frames; return performs zero material uploads\n";
        // Refill all three world-use masks for the independent retirement
        // checks below; the no-world interval legitimately retired the old ones.
        drawMaterial(6, false);
        drawMaterial(7, false);
        switchFrame = 8;
    }
    // Ordinary visibility changes must keep cold materials intact while the
    // active + warm cache is below budget. Previously every completed slot
    // discarded fine mip heaps here even with abundant free memory.
    const auto warmResource = state.materialTextures[usedTextureIndex];
    const auto warmStreaming = state.streamingMaterials[usedTextureIndex];
    const auto warmBytes = warmStreaming ? warmStreaming->mappedBytes() : 0;
    for (unsigned int frame = 0; frame < 3; ++frame)
        drawMaterial(switchFrame++, true);
    require(state.materialTextures[usedTextureIndex].Get() == warmResource.Get(),
        "below-budget visibility changes preserve the original resource");
    if (warmStreaming)
        require(warmStreaming->readyForSampling() && warmStreaming->mappedBytes() == warmBytes,
            "below-budget cold material retains every fine mip");
    const auto warmUploads = state.materialUploads;
    drawMaterial(switchFrame++, false);
    require(state.materialUploads == warmUploads,
        "returning to a warm material performs zero uploads");
    require(readMaterialPixels(usedTextureIndex) == originalMaterialPixels,
        "warm visibility changes preserve all four channels and every mip");
    // Refill the old material's slot masks before pressure tests below.
    drawMaterial(switchFrame++, false);
    drawMaterial(switchFrame++, false);
    std::cout << "PASS: below-budget visibility changes retain full texture detail and return with zero uploads\n";
    if (!disableFeedbackCapture && state.streamingMaterials[usedTextureIndex])
    {
        require(state.feedbackCaptures > 0, "production DXR feedback PSO dispatched");
        const auto fullResidentBytes = state.streamingMaterials[usedTextureIndex]->mappedBytes();
        // Force a small real deficit. Fine mips should leave only in response
        // to pressure, and a retained tail must avoid whole-resource eviction.
        for (unsigned int frame = 0; frame < 3; ++frame)
            drawMaterial(switchFrame++, true);
        state.materialBudgetLimitBytes = state.materialResidency.residentBytes() - 1;
        const auto evictionsBeforeTrim = state.materialEvictions;
        trimMaterialTextures();
        require(state.materialTextures[usedTextureIndex] &&
            state.streamingMaterials[usedTextureIndex]->mappedBytes() < fullResidentBytes &&
            !state.streamingMaterials[usedTextureIndex]->readyForSampling(),
            "inactive mip heaps trimmed from production DXR hardware feedback");
        require(state.materialEvictions == evictionsBeforeTrim,
            "small pressure deficit is satisfied without whole-material eviction");
        state.materialBudgetLimitBytes = ~0ull;
        drawMaterial(switchFrame++, false);
        require(state.streamingMaterials[usedTextureIndex]->readyForSampling(),
            "full quality restored before reactivated DXR dispatch");
        require(readMaterialPixels(usedTextureIndex) == originalMaterialPixels,
            "production warm-mip restoration preserves every texture channel and mip");
        std::cout << "PASS: production DXR feedback capture, inactive physical mip trimming and exact full-quality restoration\n";
        drawMaterial(switchFrame++, false);
        drawMaterial(switchFrame++, false);
    }
    state.materialBudgetLimitBytes = 0; // Force cold eviction after warm-cache checks.
    drawMaterial(switchFrame, true);
    require(!!state.materialTextures[usedTextureIndex], "old material survives unfinished flight slots");
    drawMaterial(switchFrame + 1, true);
    require(!!state.materialTextures[usedTextureIndex], "all old descriptor users must retire");
    drawMaterial(switchFrame + 2, true);
    require(!state.materialTextures[usedTextureIndex] && state.materialTextures[unusedTextureIndex],
        "cold material evicted only after every old slot completes");
    const auto uploadsBefore = state.materialUploads;
    drawMaterial(switchFrame + 3, false);
    require(state.materialUploads == uploadsBefore + 1 && state.materialTextures[usedTextureIndex],
        "material reuploads at its original stable descriptor index");
    require(readMaterialPixels(usedTextureIndex) == originalMaterialPixels,
        "all material pixels and every mip remain byte-exact after eviction and reupload");
    state.memoryBudgetAdapter = savedBudgetAdapter;
    state.materialBudgetLimitBytes = savedBudget;
    std::cout << "PASS: active-only GPU material uploads, fence-safe eviction, stable descriptor reupload and byte-exact four-channel mip pixels\n";
    // Test the actual any-hit plane on an opaque, normal-mapped triangle.
    // Read primary hit identities: a partial plane must cut only one half;
    // a negative constant plane must eliminate every material contribution.
    auto clippedHitCount = [&](unsigned int frame, unsigned int mode) {
        raytracingSetFrameSlot(frame%3);
        raytracingBeginFrame();
        raytracingBeginScene(60,1,identity);
        raytracingSubmitGeometry(&mesh,vertices,3,12,indices,1,6,0,
            &surface,sizeof(surface),transform,baseColor,3);
        SceneInstance &instance=state.instances.back();
        instance.clipEnabled=mode?1:0;
        instance.clipEquationEye[0]=mode==1?1.f:0.f;
        instance.clipEquationEye[1]=instance.clipEquationEye[2]=0;
        instance.clipEquationEye[3]=mode==2?-1.f:0.f;
        raytracingEndScene();
        require(raytracingRecord(commands.Get(),sky.Get(),nullptr,0,nullptr,0),"clipped ray dispatch");
        auto metadata=state.surfaceHistoryPrevious;
        ComPtr<ID3D12Resource> readback;
        UINT64 bytes=metadata->GetDesc().Width;
        require(createBuffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST,readback),"clip metadata readback");
        transitionResource(commands.Get(),metadata.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands->CopyBufferRegion(readback.Get(),0,metadata.Get(),0,bytes);
        transitionResource(commands.Get(),metadata.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transitionResource(commands.Get(),state.motion.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        submit();
        unsigned int *data=nullptr,count=0;
        readback->Map(0,nullptr,reinterpret_cast<void **>(&data));
        for(UINT64 pixel=0;pixel<bytes/16;++pixel) if(data[pixel*4]) ++count;
        readback->Unmap(0,nullptr);
        return count;
    };
    unsigned int whole=clippedHitCount(5,0),partial=clippedHitCount(6,1),hidden=clippedHitCount(7,2);
    require(whole>0 && partial>0 && partial<whole && hidden==0,"opaque hyperspace clipping on GPU");
    require(clippedHitCount(8,0)>0,"clip state removed for next instance/frame");
    // Re-establish stable history after the deliberate disocclusion before
    // the existing convergence assertion at the end of this smoke test.
    for(unsigned int frame=9;frame<14;++frame) require(clippedHitCount(frame,0)>0,"restored unclipped scene");
    std::cout << "PASS: opaque normal-mapped hyperspace clipping: " << whole << " full / " << partial << " partial / " << hidden << " hidden hits\n";
    // Execute the production sunlight shader, not merely PSO creation. The
    // triangle at z=-3 must cast a shadow into a zero-cloud world-space field.
    ComPtr<ID3D12Resource> sunField, sunUpload, sunReadback;
    ComPtr<ID3D12DescriptorHeap> sunHeap;
    if (sunPipeline) {
        D3D12_RESOURCE_DESC field = {};
        field.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        field.Width = field.Height = field.DepthOrArraySize = 8;
        field.MipLevels = 1; field.SampleDesc.Count = 1;
        field.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        field.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        require(SUCCEEDED(device->CreateCommittedResource(&properties,
            D3D12_HEAP_FLAG_NONE, &field, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr, IID_PPV_ARGS(&sunField))), "sun field allocation");
        require(createBuffer(512, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ, sunUpload), "sun constants");
        float constants[128] = {};
        constants[7]=1; constants[18]=8; constants[19]=3;
        constants[42]=1; constants[43]=1;
        constants[52]=-2; constants[53]=-2; constants[54]=-5; constants[55]=8;
        constants[56]=4; constants[57]=4; constants[58]=4; constants[59]=8;
        UINT64 sunScene=raytracingSunVisibilityScene(constants+64);
        constants[76]=1; constants[77]=100; constants[78]=0.001f;
        void *upload=nullptr; sunUpload->Map(0,nullptr,&upload);
        std::memcpy(upload,constants,sizeof(constants)); sunUpload->Unmap(0,nullptr);
        D3D12_DESCRIPTOR_HEAP_DESC sunHeapDesc = {};
        sunHeapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        sunHeapDesc.NumDescriptors=4; sunHeapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        require(SUCCEEDED(device->CreateDescriptorHeap(&sunHeapDesc,
            IID_PPV_ARGS(&sunHeap))), "sun descriptors");
        auto cpu=sunHeap->GetCPUDescriptorHandleForHeapStart();
        UINT increment=device->GetDescriptorHandleIncrementSize(sunHeapDesc.Type);
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv = {};
        nullSrv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
        nullSrv.Buffer.NumElements=1; nullSrv.Buffer.StructureByteStride=256;
        for (unsigned int i=0;i<3;++i) {
            device->CreateShaderResourceView(nullptr,&nullSrv,cpu); cpu.ptr+=increment;
        }
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
        uav.Format=field.Format; uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;
        uav.Texture3D.WSize=8;
        device->CreateUnorderedAccessView(sunField.Get(),nullptr,&uav,cpu);
        ID3D12DescriptorHeap *sunHeaps[]={sunHeap.Get()};
        commands->SetDescriptorHeaps(1,sunHeaps);
        commands->SetPipelineState(sunPipeline.Get());
        commands->SetComputeRootSignature(sunRoot.Get());
        auto gpu=sunHeap->GetGPUDescriptorHandleForHeapStart();
        commands->SetComputeRootDescriptorTable(0,gpu); gpu.ptr+=3*increment;
        commands->SetComputeRootDescriptorTable(2,gpu);
        commands->SetComputeRootConstantBufferView(1,sunUpload->GetGPUVirtualAddress());
        commands->SetComputeRootShaderResourceView(3,sunScene);
        commands->SetComputeRootConstantBufferView(4,sunUpload->GetGPUVirtualAddress()+256);
        commands->Dispatch(1,1,1);
        transitionResource(commands.Get(),sunField.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT64 bytes=0;
        device->GetCopyableFootprints(&field,0,1,0,&footprint,nullptr,nullptr,&bytes);
        require(createBuffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST,sunReadback),"sun readback");
        D3D12_TEXTURE_COPY_LOCATION from = {},to = {};
        from.pResource=sunField.Get(); from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        to.pResource=sunReadback.Get(); to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        to.PlacedFootprint=footprint;
        commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        submit();
        void *data=nullptr; sunReadback->Map(0,nullptr,&data);
        unsigned int shadows=0,lit=0;
        for (unsigned int z=0;z<8;++z) for (unsigned int y=0;y<8;++y)
        for (unsigned int x=0;x<8;++x) {
            auto row=reinterpret_cast<float *>(static_cast<unsigned char *>(data)+
                (z*8+y)*footprint.Footprint.RowPitch);
            float visibility=row[x*4+3];
            require(std::isfinite(visibility),"finite sun visibility");
            require(std::abs(row[x*4]-visibility)<0.00001f,"ordinary blockers affect both shaft and cloud channels");
            if (visibility<0.5f) ++shadows; else ++lit;
        }
        sunReadback->Unmap(0,nullptr);
        require(shadows>0 && lit>0,"zero-cloud field contains lit AND geometry-shadowed cells");
        std::cout << "PASS: production sun visibility dispatch: " << shadows <<
            " shadowed / " << lit << " lit cells, no authored clouds\n";
        // Same actual blocker remains in cloud shadows but is exempt from shafts.
        const unsigned int originalMask=state.instances[0].instanceMask;
        state.instances[0].instanceMask=solidShadowMask(3u|excludeAtmosphericShafts);
        ComPtr<ID3D12GraphicsCommandList4> sunCommands; commands.As(&sunCommands);
        require(buildTopLevel(sunCommands.Get(),state.frames[state.activeFrameSlot]),"exempt blocker TLAS update");
        transitionResource(commands.Get(),sunField.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commands->SetDescriptorHeaps(1,sunHeaps);
        commands->SetPipelineState(sunPipeline.Get()); commands->SetComputeRootSignature(sunRoot.Get());
        gpu=sunHeap->GetGPUDescriptorHandleForHeapStart(); commands->SetComputeRootDescriptorTable(0,gpu); gpu.ptr+=3*increment;
        commands->SetComputeRootDescriptorTable(2,gpu);
        commands->SetComputeRootConstantBufferView(1,sunUpload->GetGPUVirtualAddress());
        commands->SetComputeRootShaderResourceView(3,raytracingSunVisibilityScene(constants+64));
        commands->SetComputeRootConstantBufferView(4,sunUpload->GetGPUVirtualAddress()+256);
        commands->Dispatch(1,1,1);
        transitionResource(commands.Get(),sunField.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands->CopyTextureRegion(&to,0,0,0,&from,nullptr); submit();
        sunReadback->Map(0,nullptr,&data); unsigned int exemptShadows=0;
        for(unsigned int z=0;z<8;++z) for(unsigned int y=0;y<8;++y) for(unsigned int x=0;x<8;++x) {
            auto row=reinterpret_cast<float *>(static_cast<unsigned char *>(data)+(z*8+y)*footprint.Footprint.RowPitch);
            require(std::abs(row[x*4]-1.0f)<0.00001f,"exempt asteroid creates no atmospheric shaft occlusion");
            if(row[x*4+3]<0.5f) ++exemptShadows;
        }
        sunReadback->Unmap(0,nullptr); require(exemptShadows==shadows,"exemption preserves actual cloud shadows");
        state.instances[0].instanceMask=originalMask;
        std::cout << "PASS: exempt asteroid has zero shaft occlusion while retaining all cloud-shadow cells\n";
    }
    // Execute the real cloud lighting function, not just its visibility query.
    // This catches an unused visibility argument even when all ray tests pass.
    {
        std::ifstream sourceFile(HW_DUST_SOURCE_PATH);
        require(!!sourceFile, "production dust shader source");
        std::ostringstream sourceStream; sourceStream << sourceFile.rdbuf();
        std::string source = sourceStream.str();
        // Isolate the production integrator using a deterministic shadow tube.
        // The actual texture/ray-query path is tested separately above.
        auto visibilityStart=source.find("float DustProceduralSunVisibility(");
        auto visibilityEnd=source.find("float DustShadowStepLength(",visibilityStart);
        require(visibilityStart!=std::string::npos && visibilityEnd!=std::string::npos,"shaft integrator located");
        source.replace(visibilityStart,visibilityEnd-visibilityStart,
            "float DustProceduralSunVisibility(float3 p, bool shafts = false) { return abs(p.x)<0.25 ? 0.0 : 1.0; }\n");
        source += R"(
RWStructuredBuffer<float4> LightingTestOutput : register(u5);
[numthreads(1,1,1)] void LightingTestCS(uint3 id : SV_DispatchThreadID) {
    DustVolume volume = (DustVolume)0;
    volume.flags = DUST_RECEIVE_MISSION_KEY;
    volume.scattering = 1.0;
    DustRotationContext rotation = (DustRotationContext)0;
    float3 lit = DustLightingAt(volume,rotation,0,0,0,float3(0,0,1),1.0);
    float3 shadow = DustLightingAt(volume,rotation,0,0,0,float3(0,0,1),0.0);
    LightingTestOutput[0] = float4(lit,1);
    LightingTestOutput[1] = float4(shadow,1);
    LightingTestOutput[2] = float4(AtmosphericSunScattering(float3(0.5,0,-3),float3(0,0,1),10),1);
    LightingTestOutput[3] = float4(AtmosphericSunScattering(float3(1.4,0,-3),float3(0,0,1),10),1);
}
)";
        ComPtr<ID3DBlob> shader, errors, rootBytes;
        HRESULT compileResult = D3DCompile(source.data(),source.size(),"LightingTest",
            nullptr,nullptr,"LightingTestCS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&shader,&errors);
        if (FAILED(compileResult) && errors)
            std::cerr << static_cast<const char *>(errors->GetBufferPointer());
        require(SUCCEEDED(compileResult),"cloud lighting test compile");
        D3D12_ROOT_PARAMETER parameters[2] = {};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[1].Descriptor.ShaderRegister=5;
        D3D12_ROOT_SIGNATURE_DESC rootDescription = {};
        rootDescription.NumParameters=2; rootDescription.pParameters=parameters;
        require(SUCCEEDED(D3D12SerializeRootSignature(&rootDescription,
            D3D_ROOT_SIGNATURE_VERSION_1,&rootBytes,&errors)),"lighting test root");
        ComPtr<ID3D12RootSignature> root;
        require(SUCCEEDED(device->CreateRootSignature(0,rootBytes->GetBufferPointer(),
            rootBytes->GetBufferSize(),IID_PPV_ARGS(&root))),"lighting test root creation");
        ComPtr<ID3D12PipelineState> pipeline;
        D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature=root.Get(); pso.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
        require(SUCCEEDED(device->CreateComputePipelineState(&pso,IID_PPV_ARGS(&pipeline))),"lighting test PSO");
        ComPtr<ID3D12Resource> upload,output,readback;
        require(createBuffer(256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,upload),"lighting upload");
        require(createBuffer(64,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,output),"lighting output");
        require(createBuffer(64,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST,readback),"lighting readback");
        float constants[64] = {};
        constants[7]=1; constants[39]=1; constants[42]=1; constants[43]=1;
        unsigned int styleFlags = 8u;
        std::memcpy(&constants[16], &styleFlags, sizeof(styleFlags));
        constants[44]=constants[45]=constants[46]=1;
        constants[48]=constants[49]=constants[50]=0.1f; constants[51]=1;
        constants[19]=3; constants[52]=constants[53]=constants[54]=-2; constants[55]=8;
        constants[56]=constants[57]=constants[58]=4; constants[59]=8;
        void *mapped=nullptr; upload->Map(0,nullptr,&mapped);
        std::memcpy(mapped,constants,sizeof(constants)); upload->Unmap(0,nullptr);
        commands->SetPipelineState(pipeline.Get()); commands->SetComputeRootSignature(root.Get());
        commands->SetComputeRootConstantBufferView(0,upload->GetGPUVirtualAddress());
        commands->SetComputeRootUnorderedAccessView(1,output->GetGPUVirtualAddress());
        commands->Dispatch(1,1,1);
        transitionResource(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands->CopyResource(readback.Get(),output.Get()); submit();
        readback->Map(0,nullptr,&mapped); auto values=static_cast<float *>(mapped);
        require(std::isfinite(values[0]) && values[0]>values[4]+0.1f,
            "geometry visibility changes actual cloud sunlight");
        require(values[4]>0,"shadow preserves ambient/multiple scattering");
        std::cout << "PASS: production cloud lighting GPU output: lit=" << values[0]
            << " shadow=" << values[4] << " (ambient preserved)\n";
        require(std::isfinite(values[8]) && values[8]>0.02f,"localized shaft has measurable radiance");
        require(std::abs(values[12])<0.000001f,"unoccluded atmosphere adds no washout");
        std::cout << "PASS: production shaft integrator GPU output: beam=" << values[8]
            << " unoccluded=" << values[12] << "\n";
        float originalBeam = values[8];
        float originalCloud = values[0];
        float originalShadow = values[4];
        readback->Unmap(0,nullptr);
        constants[7]=2;
        styleFlags = 8u | 4u | (255u << 8u);
        std::memcpy(&constants[16], &styleFlags, sizeof(styleFlags));
        upload->Map(0,nullptr,&mapped);
        std::memcpy(mapped,constants,sizeof(constants)); upload->Unmap(0,nullptr);
        transitionResource(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commands->SetPipelineState(pipeline.Get()); commands->SetComputeRootSignature(root.Get());
        commands->SetComputeRootConstantBufferView(0,upload->GetGPUVirtualAddress());
        commands->SetComputeRootUnorderedAccessView(1,output->GetGPUVirtualAddress());
        commands->Dispatch(1,1,1);
        transitionResource(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands->CopyResource(readback.Get(),output.Get()); submit();
        readback->Map(0,nullptr,&mapped); values=static_cast<float *>(mapped);
        require(std::abs(values[8]-2*originalBeam)<0.00001f,"mission shaft strength doubles beam energy");
        require(std::abs(values[9])+std::abs(values[10])<0.000001f,"mission red tint affects shafts only");
        require(std::abs(values[0]-originalCloud)<0.000001f,"shaft strength and color leave cloud lighting unchanged");
        readback->Unmap(0,nullptr);
        std::cout << "PASS: mission shaft strength/color GPU output; cloud lighting unchanged\n";
        constants[7]=0; styleFlags=0;
        std::memcpy(&constants[16], &styleFlags, sizeof(styleFlags));
        upload->Map(0,nullptr,&mapped);
        std::memcpy(mapped,constants,sizeof(constants)); upload->Unmap(0,nullptr);
        transitionResource(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commands->SetPipelineState(pipeline.Get()); commands->SetComputeRootSignature(root.Get());
        commands->SetComputeRootConstantBufferView(0,upload->GetGPUVirtualAddress());
        commands->SetComputeRootUnorderedAccessView(1,output->GetGPUVirtualAddress());
        commands->Dispatch(1,1,1);
        transitionResource(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands->CopyResource(readback.Get(),output.Get()); submit();
        readback->Map(0,nullptr,&mapped); values=static_cast<float *>(mapped);
        require(std::abs(values[0]-originalCloud)<0.000001f && std::abs(values[4]-originalShadow)<0.000001f,
            "god rays disabled preserve cloud sunlight and asteroid shadow exactly");
        require(std::abs(values[8])+std::abs(values[9])+std::abs(values[10])<0.000001f,
            "god rays disabled remove standalone shafts");
        readback->Unmap(0,nullptr);
        std::cout << "PASS: god rays OFF removes shafts but preserves cloud shadows exactly\n";
    }
    ComPtr<ID3D12Resource> readback;
    require(createBuffer(64 * 64 * 16, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_COPY_DEST, readback), "readback");
    transitionResource(commands.Get(), state.surfaceHistoryPrevious.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commands->CopyResource(readback.Get(), state.surfaceHistoryPrevious.Get());
    submit();
    void *mapped = nullptr; D3D12_RANGE readRange = {0, 64*64*16};
    require(SUCCEEDED(readback->Map(0, &readRange, &mapped)), "readback mapping");
    auto center = static_cast<unsigned int *>(mapped) + (32 * 64 + 32) * 4;
    require(center[0] != 0 && center[2] >= 3, "surface history convergence");
    readback->Unmap(0, nullptr);
    if (messages) for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
        SIZE_T size = 0; messages->GetMessage(i, nullptr, &size);
        std::vector<unsigned char> storage(size);
        auto message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        messages->GetMessage(i, message, &size);
        if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
            std::cerr << message->pDescription << '\n';
            throw std::runtime_error("D3D12 validation error");
        }
    }
    if (messages) std::cout << "PASS: D3D12 debug-layer info queue checked with no validation errors\n";
    {
        static int emissionId,aliasId,blackId;
        const unsigned base[2]={0xffffffffu,0xffffffffu},emission[2]={0xff020180u,0xff201000u},black[2]={0xff000000u,0xff000000u};
        raytracingRegisterSurfaceTexture(&emissionId,2,1,base,emission,nullptr,nullptr,"raster-emission-fixture");
        RasterEmissionSource source,alias;std::vector<unsigned char> decoded;
        require(raytracingRasterEmissionSource(&emissionId,source),"shared emission source missing without raster registration");
        require(raytracingReadRasterEmission(source.token,decoded) && decoded.size()==sizeof(emission) && !std::memcmp(decoded.data(),emission,sizeof(emission)),"shared lossless emission extraction changed pixels");
        require(raytracingTryAliasSurfaceTexture(&aliasId,"raster-emission-fixture") && raytracingRasterEmissionSource(&aliasId,alias) && alias.token==source.token,"named material aliases duplicate emission backing");
        raytracingRegisterSurfaceTexture(&blackId,2,1,base,black,nullptr,nullptr,"raster-black-fixture");
        require(!raytracingRasterEmissionSource(&blackId,alias),"black map creates an emission resource");
        raytracingUnregisterSurfaceTexture(&aliasId);require(!raytracingRasterEmissionSource(&aliasId,alias),"unregistered pointer resolves stale emission");
        require(!raytracingReadRasterEmission(~std::uint64_t(0),decoded),"invalid emission token accepted");
        raytracingUnregisterSurfaceTexture(&emissionId);raytracingUnregisterSurfaceTexture(&blackId);
        std::cout<<"PASS: shared lossless emission extraction, alias reuse, black-map skip, unregister and invalid-token checks\n";
    }
    commands->Close(); CloseHandle(event);
    raytracingShutdown();
    std::cout << "PASS: DXR pipeline, GPU material mips, hyperspace clipping, history convergence, D3D12 validation\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n'; return 1;
}
