#include "WindowsSplash.h"

#if defined(_WIN32)
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include "stb_image.h"

namespace
{
constexpr UINT visibilityMessage = WM_APP + 42;
constexpr UINT_PTR heartbeatTimer = 1;

// The worker alone owns all HWND/GDI state. Handles remain alive until joined.
struct SplashControl
{
    std::mutex mutex;
    HANDLE thread = nullptr, ready = nullptr, stop = nullptr;
    DWORD threadId = 0;
    std::atomic<HWND> window{nullptr};
    std::atomic<bool> visible{true};
} control;

struct SplashState
{
    HWND window = nullptr;
    HBITMAP artwork = nullptr, canvas = nullptr;
    HFONT headingFont = nullptr, bodyFont = nullptr;
    HDC canvasDc = nullptr;
    HGDIOBJ oldCanvas = nullptr;
    int canvasWidth = 0, canvasHeight = 0;
    int artworkWidth = 0, artworkHeight = 0;
    HWWindowsSplashSnapshot snapshot{};
};

#if defined(HW_WINDOWS_SPLASH_TESTING)
// The isolated CPU fixture uses the actual owner thread without showing a HWND.
std::atomic<unsigned long long> splashTestTimerTicks{0};
std::atomic<unsigned int> splashTestCompleted{0};
#endif

double steadySeconds()
{
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::wstring executableDirectory()
{
    wchar_t path[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, _countof(path));
    if (length == 0 || length >= _countof(path)) return {};
    const std::wstring result(path, length);
    const size_t slash = result.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : result.substr(0, slash);
}

bool readFile(const std::wstring &path, std::vector<unsigned char> &bytes)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
              size.QuadPart <= 32ll * 1024ll * 1024ll;
    if (ok)
    {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
            &read, nullptr) && read == bytes.size();
    }
    CloseHandle(file);
    if (!ok) bytes.clear();
    return ok;
}

bool loadArtwork(SplashState &state)
{
    const std::wstring base = executableDirectory();
    std::vector<unsigned char> file;
    if (!readFile(base + L"\\Data\\UI\\startup_splash.png", file) &&
        !readFile(base + L"\\..\\..\\..\\..\\assets\\UI\\startup_splash.png", file))
        return false;
    int channels = 0;
    stbi_uc *rgba = stbi_load_from_memory(file.data(), static_cast<int>(file.size()),
        &state.artworkWidth, &state.artworkHeight, &channels, 4);
    if (rgba == nullptr || state.artworkWidth <= 0 || state.artworkHeight <= 0)
    {
        if (rgba != nullptr) stbi_image_free(rgba);
        return false;
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = state.artworkWidth;
    info.bmiHeader.biHeight = -state.artworkHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *dibPixels = nullptr;
    state.artwork = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS,
        &dibPixels, nullptr, 0);
    if (state.artwork == nullptr || dibPixels == nullptr)
    {
        if (state.artwork != nullptr) DeleteObject(state.artwork);
        state.artwork = nullptr;
        stbi_image_free(rgba);
        return false;
    }
    auto *bgra = static_cast<unsigned char *>(dibPixels);
    const size_t count = static_cast<size_t>(state.artworkWidth) * state.artworkHeight;
    for (size_t index = 0; index < count; ++index)
    {
        bgra[index * 4] = rgba[index * 4 + 2];
        bgra[index * 4 + 1] = rgba[index * 4 + 1];
        bgra[index * 4 + 2] = rgba[index * 4];
        bgra[index * 4 + 3] = 255;
    }
    stbi_image_free(rgba);
    return true;
}

void fill(HDC target, const RECT &rectangle, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    if (brush == nullptr) return;
    FillRect(target, &rectangle, brush);
    DeleteObject(brush);
}

void text(HDC target, const char *value, RECT rectangle, COLORREF color)
{
    wchar_t wide[HW_SPLASH_TEXT_SIZE + 1] = {};
    size_t length = 0;
    if (value != nullptr)
        while (length < HW_SPLASH_TEXT_SIZE && value[length] != '\0') ++length;
    if (length != 0)
        MultiByteToWideChar(CP_UTF8, 0, value, static_cast<int>(length),
            wide, HW_SPLASH_TEXT_SIZE);
    SetTextColor(target, color);
    DrawTextW(target, wide, -1, &rectangle,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void elapsedText(double seconds, char *destination, size_t capacity)
{
    const auto whole = static_cast<unsigned long long>(std::max(0.0, seconds));
    if (whole >= 3600)
        std::snprintf(destination, capacity, "%llu:%02llu:%02llu",
            whole / 3600, whole / 60 % 60, whole % 60);
    else
        std::snprintf(destination, capacity, "%llu:%02llu", whole / 60, whole % 60);
}

RECT progressTrack(const RECT &client)
{
    const int margin = std::max(16L, client.right / 28);
    return {margin, client.bottom - 24, client.right - margin, client.bottom - 18};
}

// The bar depends only on actual completed work. Timers update elapsed text
// and the activity dots, never percentage. Also used directly by offscreen QA.
void paintSplash(HDC target, const RECT &client, const SplashState &state,
                 const HWWindowsSplashSnapshot &snapshot, double now)
{
    const int saved = SaveDC(target);
    FillRect(target, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    if (state.artwork != nullptr)
    {
        HDC source = CreateCompatibleDC(target);
        if (source != nullptr)
        {
            HGDIOBJ old = SelectObject(source, state.artwork);
            SetStretchBltMode(target, HALFTONE);
            SetBrushOrgEx(target, 0, 0, nullptr);
            StretchBlt(target, 0, 0, client.right, client.bottom,
                source, 0, 0, state.artworkWidth, state.artworkHeight, SRCCOPY);
            SelectObject(source, old);
            DeleteDC(source);
        }
    }
    const int margin = std::max(16L, client.right / 28);
    const int bandHeight = std::min(client.bottom, std::max(232L, client.bottom * 43 / 100));
    const int top = client.bottom - bandHeight;
    const int line = std::max(16, std::min(21, bandHeight / 13));
    fill(target, RECT{0, top, client.right, client.bottom}, RGB(4, 8, 12));
    fill(target, RECT{margin, top + 15, margin + 3, top + 40}, RGB(222, 162, 62));
    SetBkMode(target, TRANSPARENT);
    SelectObject(target, state.headingFont != nullptr ?
        state.headingFont : GetStockObject(DEFAULT_GUI_FONT));
    text(target, "HOMEWORLD / MODERN",
        RECT{margin + 13, top + 10, client.right - margin, top + 43}, RGB(228, 222, 201));
    SelectObject(target, state.bodyFont != nullptr ?
        state.bodyFont : GetStockObject(DEFAULT_GUI_FONT));
    char counts[HW_SPLASH_TEXT_SIZE] = {};
    if (snapshot.total != 0)
        std::snprintf(counts, sizeof(counts), "CHECKS %u/%u  /  %.2f%%",
            snapshot.completed, snapshot.total,
            std::min(snapshot.percentHundredths, 10000u) / 100.0);
    else
        std::snprintf(counts, sizeof(counts), "BUILDING STARTUP CHECKLIST");
    RECT countLine = {margin, top + 47, client.right - margin, top + 47 + line};
    text(target, counts, countLine, RGB(222, 162, 62));
    int y = countLine.bottom + 3;
    RECT task = {margin, y, client.right - margin, y + line + 4};
    text(target, snapshot.task[0] != '\0' ? snapshot.task : "INITIALIZING COMMAND SYSTEMS",
        task, RGB(228, 222, 201));
    y = task.bottom;
    RECT detail = {margin, y, client.right - margin, y + line};
    text(target, snapshot.detail, detail, RGB(156, 177, 180));
    y = detail.bottom + 2;

    char taskElapsed[32] = {}, totalElapsed[32] = {}, activity[HW_SPLASH_TEXT_SIZE] = {};
    elapsedText(now - snapshot.taskStartedSeconds, taskElapsed, sizeof(taskElapsed));
    elapsedText(now - snapshot.startedSeconds, totalElapsed, sizeof(totalElapsed));
    const unsigned int heartbeat =
        static_cast<unsigned int>(std::max(0.0, now - snapshot.startedSeconds) * 2) % 4;
    const char *dots[] = {"", ".", "..", "..."};
    const char *stateText = snapshot.failed != 0 ? "CHECK FAILED" :
        (snapshot.active != 0 ? "WORKING" : "STARTUP CHECKLIST");
    std::snprintf(activity, sizeof(activity),
        "%s%s  /  TASK %s  /  STARTUP %s  /  %u SKIPPED", stateText,
        snapshot.active != 0 ? dots[heartbeat] : "", taskElapsed, totalElapsed, snapshot.skipped);
    RECT elapsed = {margin, y, client.right - margin, y + line};
    text(target, activity, elapsed, snapshot.failed != 0 ?
        RGB(235, 130, 100) : RGB(130, 155, 158));
    y = elapsed.bottom + 7;
    const RECT track = progressTrack(client);
    const int historyLine = std::max(14, line - 2);
    for (unsigned int index = 0; index < HW_SPLASH_HISTORY_LINES; ++index)
    {
        if (y + historyLine > track.top - 8) break;
        text(target, snapshot.history[index],
            RECT{margin, y, client.right - margin, y + historyLine}, RGB(108, 135, 140));
        y += historyLine;
    }
    fill(target, track, RGB(34, 48, 54));
    RECT value = track;
    value.right = value.left + static_cast<LONG>(
        static_cast<unsigned long long>(value.right - value.left) *
        std::min(snapshot.percentHundredths, 10000u) / 10000u);
    fill(target, value, RGB(222, 162, 62));
    if (saved != 0) RestoreDC(target, saved);
}

void releaseCanvas(SplashState &state)
{
    if (state.canvasDc != nullptr && state.oldCanvas != nullptr)
        SelectObject(state.canvasDc, state.oldCanvas);
    if (state.canvas != nullptr) DeleteObject(state.canvas);
    if (state.canvasDc != nullptr) DeleteDC(state.canvasDc);
    state.canvasDc = nullptr;
    state.canvas = nullptr;
    state.oldCanvas = nullptr;
    state.canvasWidth = state.canvasHeight = 0;
}

void refreshSnapshot(SplashState &state)
{
    hwWindowsSplashGetSnapshot(&state.snapshot);
#if defined(HW_WINDOWS_SPLASH_TESTING)
    splashTestCompleted.store(state.snapshot.completed);
#endif
    if (state.window != nullptr) InvalidateRect(state.window, nullptr, FALSE);
}

void applyVisibility(SplashState &state)
{
    if (state.window == nullptr) return;
#if defined(HW_WINDOWS_SPLASH_TESTING)
    ShowWindow(state.window, SW_HIDE);
#else
    ShowWindow(state.window, control.visible.load() ? SW_SHOWNOACTIVATE : SW_HIDE);
#endif
}

LRESULT CALLBACK splashWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<SplashState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        state = static_cast<SplashState *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->window = window;
    }
    if (state == nullptr) return DefWindowProcW(window, message, wParam, lParam);
    switch (message)
    {
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wParam == heartbeatTimer)
        {
#if defined(HW_WINDOWS_SPLASH_TESTING)
            ++splashTestTimerTicks;
#endif
            refreshSnapshot(*state);
        }
        return 0;
    case WM_CLOSE:
        control.visible.store(false);
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint = {};
        HDC target = BeginPaint(window, &paint);
        RECT client = {};
        GetClientRect(window, &client);
        if (state->canvasWidth != client.right || state->canvasHeight != client.bottom)
        {
            releaseCanvas(*state);
            state->canvasDc = CreateCompatibleDC(target);
            state->canvas = CreateCompatibleBitmap(target, client.right, client.bottom);
            if (state->canvasDc != nullptr && state->canvas != nullptr)
            {
                state->oldCanvas = SelectObject(state->canvasDc, state->canvas);
                state->canvasWidth = client.right;
                state->canvasHeight = client.bottom;
            }
            else releaseCanvas(*state);
        }
        HDC drawing = state->canvasDc != nullptr ? state->canvasDc : target;
        paintSplash(drawing, client, *state, state->snapshot, steadySeconds());
        if (state->canvasDc != nullptr)
            BitBlt(target, 0, 0, client.right, client.bottom, state->canvasDc, 0, 0, SRCCOPY);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window, heartbeatTimer);
        control.window.store(nullptr);
        state->window = nullptr;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void releaseState(SplashState &state)
{
    if (state.window != nullptr) DestroyWindow(state.window);
    releaseCanvas(state);
    if (state.artwork != nullptr) DeleteObject(state.artwork);
    if (state.headingFont != nullptr) DeleteObject(state.headingFont);
    if (state.bodyFont != nullptr) DeleteObject(state.bodyFont);
}

DWORD WINAPI splashThread(void *)
{
    SplashState state;
    try
    {
        MSG message = {};
        // Queue first, so visibility requests can arrive during artwork I/O.
        PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        WNDCLASSW windowClass = {};
        windowClass.lpfnWndProc = splashWindowProcedure;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.lpszClassName = L"HomeworldModernStartupSplash";
        RegisterClassW(&windowClass);
        loadArtwork(state);
        const int desktopWidth = GetSystemMetrics(SM_CXSCREEN);
        const int desktopHeight = GetSystemMetrics(SM_CYSCREEN);
        int width = std::min(1120, desktopWidth * 3 / 5), height = width * 9 / 16;
        if (height > desktopHeight * 4 / 5)
        {
            height = desktopHeight * 4 / 5;
            width = height * 16 / 9;
        }
        state.headingFont = CreateFontW(-std::max(18, height / 25),
            0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_DONTCARE, L"Bahnschrift SemiCondensed");
        state.bodyFont = CreateFontW(-std::max(11, height / 40),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_DONTCARE, L"Bahnschrift");
        refreshSnapshot(state);
        if (WaitForSingleObject(control.stop, 0) != WAIT_OBJECT_0)
            CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, windowClass.lpszClassName,
                L"Homeworld Modern", WS_POPUP, (desktopWidth - width) / 2,
                (desktopHeight - height) / 2, width, height,
                nullptr, nullptr, windowClass.hInstance, &state);
        control.window.store(state.window);
        if (state.window != nullptr)
        {
            SetTimer(state.window, heartbeatTimer, 250, nullptr);
            applyVisibility(state);
        }
        SetEvent(control.ready);
        bool running = state.window != nullptr;
        while (running)
        {
            const DWORD wait = MsgWaitForMultipleObjectsEx(1, &control.stop,
                INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wait != WAIT_OBJECT_0 + 1) break;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT) { running = false; break; }
                if (message.hwnd == nullptr && message.message == visibilityMessage)
                    applyVisibility(state);
                else
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        }
    }
    catch (...)
    {
        // A splash failure must not abort an otherwise viable startup.
        SetEvent(control.ready);
    }
    releaseState(state);
    control.window.store(nullptr);
    return 0;
}

void releaseJoinedThread()
{
    if (control.thread != nullptr) CloseHandle(control.thread);
    if (control.ready != nullptr) CloseHandle(control.ready);
    if (control.stop != nullptr) CloseHandle(control.stop);
    control.thread = control.ready = control.stop = nullptr;
    control.threadId = 0;
}
}

extern "C" void hwWindowsSplashStart(void)
{
    std::lock_guard<std::mutex> lock(control.mutex);
    if (control.thread != nullptr)
    {
        if (WaitForSingleObject(control.thread, 0) != WAIT_OBJECT_0) return;
        releaseJoinedThread();
    }
    hwWindowsSplashProgressReset();
    control.visible.store(true);
    control.ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    control.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (control.ready == nullptr || control.stop == nullptr)
    {
        releaseJoinedThread();
        return;
    }
    control.thread = CreateThread(nullptr, 0, splashThread, nullptr, 0, &control.threadId);
    if (control.thread == nullptr)
    {
        releaseJoinedThread();
        return;
    }
    // Bounded: startup never has to pump splash messages during compilation.
    WaitForSingleObject(control.ready, 1500);
}

extern "C" void hwWindowsSplashSetVisible(int visible)
{
    std::lock_guard<std::mutex> lock(control.mutex);
    control.visible.store(visible != 0);
    if (control.threadId != 0)
        PostThreadMessageW(control.threadId, visibilityMessage, 0, 0);
}

extern "C" void hwWindowsSplashFinish(void)
{
    // The worker never takes this lifecycle lock. Serializing Start/Finish
    // prevents a concurrent caller from closing a handle being waited on.
    std::lock_guard<std::mutex> lock(control.mutex);
    hwWindowsSplashProgressStop();
    if (control.thread == nullptr) return;
    SetEvent(control.stop);
    if (WaitForSingleObject(control.thread, 5000) == WAIT_OBJECT_0)
        releaseJoinedThread();
    // Never kill the thread or free its live context on timeout. A later
    // Finish/Start can reap it after exit; at most one worker can exist.
}

#else
extern "C" void hwWindowsSplashStart(void) { hwWindowsSplashProgressReset(); }
extern "C" void hwWindowsSplashSetVisible(int) {}
extern "C" void hwWindowsSplashFinish(void) { hwWindowsSplashProgressStop(); }
#endif
