// Optional production frame-generation integration fixture. It never opens the
// game or shows/focuses its private HWND. Run only when GPU testing is permitted.
#include "../src/Modern/ModernFrameGeneration.h"
#include "../src/Modern/FrameWorker.h"
#include "../src/Modern/ModernDLAA.h"
#include <windows.h>
#include <wrl/client.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <sl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT buffers = 3;
constexpr D3D12_RESOURCE_STATES colorRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
constexpr D3D12_RESOURCE_STATES depthRead = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
void check(HRESULT result, const char *message) {
    if (FAILED(result)) {
        std::cerr << message << " HRESULT=0x" << std::hex << static_cast<unsigned long>(result) << std::dec << '\n';
        throw std::runtime_error(message);
    }
}
void barrier(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    if (before == after) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    commands->ResourceBarrier(1, &b);
}

struct Fixture {
    HWND window = nullptr;
    HANDLE fenceEvent = nullptr;
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12InfoQueue> info;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain4> swapchain;
    std::array<ComPtr<ID3D12Resource>, buffers> backbuffers;
    ComPtr<ID3D12DescriptorHeap> rtvHeap, uavHeap;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12Resource> hudless, depth, motion;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    UINT64 serial = 0;
    UINT rtvStride = 0;
    bool streamline = false;
    UINT presentInterval = 0;

    void drain() {
        if (!queue || !fence || !fenceEvent) return;
        check(queue->Signal(fence.Get(), ++serial), "signal fixture fence");
        if (fence->GetCompletedValue() < serial) {
            check(fence->SetEventOnCompletion(serial, fenceEvent), "set fixture fence event");
            require(WaitForSingleObject(fenceEvent, 15000) == WAIT_OBJECT_0, "fixture GPU fence timed out");
        }
    }
    ~Fixture() {
        // Production owns the present/interpolation threads. Stop them before
        // their borrowed input textures, queue or wrapper are released.
        hwmodern::dlaaSetFrameGenerationEnabled(false);
        hwmodern::fsrFrameGenerationSetEnabled(false);
        try { drain(); } catch (...) {}
        hwmodern::fsrFrameGenerationRelease();
        if (streamline) hwmodern::dlaaReleaseDevice();
        for (auto &b : backbuffers) b.Reset();
        swapchain.Reset();
        if (window) DestroyWindow(window);
        if (fenceEvent) CloseHandle(fenceEvent);
    }
};

void pumpPrivateWindowMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void createDevice(Fixture &f, bool useStreamline) {
    f.streamline = useStreamline;
    if (useStreamline) hwmodern::dlaaEarlyInitialize();
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();

    // Choose the actual system exports for the native case and the exact
    // production interposer exports for coexistence. Import-library ordering
    // must not accidentally turn the "native" case into another proxy test.
    HMODULE dxgiModule = nullptr, d3dModule = nullptr;
    if (useStreamline) {
        dxgiModule = d3dModule = GetModuleHandleW(L"sl.interposer.dll");
        require(dxgiModule != nullptr, "Streamline interposer did not load");
    } else {
        dxgiModule = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        d3dModule = LoadLibraryExW(L"d3d12.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    require(dxgiModule && d3dModule, "system graphics DLLs unavailable");
    using FactoryFunction = HRESULT(WINAPI *)(UINT, REFIID, void **);
    using DeviceFunction = HRESULT(WINAPI *)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    auto makeFactory = reinterpret_cast<FactoryFunction>(GetProcAddress(dxgiModule, "CreateDXGIFactory2"));
    auto makeDevice = reinterpret_cast<DeviceFunction>(GetProcAddress(d3dModule, "D3D12CreateDevice"));
    require(makeFactory && makeDevice, "graphics creation export missing");
    check(makeFactory(0, IID_PPV_ARGS(&f.factory)), "create fixture factory");
    check(f.factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&f.adapter)), "select high-performance adapter");
    DXGI_ADAPTER_DESC1 adapter{};
    check(f.adapter->GetDesc1(&adapter), "get adapter description");
    char adapterName[256]{};
    WideCharToMultiByte(CP_UTF8, 0, adapter.Description, -1, adapterName, sizeof(adapterName), nullptr, nullptr);
    std::cerr << "GPU: " << adapterName << "; vendor=0x" << std::hex << adapter.VendorId << std::dec << '\n';
    check(makeDevice(f.adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&f.device)), "create fixture device");
    f.device.As(&f.info);
    if (useStreamline) {
        // The return value is DLSS-SR support; FG capability is checked through
        // its independent getter below, rather than inferred from that value.
        hwmodern::dlaaSetDevice(f.device.Get(), f.adapter.Get());
        hwmodern::dlaaSetFrameGenerationEnabled(false);
    }
    D3D12_COMMAND_QUEUE_DESC queue{};
    queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(f.device->CreateCommandQueue(&queue, IID_PPV_ARGS(&f.queue)), "create fixture queue");
    check(f.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&f.allocator)), "create fixture allocator");
    check(f.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, f.allocator.Get(),
        nullptr, IID_PPV_ARGS(&f.commands)), "create fixture commands");
    check(f.commands->Close(), "close initial command list");
    check(f.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f.fence)), "create fixture fence");
    f.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    require(f.fenceEvent != nullptr, "create fixture fence event");
}

void createSwapchain(Fixture &f, UINT width, UINT height, bool useAmdProxy, bool upgradeOuter) {
    WNDCLASSW cls{};
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"OpenHWFrameGenerationPrivateFixture";
    require(RegisterClassW(&cls) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "register fixture window class");
    f.window = CreateWindowExW(WS_EX_TOOLWINDOW, cls.lpszClassName, L"OpenHW frame-generation fixture",
        WS_OVERLAPPEDWINDOW, 0, 0, width, height, nullptr, nullptr, cls.hInstance, nullptr);
    require(f.window != nullptr, "create private hidden HWND");
    require(!IsWindowVisible(f.window), "fixture HWND must remain hidden");

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width; desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = buffers;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    if (useAmdProxy) {
        IDXGISwapChain4 *created = nullptr;
        require(hwmodern::fsrFrameGenerationCreateSwapchain(f.factory.Get(), f.queue.Get(),
            f.window, &desc, &created), "production AMD ForHwnd creation failed");
        f.swapchain.Attach(created);
        require(f.swapchain && hwmodern::fsrFrameGenerationAvailable(), "production wrapper availability mismatch");
    } else {
        ComPtr<IDXGISwapChain1> created;
        check(f.factory->CreateSwapChainForHwnd(f.queue.Get(), f.window, &desc,
            nullptr, nullptr, &created), "create direct Streamline swapchain");
        check(created.As(&f.swapchain), "query direct Streamline swapchain4");
    }
    if (upgradeOuter) {
        // The official SL constructor AddRefs the original; Upgrade does not
        // consume our caller reference. Keep it owned until replacement is
        // known, then release it once the new outer proxy owns its own ref.
        ComPtr<IDXGISwapChain4> original;
        original.Swap(f.swapchain);
        auto *outer = original.Get();
        const sl::Result upgraded = slUpgradeInterface(reinterpret_cast<void **>(&outer));
        if (outer == original.Get()) f.swapchain.Swap(original);
        else f.swapchain.Attach(outer);
        std::cerr << "Outer AMD proxy slUpgradeInterface result=" << static_cast<unsigned>(upgraded) << '\n';
        require(upgraded == sl::Result::eOk, "upgrade outer AMD proxy for Streamline interception");
    }
    f.factory->MakeWindowAssociation(f.window, DXGI_MWA_NO_ALT_ENTER);

    D3D12_DESCRIPTOR_HEAP_DESC rtv{};
    rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; rtv.NumDescriptors = buffers;
    check(f.device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&f.rtvHeap)), "create fixture RTV heap");
    f.rtvStride = f.device->GetDescriptorHandleIncrementSize(rtv.Type);
    auto handle = f.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < buffers; ++i) {
        check(f.swapchain->GetBuffer(i, IID_PPV_ARGS(&f.backbuffers[i])), "get proxy backbuffer");
        f.device->CreateRenderTargetView(f.backbuffers[i].Get(), nullptr, handle);
        handle.ptr += f.rtvStride;
    }
    const HRESULT presentTest = f.swapchain->Present(0, DXGI_PRESENT_TEST);
    std::cerr << "Hidden-window Present(TEST) HRESULT=0x" << std::hex
        << static_cast<unsigned long>(presentTest) << std::dec
        << "; visible=" << IsWindowVisible(f.window)
        << "; minimized=" << IsIconic(f.window) << '\n';
}

void createInputs(Fixture &f, UINT width, UINT height) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto texture = [&](DXGI_FORMAT format, ComPtr<ID3D12Resource> &resource) {
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = width; d.Height = height; d.DepthOrArraySize = 1; d.MipLevels = 1;
        d.Format = format; d.SampleDesc.Count = 1;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        check(f.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource)), "create FG input texture");
    };
    texture(DXGI_FORMAT_R8G8B8A8_UNORM, f.hudless);
    texture(DXGI_FORMAT_R32_FLOAT, f.depth);
    texture(DXGI_FORMAT_R16G16_FLOAT, f.motion);
    D3D12_DESCRIPTOR_HEAP_DESC descriptors{};
    descriptors.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptors.NumDescriptors = 3; descriptors.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    check(f.device->CreateDescriptorHeap(&descriptors, IID_PPV_ARGS(&f.uavHeap)), "create fixture UAV heap");
    auto cpu = f.uavHeap->GetCPUDescriptorHandleForHeapStart();
    for (auto *resource : {f.hudless.Get(), f.depth.Get(), f.motion.Get()}) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = resource->GetDesc().Format; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        f.device->CreateUnorderedAccessView(resource, nullptr, &uav, cpu);
        cpu.ptr += f.device->GetDescriptorHandleIncrementSize(descriptors.Type);
    }
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; range.NumDescriptors = 3;
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable = {1, &range};
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants = {0, 0, 4};
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = 2; root.pParameters = params;
    ComPtr<ID3DBlob> bytes, errors;
    check(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &bytes, &errors), "serialize fixture root");
    check(f.device->CreateRootSignature(0, bytes->GetBufferPointer(), bytes->GetBufferSize(),
        IID_PPV_ARGS(&f.root)), "create fixture root");
    static const char shader[] = R"(
RWTexture2D<float4> Color : register(u0);
RWTexture2D<float> Depth : register(u1);
RWTexture2D<float2> Motion : register(u2);
cbuffer Parameters : register(b0) { uint Width, Height, Frame, Reset; };
[numthreads(8,8,1)] void main(uint3 p : SV_DispatchThreadID) {
    if (p.x >= Width || p.y >= Height) return;
    uint left = 20 + 2 * Frame;
    bool isObject = p.x >= left && p.x < left + Width / 5 && p.y > Height / 3 && p.y < Height * 2 / 3;
    float grid = ((p.x / 12 + p.y / 12) & 1) ? 0.14 : 0.22;
    Color[p.xy] = isObject ? float4(0.9,0.25,0.1,1) : float4(grid,grid * 1.4,grid * 1.8,1);
    float n = 0.05, f = 500000.0, z = 100.0;
    Depth[p.xy] = isObject ? f * (z - n) / (z * (f - n)) : 1.0;
    Motion[p.xy] = isObject && Reset == 0 ? float2(-2.0,0.0) : float2(0.0,0.0);
})";
    const HRESULT compiled = D3DCompile(shader, sizeof(shader) - 1, "frame_generation_fixture", nullptr, nullptr,
        "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytes, &errors);
    if (FAILED(compiled) && errors) std::cerr.write(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());
    check(compiled, "compile fixture shader");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = f.root.Get();
    pipeline.CS = {bytes->GetBufferPointer(), bytes->GetBufferSize()};
    check(f.device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&f.pipeline)), "create fixture compute pipeline");
}

struct PresentProbe {
    bool supported = false;
    UINT count = 0;
};
PresentProbe presentCount(Fixture &f) {
    PresentProbe result;
    result.supported = SUCCEEDED(f.swapchain->GetLastPresentCount(&result.count));
    return result;
}
PresentProbe settlePresents(Fixture &f) {
    PresentProbe previous = presentCount(f);
    unsigned stable = 0;
    for (unsigned i = 0; i < 200; ++i) {
        Sleep(5);
        pumpPrivateWindowMessages();
        const auto current = presentCount(f);
        if (current.supported && previous.supported && current.count == previous.count) ++stable;
        else stable = 0;
        previous = current;
        if (stable >= 10) break;
    }
    return previous;
}

void checkDebugMessages(Fixture &f);

bool workerPreparation=false;
hwmodern::FrameWorker postReplayWorker(L"Offline FG post recording");
void render(Fixture &f, UINT width, UINT height, UINT frame, bool initialized,
    bool amd, bool nvidia, float dt) {
    f.drain();
    check(f.allocator->Reset(), "reset fixture allocator");
    check(f.commands->Reset(f.allocator.Get(), nullptr), "reset fixture commands");
    if (f.streamline) hwmodern::dlaaFrameGenerationBeginFrame(frame);
    auto record=[&] {
    if (initialized) {
        barrier(f.commands.Get(), f.hudless.Get(), colorRead, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        barrier(f.commands.Get(), f.depth.Get(), depthRead, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        barrier(f.commands.Get(), f.motion.Get(), colorRead, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    f.commands->SetPipelineState(f.pipeline.Get());
    f.commands->SetComputeRootSignature(f.root.Get());
    ID3D12DescriptorHeap *heaps[] = {f.uavHeap.Get()};
    f.commands->SetDescriptorHeaps(1, heaps);
    f.commands->SetComputeRootDescriptorTable(0, f.uavHeap->GetGPUDescriptorHandleForHeapStart());
    const bool resetHistory = frame == 0 || frame == 4;
    const UINT constants[] = {width, height, frame, resetHistory ? 1u : 0u};
    f.commands->SetComputeRoot32BitConstants(1, 4, constants, 0);
    f.commands->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
    barrier(f.commands.Get(), f.hudless.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    barrier(f.commands.Get(), f.depth.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, depthRead);
    barrier(f.commands.Get(), f.motion.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, colorRead);
    const UINT index = f.swapchain->GetCurrentBackBufferIndex();
    auto *back = f.backbuffers[index].Get();
    barrier(f.commands.Get(), back, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    f.commands->CopyResource(back, f.hudless.Get());
    barrier(f.commands.Get(), f.hudless.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, colorRead);
    barrier(f.commands.Get(), back, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto rtv = f.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += index * f.rtvStride;
    const float ui[] = {0.15f, 0.9f, 0.85f, 1.0f};
    const D3D12_RECT uiRect = {8, 8, static_cast<LONG>(width / 3), 20};
    f.commands->ClearRenderTargetView(rtv, ui, 1, &uiRect);
    barrier(f.commands.Get(), back, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    if (amd) {
        hwmodern::FsrFrameGenerationCamera camera;
        camera.nearPlane = 0.05f; camera.farPlane = 500000.0f;
        const bool prepared = hwmodern::fsrFrameGenerationPrepare(f.commands.Get(), f.swapchain.Get(),
            f.hudless.Get(), f.depth.Get(), f.motion.Get(), width, height, width, height,
            0.0f, 0.0f, 1.04719755f, dt, resetHistory, frame, &camera);
        if (!prepared) {
            const auto s = hwmodern::fsrFrameGenerationStats();
            std::cerr << "AMD stage=" << s.lastFailureStage << " error=" << s.lastError << '\n';
        }
        require(prepared, "production AMD prepare failed");
    }
    if (nvidia) require(hwmodern::dlaaFrameGenerationPrepare(f.commands.Get(), f.hudless.Get(),
        f.depth.Get(), f.motion.Get(), width, height, width, height, 60.0f, 0.0f, 0.0f,
        resetHistory, colorRead, depthRead, colorRead), "production NVIDIA FG prepare failed");
    check(f.commands->Close(), "close fixture commands");
    };
    if(workerPreparation){hwmodern::FrameJob job(postReplayWorker,record);require(job.wait(),"FG worker recording failed");}
    else record();
    ID3D12CommandList *lists[] = {f.commands.Get()};
    f.queue->ExecuteCommandLists(1, lists);
    if (f.streamline) hwmodern::dlaaFrameGenerationPresentBegin();
    const HRESULT presented = f.swapchain->Present(f.presentInterval, 0);
    if (f.streamline) hwmodern::dlaaFrameGenerationPresentEnd(SUCCEEDED(presented));
    if (FAILED(presented)) {
        std::cerr << "Present failure: fixture frame=" << frame << "; AMD prepared=" << amd
            << "; NVIDIA prepared=" << nvidia << "; device removal=0x" << std::hex
            << static_cast<unsigned long>(f.device->GetDeviceRemovedReason()) << std::dec << '\n';
        try { checkDebugMessages(f); } catch (const std::exception &error) { std::cerr << error.what() << '\n'; }
    }
    check(presented, "present fixture frame");
    pumpPrivateWindowMessages();
}

void checkDebugMessages(Fixture &f) {
    if (!f.info) {
        std::cerr << "NOTE: D3D12 debug layer unavailable; error-free validation unproven\n";
        return;
    }
    unsigned errors = 0;
    const UINT64 count = f.info->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 i = 0; i < count; ++i) {
        SIZE_T size = 0;
        f.info->GetMessage(i, nullptr, &size);
        std::vector<char> storage(size);
        auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        if (SUCCEEDED(f.info->GetMessage(i, message, &size)) &&
            (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)) {
            ++errors;
            std::cerr << "D3D12: " << message->pDescription << '\n';
        }
    }
    require(errors == 0, "D3D12 validation errors");
}
} // namespace

int main(int argc, char **argv) try {
    bool streamline = false, nvidia = false, amdProxy = true, upgradeOuter = false, vsync = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--worker")) workerPreparation=true;
        else if (!std::strcmp(argv[i], "--streamline")) streamline = true;
        else if (!std::strcmp(argv[i], "--nvidia")) nvidia = streamline = true;
        else if (!std::strcmp(argv[i], "--nvidia-direct")) { nvidia = streamline = true; amdProxy = false; }
        else if (!std::strcmp(argv[i], "--upgrade-outer")) upgradeOuter = true;
        else if (!std::strcmp(argv[i], "--vsync")) vsync = true;
        else throw std::runtime_error("usage: frame_generation_smoke [--streamline] [--nvidia | --nvidia-direct] [--upgrade-outer] [--vsync]");
    }
    require(!upgradeOuter || (nvidia && amdProxy), "--upgrade-outer requires --nvidia AMD-off proxy mode");
    const UINT width = nvidia ? 1280u : 320u, height = nvidia ? 720u : 180u;
    Fixture f;
    f.presentInterval = vsync ? 1u : 0u;
    createDevice(f, streamline);
    createSwapchain(f, width, height, amdProxy, upgradeOuter);
    createInputs(f, width, height);
    if (nvidia) require(hwmodern::dlaaFrameGenerationAvailable(), "NVIDIA DLSS-G/Reflex unsupported by this adapter/runtime");
    std::cerr << "Mode: " << (nvidia ? (amdProxy ? "NVIDIA FG through AMD-off proxy" : "NVIDIA FG through direct Streamline swapchain") :
        (streamline ? "AMD FG with production Streamline interposer" : "AMD FG with system DXGI/D3D12"))
        << "; hidden HWND; " << width << 'x' << height << "; VSync=" << f.presentInterval << '\n';

    hwmodern::fsrFrameGenerationSetEnabled(false);
    // Match a selected NVIDIA provider in a menu: token/present markers exist,
    // while no valid scene inputs means PresentBegin explicitly keeps FG off.
    if (nvidia) hwmodern::dlaaSetFrameGenerationEnabled(true);
    for (UINT i = 0; i < 4; ++i) render(f, width, height, i, i != 0, false, false, 16.6667f);
    f.drain();
    const auto nativeBefore = settlePresents(f);
    const auto amdBefore = hwmodern::fsrFrameGenerationStats();
    const auto nvPresentedBefore = hwmodern::dlaaFrameGenerationPresentedFrames();
    const auto nvApplicationBefore = hwmodern::dlaaFrameGenerationApplicationFrames();
    hwmodern::fsrFrameGenerationSetEnabled(!nvidia);
    if (nvidia) hwmodern::dlaaSetFrameGenerationEnabled(true);

    constexpr UINT applicationFrames = 48;
    auto previous = std::chrono::steady_clock::now();
    for (UINT i = 0; i < applicationFrames; ++i) {
        const auto now = std::chrono::steady_clock::now();
        const float dt = i == 0 ? 16.6667f : std::max(0.1f,
            std::chrono::duration<float, std::milli>(now - previous).count());
        previous = now;
        render(f, width, height, i + 4, true, !nvidia, nvidia, dt);
        // Keep the NVIDIA functional fixture below common display refresh
        // limits so legitimate generated-frame pacing drops do not masquerade
        // as a broken setup. This is a functional test, never an FPS benchmark.
        Sleep(nvidia ? 34u : 8u);
    }
    f.drain();
    if (!nvidia) hwmodern::fsrFrameGenerationSuspend();
    const auto nativeAfter = settlePresents(f);
    const auto amdAfter = hwmodern::fsrFrameGenerationStats();
    const auto nvPresentedAfter = hwmodern::dlaaFrameGenerationPresentedFrames();
    const auto nvApplicationAfter = hwmodern::dlaaFrameGenerationApplicationFrames();
    const bool countSupported = nativeBefore.supported && nativeAfter.supported;
    const UINT nativeDelta = nativeAfter.count - nativeBefore.count;
    std::cerr << "Application presents=" << applicationFrames << "; native present counter supported=" << countSupported
        << "; native present delta=" << nativeDelta << '\n';
    std::cerr << "AMD configured=" << amdAfter.configuredFrames - amdBefore.configuredFrames
        << "; prepared=" << amdAfter.preparedFrames - amdBefore.preparedFrames
        << "; callbacks=" << amdAfter.generationCallbacks - amdBefore.generationCallbacks
        << "; successful dispatches=" << amdAfter.generationDispatches - amdBefore.generationDispatches
        << "; generated requests=" << amdAfter.generatedFrameRequests - amdBefore.generatedFrameRequests
        << "; dispatch failures=" << amdAfter.dispatchFailures << '\n';
    if (nvidia) {
        std::cerr << "NVIDIA provider presented=" << nvPresentedAfter - nvPresentedBefore
            << "; application=" << nvApplicationAfter - nvApplicationBefore
            << "; active=" << hwmodern::dlaaFrameGenerationActive()
            << "; reason=" << hwmodern::dlaaFrameGenerationReason() << '\n';
        require(hwmodern::dlaaFrameGenerationActive() && !hwmodern::dlaaFrameGenerationFailed(), "NVIDIA generation never became active");
        require(amdAfter.generationCallbacks == amdBefore.generationCallbacks, "disabled AMD proxy generated while NVIDIA was selected");
    } else {
        require(amdAfter.preparedFrames - amdBefore.preparedFrames == applicationFrames, "AMD prepare count mismatch");
        require(amdAfter.generationCallbacks > amdBefore.generationCallbacks, "AMD generation callback never ran");
        require(amdAfter.generationDispatches > amdBefore.generationDispatches &&
            amdAfter.generatedFrameRequests > amdBefore.generatedFrameRequests, "AMD callback never dispatched generation");
        require(!amdAfter.failed && amdAfter.dispatchFailures == 0, "AMD generation failed");
    }

    // Exercise real disabling while inputs remain alive. This must configure the
    // provider off, rather than just changing a UI/requested/active boolean.
    hwmodern::dlaaSetFrameGenerationEnabled(false);
    hwmodern::fsrFrameGenerationSetEnabled(false);
    require(!hwmodern::dlaaFrameGenerationDisableFailed(), "NVIDIA provider disable failed");
    require(!hwmodern::fsrFrameGenerationDisableFailed(), "AMD provider disable failed");
    const auto nativeBeforeOff = presentCount(f);
    const auto callbacksBeforeOff = hwmodern::fsrFrameGenerationStats().generationCallbacks;
    for (UINT i = 0; i < 4; ++i) render(f, width, height, applicationFrames + 4 + i, true, false, false, 16.6667f);
    f.drain();
    const auto nativeAfterOff = settlePresents(f);
    if (nativeBeforeOff.supported && nativeAfterOff.supported) {
        const UINT offDelta = nativeAfterOff.count - nativeBeforeOff.count;
        std::cerr << "Disabled application presents=4; native present delta=" << offDelta << '\n';
        require(offDelta <= 4, "extra native presentations continued after disable");
    }
    require(hwmodern::fsrFrameGenerationStats().generationCallbacks == callbacksBeforeOff,
        "AMD callbacks continued after explicit disable");
    require(!IsWindowVisible(f.window), "fixture HWND became visible");
    checkDebugMessages(f);
    if(workerPreparation) std::cerr<<"PASS: production FG prepare on persistent post worker; Present on caller thread\n";

    if (!countSupported || nativeDelta <= applicationFrames) {
        std::cerr << "INCONCLUSIVE: production callbacks/dispatch passed, but hidden-window native present counter did not prove additional submissions. No displayed-frame/FPS claim.\n";
        return 2;
    }
    std::cerr << "PASS: production swapchain, prepare, generation, disable and extra native present submissions; hidden HWND. Physical display/visual-quality parity is outside this fixture.\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
