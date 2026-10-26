#include "OpenHWArchive.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>
#pragma pack(push,1)
struct Header {char magic[8];uint32_t version,count;uint64_t offset;};
struct Entry {char name[256];uint64_t offset;uint32_t size,crc;};
#pragma pack(pop)
static void pack(const std::filesystem::path& file,const std::vector<std::pair<std::string,std::string>>& contents)
{
    Header h{{'O','B','I','G','P','K','0','1'},1,(uint32_t)contents.size(),sizeof(Header)+contents.size()*sizeof(Entry)};
    std::ofstream f(file,std::ios::binary);f.write((const char*)&h,sizeof(h));uint64_t offset=h.offset;
    for(const auto& item:contents){Entry e{};std::snprintf(e.name,sizeof(e.name),"%s",item.first.c_str());
        e.offset=offset;e.size=(uint32_t)item.second.size();offset+=e.size;f.write((const char*)&e,sizeof(e));}
    for(const auto& item:contents)f.write(item.second.data(),item.second.size());
}
static bool content(const char *name,const char *wanted)
{
    int size;const void *bytes=openhwArchiveView(name,&size);
    return bytes&&size==(int)std::strlen(wanted)&&!std::memcmp(bytes,wanted,size);
}
#define CHECK(x) do {if(!(x)){std::printf("failed line %d: %s\n",__LINE__,#x);openhwArchiveUnmount();return 1;}}while(0)
int main()
{
    auto root=std::filesystem::temp_directory_path()/
        ("OpenHW-obig-presentation-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    std::filesystem::create_directory(root);
    auto base=root/"base.obig",update=root/"update.obig",bad=root/"bad.obig";char error[256];
    pack(base,{{"art/a.bin","base"},{"art/b.bin","retained"}});
    pack(update,{{"art/a.bin","updated"},{"art/new.bin","added"}});
    pack(bad,{{"../invalid","bad"}});
    CHECK(!openhwArchiveMountPresentation(update.c_str(),error,sizeof(error)));
    CHECK(openhwArchiveMount(base.c_str(),error,sizeof(error)));
    CHECK(content("ART\\A.BIN","base"));
    CHECK(!openhwArchiveMountPresentation(bad.c_str(),error,sizeof(error)));
    CHECK(content("art/a.bin","base"));
    CHECK(openhwArchiveMountPresentation(update.c_str(),error,sizeof(error)));
    CHECK(content("art/a.bin","updated") && content("art/b.bin","retained") && content("art/new.bin","added"));
    CHECK(!openhwArchiveMountPresentation(update.c_str(),error,sizeof(error)));
    CHECK(content("art/a.bin","updated"));
    CHECK(openhwArchiveCount()==2 && !std::strcmp(openhwArchiveName(0),"art/a.bin"));
    int size;CHECK(!openhwArchiveView("art/../a.bin",&size));
    openhwArchiveUnmount();CHECK(!openhwArchiveMounted() && !openhwArchiveView("art/a.bin",&size));
    std::filesystem::remove(base);std::filesystem::remove(update);std::filesystem::remove(bad);std::filesystem::remove(root);
    std::puts("Presentation archive priority, isolation, bounds, failure recovery and teardown passed");return 0;
}
