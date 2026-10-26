// CPU/GDI only. The production owner thread is compiled to remain hidden.
#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../src/ThirdParty/stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#pragma warning(pop)
#define HW_WINDOWS_SPLASH_TESTING 1
#include "../src/Modern/WindowsSplash.cpp"

#include <cstdlib>
#include <cstring>
#include <thread>

namespace
{
std::mutex modelMutex;
HWWindowsSplashSnapshot model{};
unsigned int resets = 0;
unsigned int checks = 0;

void require(bool ok, const char *name)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}

struct Surface
{
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = nullptr;
    HGDIOBJ old = nullptr;
    void *pixels = nullptr;
    int width, height;
    Surface(int w, int h) : width(w), height(h)
    {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        require(dc && bitmap && pixels, "create offscreen DIB");
        old = SelectObject(dc, bitmap);
    }
    ~Surface()
    {
        SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
    }
    COLORREF pixel(int x,int y) const {
        // GetPixel can quantize through the active display/RDP color format.
        // Verify the actual 32-bit offscreen canvas independently of that device.
        GdiFlush();const auto p=static_cast<const unsigned*>(pixels)[y*width+x];
        return RGB((p>>16)&255,(p>>8)&255,p&255);
    }
    void save(const char *directory, const char *name)
    {
        char path[32768]{};
        std::snprintf(path, sizeof(path), "%s/%s.bmp", directory, name);
        FILE *file = nullptr;
        require(fopen_s(&file, path, "wb") == 0 && file, "open offscreen artifact");
        BITMAPFILEHEADER fileHeader{};
        fileHeader.bfType = 0x4d42;
        fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        fileHeader.bfSize = fileHeader.bfOffBits + width * height * 4;
        BITMAPINFOHEADER image{};
        image.biSize = sizeof(image); image.biWidth = width; image.biHeight = -height;
        image.biPlanes = 1; image.biBitCount = 32; image.biCompression = BI_RGB;
        require(std::fwrite(&fileHeader, sizeof(fileHeader), 1, file) == 1 &&
            std::fwrite(&image, sizeof(image), 1, file) == 1 &&
            std::fwrite(pixels, static_cast<size_t>(width) * height * 4, 1, file) == 1,
            "write offscreen artifact");
        std::fclose(file);
    }
};

HWWindowsSplashSnapshot example()
{
    HWWindowsSplashSnapshot snapshot{};
    snapshot.total = 80; snapshot.completed = 30; snapshot.skipped = 4;
    snapshot.percentHundredths = 3750; snapshot.active = 1;
    snapshot.startedSeconds = 100; snapshot.taskStartedSeconds = 120;
    strcpy_s(snapshot.task, "COMPILING PATH TRACING / CLOSEST-HIT MATERIALS");
    strcpy_s(snapshot.detail, "Building renderer shader 17/42; cache miss, waiting for compiler to finish.");
    strcpy_s(snapshot.history[0], "DONE / D3D12 device and presentation queue ready");
    strcpy_s(snapshot.history[1], "SKIPPED / Intel frame generation is not implemented");
    strcpy_s(snapshot.history[2], "DONE / Loaded 1,482 remastered ship resources");
    strcpy_s(snapshot.history[3], "DONE / Validated mission artwork and shader cache");
    return snapshot;
}

void drawingTests(const char *directory)
{
    SplashState state;
    state.headingFont = CreateFontW(-25, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FF_DONTCARE, L"Bahnschrift SemiCondensed");
    state.bodyFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FF_DONTCARE, L"Bahnschrift");
    Surface surface(1120, 630);
    RECT client{0, 0, 1120, 630};
    const RECT track = progressTrack(client);
    const int centerY = (track.top + track.bottom) / 2;
    auto snapshot = example();
    snapshot.percentHundredths = 0;
    paintSplash(surface.dc, client, state, snapshot, 125.0);
    require(surface.pixel( track.left + 1, centerY) == RGB(34, 48, 54),
        "zero work never invents progress");
    require(surface.pixel( 10, 10) == RGB(0, 0, 0), "missing-art fallback paints black");
    snapshot.percentHundredths = 3750;
    paintSplash(surface.dc, client, state, snapshot, 125.0);
    const int edge = track.left + (track.right - track.left) * 3750 / 10000;
    require(surface.pixel( edge - 1, centerY) == RGB(222, 162, 62), "exact completed work fill");
    require(surface.pixel( edge, centerY) == RGB(34, 48, 54), "exact completed work boundary");
    paintSplash(surface.dc, client, state, snapshot, 9876.0);
    require(surface.pixel( edge - 1, centerY) == RGB(222, 162, 62) &&
        surface.pixel( edge, centerY) == RGB(34, 48, 54), "elapsed heartbeat cannot advance bar");
    snapshot.percentHundredths = 10000;
    paintSplash(surface.dc, client, state, snapshot, 125.0);
    require(surface.pixel( track.right - 1, centerY) == RGB(222, 162, 62), "100 percent reaches end");
    snapshot.percentHundredths = 20000;
    paintSplash(surface.dc, client, state, snapshot, 125.0);
    require(surface.pixel( track.right + 1, centerY) == RGB(4, 8, 12), "invalid progress stays inside track");
    snapshot = example();
    require(loadArtwork(state), "load exact shipped startup artwork");
    paintSplash(surface.dc, client, state, snapshot, 125.0);
    surface.save(directory, "splash-1120x630");

    // Long UTF-8 detail and all four ticker rows must clip inside the small canvas.
    memset(snapshot.detail, 'X', sizeof(snapshot.detail));
    for (const auto size : {std::pair<int, int>{640, 360}, {384, 216}})
    {
        Surface small(size.first, size.second);
        paintSplash(small.dc, RECT{0, 0, size.first, size.second}, state, snapshot, 125.5);
        small.save(directory, size.first == 640 ? "splash-640x360" : "splash-384x216");
    }
    releaseState(state);
}

void lifecycleCycle(bool verifyResponsiveness)
{
    const unsigned int oldResets = resets;
    hwWindowsSplashStart();
    const HWND window = control.window.load();
    require(window != nullptr && IsWindow(window), "owner thread creates HWND");
    require(!IsWindowVisible(window), "fixture never shows a window");
    hwWindowsSplashStart();
    require(resets == oldResets + 1, "duplicate Start preserves current checklist");
    hwWindowsSplashSetVisible(0); hwWindowsSplashSetVisible(1);
    if (verifyResponsiveness)
    {
        const auto ticks = splashTestTimerTicks.load();
        {
            std::lock_guard<std::mutex> lock(modelMutex);
            model.completed = 7; model.percentHundredths = 7000; ++model.revision;
        }
        // The startup caller blocks and pumps no messages, as during compilation.
        Sleep(800);
        require(splashTestTimerTicks.load() >= ticks + 2, "independent UI heartbeat while caller blocked");
        require(splashTestCompleted.load() == 7, "worker observes model updates while caller blocked");
        require(!IsWindowVisible(window), "visibility messages remain hidden in fixture");
    }
    DWORD_PTR result = 0;
    require(SendMessageTimeoutW(window, WM_PAINT, 0, 0, SMTO_ABORTIFHUNG, 2000, &result) != 0,
        "owner processes paint and allocates buffered GDI canvas");
    const auto finishStarted = steadySeconds();
    hwWindowsSplashFinish(); hwWindowsSplashFinish();
    require(steadySeconds() - finishStarted < 2.0, "Finish wakes and joins promptly");
    require(control.window.load() == nullptr && control.thread == nullptr &&
        control.stop == nullptr && control.ready == nullptr, "joined lifecycle releases all handles");
    require(!IsWindow(window), "HWND destroyed by owner");
}
}

extern "C" void hwWindowsSplashProgressReset(void)
{
    std::lock_guard<std::mutex> lock(modelMutex);
    model = {}; model.total = 10; model.active = 1;
    model.startedSeconds = model.taskStartedSeconds = steadySeconds(); ++resets;
}
extern "C" void hwWindowsSplashProgressStop(void)
{
    std::lock_guard<std::mutex> lock(modelMutex); model.active = 0;
}
extern "C" void hwWindowsSplashGetSnapshot(HWWindowsSplashSnapshot *snapshot)
{
    std::lock_guard<std::mutex> lock(modelMutex); *snapshot = model;
}

int main(int argc, char **argv)
{
    require(argc == 2, "artifact output argument");
    drawingTests(argv[1]);
    lifecycleCycle(true); // Warm the process-local font/window-class caches.
    const DWORD baselineGdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    DWORD baselineHandles = 0;
    require(GetProcessHandleCount(GetCurrentProcess(), &baselineHandles), "query baseline handles");
    for (int index = 0; index < 5; ++index) lifecycleCycle(false);
    DWORD finalHandles = 0;
    require(GetProcessHandleCount(GetCurrentProcess(), &finalHandles), "query final handles");
    require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == baselineGdi,
        "repeated lifecycle leaves no GDI handles");
    require(finalHandles == baselineHandles, "repeated lifecycle leaves no event/thread handles");
    std::printf("Windows splash CPU/GDI tests passed: %u checks; hidden owner timer, exact progress, no handle leaks.\n", checks);
}
