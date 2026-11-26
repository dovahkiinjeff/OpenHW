#include "../src/Game/OpenHWArchive.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#undef NDEBUG
#include <cassert>
namespace fs=std::filesystem;
static void put(const fs::path& p,const char* value){fs::create_directories(p.parent_path());std::ofstream(p)<<value;}
static std::string get(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    const auto root=fs::temp_directory_path()/("openhw-mp-mod-warning-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    char names[256],path[2048];
    const auto order=root/"mods/load-order.txt";
    put(order,"# disabled\n#disabled=Old\nMissing\nTest\n");
    put(root/"mods/Test/r1/interceptor.ship","mod gameplay");
    openhwArchiveSetLooseRoot(root.string().c_str());
    assert(openhwMultiplayerModsActive(names,sizeof(names))==1);
    assert(std::string(names)=="Test");
    const auto original=get(order);
    assert(openhwModOverride("r1/interceptor.ship",path,sizeof(path)));
    assert(get(path)=="mod gameplay");
    /* Warning checks are read-only, not an automatic mod toggle. */
    assert(openhwMultiplayerModsActive(nullptr,0)==1);
    assert(get(order)==original);
    put(order,"#disabled=Test\n");
    /* Already-loaded gameplay still requires a fresh process. */
    assert(openhwMultiplayerModsActive(names,sizeof(names))==1);
    assert(std::string(names).find("earlier in this session")!=std::string::npos);
    openhwArchiveUnmount();
    openhwArchiveSetLooseRoot(root.string().c_str());
    assert(!openhwMultiplayerModsActive(names,sizeof(names)));
    assert(!names[0]);
    assert(!openhwModOverride("r1/interceptor.ship",path,sizeof(path)));
    /* Official native subsystem aliases are not mods and never warn. */
    put(root/"Data/p2/p2advanceswarmer/rmnative/c1_m0.dds","official material");
    assert(openhwModOverride("subsystems/hw1_fightertank/m0.dds",path,sizeof(path)));
    assert(!openhwMultiplayerModsActive(names,sizeof(names)));
    openhwArchiveUnmount();fs::remove_all(root);
}
