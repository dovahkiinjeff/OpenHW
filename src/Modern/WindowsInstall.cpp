#include "DiagnosticPolicy.h"
#include "WindowsInstall.h"
#include "WindowsSplash.h"
#include "../Game/OpenHWArchive.h"

#if defined(_WIN32)

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>

extern "C" void fileHomeworldDataPathSet(char *path);
extern "C" int fileOverrideBigPathSet(char *path);
extern "C" int fileUserSettingsPathSet(char *path);

namespace
{
std::wstring moduleDirectory()
{
    wchar_t path[32768] = {};
    DWORD count = GetModuleFileNameW(nullptr, path,
        static_cast<DWORD>(_countof(path)));
    if (count == 0 || count >= _countof(path)) return {};
    std::wstring result(path, count);
    const size_t slash = result.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() :
        result.substr(0, slash);
}

std::wstring parentDirectory(const std::wstring &path)
{
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() :
        path.substr(0, slash);
}

std::wstring fullPath(const std::wstring &path)
{
    wchar_t resolved[32768] = {};
    DWORD count = GetFullPathNameW(path.c_str(),
        static_cast<DWORD>(_countof(resolved)), resolved, nullptr);
    return count == 0 || count >= _countof(resolved) ? path :
        std::wstring(resolved, count);
}

bool regularFile(const std::wstring &path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool validDataDirectory(const std::wstring &path)
{
    /* Only the BIG archive is required to bind the original game data.
       Speech/music stream archives are optional: installations that do not
       provide them still run with SFX and the rest of the game data. */
    return regularFile(path + L"\\homeworld.big");
}

std::wstring resolveDataDirectory(const std::wstring &path)
{
    if (path.empty()) return {};

    const std::wstring resolved = fullPath(path);
    const std::wstring candidates[] = {
        resolved,
        resolved + L"\\Data",
        resolved + L"\\data"
    };
    for (const std::wstring &candidate : candidates)
    {
        const std::wstring normalized = fullPath(candidate);
        if (validDataDirectory(normalized)) return normalized;
    }
    return {};
}

std::wstring environmentPath(const wchar_t *name)
{
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) return {};
    std::vector<wchar_t> value(static_cast<size_t>(needed));
    const DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0 || written >= needed) return {};
    return fullPath(value.data());
}

std::wstring locateDataNearModern()
{
    // RTX deployments live below the legally installed Homeworld tree, e.g.
    //   Homeworld1Classic\\RTX\\RTX Deployed\\HomeworldModern.exe
    // Walk several ancestors and accept either files directly in that folder
    // or the conventional Data subdirectory.  This also keeps standalone
    // deployments working when the data is deliberately placed beside the EXE.
    std::wstring current = moduleDirectory();
    for (int depth = 0; depth < 6 && !current.empty(); ++depth)
    {
        const std::wstring found = resolveDataDirectory(current);
        if (!found.empty()) return found;
        const std::wstring parent = parentDirectory(current);
        if (parent.empty() || parent == current) break;
        current = parent;
    }
    return {};
}

std::wstring perUserGameRoot()
{
    PWSTR documents = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(
            FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &documents)) &&
        documents != nullptr)
    {
        result.assign(documents);
        CoTaskMemFree(documents);
    }
    else
    {
        wchar_t path[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL |
            CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            return moduleDirectory() + L"\\UserData";
        }
        result.assign(path);
    }
    return result + L"\\My Games\\Homeworld Modern";
}

std::wstring readSetting(const std::wstring &config, const wchar_t *name)
{
    wchar_t value[32768] = {};
    GetPrivateProfileStringW(L"Installation", name, L"", value,
        static_cast<DWORD>(_countof(value)), config.c_str());
    return value;
}

void saveInstallation(const std::wstring &config,
                      const std::wstring &dataDirectory)
{
    WritePrivateProfileStringW(L"Installation", L"DataDirectory",
        dataDirectory.c_str(), config.c_str());
}

std::wstring selectDataDirectory()
{
    hwWindowsSplashUpdate("Waiting for you to locate Homeworld.big");
    hwWindowsSplashSetVisible(0);

    wchar_t path[32768] = L"Homeworld.big";
    const wchar_t filter[] =
        L"Homeworld data archive (Homeworld.big)\0Homeworld.big\0"
        L"BIG archives (*.big)\0*.big\0\0";
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(_countof(path));
    dialog.lpstrTitle =
        L"Locate Homeworld.big from your installed Homeworld 1 Classic data";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_DONTADDTORECENT;
    const bool selected = GetOpenFileNameW(&dialog) != FALSE;

    hwWindowsSplashSetVisible(1);
    hwWindowsSplashUpdate("Checking the selected game data directory");
    if (!selected) return {};

    const std::wstring directory = parentDirectory(fullPath(path));
    return validDataDirectory(directory) ? directory : std::wstring();
}

bool narrowPath(const std::wstring &source, std::vector<char> &destination)
{
    const int length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
        source.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) return false;
    destination.resize(static_cast<size_t>(length));
    BOOL usedDefault = FALSE;
    if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, source.c_str(), -1,
        destination.data(), length, nullptr, &usedDefault) <= 0 || usedDefault)
    {
        return false;
    }
    return true;
}

void prepareLocalAssetImport(const std::wstring &root, const std::wstring &logs)
{
    const std::wstring package = moduleDirectory() + L"\\AssetImporter";
    const std::wstring executable = package + L"\\HomeworldAssetImporter.exe";
    const std::wstring cache = root + L"\\ImportedAssets";
    if (regularFile(executable))
    {
        hwWindowsSplashUpdate("checking local Remastered background imports");
        std::wstring command = L"\"" + executable + L"\" --only remastered --first-boot --cache \"" +
            cache + L"\" --tools \"" + package + L"\\Tools\" --runtime \"" +
            moduleDirectory() + L"\"";
        SECURITY_ATTRIBUTES security = {sizeof(security), nullptr, TRUE};
        HANDLE log = CreateFileW(
#if HW_ENABLE_DIAGNOSTICS
            (logs + L"\\asset-import.log").c_str(), FILE_APPEND_DATA,
#else
            L"NUL", GENERIC_WRITE,
#endif
            FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = startup.hStdError = log;
        startup.hStdInput = input;
        PROCESS_INFORMATION process = {};
        bool started = log != INVALID_HANDLE_VALUE && input != INVALID_HANDLE_VALUE &&
            CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, package.c_str(), &startup, &process) != FALSE;
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        DWORD exitCode = 1;
        if (started)
        {
            CloseHandle(process.hThread);
            while (WaitForSingleObject(process.hProcess, 50) == WAIT_TIMEOUT)
            {
                MSG message;
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                hwWindowsSplashUpdate(HW_ENABLE_DIAGNOSTICS ? "local asset conversion — progress is in Logs/asset-import.log" : "Converting local assets");
            }
            GetExitCodeProcess(process.hProcess, &exitCode);
            CloseHandle(process.hProcess);
        }
        if (exitCode != 0)
            MessageBoxW(nullptr,
                L"Local asset import did not finish. The last valid cache and installed game files "
                L"are unchanged. "
#if HW_ENABLE_DIAGNOSTICS
                L"See My Games\\Homeworld Modern\\Logs\\asset-import.log. "
#endif
                L"The current build will continue without the new import.",
                L"Optional asset import", MB_OK | MB_ICONWARNING | MB_TASKMODAL);
    }
    else
        HW_DIAG_FPRINTF(stderr, "[Install] Optional AssetImporter is not packaged; no import was run.\n");

    // The importer atomically publishes this UTF-8 pointer only after validating
    // all generated outputs. Never let it redirect the engine outside its cache.
    std::ifstream statusFile(cache + L"\\status.json", std::ios::binary);
    std::string status((std::istreambuf_iterator<char>(statusFile)), std::istreambuf_iterator<char>());
    if (status.find("\"status\": \"not-installed\"") != std::string::npos) return;
    std::ifstream pointer(cache + L"\\active-root.txt", std::ios::binary);
    std::string encoded((std::istreambuf_iterator<char>(pointer)), std::istreambuf_iterator<char>());
    if (encoded.empty() || encoded.size() > 32000 || encoded.find('\0') != std::string::npos) return;
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded.data(),
        static_cast<int>(encoded.size()), nullptr, 0);
    if (length <= 0) return;
    std::wstring decoded(static_cast<size_t>(length), 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded.data(),
        static_cast<int>(encoded.size()), &decoded[0], length);
    decoded = fullPath(decoded);
    const std::wstring prefix = fullPath(cache) + L"\\generation-";
    if (decoded.size() <= prefix.size() || _wcsnicmp(decoded.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        decoded.size() < 8 || decoded.substr(decoded.size()-8) != L"\\runtime") return;
    const std::wstring receipt = parentDirectory(decoded) + L"\\receipt.json";
    std::vector<char> narrow;
    if (!regularFile(receipt) || !narrowPath(decoded, narrow)) return;
    SetEnvironmentVariableA("HW_IMPORTED_ASSETS", narrow.data());
    _putenv_s("HW_IMPORTED_ASSETS", narrow.data());
    HW_DIAG_FPRINTF(stderr, "[Install] Local converted asset cache: %s\n", narrow.data());
}
}

extern "C" int hwWindowsInstallPrepare(void)
{
    const bool portable = regularFile(moduleDirectory() + L"\\openhw.portable");
    const std::wstring root = portable ? moduleDirectory() : perUserGameRoot();
    const bool looseData = portable && regularFile(root + L"\\openhw.loose");
    if(portable)
    {
        SetCurrentDirectoryW(root.c_str());
        SetEnvironmentVariableW(L"HW_PORTABLE_ROOT",root.c_str());
        _wputenv_s(L"HW_PORTABLE_ROOT",root.c_str());
        SetEnvironmentVariableA("HW_LOOSE_TEXTURE_OVERRIDES","1");
        _putenv_s("HW_LOOSE_TEXTURE_OVERRIDES","1");
        char reason[256]{};
        if(!regularFile(root+L"\\setup-complete.json") ||
           (!looseData && !openhwArchiveMount((root+L"\\OpenHW-HW1.obig").c_str(),reason,sizeof(reason))))
        {
            MessageBoxA(nullptr,*reason?reason:"Run OpenHW-Setup.exe in this folder to complete setup.",
                "OpenHW setup required",MB_OK|MB_ICONERROR|MB_TASKMODAL);
            return 0;
        }
        const auto presentation=root+L"\\OpenHW-Presentation.obig";
        if(!looseData && regularFile(presentation) && !openhwArchiveMountPresentation(presentation.c_str(),reason,sizeof(reason))) {
            MessageBoxA(nullptr,reason,"OpenHW presentation update failed",MB_OK|MB_ICONERROR|MB_TASKMODAL);
            return 0;
        }
        if(looseData){
            if(GetFileAttributesW((root+L"\\Data").c_str())==INVALID_FILE_ATTRIBUTES){
                MessageBoxW(nullptr,L"Loose Data directory is missing.",L"OpenHW WIP setup required",MB_OK|MB_ICONERROR|MB_TASKMODAL);
                return 0;
            }
            std::vector<char> looseRoot;
            if(!narrowPath(root,looseRoot))return 0;
            openhwArchiveSetLooseRoot(looseRoot.data());
            SetEnvironmentVariableA("HW_LOOSE_DATA","1");
            _putenv_s("HW_LOOSE_DATA","1");
        }
    }
    hwWindowsSplashUpdate("Preparing settings, saves and log directories");
    const std::wstring settings = root;
    const std::wstring config = root + L"\\installation.ini";
    const std::wstring development = root + (looseData ? L"\\Dev" : L"\\Development");
    const std::wstring logs = portable ? development + L"\\Logs" : root + L"\\Logs";
    if (portable)
    {
        SHCreateDirectoryExW(nullptr, development.c_str(), nullptr);
        const std::wstring temporary = development + L"\\Temp";
        SHCreateDirectoryExW(nullptr, temporary.c_str(), nullptr);
        SetEnvironmentVariableW(L"HW_DEVELOPMENT_ROOT", development.c_str());
        _wputenv_s(L"HW_DEVELOPMENT_ROOT", development.c_str());
        // Includes the lossless material backing cache and third-party tools.
        // Never use the user's AppData/Temp for portable game-owned files.
        for (const wchar_t *name : {L"TEMP", L"TMP", L"TMPDIR"}) {
            SetEnvironmentVariableW(name, temporary.c_str());
            _wputenv_s(name, temporary.c_str());
        }
    }
    SHCreateDirectoryExW(nullptr, settings.c_str(), nullptr);
    if(looseData)SHCreateDirectoryExW(nullptr,(root+L"\\Dev").c_str(),nullptr);
#if HW_ENABLE_DIAGNOSTICS
    SHCreateDirectoryExW(nullptr, logs.c_str(), nullptr);
#endif
    // Prepare save trees before the first scripted autosave, rather than
    // relying on a previously populated developer/user profile.
    for (const wchar_t *relative : {L"\\SavedGames\\SinglePlayer",
                                   L"\\SavedGames\\MultiPlayer",
                                   L"\\SavedGames\\Training",
                                   L"\\SavedGames\\RecordedGames"})
    {
        const std::wstring directory = settings + relative;
        const int result = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
        const DWORD attributes = GetFileAttributesW(directory.c_str());
        if ((result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS &&
             result != ERROR_FILE_EXISTS) || attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            HW_DIAG_FPRINTF(stderr, "[Install] Unable to prepare save directory: %ls (error %d).\n",
                directory.c_str(), result);
            // Saving reports a recoverable error in-game. Do not kill a
            // mission merely because this profile cannot currently save.
        }
    }

    // The original executable is not a runtime dependency.  Bind directly to
    // the legally installed data instead.  Prefer an explicit HW_Data value,
    // then a previously remembered directory, then auto-discover around the
    // deployed executable.  Only show a picker when all automatic routes fail.
    hwWindowsSplashUpdate("Locating and validating installed game archives");
    std::wstring dataDirectory = portable ? root + L"\\Data" :
        resolveDataDirectory(environmentPath(L"HW_Data"));
    if (portable && dataDirectory.empty())
    {
        MessageBoxW(nullptr, L"Portable OpenHW setup is incomplete. Run setup in this folder before starting the game. No external installation will be used as a fallback.",
            L"OpenHW setup required", MB_OK | MB_ICONERROR | MB_TASKMODAL);
        return 0;
    }
    if (!portable && dataDirectory.empty())
    {
        dataDirectory = resolveDataDirectory(
            readSetting(config, L"DataDirectory"));
    }
    if (dataDirectory.empty())
    {
        dataDirectory = locateDataNearModern();
    }
    if (dataDirectory.empty())
    {
        dataDirectory = selectDataDirectory();
        if (dataDirectory.empty())
        {
            MessageBoxW(nullptr,
                L"OpenHW needs the installed Homeworld 1 Classic "
                L"Homeworld.big archive. The original Homeworld.exe, "
                L"HW_Comp.vce and HW_Music.wxd are not required to launch. "
                L"Place the RTX deployment beneath your Homeworld1Classic "
                L"installation or select Homeworld.big when prompted.",
                L"Homeworld data not found",
                MB_OK | MB_ICONERROR | MB_TASKMODAL);
            return 0;
        }
    }
    saveInstallation(config, dataDirectory);

    std::vector<char> dataNarrow;
    std::vector<char> overrideNarrow;
    std::vector<char> settingsNarrow;
    if (!narrowPath(dataDirectory, dataNarrow) ||
        !narrowPath(moduleDirectory(), overrideNarrow) ||
        !narrowPath(settings, settingsNarrow))
    {
        MessageBoxW(nullptr,
            L"OpenHW cannot currently open an installation "
            L"whose path contains characters outside the active Windows "
            L"code page. Move the mod or game to a simpler path and retry.",
            L"Unsupported installation path", MB_OK | MB_ICONERROR |
            MB_TASKMODAL);
        return 0;
    }

    hwWindowsSplashUpdate("Connecting game data, visual assets and save paths");
    fileHomeworldDataPathSet(dataNarrow.data());
    fileOverrideBigPathSet(looseData?dataNarrow.data():overrideNarrow.data());
    fileUserSettingsPathSet(settingsNarrow.data());
    SetEnvironmentVariableW(L"HW_Data", dataDirectory.c_str());
    _wputenv_s(L"HW_Data", dataDirectory.c_str());
    if (portable)
    {
        // Never rediscover or revalidate the original installation at runtime.
        SetCurrentDirectoryW(root.c_str());
        SetEnvironmentVariableW(L"HW_PORTABLE_ROOT", root.c_str());
        _wputenv_s(L"HW_PORTABLE_ROOT", root.c_str());
        const std::wstring imported = looseData?dataDirectory:root;
        SetEnvironmentVariableW(L"HW_IMPORTED_ASSETS", imported.c_str());
        _wputenv_s(L"HW_IMPORTED_ASSETS", imported.c_str());
        for (const wchar_t *relative : {L"\\Modding", L"\\mods"})
            SHCreateDirectoryExW(nullptr, (root + relative).c_str(), nullptr);
        for (const wchar_t *relative : {
#if HW_ENABLE_DIAGNOSTICS
            L"\\Diagnostics",
#endif
            L"\\ShaderCache"})
            SHCreateDirectoryExW(nullptr, (development + relative).c_str(), nullptr);
    }
    else prepareLocalAssetImport(root, logs);
    /* Diagnostic launches provide their own run-specific crash directory.
       Preserve it so run-with-log.ps1 can package the dump with its exact
       executable and PDB; normal launches use the My Games Logs folder. */
#if HW_ENABLE_DIAGNOSTICS
    wchar_t diagnosticCrashDirectory[2] = {};
    if (portable || GetEnvironmentVariableW(L"HW_CRASH_DIR", diagnosticCrashDirectory,
                                _countof(diagnosticCrashDirectory)) == 0)
    {
        SetEnvironmentVariableW(L"HW_CRASH_DIR", logs.c_str());
    }
#endif
    HW_DIAG_FPRINTF(stderr,
        "[Install] Verified external Homeworld data.\n"
        "[Install] Content root: %ls\n"
        "[Install] Loose-file override root: %ls\n"
        "[Install] Player data root: %ls\n",
        dataDirectory.c_str(), moduleDirectory().c_str(), settings.c_str());
    return 1;
}

#else

extern "C" int hwWindowsInstallPrepare(void) { return 1; }

#endif
