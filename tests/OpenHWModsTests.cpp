#include "../src/Modern/OpenHWMods.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#undef NDEBUG
#include <cassert>
extern "C" int openhwModOverride(const char*,char*,size_t){return 0;}
namespace fs=std::filesystem;
static void put(const fs::path& path,const char* text){fs::create_directories(path.parent_path());std::ofstream(path)<<text;}
static std::string get(const fs::path& path){std::ifstream input(path);return std::string(std::istreambuf_iterator<char>(input),{});}
int main(){
 const auto root=fs::temp_directory_path()/("openhw-mod-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 fs::create_directories(root/"mods/First");fs::create_directories(root/"mods/Second");std::string r=root.string();char error[512]{};OpenHWModEntry rows[128]{};
 assert(openhwModsRead(r.c_str(),rows,128,error,sizeof(error))==2);
 assert(!rows[0].enabled&&!rows[1].enabled);
 std::swap(rows[0],rows[1]);rows[0].enabled=1;
 assert(openhwModsSave(r.c_str(),rows,2,error,sizeof(error)));
 assert(openhwModsRead(r.c_str(),rows,128,error,sizeof(error))==2);
 assert(std::string(rows[0].name)=="Second"&&rows[0].enabled&&!rows[1].enabled);
 put(root/"Data/R1/StandardFrigate/RMNative/hull.hod","model");
 put(root/"Data/Subsystems/Test/subsystem.owsubsystem","hull @/Subsystems/Shared/model.hod");
 put(root/"Data/Subsystems/Shared/model.hod","shared");
 const char* ship="OPENHW_SHIP_2\nStartShipHardPointConfig(NewShipType, \"Gun0\", \"Mount\", \"Weapon\", \"Innate\", \"Ship\", \"Test\");\n[Ship]\n[End]\n";
 const char* path="R1/StandardFrigate/RMNative/ship.owshipdef";
 assert(openhwModPublish(r.c_str(),"Custom",0,path,ship,strlen(ship),0,error,sizeof(error))==1);
 assert(get(root/"mods/Custom/Subsystems/Shared/model.hod")=="shared");
 assert(get(root/"mods/Custom"/path)==ship);
 put(root/"mods/Custom/unrelated/keep.txt","keep");
 put(root/"mods/Custom"/path,"old");
 assert(openhwModPublish(r.c_str(),"Custom",1,path,ship,strlen(ship),0,error,sizeof(error))==2);
 assert(get(root/"mods/Custom"/path)=="old");
 assert(openhwModPublish(r.c_str(),"Custom",1,path,ship,strlen(ship),1,error,sizeof(error))==1);
 assert(get(root/"mods/Custom/unrelated/keep.txt")=="keep");
 assert(openhwModPublish(r.c_str(),"../bad",0,path,ship,strlen(ship),0,error,sizeof(error))==0);
 assert(openhwModPublish(r.c_str(),"Custom",0,path,ship,strlen(ship),0,error,sizeof(error))==0);
 // Only this uniquely created test directory is removed.
 assert(fs::equivalent(root.parent_path(),fs::temp_directory_path())&&root.filename().string().rfind("openhw-mod-test-",0)==0);
 fs::remove_all(root);
}
