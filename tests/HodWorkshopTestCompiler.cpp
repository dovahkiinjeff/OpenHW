#include "OpenHWShipDocument.h"
#include "../src/Game/OpenHWArchive.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
struct Files {fs::path edited, ship;};

static int readFile(void* context, const char* key, const void** bytes, size_t* size) {
    auto& paths = *static_cast<Files*>(context);
    const auto installation = fs::path(_wgetenv(L"OPENHW_EDITOR_TEST_ROOT"));
    const bool shared = std::strncmp(key, "@/", 2) == 0;
    const auto member = fs::u8path(shared ? key + 2 : key);
    for (const auto& candidate : {shared ? fs::path{} : paths.edited/member,
            installation/"Data"/(shared ? member : paths.ship/member),
            installation/(shared ? member : paths.ship/member)}) {
        std::ifstream file(candidate, std::ios::binary);
        if (!file) continue;
        std::string data{std::istreambuf_iterator<char>(file), {}};
        void* buffer = std::malloc(data.size());
        if (!buffer) return 0;
        *bytes = std::memcpy(buffer, data.data(), data.size());
        *size = data.size();
        return 1;
    }
    char error[256]={};
    if (!openhwArchiveMounted() && !openhwArchiveMount((installation/L"OpenHW-HW1.obig").c_str(),error,sizeof(error))) return 0;
    const auto resource=(shared ? member : paths.ship/member).generic_string();
    int length=0;
    const auto* view=openhwArchiveView(resource.c_str(),&length);
    if(!view || length<=0)return 0;
    void* buffer=std::malloc(length);
    if(!buffer)return 0;
    *bytes=std::memcpy(buffer,view,length);
    *size=length;
    return 1;
}
static void release(void*, const void* ptr) {std::free(const_cast<void*>(ptr));}
extern "C" __declspec(dllexport) int complete(const char* root, const char* text, size_t size,
        char* out, size_t cap, char* error, size_t errorSize) {
    Files paths{{},root};
    void* result=nullptr;
    size_t length=0;
    if (!openhwShipDocumentComplete(text,size,readFile,release,&paths,&result,&length,error,errorSize)) return 0;
    const bool fits=length<cap;
    if (fits) {std::memcpy(out,result,length);out[length]=0;}
    openhwShipDocumentFree(result);
    return fits;
}
extern "C" __declspec(dllexport) int validate_saved(const wchar_t* folder, const char* definition,
        size_t length, char* error, size_t errorSize) {
    Files paths{folder,"R1/LightInterceptor/RMNative"};
    auto* asset=hwRmVisualPackageRead("LightInterceptor",definition,length,readFile,release,[](void*,HwRmVisualAsset*,const char*,char*,size_t){return 1;},&paths,error,errorSize);
    if (!asset) return 0;
    hwRmVisualPackageFree(asset);
    return 1;
}
