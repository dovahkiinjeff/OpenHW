#include "RemasteredVisualPackage.h"
#include "OpenHWShipDocument.h"
#include "NativeVisualScope.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct AssetDelete {void operator()(HwRmVisualAsset *p) const {hwRmVisualAssetFree(p);}};
using Asset=std::unique_ptr<HwRmVisualAsset,AssetDelete>;
struct Attachment {std::string slot,anchor,path;unsigned role;};
struct Surface {unsigned component,material;std::string label;};
struct Marker {std::string semantic,slot,joint;};
void message(char *error,size_t size,const char *value) {if(error && size) std::snprintf(error,size,"%s",value);}
bool kushan(const char *name) {
    static const char *types[]={"AdvanceSupportFrigate","AttackBomber","Carrier","CloakedFighter",
        "CloakGenerator","CryoTray","DDDFrigate","Drone","GravWellGenerator","HeavyCorvette",
        "HeavyCruiser","HeavyDefender","HeavyInterceptor","IonCannonFrigate","LightCorvette",
        "LightDefender","LightInterceptor","MinelayerCorvette","MissileDestroyer","Mothership",
        "MultiGunCorvette","Probe","ProximitySensor","RepairCorvette","ResearchShip",
        "ResourceCollector","ResourceController","SalCapCorvette","SensorArray","StandardDestroyer",
        "StandardFrigate","TargetDrone"};
    if(name) for(const auto *type:types) if(std::strcmp(name,type)==0) return true;
    return false;
}
bool token(const std::string& value,bool path=false) {
    if(value.empty() || value.size()>255 || value.find("..")!=std::string::npos) return false;
    for(unsigned char c:value) if(!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
        (c>='0' && c<='9') || c=='_' || c=='-' || c=='.' || c=='@' || (path && c=='/'))) return false;
    return !path || (value.front()!='/' && value.back()!='/' && value.find("//")==std::string::npos);
}
unsigned integer(const std::string& text,unsigned maximum) {
    if(text.empty() || text.size()>8 || !std::all_of(text.begin(),text.end(),[](char c){return c>='0' && c<='9';}))
        throw std::runtime_error("invalid native package integer");
    unsigned value=unsigned(std::stoul(text));if(value>maximum) throw std::runtime_error("native package integer exceeds limit");return value;
}
struct Input {
    HwRmPackageReleaseFile release;void *user;const void *bytes=nullptr;size_t size=0;
    Input(HwRmPackageReadFile read,HwRmPackageReleaseFile release,void *user,const std::string& path,size_t cap)
        :release(release),user(user) {
        if(!token(path,true) || !read(user,path.c_str(),&bytes,&size) || !bytes || !size || size>cap) {
            if(bytes) release(user,bytes);bytes=nullptr;throw std::runtime_error("missing/oversized native package source: "+path);
        }
    }
    ~Input() {if(bytes) release(user,bytes);}
};
}
struct HwRmVisualPackage {Asset asset;float scale=0;std::vector<Surface> surfaces;};
extern "C" int hwRmShipDefinitionGameplay(const void *manifest,size_t size,const char *type,char *relative,size_t capacity) {
    if(!manifest||!size||size>128*1024||!type||!relative||!capacity)return 0;
    relative[0]=0;std::istringstream input(std::string(static_cast<const char*>(manifest),size));
    std::string line,identity,gameplay;bool foundType=false,foundGame=false;
    if(!std::getline(input,line)||(line!="OPENHW_SHIP_1"&&line!="OPENHW_SHIP_1\r"))return 0;
    while(std::getline(input,line)){
        std::istringstream row(line);std::string word,value,extra;row>>word;
        if(word=="type"||word=="gameplay"){
            if(!(row>>value)||(row>>extra))return 0;
            if(word=="type"){if(foundType)return 0;foundType=true;identity=value;}
            else {if(foundGame)return 0;foundGame=true;gameplay=value;}
        }
    }
    if(!foundType||!foundGame||identity!=type||!token(gameplay,true)||gameplay=="-"||gameplay.size()>=capacity)return 0;
    memcpy(relative,gameplay.c_str(),gameplay.size()+1);return 1;
}
namespace {
struct Validate {const HwRmVisualPackage *package;bool valid=true;};
void validateSurface(void *user,const HwRmSubsystemDraw *draw) {
    auto& state=*static_cast<Validate*>(user);
    state.valid &= hwRmVisualPackageMaterial(state.package,draw->component,draw->part->material)!=nullptr;
    if(std::strcmp(draw->material->shader,"default")==0) {
        if(draw->material->parameterCount || !draw->part->vertexColors) {state.valid=false;return;}
        bool uniform=true;
        for(unsigned i=1;i<draw->part->vertexCount;++i) uniform &= draw->part->vertexColors[i]==draw->part->vertexColors[0];
        for(unsigned i=0;i<draw->part->vertexCount;++i) {
            const unsigned color=draw->part->vertexColors[i];
            const bool neutral=(color>>24)==((color>>16)&255) && (color>>24)==((color>>8)&255);
            state.valid &= (color&255)==255 && (neutral || uniform);
        }
    }
}
void validateNode(void*,const HwRmSubsystemNode*) {}
}
extern "C" HwRmVisualPackage *hwRmVisualPackageRead(const char *type,const void *manifest,size_t size,
    HwRmPackageReadFile read,HwRmPackageReleaseFile release,HwRmPackageConfigure configure,void *user,
    char *error,size_t errorSize) {
    if(manifest && size>=18 && !std::memcmp(manifest,"OPENHW_SUBSYSTEM_2",18)) {
        std::istringstream input(std::string(static_cast<const char*>(manifest),size));std::string row,visual="OPENHW_SUBSYSTEM_1\n";
        std::getline(input,row);while(std::getline(input,row)) {if(row.find("[Weapons]")!=std::string::npos)break;
            if(row.compare(0,4,"gun ")&&row.compare(0,1,"#"))visual+=row+"\n";}
        return hwRmVisualPackageRead(type,visual.data(),visual.size(),read,release,configure,user,error,errorSize);
    }
    if(manifest && size>=13 && !std::memcmp(manifest,"OPENHW_SHIP_2",13)) {
        void* compiled=nullptr;size_t length=0;
        if(!openhwShipDocumentCompile(manifest,size,read,release,user,0,&compiled,&length,error,errorSize))return nullptr;
        auto result=hwRmVisualPackageRead(type,compiled,length,read,release,configure,user,error,errorSize);
        openhwShipDocumentFree(compiled);return result;
    }
    try {
        const bool subsystem=type && !std::strncmp(type,"Subsystem_",10) && token(type+10);
        const bool scaffold=type && std::strcmp(type,"Scaffold")==0;
        const bool raider=hwRmNativeP1Type(type)!=0;
        const bool kadeshi=hwRmNativeP2Type(type)!=0;
        const bool council=hwRmNativeP3Type(type)!=0;
        const bool trader=hwRmNativeTraderType(type)!=0;
        const bool taiidan=type && !std::strncmp(type,"Taiidan_",8) && hwRmNativeR2Type(type+8);
        const bool derelict=type && !std::strncmp(type,"Derelict_",9) && hwRmNativeDerelictType(type+9);
        if((!kushan(type) && !scaffold && !raider && !kadeshi && !council && !trader && !derelict && !taiidan && !subsystem) || !manifest || !size || size>128*1024 || !read || !release || !configure)
            throw std::runtime_error("invalid/out-of-scope native R1 package");
        const auto *chars=static_cast<const char*>(manifest);
        for(size_t i=0;i<size;++i) if((static_cast<unsigned char>(chars[i])<32 && chars[i]!='\r' && chars[i]!='\n' && chars[i]!='\t') || static_cast<unsigned char>(chars[i])>126)
            throw std::runtime_error("non-ASCII native package manifest");
        std::istringstream input(std::string(chars,size));std::string line,hull,mad;
        const std::string version=subsystem ? "OPENHW_SUBSYSTEM_1" : scaffold ? "HW_RM_SCAFFOLD_VISUAL_1" :
            trader ? "HW_RM_TRADERS_VISUAL_1" : derelict ? "HW_RM_DERELICT_VISUAL_1" : kadeshi ? "HW_RM_P2_VISUAL_1" : council ? "HW_RM_P3_VISUAL_1" : raider ? "HW_RM_P1_VISUAL_1" : taiidan ? "HW_RM_R2_VISUAL_1" : "HW_RM_R1_VISUAL_1";
        if(!std::getline(input,line))throw std::runtime_error("missing ship definition version");
        const bool definition=line=="OPENHW_SHIP_1"||line=="OPENHW_SHIP_1\r";
        if(!definition && line!=version && line!=version+"\r")
            throw std::runtime_error("unsupported native R1 package version");
        auto package=std::make_unique<HwRmVisualPackage>();
        std::vector<Attachment> attachments;std::vector<Marker> markers,animationAliases;
        std::vector<std::string> selectedClips;
        bool typeSeen=false,scaleSeen=false,hullSeen=false,madSeen=false;unsigned rows=0;
        struct Gun {unsigned index;std::string slot,azimuth,declination,recoil,muzzle;};
        struct Event {std::string name;std::vector<std::string> clips;};
        std::vector<Gun> guns;std::vector<Event> events;std::string gameplay;
        std::vector<std::pair<unsigned,std::string>> weaponEvents;
        std::vector<std::pair<unsigned,std::string>> weaponFire;
        std::vector<std::pair<std::string,std::string>> presentation;
        struct Mount {std::string slot;float matrix[16];};std::vector<Mount> mounts;
        struct Joint {std::string slot,name,parent;float matrix[16];};std::vector<Joint> authoredJoints;
        while(std::getline(input,line)) {
            if(++rows>1024 || line.size()>1024) throw std::runtime_error("native package row limit");
            std::istringstream row(line);std::vector<std::string> fields;std::string field;
            while(row>>field) fields.push_back(field);
            if(fields.empty()) continue;
            if(definition && fields[0][0]=='#')continue;
            if(fields[0]=="type" && fields.size()==2 && !typeSeen) {if(fields[1]!=type) throw std::runtime_error("native package identity mismatch");typeSeen=true;}
            else if(fields[0]=="scale" && fields.size()==2 && !scaleSeen) {
                std::istringstream number(fields[1]);char trailing;
                if(!(number>>package->scale) || (number>>trailing) || !std::isfinite(package->scale) || package->scale<=0 || package->scale>100000)
                    throw std::runtime_error("invalid native package scale");scaleSeen=true;
            } else if(fields[0]=="hull" && fields.size()==2 && !hullSeen) {hull=fields[1];hullSeen=true;}
            else if(fields[0]=="mad" && fields.size()==2 && !madSeen) {mad=fields[1];madSeen=true;}
            else if(fields[0]=="attach" && fields.size()==5 && attachments.size()<255) {
                if(!token(fields[1]) || !token(fields[2]) || !token(fields[4],true)) throw std::runtime_error("invalid native attachment token");
                attachments.push_back({fields[1],fields[2],fields[4],integer(fields[3],3)});
            } else if(fields[0]=="surface" && fields.size()==4 && package->surfaces.size()<1024) {
                Surface s{integer(fields[1],255),integer(fields[2],1023),fields[3]};
                if(!token(s.label,true)) throw std::runtime_error("invalid native texture label");
                for(const auto& other:package->surfaces) if(other.component==s.component && other.material==s.material)
                    throw std::runtime_error("duplicate native surface namespace");
                package->surfaces.push_back(std::move(s));
            } else if(definition && fields[0]=="subsystem" && fields.size()==5 && attachments.size()<255) {
                for(unsigned i=1;i<5;i++)if(!token(fields[i]))throw std::runtime_error("invalid subsystem declaration");
                const std::string root="@/Subsystems/"+fields[4]+"/";
                Input description(read,release,user,root+"subsystem.owsubsystem",128*1024);
                std::istringstream source(std::string(static_cast<const char*>(description.bytes),description.size));
                std::string header,rowText,model,identity;
                if(!std::getline(source,header)||(header!="OPENHW_SUBSYSTEM_1"&&header!="OPENHW_SUBSYSTEM_1\r"))throw std::runtime_error("invalid subsystem definition header");
                while(std::getline(source,rowText)) {std::istringstream r(rowText);std::vector<std::string> f;std::string v;while(r>>v)f.push_back(v);if(f.empty())continue;
                    if(f[0]=="type"&&f.size()==2&&identity.empty())identity=f[1];
                    else if(f[0]=="hull"&&f.size()==2&&model.empty()&&token(f[1],true))model=f[1];
                    else if(f[0]=="scale"&&f.size()==2&&f[1]=="1"){}
                    else if(f[0]=="mad"&&f.size()==2&&f[1]=="-"){}
                    else if(f[0]=="surface"&&f.size()==4&&f[1]=="0"&&token(f[3]))package->surfaces.push_back({unsigned(attachments.size()+1),integer(f[2],1023),root+f[3]});
                    else if(f[0]=="presentation"&&f.size()==3&&f[1]=="Hull"&&token(f[2],true))presentation.emplace_back(fields[1],root+f[2]);
                    else throw std::runtime_error("unsupported subsystem definition directive");
                }
                if(identity!="Subsystem_"+fields[4]||model.empty())throw std::runtime_error("subsystem identity/model mismatch");
                attachments.push_back({fields[1],fields[2],root+model,integer(fields[3],3)});
            } else if(definition && fields[0]=="joint" && fields.size()==10 && authoredJoints.size()<512) {
                Joint joint{};joint.slot=fields[1];joint.name=fields[2];joint.parent=fields[3];
                if(!token(joint.slot)||!token(joint.name)||!token(joint.parent))throw std::runtime_error("invalid authored joint name");
                float position[3],angle[3];for(unsigned i=0;i<6;i++){std::istringstream n(fields[i+4]);float value;char extra;if(!(n>>value)||(n>>extra)||!std::isfinite(value)||std::abs(value)>1000000)throw std::runtime_error("invalid authored joint transform");if(i<3)position[i]=value;else angle[i-3]=value*.01745329252f;}
                const float cx=std::cos(angle[0]),sx=std::sin(angle[0]),cy=std::cos(angle[1]),sy=std::sin(angle[1]),cz=std::cos(angle[2]),sz=std::sin(angle[2]);float*m=joint.matrix;
                m[0]=cz*cy;m[1]=sz*cy;m[2]=-sy;m[4]=cz*sy*sx-sz*cx;m[5]=sz*sy*sx+cz*cx;m[6]=cy*sx;m[8]=cz*sy*cx+sz*sx;m[9]=sz*sy*cx-cz*sx;m[10]=cy*cx;std::copy(position,position+3,m+12);m[15]=1;
                for(const auto& old:authoredJoints)if(old.slot==joint.slot&&old.name==joint.name)throw std::runtime_error("duplicate authored joint");authoredJoints.push_back(joint);
            } else if(definition && fields[0]=="mount" && fields.size()==8 && mounts.size()<255 && token(fields[1])) {
                Mount mount{};mount.slot=fields[1];float position[3],angle[3];
                for(unsigned i=0;i<6;i++){std::istringstream n(fields[i+2]);float value;char extra;if(!(n>>value)||(n>>extra)||!std::isfinite(value)||std::abs(value)>1000000)throw std::runtime_error("invalid subsystem mount transform");if(i<3)position[i]=value;else angle[i-3]=value*.01745329252f;}
                const float cx=std::cos(angle[0]),sx=std::sin(angle[0]),cy=std::cos(angle[1]),sy=std::sin(angle[1]),cz=std::cos(angle[2]),sz=std::sin(angle[2]);
                float* m=mount.matrix;m[0]=cz*cy;m[1]=sz*cy;m[2]=-sy;m[4]=cz*sy*sx-sz*cx;m[5]=sz*sy*sx+cz*cx;m[6]=cy*sx;m[8]=cz*sy*cx+sz*sx;m[9]=sz*sy*cx-cz*sx;m[10]=cy*cx;std::copy(position,position+3,m+12);m[15]=1;
                for(const auto& old:mounts)if(old.slot==mount.slot)throw std::runtime_error("duplicate subsystem mount transform");mounts.push_back(mount);
            } else if(fields[0]=="presentation" && fields.size()==3 && presentation.size()<256 && token(fields[1]) && token(fields[2],true)) {
                for(const auto& prior:presentation)if(prior.first==fields[1])throw std::runtime_error("duplicate component event source");
                presentation.emplace_back(fields[1],fields[2]);
            } else if(fields[0]=="marker" && fields.size()==4 && markers.size()<256) {
                if(!token(fields[1]) || !token(fields[2]) || !token(fields[3])) throw std::runtime_error("invalid native marker token");
                markers.push_back({fields[1],fields[2],fields[3]});
            } else if(fields[0]=="animation_alias" && fields.size()==4 && animationAliases.size()<256) {
                if(!token(fields[1]) || fields[2]!="Hull" || !token(fields[3]))
                    throw std::runtime_error("invalid native hull animation alias");
                animationAliases.push_back({fields[1],fields[2],fields[3]});
            } else if(fields[0]=="animation_clip" && fields.size()==2 && selectedClips.size()<4 &&
                !std::strcmp(type,"Taiidan_HeavyCorvette") && token(fields[1])) {
                if(fields[1]!="Wings_Open" && fields[1]!="Wings_Close" && fields[1]!="Wings_Open_DMG" && fields[1]!="Wings_Close_DMG")
                    throw std::runtime_error("unsupported Taiidan heavy corvette clip selection");
                if(std::find(selectedClips.begin(),selectedClips.end(),fields[1])!=selectedClips.end())
                    throw std::runtime_error("duplicate selected package clip");
                selectedClips.push_back(fields[1]);
            } else if(definition && fields[0]=="gameplay" && fields.size()==2 && gameplay.empty() && token(fields[1],true)) {
                gameplay=fields[1];
            } else if(definition && fields[0]=="gun" && fields.size()==7 && guns.size()<64) {
                for(unsigned i=2;i<7;i++)if(!token(fields[i]))throw std::runtime_error("invalid gun joint token");
                Gun g{integer(fields[1],63),fields[2],fields[3],fields[4],fields[5],fields[6]};
                for(const auto& prior:guns)if(prior.index==g.index)throw std::runtime_error("duplicate gameplay gun index");
                guns.push_back(std::move(g));
            } else if(definition && fields[0]=="weaponFire" && fields.size()==3 && weaponFire.size()<64 && token(fields[2],true)) {
                unsigned gun=integer(fields[1],63);
                for(const auto& prior:weaponFire)if(prior.first==gun)throw std::runtime_error("duplicate weaponfire declaration");
                weaponFire.emplace_back(gun,fields[2]);
            } else if(definition && fields[0]=="weaponEvent" && fields.size()==3 && weaponEvents.size()<64 && token(fields[2])) {
                unsigned gun=integer(fields[1],63);
                for(const auto& prior:weaponEvents)if(prior.first==gun)throw std::runtime_error("duplicate weapon event declaration");
                weaponEvents.emplace_back(gun,fields[2]);
            } else if(definition && fields[0]=="event" && fields.size()>=3 && fields.size()<=6 && events.size()<64) {
                for(unsigned i=1;i<fields.size();i++)if(!token(fields[i]))throw std::runtime_error("invalid animation event token");
                for(const auto& prior:events)if(prior.name==fields[1])throw std::runtime_error("duplicate animation event");
                events.push_back({fields[1],std::vector<std::string>(fields.begin()+2,fields.end())});
            } else throw std::runtime_error("invalid/duplicate native package directive");
        }
        if(!typeSeen || !scaleSeen || !hullSeen || !madSeen) throw std::runtime_error("incomplete native R1 package header");
        if(subsystem && (definition || !attachments.empty() || !markers.empty() || !animationAliases.empty() || mad!="-"))throw std::runtime_error("invalid standalone subsystem package");
        if(definition && !derelict && !scaffold && gameplay.empty())throw std::runtime_error("ship definition requires gameplay file");
        if(!selectedClips.empty() && selectedClips.size()!=4) throw std::runtime_error("incomplete Taiidan wing clip selection");
        if((scaffold || derelict) && (mad!="-" || !attachments.empty() || !markers.empty() || !animationAliases.empty()))
            throw std::runtime_error("scaffold package must be a rigid visual-only hull");
        Input hullInput(read,release,user,hull,64*1024*1024);
        if(mad!="-") {
            Input madInput(read,release,user,mad,16*1024*1024);
            std::vector<HwRmAnimationTarget> targets;
            for(const auto& alias:animationAliases) targets.push_back({alias.semantic.c_str(),0,alias.joint.c_str()});
            std::vector<const char*> clips;for(const auto& clip:selectedClips) clips.push_back(clip.c_str());
            package->asset.reset(hwRmVisualAssetCreateWithAnimationClips(hullInput.bytes,hullInput.size,
                madInput.bytes,madInput.size,targets.data(),unsigned(targets.size()),clips.data(),unsigned(clips.size()),error,errorSize));
        } else {
            if(!animationAliases.empty() || !selectedClips.empty()) throw std::runtime_error("animation selection/alias without MAD");
            package->asset.reset(hwRmVisualAssetCreate(hullInput.bytes,hullInput.size,nullptr,0,error,errorSize));
        }
        if(!package->asset) return nullptr;
        for(const auto& a:attachments) {
            Input source(read,release,user,a.path,64*1024*1024);
            if(!hwRmVisualAssetAttach(package->asset.get(),a.path.c_str(),a.slot.c_str(),a.anchor.c_str(),a.role,source.bytes,source.size,error,errorSize)) return nullptr;
        }
        for(const auto& item:presentation) {
            Input source(read,release,user,item.second,4*1024*1024);
            if(!hwRmVisualAssetBindPresentation(package->asset.get(),item.first.c_str(),source.bytes,source.size,error,errorSize))return nullptr;
        }
        for(const auto& mount:mounts)if(!hwRmVisualAssetMount(package->asset.get(),mount.slot.c_str(),mount.matrix))throw std::runtime_error("missing/frozen subsystem mount slot");
        std::vector<bool> jointApplied(authoredJoints.size(),false);unsigned remaining=unsigned(authoredJoints.size());
        while(remaining){unsigned progress=0;
            for(unsigned i=0;i<authoredJoints.size();i++){if(jointApplied[i])continue;const auto& joint=authoredJoints[i];bool pendingParent=false;
                for(unsigned j=0;j<authoredJoints.size();j++)if(!jointApplied[j]&&authoredJoints[j].slot==joint.slot&&authoredJoints[j].name==joint.parent)pendingParent=true;
                if(pendingParent)continue;
                if(!hwRmVisualAssetAuthorJoint(package->asset.get(),joint.slot.c_str(),joint.name.c_str(),joint.parent.c_str(),joint.matrix))throw std::runtime_error("invalid, cyclic or missing authored joint");
                jointApplied[i]=true;--remaining;++progress;
            }if(!progress)throw std::runtime_error("cyclic authored joint declarations");
        }
        for(const auto& m:markers) if(!hwRmVisualAssetBindMarker(package->asset.get(),m.semantic.c_str(),m.slot.c_str(),m.joint.c_str(),error,errorSize)) return nullptr;
        if(definition) {
            for(const auto& gun:guns)if(!hwRmVisualAssetBindGun(package->asset.get(),gun.index,gun.slot.c_str(),
                gun.azimuth=="-"?nullptr:gun.azimuth.c_str(),gun.declination=="-"?nullptr:gun.declination.c_str(),
                gun.recoil=="-"?nullptr:gun.recoil.c_str(),gun.muzzle.c_str(),error,errorSize))return nullptr;
            for(const auto& event:events){std::vector<const char*> clips;for(const auto& clip:event.clips)clips.push_back(clip.c_str());
                if(!hwRmVisualAssetBindAnimation(package->asset.get(),event.name.c_str(),clips.data(),unsigned(clips.size()),error,errorSize))return nullptr;}
            for(const auto& cue:weaponEvents)
                if(!hwRmVisualAssetBindGunPresentation(package->asset.get(),cue.first,cue.second.c_str(),error,errorSize))return nullptr;
            for(const auto& weapon:weaponFire){Input source(read,release,user,weapon.second,1024*1024);
                if(!hwRmVisualAssetBindWeaponPresentation(package->asset.get(),weapon.first,source.bytes,source.size,error,errorSize))return nullptr;}
        } else if(!subsystem && !configure(user,package->asset.get(),type,error,errorSize)) return nullptr;
        auto *instance=hwRmVisualInstanceCreate(package->asset.get());if(!instance) throw std::runtime_error("native package instance allocation failed");
        bool valid=true;char reason[512]{};
        for(unsigned lod=0;lod<5 && valid;++lod) {
            Validate validation{package.get(),true};
            valid=hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(instance),lod,package->scale,nullptr,nullptr,validateSurface,&validation,reason,sizeof(reason))!=0 && validation.valid;
            if(valid) valid=hwRmSubsystemVisitHierarchy(hwRmVisualInstanceSubsystem(instance),lod,package->scale,255,validateNode,nullptr,reason,sizeof(reason))!=0;
        }
        hwRmVisualInstanceFree(instance);
        if(!valid) throw std::runtime_error(*reason?reason:"unmapped visible native material");
        message(error,errorSize,"");return package.release();
    } catch(const std::exception& e) {message(error,errorSize,e.what());return nullptr;}
}
extern "C" void hwRmVisualPackageFree(HwRmVisualPackage *package) {delete package;}
extern "C" float hwRmVisualPackageScale(const HwRmVisualPackage *package) {return package?package->scale:0;}
extern "C" const char *hwRmVisualPackageMaterial(const HwRmVisualPackage *package,unsigned component,unsigned material) {
    if(package) for(const auto& surface:package->surfaces) if(surface.component==component && surface.material==material) return surface.label.c_str();
    return nullptr;
}
extern "C" HwRmVisualAsset *hwRmVisualPackageTakeAsset(HwRmVisualPackage *package) {return package?package->asset.release():nullptr;}
#include "OpenHWShipDocument.inc"
