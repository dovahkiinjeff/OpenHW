// Single-download launcher. Its authenticated ZIP overlay is extracted only
// beside this EXE; neither PyInstaller nor setup uses the system Temp folder.
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <bcrypt.h>
#include <wincrypt.h>
#ifdef OPENHW_PATCH_INSTALLER
#include <shlobj.h>
#include <tlhelp32.h>
#endif
#include <array>
#include <vector>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#pragma pack(push,1)
struct Footer { char magic[16]; unsigned long long offset,bytes; unsigned char sha[32]; };
#pragma pack(pop)
static_assert(sizeof(Footer)==64,"Installer footer layout");
static HWND windowHandle=nullptr,labelHandle=nullptr,progressHandle=nullptr;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void pump(){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
static void status(const wchar_t* text){if(labelHandle)SetWindowTextW(labelHandle,text);pump();}
static void createProgress(){
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_PROGRESS_CLASS};InitCommonControlsEx(&controls);
    windowHandle=CreateWindowExW(WS_EX_DLGMODALFRAME,L"STATIC",
#ifdef OPENHW_PATCH_INSTALLER
        L"OpenHW 1.0.3 patch",
#else
        L"OpenHW 1.0 setup",
#endif
        WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,560,165,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    labelHandle=CreateWindowExW(0,L"STATIC",L"Preparing installer files in this folder…",WS_CHILD|WS_VISIBLE,20,24,510,42,windowHandle,nullptr,nullptr,nullptr);
    progressHandle=CreateWindowExW(0,PROGRESS_CLASSW,nullptr,WS_CHILD|WS_VISIBLE|PBS_MARQUEE,20,80,510,22,windowHandle,nullptr,nullptr,nullptr);
    SendMessageW(progressHandle,PBM_SETMARQUEE,TRUE,30);UpdateWindow(windowHandle);pump();
}
static std::wstring literal(const std::wstring& input){std::wstring out=L"'";for(wchar_t c:input){out+=c;if(c==L'\'')out+=c;}return out+L"'";}
static std::wstring quote(const std::wstring& input){return L"\""+input+L"\"";}
static DWORD launch(const std::wstring& application,std::wstring arguments,HANDLE log,bool visible=false){
    STARTUPINFOW startup={sizeof(startup)};PROCESS_INFORMATION process={};
    startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=visible?SW_SHOWNORMAL:SW_HIDE;
    SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE};
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log!=INVALID_HANDLE_VALUE){startup.dwFlags|=STARTF_USESTDHANDLES;startup.hStdOutput=startup.hStdError=log;startup.hStdInput=input;}
    std::wstring command=quote(application)+L" "+arguments;
    const BOOL started=CreateProcessW(application.c_str(),command.data(),nullptr,nullptr,log!=INVALID_HANDLE_VALUE,
        CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);
    if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
    check(started!=0,"Cannot start the local installer helper.");
    CloseHandle(process.hThread);
    while(WaitForSingleObject(process.hProcess,100)==WAIT_TIMEOUT)pump();
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);return code;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR args,int){
    const bool quiet=args && std::wstring(args).find(L"--verify-payload")!=std::wstring::npos;
    HANDLE source=INVALID_HANDLE_VALUE,output=INVALID_HANDLE_VALUE,log=INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    try{
        wchar_t executable[32768]={};check(GetModuleFileNameW(nullptr,executable,32768)!=0,"Cannot locate this installer.");
        auto root=std::filesystem::path(executable).parent_path();
#ifdef OPENHW_PATCH_INSTALLER
        if(!std::filesystem::is_regular_file(root/L"setup-complete.json")){
            CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
            BROWSEINFOW selection={};selection.lpszTitle=L"Select your installed OpenHW folder";
            selection.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            auto item=SHBrowseForFolderW(&selection);
            if(!item)return 0;
            wchar_t selected[MAX_PATH]={};const BOOL selectedOk=SHGetPathFromIDListW(item,selected);CoTaskMemFree(item);
            check(selectedOk!=0,"Cannot read the selected installation folder.");root=selected;
        }
        check(std::filesystem::is_regular_file(root/L"setup-complete.json") &&
              std::filesystem::is_regular_file(root/L"Development/Installer/Development/python312.dll"),
              "Select an installed OpenHW 1.0 folder, not the original Collection.");
        check(std::filesystem::is_regular_file(root/L"Development/Patches/1.0.2/applied.json"),
              "Install OpenHW 1.0.1 and 1.0.2 before applying this patch.");
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        check(snapshot!=INVALID_HANDLE_VALUE,"Cannot check whether OpenHW is running.");
        PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);bool running=false;
        if(Process32FirstW(snapshot,&entry))do{
            if(!_wcsicmp(entry.szExeFile,L"OpenHW.exe") || !_wcsicmp(entry.szExeFile,L"OpenHW-Daignostics.exe"))running=true;
        }while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);
        check(!running,"Close OpenHW and OpenHW-Daignostics before applying this patch.");
        const auto backup=root/L"Development/Patches/1.0.3/Backup";
        std::filesystem::create_directories(backup);
        for(const wchar_t* name:{L"OpenHW.exe",L"OpenHW-Daignostics.exe",L"version.json"})
            if(std::filesystem::is_regular_file(root/name) && !std::filesystem::exists(backup/name))
                std::filesystem::copy_file(root/name,backup/name);
#endif
        const auto development=root/L"Development";
        std::filesystem::create_directories(development/L"Logs");
        std::filesystem::create_directories(development/L"Temp");
        for(const wchar_t* name:{L"TEMP",L"TMP",L"TMPDIR"})SetEnvironmentVariableW(name,(development/L"Temp").c_str());
        SetEnvironmentVariableW(L"PSModuleAnalysisCachePath",(development/L"Temp/PowerShellModuleAnalysisCache").c_str());
        SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE};
        log=CreateFileW((development/L"Logs/bootstrap.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,&security,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(!quiet)createProgress();
        source=CreateFileW(executable,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(source!=INVALID_HANDLE_VALUE,"Cannot read installer package.");
        LARGE_INTEGER size={},position={};check(GetFileSizeEx(source,&size)!=0 && size.QuadPart>sizeof(Footer),"Installer is incomplete.");
        position.QuadPart=size.QuadPart-sizeof(Footer);SetFilePointerEx(source,position,nullptr,FILE_BEGIN);
        Footer footer={};DWORD read=0;check(ReadFile(source,&footer,sizeof(footer),&read,nullptr)!=0 && read==sizeof(footer),"Cannot read installer footer.");
        const char magic[16]={'O','P','E','N','H','W','1','S','E','T','U','P','Z','I','P',0};
        check(std::memcmp(footer.magic,magic,16)==0 && footer.offset<=static_cast<unsigned long long>(size.QuadPart)-sizeof(Footer) &&
            footer.bytes==static_cast<unsigned long long>(size.QuadPart)-sizeof(Footer)-footer.offset,"Installer package is damaged or truncated.");
        check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot initialize package verification.");
        check(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0,"Cannot initialize package checksum.");
        const auto archive=development/L"setup-payload.zip";
        output=CreateFileW(archive.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(output!=INVALID_HANDLE_VALUE,"Cannot write here. Move this installer to a writable folder and try again.");
        position.QuadPart=footer.offset;SetFilePointerEx(source,position,nullptr,FILE_BEGIN);
        std::vector<unsigned char> buffer(4*1024*1024);unsigned long long remaining=footer.bytes;
        while(remaining){DWORD amount=DWORD(std::min<unsigned long long>(remaining,buffer.size())),written=0;
            check(ReadFile(source,buffer.data(),amount,&read,nullptr)!=0 && read==amount,"Installer package could not be read.");
            check(BCryptHashData(hash,buffer.data(),amount,0)>=0,"Cannot verify package checksum.");
            check(WriteFile(output,buffer.data(),amount,&written,nullptr)!=0 && written==amount,"Not enough free space to unpack the installer.");
            remaining-=amount;pump();}
        CloseHandle(output);output=INVALID_HANDLE_VALUE;CloseHandle(source);source=INVALID_HANDLE_VALUE;
        std::array<unsigned char,32> digest={};check(BCryptFinishHash(hash,digest.data(),32,0)>=0 &&
            std::memcmp(digest.data(),footer.sha,32)==0,"Installer checksum failed. Download the complete installer again.");
        BCryptDestroyHash(hash);hash=nullptr;BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;
        status(L"Unpacking installer support files beside this EXE…");
        const std::wstring script=L"$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath "+literal(archive.wstring())+L" -DestinationPath "+literal(root.wstring())+L" -Force";
        DWORD count=0;check(CryptBinaryToStringW(reinterpret_cast<const BYTE*>(script.data()),DWORD(script.size()*2),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&count)!=0,"Cannot prepare local extraction.");
        std::wstring encoded(count,0);check(CryptBinaryToStringW(reinterpret_cast<const BYTE*>(script.data()),DWORD(script.size()*2),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,encoded.data(),&count)!=0,"Cannot encode local extraction.");
        while(!encoded.empty() && !encoded.back())encoded.pop_back();
        wchar_t windows[32768]={};GetWindowsDirectoryW(windows,32768);
        const auto powershell=std::filesystem::path(windows)/L"System32/WindowsPowerShell/v1.0/powershell.exe";
        check(launch(powershell.wstring(),L"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand "+encoded,log)==0,
            "Could not unpack installer support files. See Development/Logs/bootstrap.log.");
        DeleteFileW(archive.c_str());
        if(windowHandle){DestroyWindow(windowHandle);windowHandle=labelHandle=progressHandle=nullptr;}
        const auto engine=development/L"Installer/OpenHW-SetupEngine.exe";
        const DWORD result=launch(engine.wstring(),
#ifdef OPENHW_PATCH_INSTALLER
            L"--apply-hotfix",
#else
            quiet?L"--verify-payload":L"",
#endif
            log,!quiet);
        if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);return int(result);
    }catch(const std::exception& error){
        if(log!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(log,error.what(),DWORD(std::strlen(error.what())),&written,nullptr);WriteFile(log,"\r\n",2,&written,nullptr);}
        if(source!=INVALID_HANDLE_VALUE)CloseHandle(source);if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);
        if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);
        if(!quiet)MessageBoxA(windowHandle,error.what(),"OpenHW setup",MB_OK|MB_ICONERROR);
        if(windowHandle)DestroyWindow(windowHandle);return 1;
    }
}
