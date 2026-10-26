#include "RemasteredWeaponPresentation.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#define CHECK(x) do {if(!(x)){std::printf("failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
    char error[512];const char *source="-- source data, not executable Lua\nfirefx=\"flash\" bulletfx=\"trace\" hit_sfx=\"SOUND/HIT\"; hit_clamp={.5,1.25}\nhit_nlips=.0005 hit_sm=true deathtype=\"deathBeam\" scartype=\"iontrail\" nopentrate_sfx=\"ignored typo\" fire_clamp={1,2}";
    auto *weapon=hwRmWeaponPresentationRead(source,std::strlen(source),error,sizeof(error));CHECK(weapon);
    CHECK(!std::strcmp(hwRmWeaponPresentationAt(weapon,HW_RM_WEAPON_FIRE)->fx,"flash"));
    CHECK(!std::strcmp(hwRmWeaponPresentationAt(weapon,HW_RM_WEAPON_BULLET)->fx,"trace"));
    const auto *hit=hwRmWeaponPresentationAt(weapon,HW_RM_WEAPON_HIT);
    CHECK(hit->authoredOptions==7 && hit->sm && hit->clampMinimum==.5f && hit->clampMaximum==1.25f);
    CHECK(!std::strcmp(hit->sound,"SOUND/HIT"));
    CHECK(hwRmWeaponPresentationAt(weapon,HW_RM_WEAPON_FIRE)->authoredOptions==0);
    CHECK(!*hwRmWeaponPresentationAt(weapon,HW_RM_WEAPON_NOPENETRATE)->sound);
    CHECK(hwRmWeaponPresentationIgnoredCount(weapon)==2);
    CHECK(!std::strcmp(hwRmWeaponPresentationDeathType(weapon),"deathBeam"));
    CHECK(!std::strcmp(hwRmWeaponPresentationScarType(weapon),"iontrail"));
    CHECK(!hwRmWeaponPresentationAt(weapon,7));hwRmWeaponPresentationFree(weapon);
    for(const char *bad:{"hitfx=os.execute(\"bad\")","hit_sm=\"on\"","hit_clamp={1}","hit_clamp={1,2,3}",
        "hit_nlips=true","hitfx=1","hitfx=\"a\" hitfx=\"b\"","hit_nlips=1e999","hitfx=\"unfinished","hit_sm=trueSomething"})
        CHECK(!hwRmWeaponPresentationRead(bad,std::strlen(bad),error,sizeof(error)));
    CHECK(!hwRmWeaponPresentationRead(nullptr,0,error,sizeof(error)));
    unsigned count=0;
    if(argc>1)for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[1])){
        if(entry.path().extension()!=".wf")continue;
        std::ifstream file(entry.path(),std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(file)),{});
        weapon=hwRmWeaponPresentationRead(bytes.data(),bytes.size(),error,sizeof(error));
        if(!weapon)std::printf("%s: %s\n",entry.path().string().c_str(),error);
        CHECK(weapon);hwRmWeaponPresentationFree(weapon);++count;
    }
    std::printf("Native weaponfire presentation validation passed; %u real files checked\n",count);return 0;
}
