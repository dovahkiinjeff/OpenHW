#include "OpenHWShipDocument.h"
#include <map>
namespace documentTests {
using Files=std::map<std::string,std::string>;
static int read(void* user,const char* path,const void** bytes,size_t* size) {
    auto& files=*static_cast<Files*>(user);auto found=files.find(path);
    if(found==files.end())return 0;
    *bytes=found->second.data();*size=found->second.size();return 1;
}
static void release(void*,const void*) {}
static std::string compile(Files& files,const std::string& ship,bool gameplay,bool success=true) {
    void* result=nullptr;size_t size=0;char error[256]={};
    const bool ok=openhwShipDocumentCompile(ship.data(),ship.size(),read,release,&files,gameplay,&result,&size,error,sizeof(error))!=0;
    if(ok!=success)throw std::runtime_error(std::string("document validation: ")+error);
    std::string output=result?std::string(static_cast<char*>(result),size):std::string();
    openhwShipDocumentFree(result);return output;
}
static void run() {
    Files files;
    files["@/Subsystems/Turret/subsystem.owsubsystem"]=
        "OPENHW_SUBSYSTEM_2\ntype Turret\nscale 1\nhull model.hod\nmad -\n"
        "surface 0 0 material\npresentation Hull events.owevents\ngun 0 Hull Azimuth Declination - Muzzle\n"
        "[Weapons]\nGUN 0\n{\nType GUN_NewGimble\nFireAnimation Fire\nWeaponFire @/RemasteredFX/WeaponFire/test.wf\nDamageLo 40\nFireTime 2.9\n}\n[End]\n";
    const std::string prefix="OPENHW_SHIP_2\ntype StandardFrigate\nscale 2\nhull hull.hod\nmad -\n";
    const std::string mount="StartShipHardPointConfig(NewShipType,\n\"GunA\", \"Hardpoint_A\", \"Weapon\", \"Innate\", \"Ship\", \"Turret\");\n";
    require(compile(files,prefix+mount+"[Ship]\n[End]\n",false).find("presentation GunA @/Subsystems/Turret/events.owevents")!=std::string::npos,
            "subsystem events not attached to their individual slot");
    const std::string body="[Ship]\nmaxhealth 12000\nGUN 0\n{\nType GUN_Fixed\nDamageLo 8\n}\n[End]\n";
    const auto ship=prefix+mount+"weaponSlot GunA 0 1\n"+body;
    auto rig=compile(files,ship,false),game=compile(files,ship,true);
    require(rig.find("attach GunA Hardpoint_A_Position 1 @/Subsystems/Turret/model.hod")!=std::string::npos,"subsystem HOD not composed");
    require(rig.find("gun 1 GunA Azimuth Declination - Muzzle")!=std::string::npos,"subsystem binding not mapped");
    require(rig.find("weaponEvent 1 Fire")!=std::string::npos,"subsystem presentation cue not mapped");
    require(game.find("FireAnimation")==std::string::npos,"presentation metadata passed to HW1 gameplay parser");
    require(game.find("WeaponFire")==std::string::npos,"weaponfire metadata passed to gameplay parser");
    require(rig.find("weaponFire 1 @/RemasteredFX/WeaponFire/test.wf")!=std::string::npos,"subsystem weaponfire binding lost");
    compile(files,prefix+"weaponEvent 0 Fire\n"+body,false,false);
    compile(files,prefix+"[Ship]\nFireAnimation Fire\n[End]\n",false,false);
    compile(files,prefix+"[Ship]\nWeaponFire test.wf\n[End]\n",false,false);
    {
        auto local=files;auto& text=local["@/Subsystems/Turret/subsystem.owsubsystem"];
        const auto at=text.find("@/RemasteredFX/WeaponFire/test.wf");text.replace(at,std::strlen("@/RemasteredFX/WeaponFire/test.wf"),"local.wf");
        require(compile(local,ship,false).find("weaponFire 1 @/Subsystems/Turret/local.wf")!=std::string::npos,"subsystem-local weaponfire path not rebased");
    }
    require(game.find("NUMBER_OF_GUNS 2")!=std::string::npos&&game.find("DamageLo 40")!=std::string::npos&&game.find("maxhealth 12000")!=std::string::npos,"embedded settings lost");
    const std::string duplicate="hardpoint CopyMount Root 1 2 3 0 90 0\nStartShipHardPointConfig(NewShipType, \"GunB\", \"CopyMount\", \"Weapon\", \"Innate\", \"Ship\", \"Turret\");\n";
    auto copied=prefix+mount+"weaponSlot GunA 0 1\n"+duplicate+body;
    {
        void* result=nullptr;size_t length=0;char error[256]{};
        require(openhwShipDocumentComplete(copied.data(),copied.size(),read,release,&files,&result,&length,error,sizeof(error))!=0,"editor completion failed");
        std::string complete((char*)result,length);openhwShipDocumentFree(result);
        require(complete.find("weaponSlot GunB 0 2")!=std::string::npos,"copied weapon assignment not persisted");
        require(compile(files,complete,false)==compile(files,copied,false),"completion changed bindings or presentation");
        require(openhwShipDocumentComplete(complete.data(),complete.size(),read,release,&files,&result,&length,error,sizeof(error))!=0,"second completion failed");
        require(complete==std::string((char*)result,length),"completion is not idempotent");openhwShipDocumentFree(result);
        auto visual=files;visual["@/Subsystems/Turret/subsystem.owsubsystem"]="OPENHW_SUBSYSTEM_1\ntype Turret\nscale 1\nhull model.hod\nmad -\n";
        require(!openhwShipDocumentComplete(copied.data(),copied.size(),read,release,&visual,&result,&length,error,sizeof(error)),"unarmed weapon accepted by editor");
    }
    require(compile(files,copied,true).find("NUMBER_OF_GUNS 3")!=std::string::npos,"duplicate failed to allocate weapon");
    require(compile(files,copied,false).find("mount GunB 1 2 3 0 90 0")!=std::string::npos,"duplicate mount lost");
    compile(files,prefix+mount+mount+body,false,false);
    compile(files,prefix+mount+"weaponSlot GunA 0 0\n"+body,true,false);
    require(compile(files,prefix+mount+"weaponSlot GunA 0 5\n"+body,false).find("gun 1 GunA")!=std::string::npos,"sparse weapon binding not compacted");
    require(compile(files,prefix+mount+"weaponSlot GunA 0 5\n"+body,false).find("weaponEvent 1 Fire")!=std::string::npos,"sparse presentation binding not compacted");
    require(compile(files,prefix+mount+"weaponSlot GunA 0 5\n"+body,false).find("weaponFire 1 @/RemasteredFX/WeaponFire/test.wf")!=std::string::npos,"sparse weaponfire binding not compacted");
    require(compile(files,prefix+mount+"weaponSlot GunA 0 5\n"+body,true).find("NUMBER_OF_GUNS 2")!=std::string::npos,"sparse weapons not compacted");
    compile(files,prefix+"weaponSlot Missing 0 1\n"+body,true,false);
    auto broken=files;broken.clear();compile(broken,ship,false,false);
    const auto legacy=std::string("OPENHW_SHIP_2\ntype CargoBarge\nvisual legacy\n")+body;
    require(openhwShipDocumentLegacyVisual(legacy.data(),legacy.size()),"legacy visual document not recognized");
    require(!openhwShipDocumentLegacyVisual(ship.data(),ship.size()),"native document misclassified");
    require(compile(files,legacy,true).find("maxhealth 12000")!=std::string::npos,"legacy visual ship lost gameplay");
    const auto spaced=prefix+"# Keep author notes\nStartShipHardPointConfig( NewShipType ,\n \"GunA\" , \"Hardpoint_A\", \"Weapon\", \"Innate\", \"Ship\", \"Turret\" );\n"+body;
    void* normalized=nullptr;size_t normalizedSize=0;char error[256]={};
    require(openhwShipDocumentNormalize(spaced.data(),spaced.size(),&normalized,&normalizedSize,error,sizeof(error))!=0,"multiline declaration normalization failed");
    const std::string canonical(static_cast<char*>(normalized),normalizedSize);openhwShipDocumentFree(normalized);
    require(canonical.find("StartShipHardPointConfig(NewShipType, \"GunA\", \"Hardpoint_A\"")!=std::string::npos,"declaration not canonical for editor");
    require(canonical.find("# Keep author notes")!=std::string::npos,"author comments lost");
    require(compile(files,canonical,true)==compile(files,spaced,true),"normalization changed gameplay");
    const auto flagged=prefix+"[Ship]\nBindGun Turret Gun5,Declination,Invert\nGUN 5\n{\nType GUN_NewGimble\n}\n[End]\n";
    require(compile(files,flagged,true).find("BindGun Turret Gun0,Declination,Invert")!=std::string::npos,"compaction lost BindGun flags");
    std::puts("OpenHW ship document composition, duplication and rejection tests passed");
}
}
