#include "DiagnosticPolicy.h"
#include "../Game/OpenHWArchive.h"
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

#pragma pack(push,1)
struct ObigHeader { char magic[8]; uint32_t version,count; uint64_t dataOffset; };
struct ObigEntry { char path[256]; uint64_t offset; uint32_t size,crc32; };
#pragma pack(pop)
static HANDLE archiveFile=INVALID_HANDLE_VALUE,archiveMapping=nullptr;
static const unsigned char *archiveView=nullptr;
static const ObigHeader *archiveHeader=nullptr;
static const ObigEntry *archiveEntries=nullptr;
static std::string archiveRoot;
static HANDLE presentationFile=INVALID_HANDLE_VALUE,presentationMapping=nullptr;
static const unsigned char *presentationView=nullptr;
static const ObigHeader *presentationHeader=nullptr;
static const ObigEntry *presentationEntries=nullptr;
static bool normalize(const char *source,std::string& key) {
    if(!source)return false;
    key=source;
    for(char& c:key){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}
    if(!archiveRoot.empty() && key.compare(0,archiveRoot.size(),archiveRoot)==0)key.erase(0,archiveRoot.size());
    if(key.empty()||key.size()>255||key.front()=='/'||key.back()=='/'||key.find(':')!=std::string::npos)return false;
    // HW1 constructs some valid resource names with repeated separators (for
    // example CursorBitmaps//cursor.LiF). Collapse those for lookups, while
    // mount's key==entry.path check still enforces canonical archive entries.
    key.erase(std::unique(key.begin(),key.end(),[](char a,char b){return a=='/'&&b=='/';}),key.end());
    size_t at=0;
    while(at<key.size()){
        size_t end=key.find('/',at);if(end==std::string::npos)end=key.size();
        const std::string part=key.substr(at,end-at);
        if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;
        for(unsigned char c:part)if(c<32||c>=127||strchr("<>\"|?*",c))return false;
        at=end+1;
    }
    return true;
}
extern "C" int openhwArchiveMounted(void){return archiveHeader!=nullptr;}
extern "C" void openhwArchiveSetLooseRoot(const char *root){
    archiveRoot=root?root:"";
    for(char& c:archiveRoot){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}
    if(!archiveRoot.empty()&&archiveRoot.back()!='/')archiveRoot+='/';
}
extern "C" unsigned int openhwArchiveCount(void){return archiveHeader?archiveHeader->count:0;}
extern "C" const char *openhwArchiveName(unsigned int i){return archiveHeader&&i<archiveHeader->count?archiveEntries[i].path:nullptr;}
extern "C" void openhwArchiveUnmount(void) {
    if(presentationView)UnmapViewOfFile(presentationView);presentationView=nullptr;
    if(presentationMapping)CloseHandle(presentationMapping);presentationMapping=nullptr;
    if(presentationFile!=INVALID_HANDLE_VALUE)CloseHandle(presentationFile);presentationFile=INVALID_HANDLE_VALUE;
    presentationHeader=nullptr;presentationEntries=nullptr;
    if(archiveView)UnmapViewOfFile(archiveView);archiveView=nullptr;
    if(archiveMapping)CloseHandle(archiveMapping);archiveMapping=nullptr;
    if(archiveFile!=INVALID_HANDLE_VALUE)CloseHandle(archiveFile);archiveFile=INVALID_HANDLE_VALUE;
    archiveHeader=nullptr;archiveEntries=nullptr;archiveRoot.clear();
}
extern "C" int openhwArchiveMountPresentation(const wchar_t *path,char *error,size_t capacity) {
    auto fail=[&](const char *reason) {
        if(error&&capacity)snprintf(error,capacity,"%s",reason);
        if(presentationView)UnmapViewOfFile(presentationView);presentationView=nullptr;
        if(presentationMapping)CloseHandle(presentationMapping);presentationMapping=nullptr;
        if(presentationFile!=INVALID_HANDLE_VALUE)CloseHandle(presentationFile);presentationFile=INVALID_HANDLE_VALUE;
        presentationHeader=nullptr;presentationEntries=nullptr;return 0;
    };
    if(!archiveHeader || presentationHeader) {
        if(error&&capacity)snprintf(error,capacity,"Base archive missing or presentation archive already mounted");return 0;
    }
    presentationFile=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,nullptr);
    if(presentationFile==INVALID_HANDLE_VALUE)return fail("Cannot open OpenHW-Presentation.obig");
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(presentationFile,&length)||length.QuadPart<sizeof(ObigHeader))return fail("Truncated presentation OBIG");
    presentationMapping=CreateFileMappingW(presentationFile,nullptr,PAGE_READONLY,0,0,nullptr);
    if(!presentationMapping)return fail("Cannot map presentation archive");
    presentationView=static_cast<const unsigned char*>(MapViewOfFile(presentationMapping,FILE_MAP_READ,0,0,0));
    if(!presentationView)return fail("Cannot map presentation view");
    const auto *header=reinterpret_cast<const ObigHeader*>(presentationView);
    const uint64_t total=(uint64_t)length.QuadPart;
    if(memcmp(header->magic,"OBIGPK01",8)||header->version!=1||!header->count||header->count>1000000||
       header->dataOffset!=sizeof(ObigHeader)+uint64_t(header->count)*sizeof(ObigEntry)||header->dataOffset>total)
        return fail("Invalid presentation OBIG index");
    const auto *entries=reinterpret_cast<const ObigEntry*>(presentationView+sizeof(ObigHeader));
    uint64_t previousEnd=header->dataOffset;
    for(uint32_t i=0;i<header->count;++i) {
        const auto& entry=entries[i];std::string key;
        if(!memchr(entry.path,0,256)||!normalize(entry.path,key)||key!=entry.path||entry.size>0x7fffffff||
           entry.offset<previousEnd||entry.offset>total||entry.size>total-entry.offset||
           (i&&strcmp(entries[i-1].path,entry.path)>=0))return fail("Invalid presentation OBIG entry");
        previousEnd=entry.offset+entry.size;
    }
    presentationHeader=header;presentationEntries=entries;
    if(error&&capacity)*error=0;
    HW_DIAG_FPRINTF(stderr,"[OBIG] Mounted presentation update: %u resources, %.1f MiB.\n",header->count,double(total)/1048576);
    return 1;
}
extern "C" int openhwArchiveMount(const wchar_t *path,char *error,size_t capacity){
    auto fail=[&](const char *reason){if(error&&capacity)snprintf(error,capacity,"%s",reason);
        if(archiveView)UnmapViewOfFile(archiveView);archiveView=nullptr;
        if(archiveMapping)CloseHandle(archiveMapping);archiveMapping=nullptr;
        if(archiveFile!=INVALID_HANDLE_VALUE)CloseHandle(archiveFile);archiveFile=INVALID_HANDLE_VALUE;
        archiveHeader=nullptr;archiveEntries=nullptr;return 0;};
    if(archiveHeader){if(error&&capacity)snprintf(error,capacity,"Archive already mounted");return 0;}
    archiveFile=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,nullptr);
    if(archiveFile==INVALID_HANDLE_VALUE)return fail("Cannot open OpenHW-HW1.obig");
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(archiveFile,&length)||length.QuadPart<sizeof(ObigHeader))return fail("Truncated OBIG header");
    archiveMapping=CreateFileMappingW(archiveFile,nullptr,PAGE_READONLY,0,0,nullptr);
    if(!archiveMapping)return fail("Cannot map OBIG archive");
    archiveView=static_cast<const unsigned char*>(MapViewOfFile(archiveMapping,FILE_MAP_READ,0,0,0));
    if(!archiveView)return fail("Cannot map OBIG archive view");
    const auto *header=reinterpret_cast<const ObigHeader*>(archiveView);
    const uint64_t total=static_cast<uint64_t>(length.QuadPart);
    if(memcmp(header->magic,"OBIGPK01",8)||header->version!=1||!header->count||header->count>1000000||
       header->dataOffset!=sizeof(ObigHeader)+uint64_t(header->count)*sizeof(ObigEntry)||header->dataOffset>total)
        return fail("Invalid OBIG index");
    archiveEntries=reinterpret_cast<const ObigEntry*>(archiveView+sizeof(ObigHeader));
    uint64_t previousEnd=header->dataOffset;
    for(uint32_t i=0;i<header->count;i++){
        const auto& e=archiveEntries[i];std::string key;
        if(!memchr(e.path,0,256)||!normalize(e.path,key)||key!=e.path||e.size>0x7fffffff||
           e.offset<previousEnd||e.offset>total||e.size>total-e.offset||
           (i&&strcmp(archiveEntries[i-1].path,e.path)>=0))return fail("Invalid OBIG entry or payload bounds");
        previousEnd=e.offset+e.size;
    }
    char root[32768]{};DWORD n=GetEnvironmentVariableA("HW_PORTABLE_ROOT",root,sizeof(root));
    if(n&&n<sizeof(root)){archiveRoot=root;for(char& c:archiveRoot){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}
        if(archiveRoot.back()!='/')archiveRoot+='/';}
    archiveHeader=header;if(error&&capacity)*error=0;
    HW_DIAG_FPRINTF(stderr,"[OBIG] Mounted %u resources, %.1f MiB.\n",header->count,double(total)/1048576);
    return 1;
}
extern "C" const void *openhwArchiveView(const char *relative,int *size){
    if(size)*size=-1;if(!archiveHeader)return nullptr;std::string key;if(!normalize(relative,key))return nullptr;
    if(presentationHeader) {
        uint32_t first=0,last=presentationHeader->count;
        while(first<last){uint32_t mid=first+(last-first)/2;int order=strcmp(key.c_str(),presentationEntries[mid].path);
            if(!order){if(size)*size=(int)presentationEntries[mid].size;return presentationView+presentationEntries[mid].offset;}
            if(order<0)last=mid;else first=mid+1;
        }
    }
    uint32_t first=0,last=archiveHeader->count;
    while(first<last){uint32_t mid=first+(last-first)/2;int order=strcmp(key.c_str(),archiveEntries[mid].path);
        if(order==0){if(size)*size=static_cast<int>(archiveEntries[mid].size);return archiveView+archiveEntries[mid].offset;}
        if(order<0)last=mid;else first=mid+1;}
    return nullptr;
}
extern "C" int openhwModOverride(const char *relative,char *path,size_t capacity){
    if(!path||!capacity||archiveRoot.empty())return 0;
    static bool initialized=false;static std::vector<std::string> mods;
    if(!initialized){initialized=true;std::ifstream input(archiveRoot+"mods/load-order.txt");std::string name;
        while(std::getline(input,name)&&mods.size()<128){if(!name.empty()&&name.back()=='\r')name.pop_back();
            if(name.empty()||name[0]=='#')continue;
            if(name.size()>64||!std::all_of(name.begin(),name.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';}))continue;
            mods.push_back(name);
        }
    }
    std::string key;if(!normalize(relative,key))return 0;
    // Gameplay content only: never redirect writes, DLLs or user settings.
    if(key.find('/')==std::string::npos)return 0;
    for(auto it=mods.rbegin();it!=mods.rend();++it){std::string candidate=archiveRoot+"mods/"+*it+"/"+key;
        DWORD attrs=GetFileAttributesA(candidate.c_str());
        if(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY)&&candidate.size()<capacity){
            memcpy(path,candidate.c_str(),candidate.size()+1);return 1;}}
    // Shipped definition updates live in Data, while the portable override
    // directory is the installation root. Resolve these before the OBIG
    // fallback so editor, simulation and visual loading see the same document.
    // Keep explicit root overrides and enabled mods higher priority.
    const std::string suffix="/rmnative/ship.owshipdef";
    if(key.size()>suffix.size()&&key.compare(key.size()-suffix.size(),suffix.size(),suffix)==0){
        const std::string direct=archiveRoot+key;
        DWORD attrs=GetFileAttributesA(direct.c_str());
        if(attrs==INVALID_FILE_ATTRIBUTES||attrs&FILE_ATTRIBUTE_DIRECTORY){
            const std::string candidate=archiveRoot+"data/"+key;
            attrs=GetFileAttributesA(candidate.c_str());
            if(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY)&&candidate.size()<capacity){
                memcpy(path,candidate.c_str(),candidate.size()+1);return 1;
            }
        }
    }
    return 0;
}
