#include "DiagnosticPolicy.h"
#include "WindowsDiagnostics.h"

#if !HW_ENABLE_DIAGNOSTICS
extern "C" void hwWindowsDiagnosticsInstall(void) {}
#elif defined(_WIN32)

#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>
#include <shellapi.h>

#include <cstdio>
#include <io.h>
#include <cwchar>
#include <cwctype>

namespace
{
    constexpr size_t CrashPathCapacity = 32768;
    wchar_t crashDirectory[CrashPathCapacity] = L"";
    volatile LONG writingCrashDump = 0;

    void selectCrashDirectory()
    {
        DWORD length = GetEnvironmentVariableW(
            L"HW_CRASH_DIR", crashDirectory,
            static_cast<DWORD>(CrashPathCapacity));

        if (length == 0 || length >= CrashPathCapacity)
        {
            length = GetModuleFileNameW(
                nullptr, crashDirectory,
                static_cast<DWORD>(CrashPathCapacity));
            if (length == 0 || length >= CrashPathCapacity)
            {
                wcscpy_s(crashDirectory, L"Logs");
            }
            else
            {
                wchar_t *separator = wcsrchr(crashDirectory, L'\\');
                if (separator != nullptr)
                {
                    *separator = L'\0';
                    wcscat_s(crashDirectory, L"\\Logs");
                }
                else
                {
                    wcscpy_s(crashDirectory, L"Logs");
                }
            }
        }

        SHCreateDirectoryExW(nullptr, crashDirectory, nullptr);
    }

    void makeCrashPath(wchar_t *destination, size_t destinationCount,
                       const wchar_t *extension, const SYSTEMTIME &time)
    {
        swprintf_s(
            destination, destinationCount,
            L"%s\\crash-%04u%02u%02u-%02u%02u%02u-%lu.%s",
            crashDirectory,
            time.wYear, time.wMonth, time.wDay,
            time.wHour, time.wMinute, time.wSecond,
            GetCurrentProcessId(), extension);
    }

    LONG WINAPI writeUnhandledExceptionDump(EXCEPTION_POINTERS *exceptionPointers)
    {
        if (InterlockedCompareExchange(&writingCrashDump, 1, 0) != 0)
        {
            return EXCEPTION_EXECUTE_HANDLER;
        }

        std::fflush(nullptr);

        SYSTEMTIME now;
        wchar_t dumpPath[CrashPathCapacity];
        wchar_t reportPath[CrashPathCapacity];
        BOOL dumpWritten = FALSE;
        DWORD dumpError = ERROR_SUCCESS;

        GetLocalTime(&now);
        makeCrashPath(dumpPath, CrashPathCapacity, L"dmp", now);
        makeCrashPath(reportPath, CrashPathCapacity, L"txt", now);

        HANDLE dumpFile = CreateFileW(
            dumpPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (dumpFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION exceptionInformation = {};
            exceptionInformation.ThreadId = GetCurrentThreadId();
            exceptionInformation.ExceptionPointers = exceptionPointers;
            exceptionInformation.ClientPointers = FALSE;

            MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
                MiniDumpNormal |
                MiniDumpWithThreadInfo |
                MiniDumpWithUnloadedModules |
                MiniDumpWithIndirectlyReferencedMemory);

            dumpWritten = MiniDumpWriteDump(
                GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
                dumpType, &exceptionInformation, nullptr, nullptr);
            if (!dumpWritten)
            {
                dumpError = GetLastError();
            }
            FlushFileBuffers(dumpFile);
            CloseHandle(dumpFile);
        }
        else
        {
            dumpError = GetLastError();
        }

        FILE *report = nullptr;
        if (_wfopen_s(&report, reportPath, L"wt") == 0 && report != nullptr)
        {
            const DWORD exceptionCode =
                exceptionPointers != nullptr &&
                exceptionPointers->ExceptionRecord != nullptr
                    ? exceptionPointers->ExceptionRecord->ExceptionCode
                    : 0;
            const void *exceptionAddress =
                exceptionPointers != nullptr &&
                exceptionPointers->ExceptionRecord != nullptr
                    ? exceptionPointers->ExceptionRecord->ExceptionAddress
                    : nullptr;

            fwprintf(report, L"Homeworld Modern unhandled exception\n");
            fwprintf(report, L"Exception code: 0x%08lX\n", exceptionCode);
            fwprintf(report, L"Exception address: %p\n", exceptionAddress);
            fwprintf(report, L"Process ID: %lu\n", GetCurrentProcessId());
            fwprintf(report, L"Thread ID: %lu\n", GetCurrentThreadId());
            fwprintf(report, L"Minidump written: %s\n", dumpWritten ? L"yes" : L"no");
            fwprintf(report, L"Minidump error: %lu\n", dumpError);
            fwprintf(report, L"Minidump path: %s\n", dumpPath);
            fclose(report);
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }
}

extern "C" int hwDiagnosticsDisabled(void)
{
    static const bool disabled=[] {
        int count=0; auto args=CommandLineToArgvW(GetCommandLineW(),&count);
        bool found=false;
        if(args) {
            for(int i=1;i<count;++i)
                if(!_wcsicmp(args[i],L"/noDiagnostics") || !_wcsicmp(args[i],L"-noDiagnostics")) found=true;
            LocalFree(args);
        }
        return found;
    }();
    return disabled;
}

extern "C" void hwWindowsDiagnosticsInstall(void)
{
    if(hwDiagnosticsDisabled()) {
        FILE *stream=nullptr;
        _wfreopen_s(&stream,L"NUL",L"wt",stderr);
        _wfreopen_s(&stream,L"NUL",L"wt",stdout);
        return;
    }
    selectCrashDirectory();
    SetUnhandledExceptionFilter(writeUnhandledExceptionDump);

    // Launch-only quiet mode; crash files remain available independently.
    const wchar_t *command = GetCommandLineW();
    const wchar_t *quietSwitches[] = {L"-noRendererLog", L"/noRendererLog", L"-benchmark", L"/benchmark"};
    for (const wchar_t *quietSwitch : quietSwitches)
    {
        const size_t length = wcslen(quietSwitch);
        for (const wchar_t *cursor = command; *cursor; ++cursor)
        {
            if ((cursor == command || iswspace(cursor[-1])) &&
                _wcsnicmp(cursor, quietSwitch, length) == 0 &&
                (cursor[length] == 0 || iswspace(cursor[length])))
            {
                FILE *quietStream = nullptr;
                _wfreopen_s(&quietStream, L"NUL", L"wt", stderr);
                _wfreopen_s(&quietStream, L"NUL", L"wt", stdout);
                return;
            }
        }
    }

    // A normal GUI/shortcut launch has no attached stderr collector. Keep
    // renderer diagnostics on disk without requiring a hidden helper launch.
    // Preserve explicit terminal/pipeline redirection used by diagnostic tools.
    HANDLE errorHandle = GetStdHandle(STD_ERROR_HANDLE);
    const DWORD errorType = errorHandle != nullptr && errorHandle != INVALID_HANDLE_VALUE
        ? GetFileType(errorHandle) : FILE_TYPE_UNKNOWN;
    if (errorType != FILE_TYPE_DISK && errorType != FILE_TYPE_PIPE)
    {
        SYSTEMTIME now;
        GetLocalTime(&now);
        wchar_t sessionPath[CrashPathCapacity];
        swprintf_s(sessionPath, CrashPathCapacity,
            L"%s\\renderer-%04u%02u%02u-%02u%02u%02u-%lu.log",
            crashDirectory, now.wYear, now.wMonth, now.wDay,
            now.wHour, now.wMinute, now.wSecond, GetCurrentProcessId());
        FILE *sessionStream = nullptr;
        if (_wfreopen_s(&sessionStream, sessionPath, L"wt", stderr) == 0)
            setvbuf(stderr, nullptr, _IONBF, 0);
    }

    const HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    const DWORD outputType=output && output!=INVALID_HANDLE_VALUE ? GetFileType(output):FILE_TYPE_UNKNOWN;
    if(outputType!=FILE_TYPE_DISK && outputType!=FILE_TYPE_PIPE) _dup2(_fileno(stderr),_fileno(stdout));
    HW_DIAG_FWPRINTF(stderr, L"[Diagnostics] Crash dumps: %s\n", crashDirectory);
    std::fflush(stderr);
}

#else

extern "C" int hwDiagnosticsDisabled(void) { return 0; }

extern "C" void hwWindowsDiagnosticsInstall(void)
{
}

#endif
