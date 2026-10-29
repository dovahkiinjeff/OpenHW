#include "RemasteredShipHod.h"
#include "NativeVisualScope.h"
#include "RemasteredShipMad.h"
#include "RemasteredSubsystem.h"
#include "RemasteredVisualAsset.h"
#include "RemasteredVisualPackage.h"
#include "../src/Game/NativeVisualRegistry.h"
#include "../src/Game/NativeVisualEvents.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <string>
#include <limits>
#include <sstream>
#include <cmath>
#include <chrono>
#include <cstdint>
#include "NativeShipHodFixture.h"
static Bytes presentationMadFixture(){
    Bytes body,version;le(version,0x104);chunk(body,"VERS",version);
    const char names[]="Fire\0Root\0Root_translateX\0";
    Bytes strings(names,names+sizeof(names));chunk(body,"STRI",strings);
    Bytes info;for(unsigned value:{30u,1u,1u,1u,1u})le(info,value);chunk(body,"INFO",info);
    Bytes curve;le(curve,10);le(curve,2);
    auto number=[&](double value){uint64_t bits;std::memcpy(&bits,&value,8);le(curve,unsigned(bits));le(curve,unsigned(bits>>32));};
    for(unsigned i=0;i<2;++i){number(i);number(i);for(float v:{1.f,0.f,1.f,0.f})f(curve,v);}
    le(curve,0);le(curve,0);chunk(body,"CURV",curve);
    Bytes mark;le(mark,0);for(float v:{0.f,1.f,0.f,1.f})f(mark,v);
    le(mark,1);le(mark,5);le(mark,1);le(mark,0);chunk(body,"MARK",mark);
    Bytes result;chunk(result,"MAD ",body,true);return result;
}
static void require(bool ok,const char* reason) {if(!ok) throw std::runtime_error(reason);}
#include "OpenHWShipDocumentTests.inc"
static void capture(void *user,const HwRmSubsystemDraw *draw) {
    static_cast<std::vector<HwRmSubsystemDraw>*>(user)->push_back(*draw);
}
static void capturePresentation(void *user,unsigned component,const HwRmPresentationEvent *,double) {
    static_cast<std::vector<unsigned>*>(user)->push_back(component);
}
struct RegistryCue {const void *ship;HwRmVisualInstance *instance;float scale;unsigned component;};
static void captureRegistryPresentation(void *user,const void *ship,HwRmVisualInstance *instance,
    float scale,unsigned component,const HwRmPresentationEvent *,double) {
    static_cast<std::vector<RegistryCue>*>(user)->push_back({ship,instance,scale,component});
}
struct RegistryConsumer {std::vector<RegistryCue> events;std::vector<std::string> weapons;};
static void consumeRegistryEvent(void *user,const void *ship,HwRmVisualInstance *instance,
    float scale,unsigned component,const HwRmPresentationEvent *event,double clock) {
    captureRegistryPresentation(&static_cast<RegistryConsumer*>(user)->events,
        ship,instance,scale,component,event,clock);
}
static void consumeRegistryWeapon(void *user,const void *,HwRmVisualInstance *,float,
    unsigned gun,unsigned slot,const HwRmWeaponPresentationSlot *value,double) {
    require(gun==0 && slot==HW_RM_WEAPON_FIRE,"gameplay dispatch used wrong weapon slot");
    static_cast<RegistryConsumer*>(user)->weapons.push_back(value->fx);
}
static int pose(void *user,unsigned component,unsigned joint,const char*,const char*,const float *rest,float *result) {
    if(component!=1 || joint!=0) return 0;
    std::memcpy(result,rest,16*sizeof(float));result[12]=*static_cast<float*>(user);return 1;
}
static HwRmHodScene *readScene(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);require(bool(file),"composition source missing");
    Bytes bytes((std::istreambuf_iterator<char>(file)),{});HwRmHodScene *scene=nullptr;char error[512];
    require(hwRmHodSceneRead(bytes.data(),bytes.size(),&scene,error,sizeof(error))!=0,error);return scene;
}
static Bytes readBytes(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);require(bool(file),"native asset source missing");
    return Bytes((std::istreambuf_iterator<char>(file)),{});
}
static void auditGunContracts(const std::filesystem::path& root,const char *manifest,bool raiders=false,bool taiidan=false) {
    std::ifstream file(manifest);require(bool(file),"native gun audit manifest missing");
    HwRmVisualAsset *asset=nullptr;std::string line,type;char error[512];unsigned ships=0,gunTotal=0;
    while(std::getline(file,line)) {
        std::istringstream row(line);std::vector<std::string> fields;std::string field;
        while(std::getline(row,field,'\t')) fields.push_back(field);
        if(fields.size()==3 && fields[0]=="H") {
            require(!asset,"nested native asset audit");type=fields[1];
            auto bytes=readBytes(root/fields[2]);
            asset=hwRmVisualAssetCreate(bytes.data(),bytes.size(),nullptr,0,error,sizeof(error));require(asset!=nullptr,error);
        } else if(fields.size()==5 && fields[0]=="A") {
            require(asset!=nullptr,"native attachment without hull");auto bytes=readBytes(root/fields[4]);
            require(hwRmVisualAssetAttach(asset,fields[4].c_str(),fields[1].c_str(),fields[2].c_str(),
                unsigned(std::stoul(fields[3])),bytes.data(),bytes.size(),error,sizeof(error))!=0,error);
        } else if(fields.size()==1 && fields[0]=="E") {
            require(asset!=nullptr,"native gun terminator without asset");
            if(!meshNativeVisualBindClassicGuns(asset,type.c_str(),error,sizeof(error))) {
                std::fprintf(stderr,"R1/%s gun mapping: %s\n",type.c_str(),error);throw std::runtime_error("native gun contract failed");
            }
            auto *a=hwRmVisualInstanceCreate(asset),*b=hwRmVisualInstanceCreate(asset);require(a && b,"native gun instances missing");
            for(unsigned i=0;i<hwRmVisualInstanceGunCount(a);++i) {
                if(taiidan) for(unsigned j=0;j<hwRmVisualInstanceGunCount(a);++j) {
                    unsigned index=0;require(hwRmVisualInstanceGunIndex(a,j,&index)!=0,"gun reset index");
                    require(hwRmVisualInstanceSetGun(a,index,0,0,0,error,sizeof(error))!=0,error);
                }
                unsigned gun=99;require(hwRmVisualInstanceGunIndex(a,i,&gun)!=0,"native gun index absent");
                float rest[16],moved[16],other[16];
                require(hwRmVisualInstanceGunMuzzle(a,gun,2.5f,rest,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceSetGun(a,gun,.2f,-.1f,1,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceGunMuzzle(a,gun,2.5f,moved,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceGunMuzzle(b,gun,2.5f,other,error,sizeof(error))!=0,error);
                bool fixed=(type=="AttackBomber" || type=="HeavyInterceptor" || type=="LightInterceptor" ||
                    type=="CloakedFighter" || type=="IonCannonFrigate" || type=="MinelayerCorvette" ||
                    type=="Carrier" || type=="MissileDestroyer" ||
                    (raiders && type!="P1StandardCorvette"));
                if(taiidan) fixed=(type=="Taiidan_AttackBomber" || type=="Taiidan_HeavyInterceptor" ||
                    type=="Taiidan_LightInterceptor" || type=="Taiidan_DefenseFighter" ||
                    type=="Taiidan_IonCannonFrigate" || type=="Taiidan_MinelayerCorvette" ||
                    type=="Taiidan_Carrier" || type=="Taiidan_MissileDestroyer" ||
                    type=="Taiidan_Mothership" || type=="Taiidan_RepairCorvette" ||
                    (type=="Taiidan_StandardDestroyer" && gun>=4) ||
                    (type=="Taiidan_StandardFrigate" && (gun==0 || gun==5)) ||
                    (type=="Taiidan_HeavyCruiser" && (gun==6 || gun==8)));
                require((std::memcmp(rest,moved,sizeof(rest))==0)==fixed,
                    "native fixed/moving weapon pose contract changed");
                require(std::memcmp(rest,other,sizeof(rest))==0,"gun controls leaked across native ships");
                if(!fixed) {
                    const float directions[][3]={{0,0,1},{.3f,.4f,.8660254f},{-.3f,-.4f,.8660254f},
                        {0,0,-1},{1,0,0},{0,1,0},{0,-1,0},{-1,0,0}};
                    for(const auto& direction:directions) {
                        float expected[3];std::memcpy(expected,direction,sizeof(expected));
                        if(type=="P1StandardCorvette") {
                            // Both mirrored side pods turn in the plane normal
                            // to source Direction (RM Y = native local X).
                            float normalDot=0,projectionLength=0;
                            for(unsigned k=0;k<3;++k) normalDot+=rest[k]*expected[k];
                            for(unsigned k=0;k<3;++k) {
                                expected[k]-=normalDot*rest[k];projectionLength+=expected[k]*expected[k];
                            }
                            for(unsigned k=0;k<3;++k) expected[k]=projectionLength<1e-12f?
                                rest[8+k]:expected[k]/std::sqrt(projectionLength);
                        }
                        std::vector<HwRmSubsystemDraw> beforeAim,afterAim;
                        require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,2.5f,nullptr,nullptr,capture,&beforeAim,error,sizeof(error))!=0,error);
                        HwRmGunAim aim{};aim.gun=gun;
                        std::memcpy(aim.heading,direction,sizeof(direction));aim.recoil=.5f;
                        if(!hwRmVisualInstanceAimGuns(a,&aim,1,error,sizeof(error))) {
                            std::fprintf(stderr,"R1/%s Gun%u aim: %s\n",type.c_str(),gun,error);
                            throw std::runtime_error("native direction aim failed");
                        }
                        float aimed[16];
                        require(hwRmVisualInstanceGunMuzzle(a,gun,2.5f,aimed,error,sizeof(error))!=0,error);
                        float alignment=0;
                        for(unsigned axis=0;axis<3;++axis) alignment+=aimed[8+axis]*expected[axis];
                        require(alignment>.9999f,"native barrel disagrees with gameplay firing direction");
                        if(type=="P1StandardCorvette") for(unsigned k=0;k<3;++k)
                            require(std::abs(aimed[k]-rest[k])<1e-5f,"single-axis turret tipped its hinge normal");
                        require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,2.5f,nullptr,nullptr,capture,&afterAim,error,sizeof(error))!=0,error);
                        require(beforeAim.size()==afterAim.size(),"aim changed visible topology");
                        bool movedGeometry=false,alignedGeometry=false;
                        for(size_t di=0;di<afterAim.size();++di) {
                            if(std::memcmp(beforeAim[di].matrix,afterAim[di].matrix,sizeof(afterAim[di].matrix))==0) continue;
                            movedGeometry=true;float dot=0;
                            for(unsigned axis=0;axis<3;++axis) dot+=afterAim[di].matrix[8+axis]*expected[axis]/2.5f;
                            alignedGeometry|=dot>.9999f;
                        }
                        if(movedGeometry) require(alignedGeometry,"visible native barrel moves but points away from shot");
                        require(hwRmVisualInstanceGunMuzzle(b,gun,2.5f,other,error,sizeof(error))!=0,error);
                        require(std::memcmp(rest,other,sizeof(rest))==0,"native aim leaked across ships");
                    }
                    HwRmGunAim invalid{};invalid.gun=gun;
                    float prior[16],afterInvalid[16];
                    require(hwRmVisualInstanceGunMuzzle(a,gun,1,prior,error,sizeof(error))!=0,error);
                    require(!hwRmVisualInstanceAimGuns(a,&invalid,1,error,sizeof(error)),"zero aim accepted");
                    require(hwRmVisualInstanceGunMuzzle(a,gun,1,afterInvalid,error,sizeof(error))!=0,error);
                    require(std::memcmp(prior,afterInvalid,sizeof(prior))==0,"invalid aim mutated pose");
                }
                for(float v:moved) require(std::isfinite(v),"nonfinite native muzzle pose");
                // A moving marker alone does not prove that visible geometry
                // follows it. Check each axis separately against actual draws.
                // Cruiser slave barrels are deliberately one rigid paired mesh;
                // HW1 slave controls mirror their master's angles.
                if(!fixed && !(type=="HeavyCruiser" && (gun==1 || gun==3))) {
                    for(unsigned axis=0;axis<2;++axis) {
                        require(hwRmVisualInstanceSetGun(a,gun,0,0,0,error,sizeof(error))!=0,error);
                        std::vector<HwRmSubsystemDraw> before,after;
                        require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,2.5f,nullptr,nullptr,capture,&before,error,sizeof(error))!=0,error);
                        require(hwRmVisualInstanceSetGun(a,gun,axis==0?.2f:0.f,axis==1?.2f:0.f,0,error,sizeof(error))!=0,error);
                        require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,2.5f,nullptr,nullptr,capture,&after,error,sizeof(error))!=0,error);
                        require(before.size()==after.size(),"gun aiming changed surface topology");bool visibleMoved=false;
                        for(size_t di=0;di<before.size();++di) visibleMoved|=std::memcmp(before[di].matrix,after[di].matrix,sizeof(before[di].matrix))!=0;
                        if(type=="P1StandardCorvette" && axis==1) {
                            require(!visibleMoved,"one-axis side turret acquired second articulation");
                        } else {
                            if(!visibleMoved) std::fprintf(stderr,"R1/%s Gun%u axis %u moves only an empty marker\n",type.c_str(),gun,axis);
                            require(visibleMoved,"native weapon axis failed to move visible barrel");
                        }
                    }
                }
                ++gunTotal;
            }
            hwRmVisualInstanceFree(a);hwRmVisualInstanceFree(b);hwRmVisualAssetFree(asset);asset=nullptr;++ships;
        } else throw std::runtime_error("malformed native gun audit manifest");
    }
    require(!asset && ships==(taiidan?29u:raiders?5u:32u) && gunTotal==(taiidan?62u:raiders?18u:55u),"incomplete audited native gun contracts");
    std::printf("Explicit native gun mappings: %u controls across %u identities validated\n",gunTotal,ships);
}
struct PackageFixture {
    Bytes bytes=sample(),events={'O','W','E','V','E','N','T','2',0,0,0,0,0,0,0,0,1,0,0,0,0};
    unsigned reads=0,releases=0;
};
static int packageRead(void *user,const char *path,const void **bytes,size_t *size) {
    auto& fixture=*static_cast<PackageFixture*>(user);
    if(!std::strcmp(path,"events.owevents")) {
        *bytes=fixture.events.data();*size=fixture.events.size();++fixture.reads;return 1;
    }
    if(std::strcmp(path,"hull.hod") && std::strcmp(path,"gun.hod")) return 0;
    *bytes=fixture.bytes.data();*size=fixture.bytes.size();++fixture.reads;return 1;
}
static void packageRelease(void *user,const void*) {++static_cast<PackageFixture*>(user)->releases;}
static int packageConfigure(void*,HwRmVisualAsset *asset,const char *type,char *error,size_t size) {
    return meshNativeVisualBindClassicEvents(asset,type,error,size) && meshNativeVisualBindClassicGuns(asset,type,error,size);
}
static int packageFileRead(void *user,const char *relative,const void **bytes,size_t *size) {
    auto *root=static_cast<const std::filesystem::path*>(user);std::ifstream file(*root/relative,std::ios::binary|std::ios::ate);
    *bytes=nullptr;*size=0;if(!file) return 0;const auto length=file.tellg();
    if(length<=0 || length>64*1024*1024) return 0;
    auto *data=std::malloc(size_t(length));if(!data) return 0;file.seekg(0);
    if(!file.read(static_cast<char*>(data),length)) {std::free(data);return 0;}
    *bytes=data;*size=size_t(length);return 1;
}
static void packageFileRelease(void*,const void *bytes) {std::free(const_cast<void*>(bytes));}
/* Parser fixture has deliberately minimal fake geometry, not the real gun
   contract. Actual package audits above use packageConfigure unchanged. */
static int packageFixtureConfigure(void*,HwRmVisualAsset*,const char*,char*,size_t) {return 1;}
static void auditRaiderDerelictPackages(const std::filesystem::path& root) {
    unsigned ships=0,derelicts=0;char error[512];
    for(const char *race:{"P1","Derelicts"}) for(const auto& folder:std::filesystem::directory_iterator(root/race)) {
        auto packageRoot=folder.path()/"RMNative";
        const auto name=folder.path().filename().string();
        const bool derelict=std::strcmp(race,"Derelicts")==0;
        const auto identity=derelict?"Derelict_"+name:name;
        auto bytes=readBytes(packageRoot/"visual.rmnative");
        auto *package=hwRmVisualPackageRead(identity.c_str(),bytes.data(),bytes.size(),packageFileRead,
            packageFileRelease,packageConfigure,&packageRoot,error,sizeof(error));
        if(!package) std::fprintf(stderr,"%s/%s: %s\n",race,name.c_str(),error);
        require(package!=nullptr,"requested native package rejected");
        auto *asset=hwRmVisualPackageTakeAsset(package);
        auto *instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"requested visual instance");
        const unsigned count=hwRmVisualInstanceGunCount(instance);
        const unsigned expected=derelict?0:name=="P1Mothership"?12:
            (name=="P1StandardCorvette" || name=="P1MissileCorvette")?2:1;
        require(count==expected,"native guns differ from original gameplay gun count");
        std::vector<unsigned> referenceMaterials;
        for(unsigned lod=0;lod<5;++lod) {
            std::vector<HwRmSubsystemDraw> draws;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),lod,1,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
            require(!draws.empty(),"requested hull disappears at a LOD");
            if(name=="P1StandardCorvette" || name=="P1MissileCorvette" || name=="P1Fighter") {
                std::vector<unsigned> materials;
                for(const auto& draw:draws) materials.push_back(draw.part->material);
                if(!lod) referenceMaterials=materials;
                require(materials==referenceMaterials,"Raider LOD changed surface material layout");
                for(const auto& draw:draws)
                    require(draw.mesh->lod==0,"Raider selected reduced source geometry");
            }
            for(const auto& d:draws) {
                require(std::strcmp(d.material->shader,"innateSS") && std::strcmp(d.material->shader,"innatess"),"carrier effect proxy became solid hull");
                for(float value:d.matrix) require(std::isfinite(value),"invalid requested hull transform");
            }
        }
        if(name=="P1IonArrayFrigate") {
            for(const char *event:{"Array_Open","Array_Close","Array_Open_DMG","Array_Close_DMG"}) {
                std::vector<HwRmSubsystemDraw> first,last,reversed;
                for(double phase:{0.,.25,.5,.75,1.,.5,0.}) {
                    require(hwRmVisualInstanceSampleEvent(instance,event,phase,error,sizeof(error))!=0,error);
                    std::vector<HwRmSubsystemDraw> draws;
                    require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),0,1,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
                    if(first.empty()) first=draws;
                    if(phase==1) last=draws;
                    reversed=draws;
                }
                require(first.size()==last.size() && first.size()==reversed.size(),"array animation lost meshes");
                bool moved=false;
                for(unsigned i=0;i<first.size();++i) {
                    moved |= std::memcmp(first[i].matrix,last[i].matrix,sizeof(first[i].matrix))!=0;
                    require(!std::memcmp(first[i].matrix,reversed[i].matrix,sizeof(first[i].matrix)),"array animation reversal failed");
                }
                require(moved,"array MAD failed to move visible panels");
            }
        }
        std::istringstream manifest(std::string(bytes.begin(),bytes.end()));std::string row;
        while(std::getline(manifest,row)) {
            std::istringstream fields(row);std::string directive,semantic,slot,joint;
            fields>>directive>>semantic>>slot>>joint;
            if(directive=="marker") {
                float transform[16];require(hwRmVisualInstanceMarker(instance,semantic.c_str(),1,transform,error,sizeof(error))!=0,error);
            }
        }
        hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);hwRmVisualPackageFree(package);
        if(derelict) ++derelicts;else ++ships;
    }
    require(ships==5 && derelicts==41,"incomplete requested native package set");
    std::printf("Native Raider/derelict packages: %u ships, %u derelicts; all five LODs validated\n",ships,derelicts);
}
static void auditPackages(const std::filesystem::path& root) {
    unsigned ready=0,blocked=0;char error[512];
    for(const auto& folder:std::filesystem::directory_iterator(root/"R1")) {
        if(!folder.is_directory()) continue;
        auto packageRoot=folder.path()/"RMNative";auto bytes=readBytes(packageRoot/"visual.rmnative");
        const auto type=folder.path().filename().string();
        auto *package=hwRmVisualPackageRead(type.c_str(),bytes.data(),bytes.size(),packageFileRead,
            packageFileRelease,packageConfigure,&packageRoot,error,sizeof(error));
        {
            if(!package) std::fprintf(stderr,"R1/%s native package rejected: %s\n",type.c_str(),error);
            require(package!=nullptr,"staged R1 package failed native runtime validation");
            auto *asset=hwRmVisualPackageTakeAsset(package);require(asset!=nullptr,"native package lost ownership");
            auto *instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"actual native package instance failed");
            const auto scale=hwRmVisualPackageScale(package);
            hwRmVisualPackageFree(package);hwRmVisualAssetFree(asset);
            std::istringstream manifest(std::string(bytes.begin(),bytes.end()));std::string row;
            while(std::getline(manifest,row)) {
                std::istringstream fields(row);std::string directive,semantic,slot,joint;
                fields>>directive>>semantic>>slot>>joint;
                if(directive=="marker") {
                    float transform[16];
                    require(hwRmVisualInstanceMarker(instance,semantic.c_str(),scale,transform,error,sizeof(error))!=0,error);
                    for(float value:transform) require(std::isfinite(value),"nonfinite actual native engine/door marker");
                }
            }
            if(type=="Mothership") {
                float closed[16],opened[16],reversed[16];
                require(hwRmVisualInstanceSampleEvent(instance,"HangarDoor_OPEN",0,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceMarker(instance,"Hangardoor",scale,closed,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceSampleEvent(instance,"HangarDoor_OPEN",1,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceMarker(instance,"Hangardoor",scale,opened,error,sizeof(error))!=0,error);
                require(std::memcmp(closed,opened,sizeof(closed))!=0,"native docking door query ignored animation");
                require(hwRmVisualInstanceSampleEvent(instance,"HangarDoor_OPEN",0,error,sizeof(error))!=0,error);
                require(hwRmVisualInstanceMarker(instance,"Hangardoor",scale,reversed,error,sizeof(error))!=0,error);
                require(std::memcmp(closed,reversed,sizeof(closed))==0,"native docking door query failed reversal");
            }
            if(type=="HeavyCorvette") {
                for(double phase: {0.,.5,1.}) {
                    require(hwRmVisualInstanceSampleEvent(instance,"Make_Ambassador",phase,error,sizeof(error))!=0,error);
                    HwRmGunAim guns[]={{0,{.3f,.4f,.8660254f},.5f},{1,{-.3f,-.4f,.8660254f},.25f}};
                    require(hwRmVisualInstanceAimGuns(instance,guns,2,error,sizeof(error))!=0,error);
                    for(const auto& gun:guns) {
                        float transform[16];
                        require(hwRmVisualInstanceGunMuzzle(instance,gun.gun,scale,transform,error,sizeof(error))!=0,error);
                        float alignment=0;
                        for(unsigned axis=0;axis<3;++axis) alignment+=transform[8+axis]*gun.heading[axis];
                        require(alignment>.9999f,"animated heavy-corvette mount broke firing direction");
                    }
                }
            }
            std::vector<HwRmSubsystemDraw> draws;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),0,1,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
            require(!draws.empty(),"actual native package lost all hull geometry");
            hwRmVisualInstanceFree(instance);++ready;
        }
    }
    require(ready==32 && blocked==0,"incomplete native R1 package validation");
    std::printf("Native R1 packages: %u loaded directly, %u explicit classic fallbacks\n",ready,blocked);
}
static void auditTaiidanPackages(const std::filesystem::path& root) {
    unsigned ships=0,guns=0,events=0;char error[512];
    for(const auto& folder:std::filesystem::directory_iterator(root/"R2")) {
        auto path=folder.path()/"RMNative";auto bytes=readBytes(path/"visual.rmnative");
        const auto name=folder.path().filename().string(),identity="Taiidan_"+name;
        auto *package=hwRmVisualPackageRead(identity.c_str(),bytes.data(),bytes.size(),packageFileRead,
            packageFileRelease,packageConfigure,&path,error,sizeof(error));
        if(!package) std::fprintf(stderr,"R2/%s: %s\n",name.c_str(),error);
        require(package!=nullptr,"Taiidan runtime package rejected");
        require(!hwRmVisualPackageRead(name.c_str(),bytes.data(),bytes.size(),packageFileRead,
            packageFileRelease,packageConfigure,&path,error,sizeof(error)),"R2 header crossed into R1");
        auto *asset=hwRmVisualPackageTakeAsset(package);
        auto *a=hwRmVisualInstanceCreate(asset),*b=hwRmVisualInstanceCreate(asset);
        require(a && b,"Taiidan instances");guns+=hwRmVisualInstanceGunCount(a);
        if(name=="StandardFrigate") {
            auto *subsystem=hwRmVisualInstanceSubsystem(a);
            for(unsigned mount=1;mount<=4;++mount) {
                const std::string prefix="Hardpoint_Gun"+std::to_string(mount);
                float socket[16],direction[16],pivot[16];
                require(hwRmSubsystemNeutralJointTransform(subsystem,0,(prefix+"_Position").c_str(),socket,error,sizeof(error))!=0,error);
                require(hwRmSubsystemNeutralJointTransform(subsystem,0,(prefix+"_Direction").c_str(),direction,error,sizeof(error))!=0,error);
                require(hwRmSubsystemNeutralJointTransform(subsystem,mount,"Weapon_Gun1_Position",pivot,error,sizeof(error))!=0,error);
                float dot=0,length=0;
                for(unsigned axis=0;axis<3;++axis) {
                    const float outward=direction[12+axis]-socket[12+axis];
                    dot+=outward*pivot[axis];length+=outward*outward;
                }
                require(dot/std::sqrt(length)>.99f,"assault cannon mounted upside down relative to source Direction");
            }
        }
        const auto scale=hwRmVisualPackageScale(package);
        for(unsigned ordinal=0;ordinal<hwRmVisualInstanceGunCount(a);++ordinal) {
            unsigned index=0;float muzzle[16];
            require(hwRmVisualInstanceGunIndex(a,ordinal,&index)!=0,"Taiidan gun ordinal");
            require(hwRmVisualInstanceGunMuzzle(a,index,scale,muzzle,error,sizeof(error))!=0,error);
            std::printf("TaiidanMuzzle %s %u %.6g %.6g %.6g\n",name.c_str(),index,muzzle[12],muzzle[13],muzzle[14]);
        }
        for(unsigned lod=0;lod<5;++lod) {
            std::vector<HwRmSubsystemDraw> draws;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),lod,scale,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
            require(!draws.empty(),"Taiidan empty LOD");
            for(const auto& draw:draws) {
                require(hwRmVisualPackageMaterial(package,draw.component,draw.part->material)!=nullptr,"Taiidan missing material");
                for(float v:draw.matrix) require(std::isfinite(v),"Taiidan nonfinite transform");
            }
        }
        std::vector<const char*> clips;
        if(name=="HeavyCorvette") clips={"Wings_Open","Wings_Close","Wings_Open_DMG","Wings_Close_DMG"};
        if(name=="HeavyInterceptor") clips={"Wings_Open","Wings_Close"};
        if(name=="LightCorvette") clips={"Wings_OPEN","Wings_CLOSE","Wings_DMG_Open","Wings_DMG_Close"};
        if(name=="SalCapCorvette") clips={"SwingCockpit_Out","SwingCockpit_IN"};
        if(name=="SensorArray" || name=="TargetDrone") clips={"open","close"};
        for(const char *clip:clips) {
            std::vector<HwRmSubsystemDraw> start,finish,other,reversed;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(b),0,scale,nullptr,nullptr,capture,&other,error,sizeof(error))!=0,error);
            for(double phase:{0.,.25,.5,.75,1.,.5,0.}) {
                require(hwRmVisualInstanceSampleEvent(a,clip,phase,error,sizeof(error))!=0,error);
                std::vector<HwRmSubsystemDraw> draws;
                require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,scale,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
                if(start.empty()) start=draws;if(phase==1) finish=draws;reversed=draws;
            }
            require(start.size()==finish.size() && start.size()==reversed.size(),"Taiidan animation changed topology");
            bool moved=false;for(unsigned i=0;i<start.size();++i) {
                moved|=std::memcmp(start[i].matrix,finish[i].matrix,sizeof(start[i].matrix))!=0;
                require(!std::memcmp(start[i].matrix,reversed[i].matrix,sizeof(start[i].matrix)),"Taiidan animation reversal");
            }
            require(moved,"Taiidan animated only empty markers");
            std::vector<HwRmSubsystemDraw> after;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(b),0,scale,nullptr,nullptr,capture,&after,error,sizeof(error))!=0,error);
            for(unsigned i=0;i<other.size();++i) require(!std::memcmp(other[i].matrix,after[i].matrix,sizeof(other[i].matrix)),"Taiidan animation leaked");
            ++events;
        }
        if(!clips.empty()) {
            auto *detached=hwRmVisualInstanceSnapshot(b);
            require(detached!=nullptr,"source clip snapshot failed");
            std::vector<HwRmSubsystemDraw> before,after;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(detached),0,scale,nullptr,nullptr,capture,&before,error,sizeof(error))!=0,error);
            require(hwRmVisualInstanceSampleSourceClip(detached,clips.front(),1,error,sizeof(error))!=0,error);
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(detached),0,scale,nullptr,nullptr,capture,&after,error,sizeof(error))!=0,error);
            bool changed=false;for(unsigned i=0;i<before.size();++i)
                changed|=std::memcmp(before[i].matrix,after[i].matrix,sizeof(before[i].matrix))!=0;
            require(changed,"source clip left detached visual pose static");
            require(!hwRmVisualInstanceSampleSourceClip(detached,"Nonexistent_Death",.5,error,sizeof(error)),"missing source clip guessed an animation");
            hwRmVisualInstanceFree(detached);
        }
        if(name=="HeavyCorvette") {
            const char *excluded="Make_Ambassador";
            // Frozen definitions reject additions as well; verify the asset's
            // explicit source selection separately before freezing it.
            auto hull=readBytes(path/"hull.hod"),mad=readBytes(path/"animation.mad");
            require(!hwRmVisualAssetCreate(hull.data(),hull.size(),mad.data(),mad.size(),error,sizeof(error)),"old integrated rig unexpectedly accepted");
            const char *selected[]={"Wings_Open","Wings_Close","Wings_Open_DMG","Wings_Close_DMG"};
            auto *selection=hwRmVisualAssetCreateWithAnimationClips(hull.data(),hull.size(),mad.data(),mad.size(),nullptr,0,selected,4,error,sizeof(error));
            require(selection!=nullptr,error);
            require(!hwRmVisualAssetBindAnimation(selection,excluded,&excluded,1,error,sizeof(error)),"excluded conversion clip activated");
            hwRmVisualAssetFree(selection);
        }
        std::istringstream manifest(std::string(bytes.begin(),bytes.end()));std::string line;
        while(std::getline(manifest,line)) {
            std::istringstream fields(line);std::string directive,semantic;fields>>directive>>semantic;
            if(directive=="marker") {float matrix[16];require(hwRmVisualInstanceMarker(a,semantic.c_str(),scale,matrix,error,sizeof(error))!=0,error);}
        }
        hwRmVisualInstanceFree(a);hwRmVisualInstanceFree(b);hwRmVisualAssetFree(asset);hwRmVisualPackageFree(package);++ships;
    }
    require(ships==29 && guns==62 && events==16,"incomplete Taiidan fleet contracts");
    std::printf("Taiidan packages: %u hulls, %u guns, %u animation events, LOD0-4 passed\n",ships,guns,events);
}
static void packageTests() {
    char error[512];PackageFixture fixture;
    const std::string header="HW_RM_R1_VISUAL_1\ntype HeavyInterceptor\nscale 2.5\nhull hull.hod\nmad -\n";
    const std::string surfaces="surface 0 0 Hull\nsurface 0 1 Engine\nsurface 0 2 Badge\n";
    const std::string definition="OPENHW_SHIP_1\ntype HeavyInterceptor\nscale 2.5\nhull hull.hod\nmad -\ngameplay gameplay.shp\n"+surfaces;
    char gameplay[256];
    require(hwRmShipDefinitionGameplay(definition.data(),definition.size(),"HeavyInterceptor",gameplay,sizeof(gameplay)) &&
        !strcmp(gameplay,"gameplay.shp"),"OpenHW gameplay member not resolved");
    require(!hwRmShipDefinitionGameplay(definition.data(),definition.size(),"Carrier",gameplay,sizeof(gameplay)),"cross-identity gameplay accepted");
    const auto unsafe=definition+"gameplay ../outside.shp\n";
    require(!hwRmShipDefinitionGameplay(unsafe.data(),unsafe.size(),"HeavyInterceptor",gameplay,sizeof(gameplay)),"duplicate/traversing gameplay accepted");
    auto *explicitPackage=hwRmVisualPackageRead("HeavyInterceptor",definition.data(),definition.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(explicitPackage!=nullptr,error);hwRmVisualPackageFree(explicitPackage);
    {
        const auto withEvents=definition+"presentation Hull events.owevents\n";
        auto *p=hwRmVisualPackageRead("HeavyInterceptor",withEvents.data(),withEvents.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
        require(p!=nullptr,error);auto *a=hwRmVisualPackageTakeAsset(p);
        require(hwRmVisualAssetPresentation(a,0)!=nullptr,"package event source not retained");
        hwRmVisualAssetFree(a);hwRmVisualPackageFree(p);
        const auto missing=definition+"presentation Missing events.owevents\n";
        require(!hwRmVisualPackageRead("HeavyInterceptor",missing.data(),missing.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"package accepted events for a missing slot");
    }
    for(const auto& bad:{definition+"gun 0 Hull - - - MissingJoint\n",definition+"gun 0 Hull - - - Root\ngun 0 Hull - - - Root\n"})
        require(!hwRmVisualPackageRead("HeavyInterceptor",bad.data(),bad.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"invalid explicit native gun accepted");
    const auto valid=header+surfaces+"marker Eng0 Hull Root\n";
    const std::string scaffold="HW_RM_SCAFFOLD_VISUAL_1\ntype Scaffold\nscale 2.5\nhull hull.hod\nmad -\n"+surfaces;
    auto *rigid=hwRmVisualPackageRead("Scaffold",scaffold.data(),scaffold.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(rigid!=nullptr,error);hwRmVisualPackageFree(rigid);
    for(const auto& bad: {scaffold+"marker Eng0 Hull Root\n",scaffold+"attach Gun Root 1 gun.hod\n"})
        require(!hwRmVisualPackageRead("Scaffold",bad.data(),bad.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"scaffold accepted ship controls/dependencies");
    require(!hwRmVisualPackageRead("Scaffold",valid.data(),valid.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"R1 manifest activated derelict");
    require(!hwRmVisualPackageRead("HeavyInterceptor",scaffold.data(),scaffold.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"scaffold manifest activated fleet hull");
    auto *package=hwRmVisualPackageRead("HeavyInterceptor",valid.data(),valid.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(package!=nullptr,error);require(hwRmVisualPackageScale(package)==2.5f,"package scale lost");
    require(std::strcmp(hwRmVisualPackageMaterial(package,0,1),"Engine")==0,"component/material namespace lost");
    auto *asset=hwRmVisualPackageTakeAsset(package);require(asset!=nullptr,"package asset transfer failed");
    require(!hwRmVisualPackageTakeAsset(package),"package transferred asset twice");
    auto *instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"transferred native package asset invalid");
    hwRmVisualPackageFree(package);hwRmVisualAssetFree(asset);
    float engine[16];require(hwRmVisualInstanceMarker(instance,"Eng0",2.5f,engine,error,sizeof(error))!=0,error);
    hwRmVisualInstanceFree(instance);
    for(const auto& rejected:std::vector<std::string>{
        header+"surface 0 0 Hull\n", // missing visible materials
        header+surfaces+"surface 0 1 Duplicate\n",
        header+surfaces+"scale 3\n",
        header+surfaces+"marker Eng0 Hull Missing\n",
        header+surfaces+"attach Gun0 Missing 1 gun.hod\n",
        "HW_RM_R1_VISUAL_1\ntype HeavyInterceptor\nscale 2\nhull ../hull.hod\nmad -\n"+surfaces,
        "HW_RM_R1_VISUAL_1\ntype HeavyInterceptor\nscale nan\nhull hull.hod\nmad -\n"+surfaces,
        header+surfaces+"surface 1 0 ../Bad\n",
        header+surfaces+"script do-something\n"}) {
        package=hwRmVisualPackageRead("HeavyInterceptor",rejected.data(),rejected.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
        require(!package,"invalid native package activated");require(fixture.reads==fixture.releases,"package source bytes leaked on rejection");
    }
    for(const char *other:{"UnknownShip","OtherGameCarrier","P2Mothership","heavyinterceptor"}) {
        const auto reads=fixture.reads;
        require(!hwRmVisualPackageRead(other,valid.data(),valid.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"out-of-scope package activated");
        require(reads==fixture.reads,"out-of-scope package read asset files");
    }
    require(fixture.reads==fixture.releases,"native package IO lifetime failed");
    auto trader=valid;
    trader.replace(trader.find("HW_RM_R1_VISUAL_1"),std::strlen("HW_RM_R1_VISUAL_1"),"HW_RM_TRADERS_VISUAL_1");
    trader.replace(trader.find("type HeavyInterceptor"),std::strlen("type HeavyInterceptor"),"type FloatingCity");
    auto *bentusi=hwRmVisualPackageRead("FloatingCity",trader.data(),trader.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(bentusi!=nullptr,error);hwRmVisualPackageFree(bentusi);
    auto station=trader;
    station.replace(station.find("type FloatingCity"),std::strlen("type FloatingCity"),"type ResearchStation");
    auto *research=hwRmVisualPackageRead("ResearchStation",station.data(),station.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(research!=nullptr,error);hwRmVisualPackageFree(research);
    auto ghost=trader;
    ghost.replace(ghost.find("type FloatingCity"),std::strlen("type FloatingCity"),"type Ghostship");
    auto *ghostship=hwRmVisualPackageRead("Ghostship",ghost.data(),ghost.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(ghostship!=nullptr,error);hwRmVisualPackageFree(ghostship);
    auto office=trader;
    office.replace(office.find("type FloatingCity"),std::strlen("type FloatingCity"),"type JunkYardHQ");
    auto *junkyard=hwRmVisualPackageRead("JunkYardHQ",office.data(),office.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(junkyard!=nullptr,error);hwRmVisualPackageFree(junkyard);
    auto generator=trader;
    generator.replace(generator.find("type FloatingCity"),std::strlen("type FloatingCity"),"type MiningBase");
    auto *fieldGenerator=hwRmVisualPackageRead("MiningBase",generator.data(),generator.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(fieldGenerator!=nullptr,error);hwRmVisualPackageFree(fieldGenerator);
    auto council=valid;
    council.replace(council.find("HW_RM_R1_VISUAL_1"),std::strlen("HW_RM_R1_VISUAL_1"),"HW_RM_P3_VISUAL_1");
    council.replace(council.find("type HeavyInterceptor"),std::strlen("type HeavyInterceptor"),"type P3Megaship");
    auto *megaship=hwRmVisualPackageRead("P3Megaship",council.data(),council.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error));
    require(megaship!=nullptr,error);hwRmVisualPackageFree(megaship);
    require(!hwRmNativeP3Type("P3Destroyer") && !hwRmNativeP3Type("P3Frigate") && !hwRmNativeP3Type("P3MegashipExtra"),"Council visual scope must stay exact");
    require(!hwRmNativeTraderType("MiningBaseExtra"),"field generator must enable its exact identity only");
    require(!hwRmNativeTraderType("JunkYardHQExtra") && !hwRmNativeTraderType("JunkYardDawg"),
        "Junkyard HQ binding must enable its exact identity only");
    require(!hwRmNativeTraderType("GhostshipHull") && !hwRmNativeTraderType("GhostshipExtra"),
        "Ghost Ship binding must enable its exact identity only");
    for(const char *type:{"ResearchStationBridge","ResearchStationTower"}) {
        require(!hwRmVisualPackageRead(type,station.data(),station.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"station subpart activated as complete station");
    }
    for(const char *type:{"P1Fighter","Derelict_Carrier","FloatingCity","ResearchStation","Ghostship","JunkYardHQ","MiningBase","P3Megaship"}) {
        require(!hwRmVisualPackageRead(type,valid.data(),valid.size(),packageRead,packageRelease,packageFixtureConfigure,&fixture,error,sizeof(error)),"R1 header crossed fleet/derelict scope");
    }
}
static void auditPose(const char *path) {
    char error[512];auto bytes=readBytes(path);HwRmHodScene *scene=nullptr;
    require(hwRmHodSceneRead(bytes.data(),bytes.size(),&scene,error,sizeof(error))!=0,error);
    auto *layer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(layer!=nullptr,error);
    auto *instance=hwRmSubsystemInstanceCreate(layer);require(instance!=nullptr,"audit instance");
    struct Audit {uint64_t hash=1469598103934665603ull;unsigned draws=0;};
    auto capture=[](void *user,const HwRmSubsystemDraw *draw) {
        auto &audit=*static_cast<Audit *>(user);++audit.draws;
        auto hash=[&](const void *data,size_t size){auto *b=static_cast<const unsigned char *>(data);while(size--){audit.hash^=*b++;audit.hash*=1099511628211ull;}};
        hash(draw->matrix,sizeof(draw->matrix));hash(&draw->component,sizeof(draw->component));
        hash(&draw->part->indexCount,sizeof(draw->part->indexCount));hash(&draw->part->material,sizeof(draw->part->material));
        hash(draw->mesh->name,std::strlen(draw->mesh->name));hash(&draw->mesh->lod,sizeof(draw->mesh->lod));
    };
    const auto *root=hwRmHodSceneJoint(scene,0);
    for(unsigned pose=0;pose<3;++pose){
        require(hwRmSubsystemSetJointControl(instance,0,root->name,pose*.1f,pose*.05f,pose*2.f,error,sizeof(error))!=0,error);
        for(unsigned lod=0;lod<16;++lod)for(float scale:{.01f,.5f,1.f,2.85848469f,10.f}) {
            Audit audit;require(hwRmSubsystemVisit(instance,lod,scale,nullptr,nullptr,capture,&audit,error,sizeof(error))!=0,error);
            float marker[16];require(hwRmSubsystemJointTransform(instance,0,root->name,scale,nullptr,nullptr,marker,error,sizeof(error))!=0,error);
            auto *data=reinterpret_cast<unsigned char *>(marker);
            for(size_t i=0;i<sizeof(marker);++i){audit.hash^=data[i];audit.hash*=1099511628211ull;}
            std::printf("POSE %u %u %.9g %u %016llx\n",pose,lod,scale,audit.draws,static_cast<unsigned long long>(audit.hash));
        }
    }
    unsigned draws=0;
    auto count=[](void *user,const HwRmSubsystemDraw *){++*static_cast<unsigned *>(user);};
    auto started=std::chrono::steady_clock::now();
    for(unsigned i=0;i<300;++i)require(hwRmSubsystemVisit(instance,0,1,nullptr,nullptr,count,&draws,error,sizeof(error))!=0,error);
    std::printf("TIMING 300 %.6f ms %u draws\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),draws);
    hwRmSubsystemInstanceFree(instance);hwRmSubsystemLayerFree(layer);hwRmHodSceneFree(scene);
}
int main(int argc,char**argv) {
    documentTests::run();
    try {
        HwHodShip ship{}; char error[256]; auto data=sample();
        packageTests();
        if(argc==3 && !std::strcmp(argv[1],"--audit-pose")){auditPose(argv[2]);return 0;}
        if(argc==3 && (std::strcmp(argv[1],"--installed-raider")==0 ||
                       std::strcmp(argv[1],"--installed-research-station")==0 ||
                       std::strcmp(argv[1],"--installed-junkyard-hq")==0 ||
                       std::strcmp(argv[1],"--installed-miningbase")==0 ||
                       std::strcmp(argv[1],"--installed-council")==0 ||
                       std::strcmp(argv[1],"--installed-ghostship")==0)) {
            const bool station=std::strcmp(argv[1],"--installed-research-station")==0;
            const bool ghost=std::strcmp(argv[1],"--installed-ghostship")==0;
            const bool office=std::strcmp(argv[1],"--installed-junkyard-hq")==0;
            const bool generator=std::strcmp(argv[1],"--installed-miningbase")==0;
            const bool council=std::strcmp(argv[1],"--installed-council")==0;
            const char *type=council?"P3Megaship":generator?"MiningBase":station?"ResearchStation":ghost?"Ghostship":office?"JunkYardHQ":"P1Fighter";
            auto root=std::filesystem::path(argv[2]);
            const auto bytes=readBytes(root/"ship.owshipdef");
            auto *package=hwRmVisualPackageRead(type,bytes.data(),bytes.size(),packageFileRead,
                packageFileRelease,packageConfigure,&root,error,sizeof(error));
            require(package!=nullptr,error);
            auto *asset=hwRmVisualPackageTakeAsset(package);
            auto *instance=hwRmVisualInstanceCreate(asset);
            require(instance!=nullptr,"native instance missing");
            for(unsigned lod=0;lod<5;++lod) {
                std::vector<HwRmSubsystemDraw> draws;
                require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),lod,1,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
                require(!draws.empty(),"native hull has no visible geometry");
                unsigned triangles=0;for(const auto& draw:draws)triangles+=draw.part->indexCount/3;
                std::printf("%s LOD%u: %zu visible draws, %u triangles\n",type,lod,draws.size(),triangles);
                for(const auto& draw:draws) for(float value:draw.matrix)
                    require(std::isfinite(value),"invalid native station transform");
                if(ghost) for(const auto& draw:draws) {
                    require(draw.mesh->lod==0,"Ghost Ship switched away from its textured Remastered source mesh");
                    require(std::strcmp(draw.material->shader,"default")!=0,"Ghost Ship selected an untextured material");
                    require(hwRmVisualPackageMaterial(package,draw.component,draw.part->material)!=nullptr,"Ghost Ship material binding missing");
                }
                require(triangles>=((station||ghost||office||generator||council)?1u:2500u),"native package selected empty/reduced source geometry");
            }
            hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);hwRmVisualPackageFree(package);
            return 0;
        }
        {
            auto* asset=hwRmVisualAssetCreate(data.data(),data.size(),nullptr,0,error,sizeof(error));require(asset!=nullptr,error);
            float authored[16]={1,0,0,0,0,0,1,0,0,-1,0,0,3,4,5,1};
            require(hwRmVisualAssetAuthorJoint(asset,"Hull","Root","-",authored)!=0,"authored root rejected");
            require(!hwRmVisualAssetAuthorJoint(asset,"Hull","Root","Root",authored),"authored self-cycle accepted");
            require(!hwRmVisualAssetAuthorJoint(asset,"Hull","Missing","-",authored),"unknown authored joint accepted");
            auto* instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"authored instance missing");float result[16];
            require(hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(instance),0,"Root",2,nullptr,nullptr,result,error,sizeof(error))!=0,error);
            require(std::abs(result[12]-6)<.0001f&&std::abs(result[13]-8)<.0001f&&std::abs(result[14]-10)<.0001f,"authored translation did not scale exactly once");
            require(std::abs(result[6]-1)<.0001f&&std::abs(result[9]+1)<.0001f,"authored rotation lost");
            require(!hwRmVisualAssetAuthorJoint(asset,"Hull","Root","-",authored),"live authored rig mutated");
            hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);
        }
        if(argc==3 && std::strcmp(argv[1],"--projectiles")==0) {
            unsigned models=0,markers=0,unresolved=0;
            for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[2])){
                if(entry.path().extension()!=".hod")continue;
                auto hod=readBytes(entry.path());auto madPath=entry.path();madPath.replace_extension(".mad");
                Bytes mad;if(std::filesystem::exists(madPath))mad=readBytes(madPath);
                auto* asset=hwRmVisualAssetCreate(hod.data(),hod.size(),mad.empty()?nullptr:mad.data(),mad.size(),error,sizeof(error));
                if(!asset)throw std::runtime_error(entry.path().string()+": "+error);
                auto events=readBytes(entry.path().parent_path()/"events.owevents");
                require(hwRmVisualAssetBindPresentation(asset,"Hull",events.data(),events.size(),error,sizeof(error))!=0,error);
                auto* instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"projectile instance creation failed");
                for(unsigned lod=0;lod<5;++lod){
                    std::vector<HwRmSubsystemDraw> draws;
                    require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),lod,1,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
                    require(!draws.empty(),"native projectile has no visual mesh");
                    for(const auto& draw:draws)for(float value:draw.matrix)require(std::isfinite(value),"invalid projectile transform");
                }
                const auto* cues=hwRmVisualInstancePresentation(instance,0);
                for(unsigned i=0;i<hwRmPresentationEventCount(cues);++i){
                    const auto* event=hwRmPresentationEventAt(cues,i);
                    if(!event->markers[0])continue;
                    float matrix[16];
                    if(!hwRmVisualInstancePresentationMarker(instance,0,event,0,1,matrix,error,sizeof(error))){
                        std::printf("Unresolved source projectile marker: %s: %s: %s\n",entry.path().string().c_str(),event->markers,error);
                        ++unresolved;
                    }else ++markers;
                }
                hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);++models;
            }
            require(models>0,"no native projectile models checked");
            std::printf("Native projectile audit: %u model/LOD0-4 checks passed, %u resolved event markers, %u unresolved source markers\n",models,markers,unresolved);
            return unresolved?1:0;
        }
        if(argc==3 && std::strcmp(argv[1],"--taiidan")==0) {
            const auto root=std::filesystem::path(argv[2]);
            const auto audit=(root/"gun-audit.tsv").string();
            auditGunContracts(root/"source",audit.c_str(),false,true);
            auditTaiidanPackages(root/"package-cache/v1");return 0;
        }
        if(argc==3 && std::strcmp(argv[1],"--kadeshi")==0) {
            const auto root=std::filesystem::path(argv[2])/"package-cache/v1/P2";
            unsigned ships=0,guns=0;
            for(const auto& folder:std::filesystem::directory_iterator(root)) {
                auto path=folder.path()/"RMNative";auto bytes=readBytes(path/"visual.rmnative");
                const auto name=folder.path().filename().string();
                auto *package=hwRmVisualPackageRead(name.c_str(),bytes.data(),bytes.size(),packageFileRead,packageFileRelease,packageConfigure,&path,error,sizeof(error));
                if(!package)std::fprintf(stderr,"P2/%s: %s\n",name.c_str(),error);
                require(package!=nullptr,"Kadeshi native package rejected");
                auto *asset=hwRmVisualPackageTakeAsset(package);auto *instance=hwRmVisualInstanceCreate(asset);
                require(instance!=nullptr,"Kadeshi instance");
                const unsigned count=hwRmVisualInstanceGunCount(instance);
                require(count==(name=="P2Swarmer"?2:name=="P2FuelPod"?1:name=="P2Mothership"?14:4),"Kadeshi gameplay gun count changed");
                for(unsigned gun=0;gun<count;++gun) {
                    float muzzle[16];
                    require(hwRmVisualInstanceGunMuzzle(instance,gun,hwRmVisualPackageScale(package),muzzle,error,sizeof(error))!=0,error);
                    for(float v:muzzle)require(std::isfinite(v),"Kadeshi invalid muzzle");
                    if(name=="P2MultiBeamFrigate") {
                        auto *other=hwRmVisualInstanceCreate(asset);float untouched[16],aimed[16];
                        require(other!=nullptr,"Kadeshi second instance");
                        HwRmGunAim aim{};aim.gun=gun;
                        aim.heading[0]=.3f;aim.heading[1]=.4f;aim.heading[2]=.8660254f;
                        require(hwRmVisualInstanceAimGuns(instance,&aim,1,error,sizeof(error))!=0,error);
                        require(hwRmVisualInstanceGunMuzzle(instance,gun,hwRmVisualPackageScale(package),aimed,error,sizeof(error))!=0,error);
                        float alignment=0;for(unsigned axis=0;axis<3;++axis)alignment+=aimed[8+axis]*aim.heading[axis];
                        require(alignment>.9999f,"Kadeshi multibeam mount disagrees with firing direction");
                        require(hwRmVisualInstanceGunMuzzle(other,gun,hwRmVisualPackageScale(package),untouched,error,sizeof(error))!=0,error);
                        require(std::memcmp(muzzle,untouched,sizeof(muzzle))==0,"Kadeshi aiming leaks across ships");
                        hwRmVisualInstanceFree(other);
                    }
                }
                for(unsigned lod=0;lod<5;++lod) {
                    std::vector<HwRmSubsystemDraw> draws;
                    require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),lod,hwRmVisualPackageScale(package),nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
                    require(!draws.empty(),"Kadeshi empty LOD");
                    unsigned triangles=0;
                    for(const auto& draw:draws) {
                        triangles+=draw.part->indexCount/3;
                        if(draw.component==0 && name!="P2Mothership") {
                            require(draw.mesh->lod==0,"Kadeshi selected a placeholder/UV-incompatible hull LOD");
                            require(std::strcmp(draw.material->shader,"default")!=0,"Kadeshi lost textured hull for flat vertex colors");
                        }
                        if(draw.component==0 && name=="P2Mothership" && lod>0)
                            require(draw.mesh->lod==1,"Kadeshi mothership lost its compatible authored LOD");
                        require(hwRmVisualPackageMaterial(package,draw.component,draw.part->material)!=nullptr,"Kadeshi missing material");
                        for(float v:draw.matrix)require(std::isfinite(v),"Kadeshi invalid transform");
                    }
                    std::printf("%s requested LOD%u: %zu draws, %u triangles; materials verified\n",name.c_str(),lod,draws.size(),triangles);
                }
                guns+=count;++ships;
                hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);hwRmVisualPackageFree(package);
            }
            require(ships==5 && guns==25,"Incomplete Kadeshi fleet");
            std::puts("Five Kadeshi native packages, 25 muzzles and LOD0-4 passed");return 0;
        }
        if(argc==3 && std::strcmp(argv[1],"--raiders-derelicts")==0) {
            const auto root=std::filesystem::path(argv[2]);
            const auto audit=(root/"hierarchy-audit.tsv").string();
            auditGunContracts(root/"source",audit.c_str(),true);
            auditRaiderDerelictPackages(root/"package-cache/v1");
            return 0;
        }
        {
            auto proxyBytes=sample(false,false,true);
            auto *proxy=hwRmVisualAssetCreate(proxyBytes.data(),proxyBytes.size(),nullptr,0,error,sizeof(error));require(proxy!=nullptr,error);
            auto *instance=hwRmVisualInstanceCreate(proxy);require(instance!=nullptr,"effect proxy instance missing");
            std::vector<HwRmSubsystemDraw> draws;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),0,1,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
            require(draws.size()==2,"effect proxy emitted an opaque ship surface");
            float marker[16];require(hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(instance),0,"HeavyInterceptor",1,nullptr,nullptr,marker,error,sizeof(error))!=0,error);
            hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(proxy);
        }
        {
            auto model=sample();
            auto mad=presentationMadFixture();
            auto *asset=hwRmVisualAssetCreate(model.data(),model.size(),mad.data(),mad.size(),error,sizeof(error));require(asset!=nullptr,error);
            require(hwRmVisualAssetAttach(asset,"shared","Turret","Root",HW_RM_TURRET,model.data(),model.size(),error,sizeof(error))!=0,error);
            Bytes events={'O','W','E','V','E','N','T','2'};
            le(events,1);le(events,1);le(events,6);
            le(events,1);le(events,0xffffffffu);le(events,0x3f800000u);le(events,0);le(events,0);le(events,1);
            le(events,0);le(events,0);le(events,0x3f800000u);le(events,0);
            for(unsigned i=0;i<5;++i)le(events,0);
            le(events,0x3f800000u);for(unsigned i=0;i<4;++i)le(events,0);
            for(unsigned char c:Bytes{0,'F','i','r','e',0})events.push_back(c);
            require(hwRmVisualAssetBindPresentation(asset,"Hull",events.data(),events.size(),error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindPresentation(asset,"Turret",events.data(),events.size(),error,sizeof(error))!=0,error);
            const char *sourceClip="Fire";
            require(hwRmVisualAssetBindAnimation(asset,"OpenMount",&sourceClip,1,error,sizeof(error))!=0,error);
            require(!hwRmVisualAssetBindPresentation(asset,"Turret",events.data(),events.size(),error,sizeof(error)),"duplicate component events accepted");
            require(!hwRmVisualAssetBindPresentation(asset,"Missing",events.data(),events.size(),error,sizeof(error)),"events attached to absent component");
            require(hwRmVisualAssetBindGun(asset,0,"Hull",nullptr,nullptr,nullptr,"Root",error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindGun(asset,1,"Turret",nullptr,nullptr,nullptr,"Root",error,sizeof(error))!=0,error);
            require(!hwRmVisualAssetBindGunPresentation(asset,0,"Missing",error,sizeof(error)),"missing cue accepted");
            require(!hwRmVisualAssetBindGunPresentation(asset,2,"Fire",error,sizeof(error)),"absent gun cue accepted");
            require(hwRmVisualAssetBindGunPresentation(asset,0,"Fire",error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindGunPresentation(asset,1,"Fire",error,sizeof(error))!=0,error);
            require(!hwRmVisualAssetBindGunPresentation(asset,0,"Fire",error,sizeof(error)),"duplicate gun cue accepted");
            const char *weaponfire="firefx=\"muzzle\" hitfx=\"impact\" hit_clamp={.5,1.25}";
            require(hwRmVisualAssetBindWeaponPresentation(asset,0,weaponfire,std::strlen(weaponfire),error,sizeof(error))!=0,error);
            require(!hwRmVisualAssetBindWeaponPresentation(asset,0,weaponfire,std::strlen(weaponfire),error,sizeof(error)),"duplicate weaponfire accepted");
            require(!hwRmVisualAssetBindWeaponPresentation(asset,2,weaponfire,std::strlen(weaponfire),error,sizeof(error)),"absent weapon accepted weaponfire");
            auto *instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"presentation instance missing");
            {
                auto* animated=hwRmVisualInstanceCreate(asset);std::vector<unsigned> samples;
                require(animated!=nullptr,"phase presentation instance missing");
                require(hwRmVisualInstanceAnimationPresentation(animated,"OpenMount",0,capturePresentation,&samples)!=0,"phase activation failed");
                require(samples.empty(),"phase-zero burst fired before advancing MAD");
                require(hwRmVisualInstanceAnimationPresentation(animated,"OpenMount",.5,capturePresentation,&samples)!=0,"MAD phase advance failed");
                require(samples.size()==1,"bound source clip did not dispatch its event timeline");
                require(hwRmVisualInstanceAnimationPresentation(animated,"OpenMount",.5,capturePresentation,&samples)!=0,"same phase rejected");
                require(hwRmVisualInstanceAnimationPresentation(animated,"OpenMount",1,capturePresentation,&samples)!=0,"MAD endpoint failed");
                require(samples.size()==1,"repeated/end phase replayed the zero-time burst");
                hwRmVisualInstanceStopPresentation(animated);
                require(hwRmVisualInstanceAnimationPresentation(animated,"OpenMount",.5,capturePresentation,&samples)!=0,"MAD restart failed");
                require(samples.size()==2,"MAD reset did not permit a new activation");
                hwRmVisualInstanceFree(animated);
            }
            auto *other=hwRmVisualInstanceCreate(asset);require(other!=nullptr,"second presentation instance missing");
            std::vector<unsigned> cues;
            {auto event=*hwRmPresentationEventAt(hwRmVisualInstancePresentation(instance,1),0);unsigned gun=99;
                event.markers="Root";
                require(hwRmVisualInstanceEventGun(instance,1,&event,&gun)&&gun==1,"component muzzle did not resolve its own gun");
                event.markers="HeavyInterceptor";
                require(!hwRmVisualInstanceEventGun(instance,1,&event,&gun),"vent marker incorrectly treated as muzzle");
            }
            require(hwRmVisualInstanceFirePresentation(instance,0,10,capturePresentation,&cues)!=0,"hull gun cue failed");
            require(hwRmVisualInstanceFirePresentation(instance,1,10,capturePresentation,&cues)!=0,"turret gun cue failed");
            require(!hwRmVisualInstanceFirePresentation(instance,2,10,capturePresentation,&cues),"unbound gun fired cue");
            require(cues.empty(),"phase-zero cue emitted during negative activation range");
            require(hwRmVisualInstanceUpdatePresentation(other,11,capturePresentation,&cues)!=0,"idle instance update failed");
            require(cues.empty(),"playback leaked between ships");
            require(hwRmVisualInstanceUpdatePresentation(instance,11,capturePresentation,&cues)!=0,"cue update failed");
            require(cues==std::vector<unsigned>({0,0,1,1}),"independent playback failed");
            {
                auto event=*hwRmPresentationEventAt(hwRmVisualInstancePresentation(instance,1),0);
                event.markers="missing,Root,HeavyInterceptor";float before[16],after[16],neutral[16];
                require(hwRmVisualInstancePresentationMarker(instance,1,&event,0,1,before,error,sizeof(error))!=0,error);
                event.markers="missing,rOoT,hEaVyInTeRcEpToR";
                require(hwRmVisualInstancePresentationMarker(instance,1,&event,0,1,neutral,error,sizeof(error))!=0,error);
                require(!std::memcmp(before,neutral,sizeof(before)),"RM case-folded event marker changed its attachment");
                require(hwRmSubsystemSetJointControl(hwRmVisualInstanceSubsystem(instance),1,"Root",.4f,0,0,error,sizeof(error))!=0,error);
                require(hwRmVisualInstancePresentationMarker(instance,1,&event,0,1,after,error,sizeof(error))!=0,error);
                require(std::memcmp(before,after,sizeof(before))!=0,"event marker froze its fire-time turret pose");
                require(hwRmVisualInstancePresentationMarker(other,1,&event,0,1,neutral,error,sizeof(error))!=0,error);
                require(!std::memcmp(before,neutral,sizeof(before)),"event marker pose leaked into another ship");
                event.markers="missing";std::memcpy(neutral,before,sizeof(before));
                require(!hwRmVisualInstancePresentationMarker(instance,1,&event,0,1,neutral,error,sizeof(error)),"missing marker accepted");
                require(!std::memcmp(before,neutral,sizeof(before)),"failed marker lookup changed output");
                event.markers="";
                require(hwRmVisualInstancePresentationMarker(instance,1,&event,0,1,neutral,error,sizeof(error))!=0,error);
                require(hwRmSubsystemComponentTransform(hwRmVisualInstanceSubsystem(instance),1,1,after,error,sizeof(error))!=0,error);
                require(!std::memcmp(after,neutral,sizeof(after)),"unmarked event did not use its component owner frame");
            }
            hwRmVisualInstanceFree(other);
            auto *hull=hwRmVisualInstancePresentation(instance,0),*turret=hwRmVisualInstancePresentation(instance,1);
            require(hull&&turret&&hull!=turret,"component event ownership collapsed");
            require(!hwRmVisualAssetBindPresentation(asset,"Hull",events.data(),events.size(),error,sizeof(error)),"live event definition mutated");
            int meshKey=0,shipA=0,shipB=0;
            require(meshNativeVisualRegister(&meshKey,asset,2,0)!=0,"event registry registration failed");
            {
                int lateShip=0;float actual[16],expected[16],rest[16];
                auto *reference=hwRmVisualInstanceCreate(asset);
                require(reference!=nullptr,"pose reference missing");
                require(hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(reference),0,"Root",2,nullptr,nullptr,rest,error,sizeof(error)),error);
                require(hwRmVisualInstanceSampleEvent(reference,"OpenMount",1,error,sizeof(error)),error);
                require(hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(reference),0,"Root",2,nullptr,nullptr,expected,error,sizeof(error)),error);
                require(std::memcmp(rest,expected,sizeof(rest))!=0,"fixture does not animate");
                meshNativeVisualSampleShipEvent(&lateShip,"OpenMount",1);
                auto *late=meshNativeVisualAcquire(&meshKey,&lateShip);
                require(late && hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(late),0,"Root",2,nullptr,nullptr,actual,error,sizeof(error)),"late pose unavailable");
                require(!std::memcmp(actual,expected,sizeof(actual)),"late visual lost finished animation");
                meshNativeVisualReleaseShip(&lateShip);
                late=meshNativeVisualAcquire(&meshKey,&lateShip);
                require(late && hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(late),0,"Root",2,nullptr,nullptr,actual,error,sizeof(error)),"reused ship unavailable");
                require(!std::memcmp(actual,rest,sizeof(actual)),"reused ship inherited old pose");
                meshNativeVisualReleaseShip(&lateShip);hwRmVisualInstanceFree(reference);
            }
            std::vector<RegistryCue> registryCues;
            require(meshNativeVisualFirePresentation(&meshKey,&shipA,1,20,captureRegistryPresentation,&registryCues)!=0,"registry fire failed");
            int lowerMesh=0;
            auto *lowerAsset=hwRmVisualAssetCreate(model.data(),model.size(),nullptr,0,error,sizeof(error));
            require(lowerAsset!=nullptr,error);
            require(hwRmVisualAssetBindPresentation(lowerAsset,"Hull",events.data(),events.size(),error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindGun(lowerAsset,0,"Hull",nullptr,nullptr,nullptr,"Root",error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindGunPresentation(lowerAsset,0,"Fire",error,sizeof(error))!=0,error);
            require(meshNativeVisualRegister(&lowerMesh,lowerAsset,2,1)!=0,"lower LOD registration failed");
            require(!meshNativeVisualFirePresentation(&lowerMesh,&shipA,0,20,captureRegistryPresentation,&registryCues),"camera LOD duplicated active ship timeline");
            require(meshNativeVisualFirePresentation(&meshKey,&shipB,0,20,captureRegistryPresentation,&registryCues)!=0,"second registry ship failed");
            require(!meshNativeVisualFirePresentation(&meshKey,&shipA,9,20,captureRegistryPresentation,&registryCues),"unknown registry gun accepted");
            require(registryCues.empty(),"registry emitted phase zero at activation");
            require(meshNativeVisualUpdatePresentation(20.5,captureRegistryPresentation,&registryCues)!=0,"registry update failed");
            require(registryCues.size()==2,"registry lost or duplicated ship events");
            for(const auto& cue:registryCues){
                require(cue.scale==2 && cue.instance==meshNativeVisualAcquire(&meshKey,cue.ship),"registry event lost live pose/scale");
                require((cue.ship==&shipA && cue.component==1)||(cue.ship==&shipB && cue.component==0),"registry event crossed ship/component");
            }
            require(meshNativeVisualUpdatePresentation(20.5,captureRegistryPresentation,&registryCues)!=0 && registryCues.size()==2,"same-clock registry update repeated cues");
            meshNativeVisualStopPresentation(&shipA);
            meshNativeVisualReleaseShip(&shipB);
            require(meshNativeVisualUpdatePresentation(21,captureRegistryPresentation,&registryCues)!=0 && registryCues.size()==2,"stopped/deleted ship dispatched event");
            require(!meshNativeVisualUpdatePresentation(std::numeric_limits<double>::quiet_NaN(),captureRegistryPresentation,&registryCues),"registry accepted invalid time");
            require(meshNativeVisualFirePresentation(&lowerMesh,&shipA,0,22,captureRegistryPresentation,&registryCues)!=0,"stopped registry retained stale LOD ownership");
            require(meshNativeVisualUpdatePresentation(22.5,captureRegistryPresentation,&registryCues)!=0 && registryCues.size()==3,"replacement timeline did not dispatch exactly once");
            meshNativeVisualStopPresentation(nullptr);
            require(meshNativeVisualUpdatePresentation(23,captureRegistryPresentation,&registryCues)!=0 && registryCues.size()==3,"global stop retained playing timeline");
            RegistryConsumer consumer;
            require(!meshNativeVisualPresentationEnabled(),"registry started with an installed consumer");
            require(!meshNativeVisualNotifyWeaponFire(&meshKey,&shipA,0,30),"no-consumer notification consumed a shot");
            meshNativeVisualSetPresentationSink(consumeRegistryEvent,consumeRegistryWeapon,&consumer);
            require(meshNativeVisualPresentationEnabled(),"consumer was not installed");
            require(meshNativeVisualNotifyWeaponFire(&meshKey,&shipA,0,30)!=0,"gameplay shot did not dispatch");
            require(consumer.events.empty() && consumer.weapons==std::vector<std::string>{"muzzle"},"weaponfire did not precede timed model cue");
            require(!meshNativeVisualNotifyWeaponFire(&lowerMesh,&shipA,0,30),"gameplay lower LOD bypassed presentation ownership");
            require(!meshNativeVisualNotifyWeaponFire(&meshKey,&shipA,99,30),"unbound gameplay shot dispatched");
            require(meshNativeVisualPumpPresentation(30.5)!=0 && consumer.events.size()==1,"simulation clock did not drive model event");
            meshNativeVisualSetPresentationSink(consumeRegistryEvent,nullptr,&consumer);
            require(meshNativeVisualPumpPresentation(31)!=0 && consumer.events.size()==1,"replacement consumer inherited partial playback");
            require(meshNativeVisualNotifyWeaponFire(&meshKey,&shipB,1,32)!=0,"model-only gameplay shot failed");
            require(meshNativeVisualPumpPresentation(32.5)!=0 && consumer.events.size()==2,"model-only event was lost");
            meshNativeVisualSetPresentationSink(nullptr,consumeRegistryWeapon,&consumer);
            require(meshNativeVisualNotifyWeaponFire(&meshKey,&shipA,0,34)!=0 && consumer.weapons.size()==2,"weapon-only consumer required a model cue");
            meshNativeVisualSetPresentationSink(nullptr,nullptr,nullptr);
            require(!meshNativeVisualPresentationEnabled(),"consumer detach failed");
            require(meshNativeVisualPumpPresentation(35)!=0 && consumer.events.size()==2,"detached consumer received events");
            require(!meshNativeVisualNotifyAnimation(&meshKey,&shipA,"OpenMount",0),"MAD cues ran without a consumer");
            meshNativeVisualSetPresentationSink(consumeRegistryEvent,nullptr,&consumer);
            require(meshNativeVisualNotifyAnimation(&meshKey,&shipA,"OpenMount",0)!=0,"MAD activation did not reach canonical instance");
            require(meshNativeVisualNotifyAnimation(&meshKey,&shipA,"OpenMount",.5)!=0,"MAD phase did not reach consumer");
            require(consumer.events.size()==3&&consumer.events.back().component==0,"MAD cue lost its source hull ownership");
            require(meshNativeVisualNotifyAnimation(&meshKey,&shipA,"OpenMount",.5)!=0&&consumer.events.size()==3,"MAD resample duplicated sound");
            require(!meshNativeVisualNotifyAnimation(&lowerMesh,&shipA,"OpenMount",.5),"lower LOD duplicated MAD presentation");
            meshNativeVisualSetPresentationSink(nullptr,nullptr,nullptr);
            meshNativeVisualReleaseMesh(&lowerMesh);
            meshNativeVisualReleaseMesh(&meshKey);
            require(!std::strcmp(hwRmWeaponPresentationAt(hwRmVisualInstanceWeaponPresentation(instance,0),HW_RM_WEAPON_HIT)->fx,"impact"),"instance lost weaponfire after asset owner release");
            require(hwRmVisualInstancePresentation(instance,0)==hull,"instance lost events on owner release");
            require(hwRmVisualInstancePresentation(instance,1)==turret,"turret events lost on owner release");
            hwRmVisualInstanceFree(instance);
        }
        {
            auto *asset=hwRmVisualAssetCreate(data.data(),data.size(),nullptr,0,error,sizeof(error));
            require(asset!=nullptr,error);
            require(hwRmVisualAssetAttach(asset,"sharedgun","Gun0","Root",HW_RM_TURRET,data.data(),data.size(),error,sizeof(error))!=0,error);
            require(hwRmVisualAssetAttach(asset,"sharedgun","Gun1","Root",HW_RM_TURRET,data.data(),data.size(),error,sizeof(error))!=0,error);
            auto mismatched=data;mismatched.back()^=1;
            require(!hwRmVisualAssetAttach(asset,"sharedgun","Gun2","Root",HW_RM_TURRET,mismatched.data(),mismatched.size(),error,sizeof(error)),"shared model key accepted mismatched source bytes");
            require(hwRmVisualAssetBindGun(asset,0,"Gun0","Root","Root","Root","Root",error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindGun(asset,1,"Gun1","Root",nullptr,nullptr,"Root",error,sizeof(error))!=0,error);
            require(hwRmVisualAssetBindMarker(asset,"REP0","Gun0","Root",error,sizeof(error))!=0,error);
            require(!hwRmVisualAssetBindMarker(asset,"REP0","Gun1","Root",error,sizeof(error)),"duplicate repair semantic accepted");
            require(!hwRmVisualAssetBindMarker(asset,"Eng0","Gun1","Missing",error,sizeof(error)),"missing engine semantic guessed");
            require(!hwRmVisualAssetBindGun(asset,2,"Gun0","Root",nullptr,nullptr,"Root",error,sizeof(error)),"two guns accepted one controlled pivot");
            require(!hwRmVisualAssetBindGun(asset,2,"Gun1",nullptr,nullptr,nullptr,"Missing",error,sizeof(error)),"missing muzzle guessed");
            auto *a=hwRmVisualInstanceCreate(asset),*b=hwRmVisualInstanceCreate(asset);
            require(a && b,"owned visual instances missing");
            require(hwRmVisualInstanceGunCount(a)==2,"explicit native guns missing");
            require(!hwRmVisualAssetBindGun(asset,3,"Hull",nullptr,nullptr,nullptr,"Root",error,sizeof(error)),"live gun definitions mutated");
            require(hwRmVisualInstanceSetGun(a,0,1.57079632679f,0,5,error,sizeof(error))!=0,error);
            float muzzleA[16],muzzleB[16];
            require(hwRmVisualInstanceGunMuzzle(a,0,1,muzzleA,error,sizeof(error))!=0,error);
            require(hwRmVisualInstanceGunMuzzle(b,0,1,muzzleB,error,sizeof(error))!=0,error);
            float repair[16];require(hwRmVisualInstanceMarker(a,"REP0",1,repair,error,sizeof(error))!=0,error);
            require(std::memcmp(repair,muzzleA,sizeof(repair))==0,"repair semantic failed to follow its authored moving pivot");
            require(!hwRmVisualAssetBindMarker(asset,"Eng0","Hull","Root",error,sizeof(error)),"live semantic mapping changed");
            std::memcpy(repair,muzzleA,sizeof(repair));
            require(!hwRmVisualInstanceMarker(a,"Eng0",1,repair,error,sizeof(error)),"unbound engine marker guessed");
            require(std::memcmp(repair,muzzleA,sizeof(repair))==0,"failed semantic query wrote output");
            require(std::abs(muzzleA[13]-5)<.001f && muzzleB[13]==0,"gun control leaked across ships or muzzle failed to follow recoil");
            require(!hwRmVisualInstanceSetGun(a,0,0,0,-1,error,sizeof(error)),"bad native recoil accepted");
            require(!hwRmVisualInstanceSetGun(a,63,0,0,0,error,sizeof(error)),"unbound native gun guessed");
            HwRmJointControlUpdate rejected[]={{1,"Root",0,0,0},{1,"Missing",0,0,0}};
            require(!hwRmSubsystemSetJointControls(hwRmVisualInstanceSubsystem(a),rejected,2,error,sizeof(error)),"invalid batch accepted");
            float unchanged[16];require(hwRmVisualInstanceGunMuzzle(a,0,1,unchanged,error,sizeof(error))!=0,error);
            require(std::memcmp(muzzleA,unchanged,sizeof(muzzleA))==0,"failed batch changed first pivot");
            HwRmGunControl badBatch[]={{0,0,0,0},{63,0,0,0}};
            require(!hwRmVisualInstanceSetGuns(a,badBatch,2,error,sizeof(error)),"unbound second gun accepted");
            require(hwRmVisualInstanceGunMuzzle(a,0,1,unchanged,error,sizeof(error))!=0,error);
            require(std::memcmp(muzzleA,unchanged,sizeof(muzzleA))==0,"failed gun batch changed first barrel");
            HwRmGunAim badAim[]={{0,{1,0,0},0},{63,{0,0,1},0}};
            require(!hwRmVisualInstanceAimGuns(a,badAim,2,error,sizeof(error)),"unbound second aim accepted");
            require(hwRmVisualInstanceGunMuzzle(a,0,1,unchanged,error,sizeof(error))!=0,error);
            require(std::memcmp(muzzleA,unchanged,sizeof(muzzleA))==0,"failed aim batch changed first barrel");
            HwRmGunAim aimedGun={0,{.3f,.4f,.8660254f},2};
            require(hwRmVisualInstanceAimGuns(a,&aimedGun,1,error,sizeof(error))!=0,error);
            require(hwRmVisualInstanceGunMuzzle(a,0,1,unchanged,error,sizeof(error))!=0,error);
            float dot=0;for(unsigned axis=0;axis<3;++axis) dot+=unchanged[8+axis]*aimedGun.heading[axis];
            require(dot>.9999f,"synthetic native aim does not follow heading");
            require(!hwRmVisualAssetAttach(asset,"other","Late","Root",HW_RM_TURRET,data.data(),data.size(),error,sizeof(error)),"owned asset modified after instance creation");
            require(!hwRmVisualInstanceAnimation(a),"invented native animation");
            require(hwRmSubsystemSetVisible(hwRmVisualInstanceSubsystem(a),1,0)!=0,"owned instance visibility failed");
            hwRmVisualAssetFree(asset);
            require(!hwRmVisualInstanceCreate(asset),"instance created on released owned asset");
            auto* detached=hwRmVisualInstanceSnapshot(a);require(detached!=nullptr,"detached current-pose snapshot failed after asset-owner release");
            float frozen[16];require(hwRmVisualInstanceGunMuzzle(detached,0,1,frozen,error,sizeof(error))!=0,error);
            require(std::memcmp(frozen,unchanged,sizeof(frozen))==0,"snapshot lost current articulated gun pose");
            std::vector<HwRmSubsystemDraw> aliveA,aliveB;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,1,nullptr,nullptr,capture,&aliveA,error,sizeof(error))!=0,error);
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(b),0,1,nullptr,nullptr,capture,&aliveB,error,sizeof(error))!=0,error);
            require(aliveA.size()==6 && aliveB.size()==9,"released source lifetime/instance independence failed");
            hwRmVisualInstanceFree(a);aliveB.clear();
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(b),0,1,nullptr,nullptr,capture,&aliveB,error,sizeof(error))!=0,error);
            require(aliveB.size()==9,"deleting one instance invalidated another");
            hwRmVisualInstanceFree(b);
            require(hwRmVisualInstanceGunMuzzle(detached,0,1,frozen,error,sizeof(error))!=0,error);
            require(std::memcmp(frozen,unchanged,sizeof(frozen))==0,"destroying original ship/asset invalidated detached FX pose");
            std::vector<HwRmSubsystemDraw> detachedDraws;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(detached),0,1,nullptr,nullptr,capture,&detachedDraws,error,sizeof(error))!=0,error);
            require(detachedDraws.size()==6,"snapshot lost authored subsystem visibility");hwRmVisualInstanceFree(detached);
        }
        {
            auto* asset=hwRmVisualAssetCreate(data.data(),data.size(),nullptr,0,error,sizeof(error));require(asset!=nullptr,error);
            const char names[]="\0Damage\0dmg01";Bytes state={'O','W','E','V','E','N','T','2'};
            le(state,2);le(state,1);le(state,sizeof(names));
            le(state,1);le(state,~0u);le(state,0x3f800000);le(state,0);le(state,0);le(state,1);
            le(state,8);le(state,0);le(state,0x3f800000);le(state,0x3e800000);le(state,0x3f800000);le(state,1);
            le(state,1);le(state,0xbf800000);le(state,0x3f800000);le(state,0);
            for(unsigned i=0;i<5;++i)le(state,0);le(state,0x3f800000);for(unsigned i=0;i<4;++i)le(state,0);
            state.insert(state.end(),names,names+sizeof(names));
            require(hwRmVisualAssetBindPresentation(asset,"Hull",state.data(),state.size(),error,sizeof(error))!=0,error);
            auto* instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"state instance missing");std::vector<unsigned> cues;
            require(hwRmVisualInstanceSetPresentationState(instance,0,"Damage",.1f,10,capturePresentation,&cues)!=0,"damage state start failed");
            require(cues.empty()&&hwRmVisualInstancePresentationState(instance,0,0)==1,"low damage enabled high-threshold child");
            require(hwRmVisualInstanceSetPresentationState(instance,0,"Damage",.3f,10,capturePresentation,&cues)!=0&&cues.size()==1,"damage threshold did not activate child");
            require(hwRmVisualInstanceSetPresentationState(instance,0,"Damage",.3f,10.1,capturePresentation,&cues)!=0&&cues.size()==1,"unchanged state restarted its effects");
            require(hwRmVisualInstanceSetPresentationState(instance,0,"Damage",.2f,10.2,capturePresentation,&cues)!=0&&hwRmVisualInstancePresentationState(instance,0,1)==0,"repair did not disable child");
            require(hwRmVisualInstanceSetPresentationState(instance,0,"Damage",.3f,10.3,capturePresentation,&cues)!=0&&cues.size()==2,"re-damage did not reactivate child");
            require(hwRmVisualInstanceSetPresentationState(instance,0,"Damage",0,10.4,capturePresentation,&cues)!=0&&hwRmVisualInstancePresentationState(instance,0,0)==0,"zero state did not stop its parent");
            require(hwRmVisualInstanceUpdatePresentation(instance,11,capturePresentation,&cues)!=0&&cues.size()==2,"disabled state dispatched stale events");
            require(!hwRmVisualInstanceSetPresentationState(instance,0,"Damage",2,11,capturePresentation,&cues),"out-of-range damage state accepted");
            HwRmPresentationEvent surface{};
            surface.markers="top_00,bottom_00,left_00,right_00,front_00,back_00";
            float frameA[16],frameB[16],frameScaled[16];
            require(hwRmVisualInstancePresentationMarker(instance,0,&surface,123,1,frameA,error,sizeof(error))!=0,error);
            require(hwRmVisualInstancePresentationMarker(instance,0,&surface,123,1,frameB,error,sizeof(error))!=0,error);
            require(!std::memcmp(frameA,frameB,sizeof(frameA)),"generated damage attachment changed between frames");
            require(hwRmVisualInstancePresentationMarker(instance,0,&surface,123,2,frameScaled,error,sizeof(error))!=0,error);
            for(unsigned axis=0;axis<3;++axis){
                require(std::fabs(frameScaled[12+axis]-2*frameA[12+axis])<0.001f,"surface attachment did not follow model scale");
                require(std::isfinite(frameA[8+axis]),"surface attachment normal is not finite");
            }
            surface.markers="not_a_real_marker";
            require(!hwRmVisualInstancePresentationMarker(instance,0,&surface,123,1,frameA,error,sizeof(error)),"unknown marker silently attached to hull centre");
            hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);
        }
        require(hwHodShipRead(data.data(),data.size(),&ship,error,sizeof(error))!=0,error);
        {
            int meshKey=0,shipA=0,shipB=0;
            auto *asset=hwRmVisualAssetCreate(data.data(),data.size(),nullptr,0,error,sizeof(error));
            require(asset!=nullptr,error);
            require(meshNativeVisualRegister(&meshKey,asset,2,0)!=0,"native mesh registration failed");
            require(!meshNativeVisualRegister(&meshKey,asset,2,0),"duplicate native mesh registration accepted");
            auto *a=meshNativeVisualAcquire(&meshKey,&shipA),*b=meshNativeVisualAcquire(&meshKey,&shipB);
            auto tokenA=meshNativeVisualInstanceToken(a),tokenB=meshNativeVisualInstanceToken(b);
            require(tokenA&&tokenB&&tokenA!=tokenB,"different ships shared a native instance token");
            require(a && b && a!=b,"native ship registry merged ship instances");
            require(meshNativeVisualAcquire(&meshKey,&shipA)==a,"save-load binding reconstruction duplicated visual state");
            float scale=0;unsigned lod=99;
            require(meshNativeVisualParameters(&meshKey,&scale,&lod) && scale==2 && lod==0,"native mesh parameters lost");
            meshNativeVisualReleaseShip(&shipA);meshNativeVisualReleaseShip(&shipA);
            require(!meshNativeVisualFindInstance(&shipA,tokenA,&scale),"deleted instance token remained valid");
            std::vector<HwRmSubsystemDraw> stillAlive;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(b),0,scale,nullptr,nullptr,capture,&stillAlive,error,sizeof(error))!=0,error);
            require(stillAlive.size()==3,"ship deletion invalidated shared asset");
            auto* replacement=meshNativeVisualAcquire(&meshKey,&shipA);
            require(replacement!=nullptr&&meshNativeVisualInstanceToken(replacement)!=tokenA,"reused Ship address retained deleted visual state");
            meshNativeVisualReleaseMesh(&meshKey);meshNativeVisualReleaseMesh(&meshKey);
            require(!meshNativeVisualAcquire(&meshKey,&shipB),"deleted mesh retained stale visual instance");
            require(!meshNativeVisualParameters(&meshKey,&scale,&lod),"deleted mesh retained asset ownership");
            meshNativeVisualReleaseShip(&shipA);meshNativeVisualReleaseShip(&shipB);
        }
        require(ship.vertexCount==9 && ship.triangleCount==3,"counts");
        require(ship.vertices[1].position[1]<-2.8f,"axis/scale");
        require(ship.triangles[2].material==0,"badge hull retained"); hwHodShipFree(&ship);hwHodShipFree(&ship);
        for(size_t size=0;size<data.size();++size) {
            require(!hwHodShipRead(data.data(),size,&ship,error,sizeof(error)),"accepted truncated HOD");
            require(!ship.vertices && !ship.triangles,"failure leaked result");
        }
        data=sample(true);require(!hwHodShipRead(data.data(),data.size(),&ship,error,sizeof(error)),"accepted bad index");
        require(!hwHodShipRead(nullptr,100,&ship,error,sizeof(error)),"accepted null input");
        {
            Bytes dock;le(dock,1);str(dock,"FighterIn");str(dock,"world");
            for(unsigned flag:{6u,10u,1u,0u,0u}) le(dock,flag);
            str(dock,"Fighter, Corvette");le(dock,0);str(dock,"FighterOut");le(dock,1);
            f(dock,12);f(dock,34);f(dock,56);
            for(unsigned i=0;i<9;++i) f(dock,i%4==0?1.f:0.f);
            f(dock,3);f(dock,100);le(dock,7);le(dock,8);
            HwRmHodScene *docking=nullptr;
            auto readDock=[&](const Bytes& payload) {
                auto fixture=sample(false,false,false,false,0xffffffff,&payload);
                return hwRmHodSceneRead(fixture.data(),fixture.size(),&docking,error,sizeof(error));
            };
            require(readDock(dock)!=0,error);
            const auto *path=hwRmHodSceneDockPath(docking,0);
            require(hwRmHodSceneDockPathCount(docking)==1 && path && path->pointCount==1 &&
                std::strcmp(path->linkedPath,"FighterOut")==0 && path->flags[1]==10 &&
                path->points[0].position[1]==34 && path->points[0].flags[1]==8,
                "docking metadata lost or transformed");
            {
                auto fixture=sample(false,false,false,false,0xffffffff,&dock);
                auto *asset=hwRmVisualAssetCreate(fixture.data(),fixture.size(),nullptr,0,error,sizeof(error));
                require(asset!=nullptr,error);
                auto *instance=hwRmVisualInstanceCreate(asset);float position[3]={};
                require(instance && hwRmVisualInstanceDockEndpoint(instance,"FighterIn",2,position),"native docking endpoint unavailable");
                require(position[0]==68 && position[1]==-24 && position[2]==112,"native endpoint basis/scale incorrect");
                require(!hwRmVisualInstanceDockEndpoint(instance,"FighterOut",2,position),"exit path accepted as latch");
                require(!hwRmVisualInstanceDockEndpoint(instance,"FighterIn",-1,position),"invalid native scale accepted");
                hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);
            }
            require(!hwRmHodSceneDockPath(docking,1) && !hwRmHodSceneDockPath(nullptr,0),"dock lookup bounds");
            hwRmHodSceneFree(docking);docking=nullptr;
            for(size_t length=0;length<dock.size();++length) {
                Bytes truncated(dock.begin(),dock.begin()+length);
                require(!readDock(truncated) && !docking,"truncated docking metadata published");
            }
            Bytes excessive=dock;for(unsigned i=0;i<4;++i) excessive[i]=255;
            require(!readDock(excessive) && !docking,"unbounded docking paths");
            Bytes tail=dock;le(tail,0);
            require(!readDock(tail) && !docking,"unconsumed docking metadata");
        }
        {
            auto exportHierarchy=[](float innerOffset,bool ordinaryDuplicate=false,bool collision=false){
                Bytes hierarchy;le(hierarchy,unsigned(collision?-5:-4));
                auto joint=[&](const char* name,const char* parent,float x){str(hierarchy,name);str(hierarchy,parent);f(hierarchy,x);for(unsigned i=1;i<9;++i)f(hierarchy,0);};
                joint("Root","",0);joint("HeavyInterceptor","Root",0);
                joint("Nozzle","Root",10);joint("Nozzle",ordinaryDuplicate?"Root":"Nozzle",innerOffset);
                if(collision)joint("Nozzle_ExportHelper","Root",0);
                return hierarchy;
            };
            auto hierarchy=exportHierarchy(0);auto fixture=sample(false,false,false,false,0xffffffff,nullptr,&hierarchy);
            auto* asset=hwRmVisualAssetCreate(fixture.data(),fixture.size(),nullptr,0,error,sizeof(error));require(asset!=nullptr,error);
            auto* instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"neutral helper instance failed");
            float anchor[16],helper[16];
            require(hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(instance),0,"Nozzle",1,nullptr,nullptr,anchor,error,sizeof(error))!=0,error);
            require(hwRmSubsystemJointTransform(hwRmVisualInstanceSubsystem(instance),0,"Nozzle_ExportHelper",1,nullptr,nullptr,helper,error,sizeof(error))!=0,error);
            require(std::abs(anchor[13]+10)<.00001f,"neutral anchor lost RM-to-HW1 basis");
            for(unsigned i=0;i<16;++i)require(std::abs(anchor[i]-helper[i])<.00001f,"neutral helper changed its source parent frame");
            hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);
            for(unsigned invalid=0;invalid<3;++invalid){
                hierarchy=exportHierarchy(invalid==0?1.f:0.f,invalid==1,invalid==2);
                fixture=sample(false,false,false,false,0xffffffff,nullptr,&hierarchy);
                HwRmHodScene* bad=nullptr;
                if(invalid==1){
                    require(hwRmHodSceneRead(fixture.data(),fixture.size(),&bad,error,sizeof(error))!=0,error);
                    require(!std::strcmp(hwRmHodSceneJoint(bad,2)->name,"Nozzle") &&
                        !std::strcmp(hwRmHodSceneJoint(bad,3)->name,"Nozzle"),"ordinary duplicates were rewritten as export helpers");
                    hwRmHodSceneFree(bad);
                }else require(!hwRmHodSceneRead(fixture.data(),fixture.size(),&bad,error,sizeof(error)) && !bad,"unsafe duplicate hierarchy was normalized");
            }
        }
        HwRmHodScene *scene=nullptr;data=sample();
        require(hwRmHodSceneRead(data.data(),data.size(),&scene,error,sizeof(error))!=0,error);
        require(hwRmHodSceneJointCount(scene)==2 && hwRmHodSceneMeshCount(scene)==1 &&
                hwRmHodSceneMaterialCount(scene)==3,"fleet scene counts");
        require(hwRmHodSceneMesh(scene,0)->parts[0].vertices[1].position[0]==1.f,"fleet reader applied interceptor scale");
        require(hwRmHodSceneMesh(scene,0)->parts[2].material==2,"fleet reader discarded badge material");
        require(!hwRmHodSceneMesh(scene,99) && !hwRmHodSceneJoint(scene,99),"fleet unchecked lookup");
        auto *layer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(layer!=nullptr,error);
        require(!hwRmSubsystemLayerAdd(layer,"Bad","Missing",HW_RM_TURRET,scene,error,sizeof(error)),"accepted missing subsystem anchor");
        require(hwRmSubsystemLayerCount(layer)==1,"failed attachment changed layer");
        require(hwRmSubsystemLayerAdd(layer,"Turret0","Root",HW_RM_TURRET,scene,error,sizeof(error))!=0,error);
        require(hwRmSubsystemLayerAdd(layer,"Turret1","Root",HW_RM_TURRET,scene,error,sizeof(error))!=0,error);
        require(!hwRmSubsystemLayerAdd(layer,"Turret0","Root",HW_RM_TURRET,scene,error,sizeof(error)),"accepted duplicate subsystem slot");
        auto *first=hwRmSubsystemInstanceCreate(layer),*second=hwRmSubsystemInstanceCreate(layer);
        require(first && second,"subsystem instance allocation");
        require(!hwRmSubsystemLayerAdd(layer,"Late","Root",HW_RM_ENGINE,scene,error,sizeof(error)),"changed live subsystem definition");
        std::vector<HwRmSubsystemDraw> draws;float offset=17;
        require(hwRmSubsystemVisit(first,0,2,pose,&offset,capture,&draws,error,sizeof(error))!=0,error);
        require(draws.size()==9,"missing subsystem/material draw");
        require(draws[0].matrix[0]==0 && draws[0].matrix[1]==-2 && draws[0].matrix[4]==2,"incorrect vertex basis/scale");
        require(draws[3].component==1 && draws[3].matrix[12]==17 && draws[6].matrix[12]==0,"shared models share turret poses");
        float marker[16];
        require(hwRmSubsystemJointTransform(first,1,"Root",2,pose,&offset,marker,error,sizeof(error))!=0,error);
        require(marker[12]==17 && marker[0]==1 && marker[5]==1,"marker lookup applied raw vertex basis/scale twice");
        const auto preserved=std::vector<float>(marker,marker+16);
        require(!hwRmSubsystemJointTransform(first,1,"Missing",2,nullptr,nullptr,marker,error,sizeof(error)),"missing marker guessed a position");
        require(std::memcmp(marker,preserved.data(),sizeof(marker))==0,"failed marker lookup overwrote output");
        require(hwRmSubsystemJointTransform(second,1,"Root",2,nullptr,nullptr,marker,error,sizeof(error))!=0,error);
        require(marker[12]==0,"marker transform leaked between instances");
        require(hwRmSubsystemSetJointControl(first,1,"Root",1.57079632679f,0,5,error,sizeof(error))!=0,error);
        require(hwRmSubsystemJointTransform(first,1,"Root",2,nullptr,nullptr,marker,error,sizeof(error))!=0,error);
        require(std::abs(marker[5])<1e-6 && std::abs(marker[6]-1)<1e-6 &&
                std::abs(marker[13]-5)<1e-6,"native yaw/recoil lost its authored pivot frame");
        require(!hwRmSubsystemSetJointControl(first,1,"Missing",0,0,0,error,sizeof(error)),"joint control guessed missing pivot");
        require(!hwRmSubsystemSetJointControl(first,1,"Root",0,0,-1,error,sizeof(error)),"negative recoil accepted");
        require(hwRmSubsystemJointTransform(second,1,"Root",2,nullptr,nullptr,marker,error,sizeof(error))!=0,error);
        require(marker[5]==1 && marker[13]==0,"native control leaked between instances");
        require(hwRmSubsystemClearJointControls(first,1)!=0,"joint control clear failed");
        require(hwRmSubsystemJointTransform(first,1,"Root",2,nullptr,nullptr,marker,error,sizeof(error))!=0,error);
        require(marker[5]==1 && marker[13]==0,"joint controls didn't restore rest pivot");
        draws.clear();require(hwRmSubsystemVisit(second,4,2,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
        require(draws.size()==9 && draws[3].matrix[12]==0,"pose leaked between ships");
        require(hwRmSubsystemSetVisible(first,1,0)!=0,"subsystem visibility");
        draws.clear();require(hwRmSubsystemVisit(first,0,2,nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
        require(draws.size()==6,"hidden subsystem still drawn");
        draws.clear();require(!hwRmSubsystemVisit(first,0,-1,nullptr,nullptr,capture,&draws,error,sizeof(error)),"accepted invalid subsystem scale");
        require(draws.empty(),"bad pose submitted partial ship");
        // Visibility does not bypass pose validation, or hide corrupted state.
        offset=std::numeric_limits<float>::quiet_NaN();
        require(!hwRmSubsystemVisit(first,0,2,pose,&offset,capture,&draws,error,sizeof(error)),"accepted NaN subsystem pose");
        require(draws.empty(),"NaN pose submitted partial ship");
        hwRmSubsystemLayerFree(layer);require(!hwRmSubsystemInstanceCreate(layer),"instance created after definition released");
        hwRmSubsystemInstanceFree(first);hwRmSubsystemInstanceFree(second);
        hwRmHodSceneFree(scene);scene=nullptr;
        for(size_t size=0;size<data.size();++size) {
            require(!hwRmHodSceneRead(data.data(),size,&scene,error,sizeof(error)),"accepted truncated fleet HOD");
            require(!scene,"fleet failure published partial data");
        }
        data=sample(true);require(!hwRmHodSceneRead(data.data(),data.size(),&scene,error,sizeof(error)),"fleet accepted bad index");
        data=sample(false,true);
        require(hwRmHodSceneRead(data.data(),data.size(),&scene,error,sizeof(error))!=0,error);
        require(hwRmHodSceneTextureCount(scene)==1 && !hwRmHodSceneTexture(scene,1),"texture table bounds");
        const auto *texture=hwRmHodSceneTexture(scene,0);
        require(std::strcmp(texture->format,"8888")==0 && texture->mipCount==1 && texture->mips[0].size==4,"texture layout lost");
        const auto *material=hwRmHodSceneMaterial(scene,0);
        require(material->parameterCount==2 && material->parameters[0].texture==0 &&
            std::strcmp(material->parameters[0].name,"$diffuse")==0 && material->parameters[1].value[2]==.75f,"material parameter lost");
        const unsigned char *pixels=nullptr,*again=nullptr;size_t pixelSize=0,againSize=0;
        require(hwRmHodSceneTextureData(scene,&pixels,&pixelSize,error,sizeof(error)) && pixelSize==4 && pixels[1]==20,"texture pool decode");
        require(hwRmHodSceneTextureData(scene,&again,&againSize,error,sizeof(error)) && again==pixels && againSize==pixelSize,"texture data decoded twice");
        HwRmHodTextureView nativeTexture{};
        require(hwRmHodMaterialTexture(scene,0,"$diffuse",0,&nativeTexture,error,sizeof(error)),"native material texture resolution failed");
        require(nativeTexture.bytes==pixels&&nativeTexture.size==4&&!strcmp(nativeTexture.format,"8888"),"native texture bytes changed");
        require(!hwRmHodMaterialTexture(scene,0,"$diffuse",1,&nativeTexture,error,sizeof(error))&&!nativeTexture.bytes,"bad mip retained stale native texture");
        require(!hwRmHodMaterialTexture(scene,0,"$missing",0,&nativeTexture,error,sizeof(error)),"missing material parameter accepted");
        require(!hwRmHodMaterialTexture(scene,999,"$diffuse",0,&nativeTexture,error,sizeof(error)),"invalid material accepted");
        hwRmHodSceneFree(scene);scene=nullptr;
        {
            auto* direct=hwRmVisualAssetCreate(data.data(),data.size(),nullptr,0,error,sizeof(error));
            require(direct!=nullptr,"direct texture asset creation failed");
            require(hwRmVisualAssetMaterial(direct,0,0)!=nullptr,"direct material absent");
            require(hwRmVisualAssetTexture(direct,0,0,"$diffuse",0,&nativeTexture,error,sizeof(error))&&nativeTexture.bytes[1]==20,"asset texture resolution changed bytes");
            require(!hwRmVisualAssetTexture(direct,1,0,"$diffuse",0,&nativeTexture,error,sizeof(error))&&!nativeTexture.bytes,"missing component texture accepted");
            hwRmVisualAssetFree(direct);
        }
        if(argc>1) {
            std::ifstream file(argv[1],std::ios::binary);require(bool(file),"source HOD missing");
            Bytes real((std::istreambuf_iterator<char>(file)),{});
            require(hwHodShipRead(real.data(),real.size(),&ship,error,sizeof(error))!=0,error);
            require(ship.vertexCount==4680 && ship.triangleCount==1560,"source geometry mismatch");
            std::printf("Source HOD verified: %u vertices, %u triangles\n",ship.vertexCount,ship.triangleCount);hwHodShipFree(&ship);
        }
        if(argc>2) {
            unsigned hods=0,mads=0;
            for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[2])) {
                if(!entry.is_regular_file() || entry.path().extension()!=".hod") continue;
                // Background scenery is not a fleet ship replacement.
                if(entry.path().stem()=="kus_headshotasteroid") continue;
                std::ifstream file(entry.path(),std::ios::binary);
                Bytes real((std::istreambuf_iterator<char>(file)),{});
                if(!hwRmHodSceneRead(real.data(),real.size(),&scene,error,sizeof(error))) {
                    std::fprintf(stderr,"%s: %s\n",entry.path().filename().string().c_str(),error);
                    throw std::runtime_error("fleet source scene rejected");
                }
                std::printf("%s: %u mesh/LOD records, %u joints; auxiliary %zu/%zu bytes\n",
                    entry.path().filename().string().c_str(),hwRmHodSceneMeshCount(scene),hwRmHodSceneJointCount(scene),
                    hwRmHodSceneAuxiliaryVertexBytes(scene),hwRmHodSceneAuxiliaryIndexBytes(scene));
                for(unsigned di=0;di<hwRmHodSceneDockPathCount(scene);++di) {
                    const auto *path=hwRmHodSceneDockPath(scene,di);
                    require(path && path->pointCount && path->points,"empty source docking path");
                    std::printf("  dock %s parent=%s points=%u compatible=%s link=%s\n",
                        path->name,path->parent,path->pointCount,path->compatibleShips,path->linkedPath);
                }
                require(hwRmHodSceneTextureData(scene,&pixels,&pixelSize,error,sizeof(error))!=0,error);
                size_t end=0;
                for(unsigned ti=0;ti<hwRmHodSceneTextureCount(scene);++ti) {
                    const auto *image=hwRmHodSceneTexture(scene,ti);
                    for(unsigned mi=0;mi<image->mipCount;++mi) {
                        const auto &mip=image->mips[mi];
                        require(mip.offset==end && mip.size<=pixelSize-end,"texture mip pool span mismatch");end+=mip.size;
                    }
                }
                require(end==pixelSize,"unclaimed texture pool bytes");
                auto madPath=entry.path();madPath.replace_extension(".mad");
                if(std::filesystem::exists(madPath)) {
                    std::ifstream madFile(madPath,std::ios::binary);
                    Bytes madBytes((std::istreambuf_iterator<char>(madFile)),{});
                    HwRmMad *mad=nullptr;
                    require(hwRmMadRead(madBytes.data(),madBytes.size(),&mad,error,sizeof(error))!=0,error);
                    std::vector<const char*> joints;
                    for(unsigned i=0;i<hwRmHodSceneJointCount(scene);++i) joints.push_back(hwRmHodSceneJoint(scene,i)->name);
                    const bool valid=hwRmMadValidateJoints(mad,joints.data(),unsigned(joints.size()),error,sizeof(error))!=0;
                    if(entry.path().stem()=="kus_heavycorvette") {
                        // The shipped Make_Ambassador companion references
                        // Weapon_Gun*_Position joints absent from this HOD.
                        require(!valid,"missing heavy-corvette joints went unnoticed");
                        std::printf("  ACTIVATION BLOCKED: %s\n",error);
                        // Exercise routing with a synthetic target hierarchy,
                        // not a claim that these frames fit the real gun HODs.
                        auto fixture=sample();HwRmHodScene *targetScene=nullptr;
                        require(hwRmHodSceneRead(fixture.data(),fixture.size(),&targetScene,error,sizeof(error))!=0,error);
                        auto *routingLayer=hwRmSubsystemLayerCreate(targetScene,error,sizeof(error));
                        require(routingLayer!=nullptr,error);
                        require(hwRmSubsystemLayerAdd(routingLayer,"Gun0","Root",HW_RM_TURRET,targetScene,error,sizeof(error))!=0,error);
                        require(hwRmSubsystemLayerAdd(routingLayer,"Gun1","Root",HW_RM_TURRET,targetScene,error,sizeof(error))!=0,error);
                        auto *routed=hwRmSubsystemInstanceCreate(routingLayer);
                        auto *separate=hwRmSubsystemInstanceCreate(routingLayer);
                        require(routed && separate,"routed animation instances");
                        HwRmAnimationTarget targets[]={{"Weapon_Gun0_Position",1,"HeavyInterceptor"},
                            {"Weapon_Gun1_Position",2,"HeavyInterceptor"}};
                        unsigned clipIndex=0;double phase=.5;
                        require(hwRmSubsystemSampleMappedAnimations(routed,0,mad,&clipIndex,&phase,1,targets,2,error,sizeof(error))!=0,error);
                        float before[2][16],after[2][16],rest[16];
                        for(unsigned i=0;i<2;++i) {
                            require(hwRmSubsystemJointTransform(routed,i+1,"HeavyInterceptor",1,nullptr,nullptr,before[i],error,sizeof(error))!=0,error);
                            require(hwRmSubsystemJointTransform(separate,i+1,"HeavyInterceptor",1,nullptr,nullptr,rest,error,sizeof(error))!=0,error);
                            require(rest[12]==0 && rest[13]==0 && rest[14]==0,"animation crossed ship instances");
                        }
                        targets[1].targetJoint="Missing";phase=1;
                        require(!hwRmSubsystemSampleMappedAnimations(routed,0,mad,&clipIndex,&phase,1,targets,2,error,sizeof(error)),"bad later animation target accepted");
                        for(unsigned i=0;i<2;++i) {
                            require(hwRmSubsystemJointTransform(routed,i+1,"HeavyInterceptor",1,nullptr,nullptr,after[i],error,sizeof(error))!=0,error);
                            require(std::memcmp(before[i],after[i],sizeof(before[i]))==0,"failed routing changed a component pose");
                        }
                        targets[1].targetJoint="HeavyInterceptor";targets[1].sourceJoint=targets[0].sourceJoint;
                        require(!hwRmSubsystemSampleMappedAnimations(routed,0,mad,&clipIndex,&phase,1,targets,2,error,sizeof(error)),"duplicate animation routing accepted");
                        hwRmSubsystemInstanceFree(routed);hwRmSubsystemInstanceFree(separate);
                        hwRmSubsystemLayerFree(routingLayer);hwRmHodSceneFree(targetScene);
                        HwRmAnimationTarget hullTargets[]={{"Weapon_Gun0_Position",0,"Hardpoint_Gun0_Position"},
                            {"Weapon_Gun1_Position",0,"Hardpoint_Gun1_Position"}};
                        auto *aliased=hwRmVisualAssetCreateWithAnimationTargets(real.data(),real.size(),
                            madBytes.data(),madBytes.size(),hullTargets,2,error,sizeof(error));
                        require(aliased!=nullptr,error);
                        auto model=readBytes(std::filesystem::path(argv[2]).parent_path()/
                            "subsystem/kus_hvcorvette1/kus_hvcorvette1.hod");
                        for(unsigned i=0;i<2;++i) {
                            const auto slot="Weapon_Gun"+std::to_string(i);
                            const auto anchor="Hardpoint_Gun"+std::to_string(i)+"_Position";
                            require(hwRmVisualAssetAttach(aliased,"shared-gun",slot.c_str(),anchor.c_str(),
                                HW_RM_TURRET,model.data(),model.size(),error,sizeof(error))!=0,error);
                        }
                        require(meshNativeVisualBindClassicEvents(aliased,"HeavyCorvette",error,sizeof(error))!=0,error);
                        require(meshNativeVisualBindClassicGuns(aliased,"HeavyCorvette",error,sizeof(error))!=0,error);
                        auto *moving=hwRmVisualInstanceCreate(aliased),*stationary=hwRmVisualInstanceCreate(aliased);
                        require(moving && stationary,"aliased heavy corvette instances");
                        float original[2][16],opened[2][16],closed[16];
                        for(unsigned i=0;i<2;++i)
                            require(hwRmVisualInstanceGunMuzzle(stationary,i,1,original[i],error,sizeof(error))!=0,error);
                        require(hwRmVisualInstanceSampleEvent(moving,"Make_Ambassador",0,error,sizeof(error))!=0,error);
                        for(unsigned i=0;i<2;++i) {
                            require(hwRmVisualInstanceGunMuzzle(moving,i,1,closed,error,sizeof(error))!=0,error);
                            for(unsigned k=0;k<16;++k)
                                require(std::abs(closed[k]-original[i][k])<.00001f,"MAD alias initial pose mismatches hull anchor frame");
                        }
                        require(hwRmVisualInstanceSampleEvent(moving,"Make_Ambassador",1,error,sizeof(error))!=0,error);
                        for(unsigned i=0;i<2;++i) {
                            require(hwRmVisualInstanceGunMuzzle(moving,i,1,opened[i],error,sizeof(error))!=0,error);
                            require(std::abs(opened[i][12]-original[i][12]-2.568731904)<.00001f,
                                "MAD alias failed to move attachment once in hull frame");
                            require(hwRmVisualInstanceGunMuzzle(stationary,i,1,closed,error,sizeof(error))!=0,error);
                            require(std::memcmp(closed,original[i],sizeof(closed))==0,"MAD alias affected another ship");
                        }
                        require(hwRmVisualInstanceSampleEvent(moving,"Make_Ambassador",0,error,sizeof(error))!=0,error);
                        hwRmVisualAssetFree(aliased);
                        require(hwRmVisualInstanceGunMuzzle(moving,0,1,closed,error,sizeof(error))!=0,error);
                        hwRmVisualInstanceFree(moving);hwRmVisualInstanceFree(stationary);
                        hullTargets[1].targetJoint="Missing";
                        require(!hwRmVisualAssetCreateWithAnimationTargets(real.data(),real.size(),madBytes.data(),
                            madBytes.size(),hullTargets,2,error,sizeof(error)),"invalid MAD alias accepted");
                        std::printf("  Heavy corvette explicit hull-anchor animation aliases passed\n");
                    } else require(valid,error);
                    {
                        auto *owned=hwRmVisualAssetCreate(real.data(),real.size(),madBytes.data(),madBytes.size(),error,sizeof(error));
                        if(!valid) require(!owned,"owned asset bypassed missing native animation joint");
                        else {
                            require(owned!=nullptr,error);
                            struct EventCheck {const char *source,*type,*events[4];};
                            const EventCheck checks[]={
                                {"kus_resourcecontroller","ResourceController",{"open","close","damageclose",nullptr}},
                                {"kus_defender","HeavyDefender",{"Arms_OPEN","Arms_CLOSE","Arms_Damaged_OPEN","Arms_Damaged_CLOSE"}},
                                {"kus_lightdefender","LightDefender",{"Arms_OPEN","Arms_CLOSE",nullptr,nullptr}},
                                {"kus_gravwellgenerator","GravWellGenerator",{"arms_open","arms_close","damage_open","damage_close"}},
                                {"kus_probe","Probe",{"opening","closing","damage_open","damage_close"}},
                                {"kus_proximitysensor","ProximitySensor",{"open","close","damage_open","damage_close"}},
                                {"kus_sensorarray","SensorArray",{"Ears_Open","Ears_Close",nullptr,nullptr}}
                            };
                            for(const auto& check:checks) if(entry.path().stem()==check.source) {
                                require(meshNativeVisualBindClassicEvents(owned,check.type,error,sizeof(error))!=0,error);
                                auto *eventInstance=hwRmVisualInstanceCreate(owned);require(eventInstance!=nullptr,"native event instance missing");
                                for(const auto *event:check.events) if(event)
                                    for(double phase:{0.,.25,.5,.75,1.,.5,0.})
                                        require(hwRmVisualInstanceSampleEvent(eventInstance,event,phase,error,sizeof(error))!=0,error);
                                hwRmVisualInstanceFree(eventInstance);
                            }
                            if(entry.path().stem()=="kus_supportfrigate") {
                                require(meshNativeVisualBindClassicEvents(owned,"AdvanceSupportFrigate",error,sizeof(error))!=0,error);
                                auto *arms=hwRmVisualInstanceCreate(owned);require(arms!=nullptr,"support event instance missing");
                                for(const char *event:{"open","close","damageclose"})
                                    for(double phase:{0.,.5,1.})
                                        require(hwRmVisualInstanceSampleEvent(arms,event,phase,error,sizeof(error))!=0,error);
                                hwRmVisualInstanceFree(arms);
                            }
                            if(entry.path().stem()=="kus_mothership") {
                                const char *open[]={"Open"},*close[]={"Close"};
                                require(hwRmVisualAssetBindAnimation(owned,"HangarDoor_OPEN",open,1,error,sizeof(error))!=0,error);
                                require(hwRmVisualAssetBindAnimation(owned,"HangarDoor_CLOSE",close,1,error,sizeof(error))!=0,error);
                                require(!hwRmVisualAssetBindAnimation(owned,"HangarDoor_OPEN",open,1,error,sizeof(error)),"duplicate gameplay event accepted");
                                auto *ownedInstance=hwRmVisualInstanceCreate(owned);require(ownedInstance!=nullptr,"owned animation instance allocation failed");
                                require(!hwRmVisualAssetBindAnimation(owned,"Late",open,1,error,sizeof(error)),"event configuration changed on live ship");
                                for(double phase:{0.,.25,.5,.75,1.,.5,0.})
                                    require(hwRmVisualInstanceSampleEvent(ownedInstance,"HangarDoor_OPEN",phase,error,sizeof(error))!=0,error);
                                require(!hwRmVisualInstanceSampleEvent(ownedInstance,"SPIN",.5,error,sizeof(error)),"unbound visual event auto-started");
                                hwRmVisualAssetFree(owned);owned=nullptr;
                                require(hwRmVisualInstanceSampleEvent(ownedInstance,"HangarDoor_CLOSE",.5,error,sizeof(error))!=0,error);
                                hwRmVisualInstanceFree(ownedInstance);
                            }
                            hwRmVisualAssetFree(owned);
                        }
                    }
                    auto *animatedLayer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));
                    require(animatedLayer!=nullptr,error);
                    auto *animated=hwRmSubsystemInstanceCreate(animatedLayer);
                    auto *unchanged=hwRmSubsystemInstanceCreate(animatedLayer);
                    require(animated && unchanged,"animated instance allocation");
                    std::vector<HwRmSubsystemDraw> restDraws,animatedDraws,otherDraws;
                    require(hwRmSubsystemVisit(unchanged,0,2.5f,nullptr,nullptr,capture,&restDraws,error,sizeof(error))!=0,error);
                    for(unsigned ci=0;ci<hwRmMadClipCount(mad);++ci) {
                        const auto *clip=hwRmMadClip(mad,ci);if(!clip->playable) continue;
                        if(!valid) {
                            require(!hwRmSubsystemSampleAnimation(animated,0,mad,ci,.5,error,sizeof(error)),"missing animation joint activated");
                            continue;
                        }
                        std::vector<HwRmSubsystemDraw> endpoint;
                        for(double phase:{0.,.25,.5,.75,1.,.5,0.}) {
                            require(hwRmSubsystemSampleAnimation(animated,0,mad,ci,phase,error,sizeof(error))!=0,error);
                            animatedDraws.clear();
                            require(hwRmSubsystemVisit(animated,0,2.5f,nullptr,nullptr,capture,&animatedDraws,error,sizeof(error))!=0,error);
                            require(animatedDraws.size()==restDraws.size(),"animation dropped a material/mesh");
                            if(endpoint.empty()) endpoint=animatedDraws;
                            else if(phase==0) for(size_t di=0;di<endpoint.size();++di)
                                require(std::memcmp(endpoint[di].matrix,animatedDraws[di].matrix,sizeof(endpoint[di].matrix))==0,"reversed animation failed to return to endpoint");
                        }
                    }
                    require(!hwRmSubsystemSampleAnimation(animated,0,mad,0,1.01,error,sizeof(error)),"accepted out-of-range animation phase");
                    if(valid) {
                        for(unsigned ci=0;ci<hwRmMadClipCount(mad);++ci) {
                            const auto *clip=hwRmMadClip(mad,ci);if(!clip->playable) continue;
                            unsigned clips[2]={ci,ci};double phases[2]={.5,.5};
                            require(hwRmSubsystemSampleAnimations(animated,0,mad,clips,phases,2,error,sizeof(error))!=0,error);
                            std::vector<HwRmSubsystemDraw> low,high;
                            require(hwRmSubsystemSampleAnimation(animated,0,mad,ci,0,error,sizeof(error))!=0,error);
                            require(hwRmSubsystemVisit(animated,0,2.5f,nullptr,nullptr,capture,&low,error,sizeof(error))!=0,error);
                            require(hwRmSubsystemSampleAnimation(animated,0,mad,ci,1,error,sizeof(error))!=0,error);
                            require(hwRmSubsystemVisit(animated,0,2.5f,nullptr,nullptr,capture,&high,error,sizeof(error))!=0,error);
                            bool moving=false;
                            for(size_t di=0;di<low.size();++di)
                                moving |= std::memcmp(low[di].matrix,high[di].matrix,sizeof(low[di].matrix))!=0;
                            if(moving) {
                                phases[0]=0;phases[1]=1;
                                require(!hwRmSubsystemSampleAnimations(animated,0,mad,clips,phases,2,error,sizeof(error)),"conflicting overlapping clips silently blended");
                            }
                            phases[0]=.5;phases[1]=.5;
                            require(hwRmSubsystemSampleAnimations(animated,0,mad,clips,phases,2,error,sizeof(error))!=0,error);
                            phases[1]=1.01;
                            std::vector<HwRmSubsystemDraw> before,after;
                            require(hwRmSubsystemVisit(animated,0,2.5f,nullptr,nullptr,capture,&before,error,sizeof(error))!=0,error);
                            require(!hwRmSubsystemSampleAnimations(animated,0,mad,clips,phases,2,error,sizeof(error)),"partial multi-clip activation accepted invalid phase");
                            require(hwRmSubsystemVisit(animated,0,2.5f,nullptr,nullptr,capture,&after,error,sizeof(error))!=0,error);
                            require(before.size()==after.size(),"failed multi-clip sample changed draw count");
                            for(size_t di=0;di<before.size();++di)
                                require(std::memcmp(before[di].matrix,after[di].matrix,sizeof(before[di].matrix))==0,"failed multi-clip sample published partial pose");
                        }
                    }
                    if(entry.path().stem()=="kus_supportfrigate") {
                        for(const char *state:{"open","close"}) {
                            unsigned clips[4];double phases[4];
                            for(unsigned arm=0;arm<4;++arm) {
                                const std::string name=std::string(state)+std::to_string(arm+1);
                                bool found=false;
                                for(unsigned ci=0;ci<hwRmMadClipCount(mad);++ci)
                                    if(name==hwRmMadClip(mad,ci)->name) {clips[arm]=ci;found=true;break;}
                                require(found,"missing native support arm clip");
                            }
                            for(double phase:{0.,.25,.5,.75,1.,.5,0.}) {
                                for(double& value:phases) value=phase;
                                require(hwRmSubsystemSampleAnimations(animated,0,mad,clips,phases,4,error,sizeof(error))!=0,error);
                            }
                        }
                        std::puts("Support frigate collective four-arm native clips passed");
                    }
                    require(hwRmSubsystemVisit(unchanged,0,2.5f,nullptr,nullptr,capture,&otherDraws,error,sizeof(error))!=0,error);
                    require(restDraws.size()==otherDraws.size(),"animation mutated another ship");
                    for(size_t di=0;di<restDraws.size();++di)
                        require(std::memcmp(restDraws[di].matrix,otherDraws[di].matrix,sizeof(restDraws[di].matrix))==0,"animation pose leaked between ships");
                    require(hwRmSubsystemClearAnimation(animated,0)!=0,"animation clear failed");
                    animatedDraws.clear();
                    require(hwRmSubsystemVisit(animated,0,2.5f,nullptr,nullptr,capture,&animatedDraws,error,sizeof(error))!=0,error);
                    for(size_t di=0;di<restDraws.size();++di)
                        require(std::memcmp(restDraws[di].matrix,animatedDraws[di].matrix,sizeof(restDraws[di].matrix))==0,"animation clear failed to restore rest pose");
                    hwRmSubsystemInstanceFree(animated);hwRmSubsystemInstanceFree(unchanged);
                    hwRmSubsystemLayerFree(animatedLayer);
                    hwRmMadFree(mad);++mads;
                }
                hwRmHodSceneFree(scene);scene=nullptr;++hods;
            }
            require(hods>=28 && mads==13,"incomplete local fleet validation");
            std::printf("Fleet source validated: %u HODs, %u MAD companions\n",hods,mads);
            auto root=std::filesystem::path(argv[2]);if(root.filename()=="ship") root=root.parent_path();
            const auto gunPath=root/"subsystem/kus_cruisergun1/kus_cruisergun1.hod";
            if(std::filesystem::exists(gunPath)) {
                auto *hull=readScene(root/"ship/kus_heavycruiser/kus_heavycruiser.hod");
                auto *gun=readScene(gunPath);
                auto *engine=readScene(root/"subsystem/kus_heavycruiserengine/kus_heavycruiserengine.hod");
                auto *realLayer=hwRmSubsystemLayerCreate(hull,error,sizeof(error));require(realLayer!=nullptr,error);
                require(hwRmSubsystemLayerAdd(realLayer,"Gun4","Hardpoint_Gun4_Position",HW_RM_TURRET,gun,error,sizeof(error))!=0,error);
                require(hwRmSubsystemLayerAdd(realLayer,"Gun6","Hardpoint_Gun6_Position",HW_RM_TURRET,gun,error,sizeof(error))!=0,error);
                require(hwRmSubsystemLayerAdd(realLayer,"Engine","Hardpoint_Engine_Position",HW_RM_ENGINE,engine,error,sizeof(error))!=0,error);
                auto *realInstance=hwRmSubsystemInstanceCreate(realLayer);require(realInstance!=nullptr,"real instance allocation");
                std::vector<HwRmSubsystemDraw> realDraws;
                require(hwRmSubsystemVisit(realInstance,4,2.5f,nullptr,nullptr,capture,&realDraws,error,sizeof(error))!=0,error);
                bool componentsSeen[4]={};float turretX[2]={},turretY[2]={},turretZ[2]={};
                for(const auto& record:realDraws) {
                    require(record.component<4 && record.part && record.material,"unqualified real draw");
                    for(float value:record.matrix) require(std::isfinite(value),"bad real attachment matrix");
                    componentsSeen[record.component]=true;
                    if(record.component==1 || record.component==2) {
                        const unsigned i=record.component-1;turretX[i]=record.matrix[12];turretY[i]=record.matrix[13];turretZ[i]=record.matrix[14];
                    }
                }
                for(unsigned ci=0;ci<3;++ci) require(componentsSeen[ci],"real visible source component not composed");
                require(!componentsSeen[3],"engine selection proxy emitted opaque geometry");
                float engineMarker[16];
                require(hwRmSubsystemJointTransform(realInstance,3,"Engine",2.5f,nullptr,nullptr,engineMarker,error,sizeof(error))!=0,error);
                require(std::abs(turretX[0]-turretX[1])+std::abs(turretY[0]-turretY[1])+std::abs(turretZ[0]-turretZ[1])>1,
                        "shared real turret model attached at one global position");
                hwRmSubsystemInstanceFree(realInstance);hwRmSubsystemLayerFree(realLayer);
                hwRmHodSceneFree(engine);hwRmHodSceneFree(gun);hwRmHodSceneFree(hull);
                std::puts("Real heavy cruiser hull + shared turrets + engine composition passed");
            }
        }
        if(argc>3) auditGunContracts(std::filesystem::path(argv[2]).parent_path(),argv[3]);
        if(argc>4) auditPackages(argv[4]);
        if(argc>5) {
            auto root=std::filesystem::path(argv[5]);auto manifest=readBytes(root/"visual.rmnative");
            auto *package=hwRmVisualPackageRead("Scaffold",manifest.data(),manifest.size(),packageFileRead,packageFileRelease,packageFixtureConfigure,&root,error,sizeof(error));
            require(package!=nullptr,error);
            auto *asset=hwRmVisualPackageTakeAsset(package);
            auto *instance=hwRmVisualInstanceCreate(asset);require(instance!=nullptr,"scaffold instance");
            for(unsigned lod=0;lod<4;++lod) {
                std::vector<HwRmSubsystemDraw> draws;
                require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),lod,hwRmVisualPackageScale(package),nullptr,nullptr,capture,&draws,error,sizeof(error))!=0,error);
                require(!draws.empty(),"scaffold LOD empty");
            }
            hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);hwRmVisualPackageFree(package);
            std::puts("Intact scaffold native package validated at LOD requests 0 through 3");
        }
        std::puts("Native interceptor HOD tests passed");return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
