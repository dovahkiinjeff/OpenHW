// Real production compute shader, offscreen textures only; no HWND/swapchain.
#include "../src/Modern/FGEffectGuides.cpp"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
using Microsoft::WRL::ComPtr;
unsigned checks = 0;
void require(bool value, const char *message)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void check(HRESULT result, const char *message)
{
    if (FAILED(result)) std::fprintf(stderr, "HRESULT 0x%08lX: %s\n", static_cast<unsigned long>(result), message);
    require(SUCCEEDED(result), message);
}
float half(std::uint16_t bits)
{
    const float sign = bits & 0x8000 ? -1.0f : 1.0f;
    const unsigned exp = (bits >> 10) & 31u, mantissa = bits & 1023u;
    if (exp == 0) return sign * std::ldexp(static_cast<float>(mantissa), -24);
    if (exp == 31) return mantissa ? std::numeric_limits<float>::quiet_NaN() : sign * INFINITY;
    return sign * std::ldexp(1.0f + mantissa / 1024.0f, static_cast<int>(exp) - 15);
}
float deviceDepth(float z) { return 500000.0f * (z - .05f) / (z * (500000.0f - .05f)); }
std::array<double, 3> ray(const hwmodern::FGEffectGuideCamera &camera, double u, double v)
{
    const double t = std::tan(camera.verticalFovRadians / 2.0);
    std::array<double, 3> result{};
    for (unsigned a = 0; a < 3; ++a)
        result[a] = camera.forward[a] + camera.right[a] * ((2 * u - 1) * t * camera.aspect) +
            camera.up[a] * ((1 - 2 * v) * t);
    return result;
}
std::array<double, 2> project(const hwmodern::FGEffectGuideCamera &camera,
    const std::array<double, 3> &world)
{
    double x = 0, y = 0, z = 0;
    for (unsigned a = 0; a < 3; ++a)
    {
        const double delta = world[a] - camera.position[a];
        x += delta * camera.right[a]; y += delta * camera.up[a]; z += delta * camera.forward[a];
    }
    const double t = std::tan(camera.verticalFovRadians / 2.0);
    return {.5 + x / (2 * z * t * camera.aspect), .5 - y / (2 * z * t)};
}
std::array<double, 2> expectedMotion(const hwmodern::FGEffectGuideCamera &current,
    const hwmodern::FGEffectGuideCamera &previous, unsigned w, unsigned h,
    unsigned x, unsigned y, double viewZ)
{
    const double u = (x + .5) / w, v = (y + .5) / h;
    auto point = ray(current, u, v);
    for (unsigned a = 0; a < 3; ++a) point[a] = current.position[a] + point[a] * viewZ;
    const auto uv = project(previous, point);
    return {(uv[0] - u) * w, (uv[1] - v) * h};
}

struct Texture
{
    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    unsigned width = 0, height = 0, bytes = 0;
};
struct Readback
{
    ComPtr<ID3D12Resource> resource;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    unsigned width = 0, height = 0, bytes = 0;
    std::vector<unsigned char> data() const
    {
        void *mapped = nullptr;
        check(resource->Map(0, nullptr, &mapped), "map readback");
        std::vector<unsigned char> output(static_cast<size_t>(width) * height * bytes);
        for (unsigned y = 0; y < height; ++y)
            std::memcpy(output.data() + static_cast<size_t>(y) * width * bytes,
                static_cast<unsigned char *>(mapped) + footprint.Offset +
                static_cast<size_t>(y) * footprint.Footprint.RowPitch, static_cast<size_t>(width) * bytes);
        D3D12_RANGE noWrite{}; resource->Unmap(0, &noWrite); return output;
    }
};

struct Fixture
{
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12InfoQueue> info;
    HANDLE event = nullptr;
    UINT64 serial = 0;
    std::vector<ComPtr<ID3D12Resource>> staging;
    explicit Fixture(bool gpuValidation = true)
    {
        ComPtr<ID3D12Debug> debug;
        check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "D3D12 debug layer available");
        debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;
        if (gpuValidation && SUCCEEDED(debug.As(&validation))) validation->SetEnableGPUBasedValidation(TRUE);
        ComPtr<IDXGIFactory6> factory; check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "create DXGI factory");
        ComPtr<IDXGIAdapter1> adapter;
        check(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)), "select high-performance GPU");
        DXGI_ADAPTER_DESC1 description{}; check(adapter->GetDesc1(&description), "GPU description");
        std::wprintf(L"FG guide GPU: %ls\n", description.Description);
        check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)), "create real GPU device");
        device.As(&info);
        D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        check(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "create GPU queue");
        check(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "create command allocator");
        check(device->CreateCommandList(0, q.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)), "create command list");
        check(commands->Close(), "close initial list");
        check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "create fence");
        event = CreateEventW(nullptr, FALSE, FALSE, nullptr); require(event != nullptr, "create fence event");
    }
    void begin()
    { check(allocator->Reset(), "reset allocator"); check(commands->Reset(allocator.Get(), nullptr), "reset commands"); }
    void finish()
    {
        check(commands->Close(), "close commands");
        ID3D12CommandList *list = commands.Get(); queue->ExecuteCommandLists(1, &list);
        check(queue->Signal(fence.Get(), ++serial), "signal completed GPU commands");
        check(fence->SetEventOnCompletion(serial, event), "arm fence event");
        require(WaitForSingleObject(event, 15000) == WAIT_OBJECT_0, "GPU compute completes");
        check(device->GetDeviceRemovedReason(), "GPU remains healthy"); staging.clear();
    }
    ComPtr<ID3D12Resource> buffer(UINT64 bytes, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state)
    {
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = heapType;
        D3D12_RESOURCE_DESC description{}; description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Width = bytes; description.Height = 1; description.DepthOrArraySize = 1;
        description.MipLevels = 1; description.SampleDesc.Count = 1; description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> result;
        check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, state,
            nullptr, IID_PPV_ARGS(&result)), "allocate fixture buffer"); return result;
    }
    void barrier(ID3D12Resource *resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
    {
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
        commands->ResourceBarrier(1, &barrier);
    }
    Texture texture(unsigned w, unsigned h, DXGI_FORMAT format, const void *data,
        D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    {
        Texture output; output.width = w; output.height = h; output.bytes = format == DXGI_FORMAT_R32G32_FLOAT ? 8 : 4;
        output.readState = readState;
        D3D12_RESOURCE_DESC description{}; description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = w; description.Height = h; description.DepthOrArraySize = 1;
        description.MipLevels = 1; description.SampleDesc.Count = 1; description.Format = format;
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&output.resource)), "allocate input texture");
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 bytes = 0;
        device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        auto upload = buffer(bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void *mapped = nullptr; check(upload->Map(0, nullptr, &mapped), "map fixture upload");
        for (unsigned y = 0; y < h; ++y)
            std::memcpy(static_cast<unsigned char *>(mapped) + footprint.Offset + static_cast<size_t>(y) * footprint.Footprint.RowPitch,
                static_cast<const unsigned char *>(data) + static_cast<size_t>(y) * w * output.bytes,
                static_cast<size_t>(w) * output.bytes);
        upload->Unmap(0, nullptr);
        D3D12_TEXTURE_COPY_LOCATION destination{}, source{};
        destination.pResource = output.resource.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.pResource = upload.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; source.PlacedFootprint = footprint;
        commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        barrier(output.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, readState);
        staging.push_back(upload); return output;
    }
    Readback read(ID3D12Resource *texture, D3D12_RESOURCE_STATES readState)
    {
        Readback output; const auto description = texture->GetDesc();
        output.width = static_cast<unsigned>(description.Width); output.height = description.Height;
        output.bytes = description.Format == DXGI_FORMAT_R32G32_FLOAT ? 8 : 4;
        UINT64 bytes = 0; device->GetCopyableFootprints(&description, 0, 1, 0, &output.footprint, nullptr, nullptr, &bytes);
        output.resource = buffer(bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        barrier(texture, readState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION destination{}, source{};
        destination.pResource = output.resource.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = output.footprint;
        source.pResource = texture; source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        barrier(texture, D3D12_RESOURCE_STATE_COPY_SOURCE, readState); return output;
    }
    void validate()
    {
        if (!info) return;
        for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index)
        {
            SIZE_T size = 0; info->GetMessage(index, nullptr, &size); std::vector<unsigned char> storage(size);
            auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data()); info->GetMessage(index, message, &size);
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
                std::fprintf(stderr, "D3D12 validation: %s\n", message->pDescription);
            require(message->Severity > D3D12_MESSAGE_SEVERITY_WARNING, "no GPU validation warnings/errors");
        }
    }
    ~Fixture() { hwmodern::fgEffectGuidesReleaseDevice(); if (event) CloseHandle(event); }
};

struct Output
{
    std::vector<unsigned char> depth, motion;
    float z(unsigned pixel) const { float value = 0; std::memcpy(&value, depth.data() + pixel * 4, 4); return value; }
    float mx(unsigned pixel) const { std::uint16_t value = 0; std::memcpy(&value, motion.data() + pixel * 4, 2); return half(value); }
    float my(unsigned pixel) const { std::uint16_t value = 0; std::memcpy(&value, motion.data() + pixel * 4 + 2, 2); return half(value); }
};

Output run(Fixture &f, unsigned w, unsigned h, const std::vector<float> &baseDepth,
    const std::vector<std::uint32_t> &baseMotion, const std::vector<float> *native, unsigned nw, unsigned nh,
    const std::vector<std::array<float, 2>> *cloud, unsigned cw, unsigned ch,
    const hwmodern::FGEffectGuideCamera &previous, hwmodern::FGEffectGuideCamera current,
    bool reset = false, bool failHistory = false,
    bool omitPreviousNative = false, bool omitPreviousCloud = false)
{
    hwmodern::fgEffectGuidesReleaseDevice(); f.begin();
    auto depth = f.texture(w, h, DXGI_FORMAT_R32_FLOAT, baseDepth.data());
    auto motion = f.texture(w, h, DXGI_FORMAT_R16G16_FLOAT, baseMotion.data(), hwmodern::motionRead);
    Texture nativeTexture, cloudTexture;
    if (native) nativeTexture = f.texture(nw, nh, DXGI_FORMAT_R32_FLOAT, native->data());
    if (cloud) cloudTexture = f.texture(cw, ch, DXGI_FORMAT_R32G32_FLOAT, cloud->data());
    const auto prepare = [&](const hwmodern::FGEffectGuideCamera &camera, unsigned slot, bool clear, bool warm = false) {
        return hwmodern::fgEffectGuidesPrepare(f.commands.Get(), f.device.Get(), slot,
            depth.resource.Get(), motion.resource.Get(), warm && omitPreviousNative ? nullptr : nativeTexture.resource.Get(),
            warm && omitPreviousCloud ? nullptr : cloudTexture.resource.Get(), camera, clear);
    };
    require(prepare(previous, 0, true, true), "warm previous camera dispatch");
    // These are two frames, not two prepares inside one unsubmitted frame.
    // The renderer advances its history descriptors after submission. Match
    // that contract before recording the next frame's resource references.
    f.finish(); f.begin();
    if (failHistory)
    {
        auto bad = previous; bad.position[0] = std::numeric_limits<float>::quiet_NaN();
        require(!prepare(bad, 1, false), "invalid camera suspends guides");
        require(hwmodern::fgEffectGuidesDepth() == nullptr && hwmodern::fgEffectGuidesMotion() == nullptr,
            "failed prepare exposes no stale output");
    }
    require(prepare(current, 1, reset), "current camera dispatch");
    const auto outDepth = f.read(hwmodern::fgEffectGuidesDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    const auto outMotion = f.read(hwmodern::fgEffectGuidesMotion(), hwmodern::motionRead);
    const auto originalDepth = f.read(depth.resource.Get(), depth.readState);
    const auto originalMotion = f.read(motion.resource.Get(), motion.readState);
    f.finish();
    require(originalDepth.data().size() == baseDepth.size() * sizeof(float) &&
        std::memcmp(originalDepth.data().data(), baseDepth.data(), baseDepth.size() * sizeof(float)) == 0,
        "original SR depth input remains byte-identical");
    require(originalMotion.data().size() == baseMotion.size() * sizeof(std::uint32_t) &&
        std::memcmp(originalMotion.data().data(), baseMotion.data(), baseMotion.size() * sizeof(std::uint32_t)) == 0,
        "original SR motion input remains byte-identical");
    return {outDepth.data(), outMotion.data()};
}

void cpuAnalyticTests()
{
    hwmodern::FGEffectGuideCamera previous, current;
    previous.frameId = 1; current.frameId = 2;
    previous.verticalFovRadians = current.verticalFovRadians = 1.5707963268f;
    previous.aspect = current.aspect = 2.0f;
    current.position[0] = 1; current.position[1] = 2;
    const auto motion = expectedMotion(current, previous, 64, 32, 32, 16, 100);
    require(std::abs(motion[0] - .16) < 1e-7 && std::abs(motion[1] + .32) < 1e-7,
        "analytic pixel motion sign, camera translation and render dimensions");
    const double angle = 2.0 * 3.141592653589793 / 180.0;
    previous = {}; current = {}; previous.position[2] = 1000;
    current.position[0] = static_cast<float>(1000 * std::sin(angle));
    current.position[2] = static_cast<float>(1000 * std::cos(angle));
    current.forward[0] = static_cast<float>(-std::sin(angle)); current.forward[2] = static_cast<float>(-std::cos(angle));
    current.right[0] = static_cast<float>(std::cos(angle)); current.right[2] = static_cast<float>(-std::sin(angle));
    previous.aspect = current.aspect = 2560.0f / 1440.0f;
    const auto finite = expectedMotion(current, previous, 2561, 1441, 1280, 720, 1000);
    const double skyPixels = std::tan(angle) * 2560.0 / (2 * std::tan(current.verticalFovRadians / 2) * current.aspect);
    require(std::abs(finite[0]) < .0001 && skyPixels > 43 && skyPixels < 44,
        "2-degree orbit keeps finite target stationary instead of 43px sky flow");
    require(hwmodern::validCamera(current), "orthonormal orbit camera valid");
    current.up[0] = current.right[0]; current.up[1] = current.right[1]; current.up[2] = current.right[2];
    require(!hwmodern::validCamera(current), "degenerate basis rejected");
}

void gpuTests(Fixture &f)
{
    unsigned w = 65, h = 33, x = 32, y = 16, pixel = y * w + x;
    std::vector<float> base(w * h, 1.0f);
    std::vector<std::uint32_t> motion(w * h, 0xc1003d00u); // (1.25,-2.5), valid finite halfs
    hwmodern::FGEffectGuideCamera previous, current;
    previous.frameId = 1; current.frameId = 2;
    previous.aspect = current.aspect = static_cast<float>(w) / h;
    previous.verticalFovRadians = current.verticalFovRadians = 1.5707963268f;
    // Preserve a range of finite depth and half values, including signed zeros
    // and minimum half subnormals, across the real typed load/store path.
    std::vector<float> variedDepth = base;
    const std::uint32_t patterns[] = {0x80000000u, 0x00018001u, 0x7bfffbffu, 0xc1003d00u};
    std::vector<std::uint32_t> variedMotion = motion;
    for (unsigned p = 0; p < w * h; ++p)
    { variedDepth[p] = static_cast<float>(p % 101) / 100; variedMotion[p] = patterns[p % 4]; }
    auto out = run(f, w, h, variedDepth, variedMotion, nullptr, 0, 0, nullptr, 0, 0, previous, current);
    require(std::memcmp(out.depth.data(), variedDepth.data(), out.depth.size()) == 0,
        "no-FX finite depth outputs exact bits including zero");
    require(std::memcmp(out.motion.data(), variedMotion.data(), out.motion.size()) == 0,
        "no-FX finite half outputs exact bits including signed zero/subnormals");

    const unsigned nw = 130, nh = 66;
    std::vector<float> native(nw * nh, FLT_MAX);
    native[0] = std::numeric_limits<float>::quiet_NaN(); native[1] = -1;
    std::vector<std::array<float, 2>> emptyClouds(17 * 9, {0, 0});
    out = run(f, w, h, variedDepth, variedMotion, &native, nw, nh, &emptyClouds, 17, 9, previous, current);
    require(std::memcmp(out.depth.data(), variedDepth.data(), out.depth.size()) == 0 &&
        std::memcmp(out.motion.data(), variedMotion.data(), out.motion.size()) == 0,
        "empty FLT_MAX/invalid native and zero cloud fields preserve opaque bits");
    const unsigned sx = static_cast<unsigned>((x + .5) / w * nw);
    const unsigned sy = nh - 1 - static_cast<unsigned>((y + .5) / h * nh);
    native[sy * nw + sx] = 100;
    current.position[0] = 1; current.position[1] = 2;
    out = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current);
    auto expected = expectedMotion(current, previous, w, h, x, y, 100);
    std::printf("Native center: depth=%.9g motion=(%.9g,%.9g) expected=(%.9g,%.9g), source=(%u,%u)\n",
        out.z(pixel), out.mx(pixel), out.my(pixel), expected[0], expected[1], sx, sy);
    require(std::abs(out.mx(pixel) - expected[0]) < .0003 && std::abs(out.my(pixel) - expected[1]) < .0003,
        "actual native-effect GPU motion includes translation, sign and render-size units");
    require(std::abs(out.z(pixel) - deviceDepth(100)) < 0.0000001f && out.z(pixel) < 1,
        "native trail motion over sky has finite depth so AMD interpolation retains it");
    require(std::memcmp(out.motion.data() + (pixel - w) * 4, motion.data() + pixel - w, 4) == 0,
        "bottom-up full-output native guide selects correct input row");
    out = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current, true);
    require(out.mx(pixel) == 0 && out.my(pixel) == 0, "camera-cut reset clears selected FX motion");
    current.frameId = 8;
    out = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current);
    require(out.mx(pixel) == 0 && out.my(pixel) == 0, "nonconsecutive application frame resets camera history");
    current.frameId = 2;
    out = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current, false, true);
    require(out.mx(pixel) == 0 && out.my(pixel) == 0, "failed prepare invalidates history before recovery");

    // AMD's dilation must not select a finite trail vector beside a moving
    // hull. Preserve the entire one-pixel opaque neighborhood, even when the
    // neighboring hull is farther away than the translucent ribbon.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            auto adjacentBase = base;
            const unsigned neighbour = (y + dy) * w + x + dx;
            adjacentBase[neighbour] = deviceDepth(200);
            out = run(f, w, h, adjacentBase, variedMotion, &native, nw, nh,
                nullptr, 0, 0, previous, current);
            require(out.z(pixel) == adjacentBase[pixel],
                "finite trail depth cannot enter opaque 3x3 dilation neighborhood");
            require(out.z(neighbour) == adjacentBase[neighbour] &&
                std::memcmp(out.motion.data() + neighbour * 4, variedMotion.data() + neighbour, 4) == 0,
                "adjacent moving hull preserves exact depth and motion bits");
        }

    // An opaque surface nearer than a transient effect keeps its original guides.
    auto occludedBase = base; occludedBase[pixel] = deviceDepth(50);
    out = run(f, w, h, occludedBase, motion, &native, nw, nh, nullptr, 0, 0, previous, current);
    require(out.z(pixel) == occludedBase[pixel] &&
        std::memcmp(out.motion.data() + pixel * 4, motion.data() + pixel, 4) == 0,
        "effect behind opaque geometry cannot replace its guides");

    // Only the engine core that actually covers a hull may own foreground
    // depth/motion. Its signed inverse depth retains MIN-blend nearest order.
    native[sy * nw + sx] = -1000000.0f / 100.0f;
    auto coveredHull = base; coveredHull[pixel] = deviceDepth(200);
    out = run(f, w, h, coveredHull, variedMotion, &native, nw, nh,
        nullptr, 0, 0, previous, current);
    expected = expectedMotion(current, previous, w, h, x, y, 100);
    require(std::abs(out.z(pixel) - deviceDepth(100)) < .0000001f &&
        std::abs(out.mx(pixel) - expected[0]) < .0005 &&
        std::abs(out.my(pixel) - expected[1]) < .0005,
        "covering trail core owns depth and finite camera motion over a moving hull");
    require(out.z(pixel + 1) == coveredHull[pixel + 1] &&
        std::memcmp(out.motion.data() + (pixel + 1) * 4, variedMotion.data() + pixel + 1, 4) == 0,
        "covering core cannot modify adjacent background pixels");
    out = run(f, w, h, occludedBase, variedMotion, &native, nw, nh,
        nullptr, 0, 0, previous, current);
    require(out.z(pixel) == occludedBase[pixel] &&
        std::memcmp(out.motion.data() + pixel * 4, variedMotion.data() + pixel, 4) == 0,
        "hidden covering core cannot replace nearer hull guides");
    native[sy * nw + sx] = 100; // Ordinary transparent effects stay protected.

    const unsigned cw = 17, ch = 9;
    std::vector<std::array<float, 2>> clouds(cw * ch, {80, .079f});
    out = run(f, w, h, base, motion, nullptr, 0, 0, &clouds, cw, ch, previous, current);
    require(std::memcmp(out.depth.data(), base.data(), out.depth.size()) == 0 &&
        std::memcmp(out.motion.data(), motion.data(), out.motion.size()) == 0,
        "subthreshold cloud coverage never blankets sky guides");
    for (auto &cloud : clouds) cloud = {80, .08f};
    out = run(f, w, h, base, motion, nullptr, 0, 0, &clouds, cw, ch, previous, current);
    x = 4; y = 3; pixel = y * w + x;
    const auto direction = ray(current, (x + .5) / w, (y + .5) / h);
    const double viewZ = 80 / std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    expected = expectedMotion(current, previous, w, h, x, y, viewZ);
    require(out.z(pixel) == base[pixel],
        "cloud finite motion retains original sky depth instead of a solid foreground surface");
    require(std::abs(out.mx(pixel) - expected[0]) < .001 && std::abs(out.my(pixel) - expected[1]) < .001,
        "quarter-resolution top-down cloud guide produces finite reprojection");

    x = 32; y = 16; pixel = y * w + x;
    const auto nativeOnly = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current);
    // Object motion intentionally differs from the camera-only cloud motion.
    // Its bits must survive each opacity step while the camera moves/zooms.
    auto movingHullDepth = variedDepth;
    for (float &z : movingHullDepth) z = deviceDepth(100.0f + z * 50.0f);
    out = run(f, w, h, movingHullDepth, variedMotion, &native, nw, nh,
        nullptr, 0, 0, previous, current);
    require(std::memcmp(out.depth.data(), movingHullDepth.data(), out.depth.size()) == 0 &&
        std::memcmp(out.motion.data(), variedMotion.data(), out.motion.size()) == 0,
        "even a nearer transparent FX fragment cannot replace a moving opaque hull's guides");
    const float coverageSteps[] = {.079f, .08f, .49f, .5f, .51f, .99f, 1.0f};
    for (float coverage : coverageSteps)
    {
        for (auto &cloud : clouds) cloud = {50, coverage};
        out = run(f, w, h, movingHullDepth, variedMotion, nullptr, 0, 0,
            &clouds, cw, ch, previous, current);
        require(std::memcmp(out.depth.data(), movingHullDepth.data(), out.depth.size()) == 0 &&
            std::memcmp(out.motion.data(), variedMotion.data(), out.motion.size()) == 0,
            "foreground cloud opacity cannot replace exact moving-hull depth/motion bits");
        out = run(f, w, h, base, motion, &native, nw, nh, &clouds, cw, ch, previous, current);
        require(std::memcmp(out.depth.data() + pixel * 4, nativeOnly.depth.data() + pixel * 4, 4) == 0 &&
            std::memcmp(out.motion.data() + pixel * 4, nativeOnly.motion.data() + pixel * 4, 4) == 0,
            "foreground cloud opacity cannot replace selected native effect guide bits");
        if (coverage >= .08f)
            require(out.z(pixel - w) == base[pixel - w] &&
                std::memcmp(out.motion.data() + (pixel - w) * 4, motion.data() + pixel - w, 4) != 0,
                "cloud finite motion remains active on adjacent sky without overriding its depth");
    }
    out = run(f, w, h, base, motion, &native, nw, nh, &clouds, cw, ch, previous, current,
        false, false, true, false);
    expected = expectedMotion(current, previous, w, h, x, y, 100);
    require(std::abs(out.mx(pixel) - expected[0]) < .0005 && std::abs(out.my(pixel) - expected[1]) < .0005,
        "newly selected native FX keeps finite motion even under an opaque cloud");

    // Orbit about a finite center: both translation and rotation cancel.
    const float angle = 2.0f * 3.14159265358979323846f / 180;
    previous = {}; current = {}; previous.frameId = 1; current.frameId = 2;
    previous.aspect = current.aspect = static_cast<float>(w) / h;
    previous.position[2] = 1000;
    current.position[0] = 1000 * std::sin(angle); current.position[2] = 1000 * std::cos(angle);
    current.right[0] = std::cos(angle); current.right[2] = -std::sin(angle);
    current.forward[0] = -std::sin(angle); current.forward[2] = -std::cos(angle);
    native[sy * nw + sx] = 1000;
    out = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current);
    require(std::abs(out.mx(pixel)) < .0001 && std::abs(out.my(pixel)) < .0001,
        "GPU finite 2-degree orbit does not emit false sky motion");

    // Independent old/new projection parameters are needed during zoom.
    previous = {}; current = {}; previous.frameId = 1; current.frameId = 2;
    previous.aspect = current.aspect = static_cast<float>(w) / h;
    current.verticalFovRadians = 1.5707963268f;
    std::fill(native.begin(), native.end(), 100.0f);
    x = 4; y = 3; pixel = y * w + x;
    out = run(f, w, h, base, motion, &native, nw, nh, nullptr, 0, 0, previous, current);
    expected = expectedMotion(current, previous, w, h, x, y, 100);
    require(std::abs(out.mx(pixel) - expected[0]) < .015 && std::abs(out.my(pixel) - expected[1]) < .015,
        "GPU zoom reprojection retains previous and current FOV");

    // Changing actual render dimensions resets history, while separate flight
    // slots keep their output resources/descriptors independently alive.
    hwmodern::fgEffectGuidesReleaseOutputs(); f.begin();
    std::vector<float> oldBase(64 * 32, 1.0f);
    std::vector<std::uint32_t> oldMotion(64 * 32, 0);
    auto oldDepthTexture = f.texture(64, 32, DXGI_FORMAT_R32_FLOAT, oldBase.data());
    auto oldMotionTexture = f.texture(64, 32, DXGI_FORMAT_R16G16_FLOAT, oldMotion.data(), hwmodern::motionRead);
    auto depthTexture = f.texture(w, h, DXGI_FORMAT_R32_FLOAT, base.data());
    auto motionTexture = f.texture(w, h, DXGI_FORMAT_R16G16_FLOAT, motion.data(), hwmodern::motionRead);
    auto nativeTexture = f.texture(nw, nh, DXGI_FORMAT_R32_FLOAT, native.data());
    previous = {}; current = {}; previous.frameId = 1; current.frameId = 2;
    previous.aspect = 2.0f; current.aspect = static_cast<float>(w) / h; current.position[0] = 1;
    require(hwmodern::fgEffectGuidesPrepare(f.commands.Get(), f.device.Get(), 0,
        oldDepthTexture.resource.Get(), oldMotionTexture.resource.Get(), nativeTexture.resource.Get(), nullptr, previous, true),
        "old-size flight prepares");
    ID3D12Resource *firstFlight = hwmodern::fgEffectGuidesMotion();
    require(hwmodern::fgEffectGuidesPrepare(f.commands.Get(), f.device.Get(), 1,
        depthTexture.resource.Get(), motionTexture.resource.Get(), nativeTexture.resource.Get(), nullptr, current, false),
        "new-size flight prepares");
    ID3D12Resource *secondFlight = hwmodern::fgEffectGuidesMotion();
    const auto resizedMotion = f.read(secondFlight, hwmodern::motionRead);
    current.frameId = 3;
    require(hwmodern::fgEffectGuidesPrepare(f.commands.Get(), f.device.Get(), 2,
        depthTexture.resource.Get(), motionTexture.resource.Get(), nativeTexture.resource.Get(), nullptr, current, false),
        "third independent flight prepares");
    require(firstFlight != secondFlight && firstFlight != hwmodern::fgEffectGuidesMotion() &&
        secondFlight != hwmodern::fgEffectGuidesMotion(), "three flight slots never alias output resources");
    f.finish();
    const Output resized{std::vector<unsigned char>(static_cast<size_t>(w) * h * 4), resizedMotion.data()};
    require(resized.mx(16 * w + 32) == 0 && resized.my(16 * w + 32) == 0,
        "actual render-size change invalidates previous reprojection");
    f.validate();
}
}

int main(int argc, char **argv)
{
    cpuAnalyticTests();
    if (argc > 1 && std::strcmp(argv[1], "--cpu-only") == 0)
    {
        std::printf("FG effect guide analytic CPU tests passed: %u checks.\n", checks);
        return 0;
    }
    const bool gpuValidation = !(argc > 1 && std::strcmp(argv[1], "--no-gpu-validation") == 0);
    Fixture fixture(gpuValidation); gpuTests(fixture);
    std::printf("FG effect guide CPU/GPU tests passed: %u checks; one real CS, unchanged SR inputs, finite camera reprojection.\n", checks);
}
