#include "CataclysmData.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <regex>
#include <set>
namespace {
namespace fs=std::filesystem;
struct Entry {fs::path archive;uint64_t offset;uint32_t stored,expanded;};
std::map<std::string,Entry> entries;
std::map<std::string,std::vector<unsigned char>> cache;
std::vector<std::string> names;
std::recursive_mutex mutex;
fs::path ownedDirectory;
std::array<float,60> weaponDamage{};
bool weaponDamageLoaded=false;
std::map<std::pair<unsigned,int>,uint32_t> speechRotations;
std::vector<unsigned char> ownedExecutable;
constexpr uint32_t maxMember=128u*1024u*1024u;
uint32_t u32(const unsigned char *p){uint32_t v;std::memcpy(&v,p,4);return v;}
void put32(unsigned char *p,uint32_t v){std::memcpy(p,&v,4);}
std::string scriptText(const char *bytes,size_t size){
    std::string text(bytes,size);std::replace(text.begin(),text.end(),'\r','\n');return text;
}
std::string key(std::string text){
    for(char& c:text){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}
    if(text.empty() || text.front()=='/' || text.find(':')!=text.npos)return {};
    size_t start=0;
    while(start<text.size()){
        auto end=text.find('/',start);if(end==text.npos)end=text.size();
        auto part=text.substr(start,end-start);
        if(part.empty()||part=="."||part=="..")return {};
        start=end+1;
    }
    return text;
}
std::vector<unsigned char> read(const fs::path& file,uint64_t offset,size_t count){
    std::ifstream in(file,std::ios::binary);
    if(!in || offset>fs::file_size(file) || count>fs::file_size(file)-offset)throw std::runtime_error("Truncated Cataclysm archive");
    std::vector<unsigned char> bytes(count);in.seekg((std::streamoff)offset);
    if(count && !in.read((char*)bytes.data(),count))throw std::runtime_error("Cannot read Cataclysm archive");
    return bytes;
}
const std::vector<unsigned char>& executableData(){
    if(ownedExecutable.empty()){
        if(ownedDirectory.empty())throw std::runtime_error("Cataclysm data is not mounted");
        auto file=ownedDirectory/"cataclysm.exe";auto length=fs::file_size(file);
        if(length<512 || length>32u*1024u*1024u)throw std::runtime_error("Unsupported Cataclysm executable size");
        auto bytes=read(file,0,(size_t)length);
        if(bytes[0]!='M'||bytes[1]!='Z')throw std::runtime_error("Invalid Cataclysm executable header");
        auto pe=u32(bytes.data()+0x3c);
        if(pe>bytes.size()-26 || std::memcmp(bytes.data()+pe,"PE\0\0",4) || bytes[pe+24]!=0x0b || bytes[pe+25]!=1)
            throw std::runtime_error("Unsupported Cataclysm executable format");
        ownedExecutable=std::move(bytes);
    }
    return ownedExecutable;
}
size_t originalDataOffset(unsigned address,size_t count){
    const auto& bytes=executableData();auto pe=u32(bytes.data()+0x3c);
    if(pe>bytes.size()-84)throw std::runtime_error("Truncated original executable");
    auto base=u32(bytes.data()+pe+52);unsigned sections=bytes[pe+6]|(bytes[pe+7]<<8);
    unsigned optional=bytes[pe+20]|(bytes[pe+21]<<8);size_t table=pe+24+optional;
    if(sections>96 || table>bytes.size() || sections>(bytes.size()-table)/40)
        throw std::runtime_error("Invalid original section table");
    for(unsigned i=0;i<sections;i++){
        auto *section=bytes.data()+table+40*i;uint64_t begin=(uint64_t)base+u32(section+12);
        uint64_t size=u32(section+16),offset=u32(section+20);
        if(address<begin || (uint64_t)address-begin>=size)continue;
        offset+=(uint64_t)address-begin;
        if(count>size-((uint64_t)address-begin) || offset>bytes.size() || count>bytes.size()-offset)
            throw std::runtime_error("Original data reference exceeds section");
        return (size_t)offset;
    }
    throw std::runtime_error("Original data reference is not in an owned PE section");
}
void index(const fs::path& file){
    auto h=read(file,0,15);
    if(std::memcmp(h.data(),"RBF1.23",7)||u32(h.data()+11)!=1)throw std::runtime_error("Not a supported Cataclysm RBF1.23 archive");
    auto count=u32(h.data()+7);
    if(!count || count>200000)throw std::runtime_error("Invalid Cataclysm archive index");
    auto table=read(file,15,(size_t)count*32);
    for(uint32_t i=0;i<count;i++){
        const auto *r=table.data()+i*32;
        /* nameLength is a 16-bit field followed by unspecified padding.
           Emergence's update102.big has nonzero padding (0x0067). */
        uint32_t length=u32(r+8)&0xffffu,stored=u32(r+12),expanded=u32(r+16),offset=u32(r+20);
        if(!length||length>1024||stored>maxMember||expanded>maxMember||stored>expanded)throw std::runtime_error("Invalid Cataclysm member size");
        auto encrypted=read(file,offset,length);std::string name;unsigned char value=0xd5;
        for(auto c:encrypted){value^=c;name.push_back((char)value);}
        name=key(name);if(name.empty())throw std::runtime_error("Unsafe Cataclysm archive member");
        if((uint64_t)offset+length+1+stored>fs::file_size(file))throw std::runtime_error("Cataclysm archive member outside file");
        entries[name]={file,(uint64_t)offset+length+1,stored,expanded};
    }
}
std::vector<unsigned char> expand(const Entry& e){
    auto source=read(e.archive,e.offset,e.stored);if(e.stored==e.expanded)return source;
    size_t bit=0;auto bits=[&](unsigned count){
        unsigned value=0;
        while(count--){if(bit>=source.size()*8)throw std::runtime_error("Truncated Cataclysm LZSS data");value=(value<<1)|((source[bit>>3]>>(7-(bit&7)))&1);bit++;}
        return value;
    };
    std::array<unsigned char,4096> window{};unsigned position=1;std::vector<unsigned char> out;out.reserve(e.expanded);
    auto append=[&](unsigned char c){if(out.size()>=e.expanded)throw std::runtime_error("Cataclysm LZSS overflow");out.push_back(c);window[position]=c;position=(position+1)&4095;};
    while(out.size()<e.expanded){
        if(bits(1))append((unsigned char)bits(8));
        else {unsigned match=bits(12);if(!match)break;unsigned length=bits(4)+2;
            for(unsigned i=0;i<length;i++)append(window[(match+i)&4095]);}
    }
    if(out.size()!=e.expanded)throw std::runtime_error("Cataclysm LZSS size mismatch");return out;
}
void cinematic(std::vector<unsigned char>& data){
    if(data.size()<132 || std::memcmp(data.data(),"Crannnberry",10) || u32(data.data()+12)!=0x101)
        throw std::runtime_error("Unsupported Cataclysm NIS header");
    auto count=u32(data.data()+36),offset=u32(data.data()+40);
    if(count>20000 || offset<132 || (uint64_t)offset+(uint64_t)count*56>data.size())
        throw std::runtime_error("Invalid Cataclysm NIS actor table");
    for(uint32_t i=0;i<count;i++){
        auto *race=data.data()+offset+i*56+12;auto original=u32(race);uint32_t adapted;
        switch(original){
        case 0:case 2:adapted=0;break; // Somtaaw and Kushan: distinct ship IDs, HW1 R1 storage
        case 1:case 3:adapted=1;break; // Beast / Taiidan
        case 4:adapted=2;break;        // Turanic Raiders
        case 5:adapted=5;break;        // Bentusi / Traders
        case 6:adapted=10;break;       // Cataclysm's derelict race
        default:
            if(original<7 || original>14)throw std::runtime_error("Unsupported Cataclysm NIS actor category");
            adapted=original-1;break; // special object categories follow NUM_RACES
        }
        put32(race,adapted);
    }
}
void validateMesh(const std::vector<unsigned char>& data){
    auto count=u32(data.data()+40);
    if(count>100000 || 68ull+112ull*count>data.size())throw std::runtime_error("Invalid adapted mesh hierarchy");
    auto span=[&](uint32_t offset,uint64_t count,uint32_t stride){
        if(count && (offset>data.size() || count*stride>data.size()-offset))
            throw std::runtime_error("Adapted mesh geometry extends outside its file");
    };
    for(uint32_t i=0;i<count;i++){
        const auto* object=data.data()+68+i*112;
        span(u32(object+24),u32(object+8),16);
        span(u32(object+28),(uint64_t)u32(object+12)+u32(object+16),16);
        span(u32(object+32),u32(object+20),40);
    }
}
void mesh(std::vector<unsigned char>& data){
    if(data.size()<68 || (std::memcmp(data.data(),"RMF97ba",7)&&std::memcmp(data.data(),"RMF99ba",7)))throw std::runtime_error("Invalid Cataclysm GEO header");
    auto version=u32(data.data()+8);
    if(version==0x402){
        try{validateMesh(data);return;}catch(const std::runtime_error&){
            // Owned NIS light-source assets retain the HW1 version tag but
            // contain Cataclysm's extended object records. Validate the actual
            // converted layout instead of trusting that tag alone.
        }
    }
    if(version!=0x500 && version!=0x402)throw std::runtime_error("Unsupported Cataclysm GEO version");
    auto count=u32(data.data()+40);if(count>100000 || (uint64_t)68+count*116>data.size())throw std::runtime_error("Invalid Cataclysm GEO hierarchy");
    auto original=data;
    for(uint32_t i=0;i<count;i++){
        const auto *in=original.data()+68+i*116;auto *out=data.data()+68+i*112;
        std::memcpy(out,in,28);std::memcpy(out+28,in+32,84);
        for(unsigned at:{36u,40u,44u}){
            uint32_t link=u32(out+at);
            if(link){if(link<68||(link-68)%116||(link-68)/116>=count)throw std::runtime_error("Invalid Cataclysm GEO link");put32(out+at,68+((link-68)/116)*112);}
        }
    }
    std::memcpy(data.data(),"RMF97ba",7);put32(data.data()+8,0x402);
    validateMesh(data);
}
}
extern "C" int hwCataDataOpen(const char *directory,char *error,size_t capacity){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try {
        if(!directory||!*directory)throw std::runtime_error("Select your Homeworld Cataclysm or Emergence installation in Launcher");
        auto root=fs::weakly_canonical(fs::u8path(directory));
        if(!ownedDirectory.empty() && fs::equivalent(root,ownedDirectory)){
            if(error&&capacity)*error=0;return 1;
        }
        hwCataDataClose();
        index(root/"cataclysm.big");
        for(const char *patch:{"update.big","update102.big"})if(fs::is_regular_file(root/patch))index(root/patch);
        for(const char *required:{"singleplayer/mission01/mission01.mission","race_sect/smothership.shp","race_sect/sacolyte.shp","race_sect/sworker.shp"})
            if(!entries.count(required))throw std::runtime_error("Selected folder does not contain the Cataclysm Mission 1 data");
        for(auto& pair:entries)names.push_back(pair.first);
        ownedDirectory=root;
        if(error&&capacity)*error=0;return 1;
    }catch(const std::exception& e){hwCataDataClose();if(error&&capacity)std::snprintf(error,capacity,"%s",e.what());return 0;}
}
extern "C" int hwCataDataOpenConfigured(char *error,size_t capacity){
    wchar_t path[32768];DWORD size=GetModuleFileNameW(nullptr,path,32768);
    if(!size||size>=32768)return hwCataDataOpen(nullptr,error,capacity);
    std::ifstream file(fs::path(path).parent_path()/"Development/Launcher/cataclysm-directory.txt",std::ios::binary);
    std::string directory;std::getline(file,directory);if(!directory.empty()&&directory.back()=='\r')directory.pop_back();
    return hwCataDataOpen(directory.c_str(),error,capacity);
}
extern "C" int hwCataDataReady(void){std::lock_guard<std::recursive_mutex> lock(mutex);return !entries.empty();}
extern "C" void hwCataDataClose(void){std::lock_guard<std::recursive_mutex> lock(mutex);cache.clear();entries.clear();names.clear();ownedDirectory.clear();ownedExecutable.clear();weaponDamageLoaded=false;weaponDamage.fill(0);speechRotations.clear();}
extern "C" unsigned hwCataDataCount(void){std::lock_guard<std::recursive_mutex> lock(mutex);return (unsigned)names.size();}
extern "C" int hwCataAnimationPath(const char *directory,const char *definition,char *path,size_t capacity){
    if(!directory || !definition || !path || !capacity)return 0;
    auto base=key(directory),name=key(definition);
    if(base.rfind("cataclysm/",0)!=0 || name.size()<=4 || name.find('/')!=name.npos ||
        name.substr(name.size()-4)!=".shp")return 0;
    if(base.back()!='/')base+='/';
    auto result=base+name.substr(0,name.size()-4)+".mad";
    if(result.size()+1>capacity)return 0;
    std::memcpy(path,result.c_str(),result.size()+1);return 1;
}
extern "C" int hwCataNavLightTexturePath(const char *name, char *path, size_t capacity)
{
    if (!name || !path || !capacity) return 0;
    auto member = key(name);
    if (member.empty() || member == "null" || member.back() == '/') return 0;
    if (member.rfind("cataclysm/", 0) != 0) member.insert(0, "cataclysm/");
    const auto lif = member + ".lif";
    if (lif.size() >= capacity) return 0;
    int size = 0;
    const auto *texture = static_cast<const unsigned char *>(hwCataDataView(lif.c_str(), &size));
    if (!texture || size < 48 || std::memcmp(texture, "Willy 7", 8) != 0 ||
        u32(texture + 8) != 0x104) return 0;
    std::memcpy(path, member.c_str(), member.size() + 1);
    return 1;
}
extern "C" int hwCataOpenHWVisualPath(const char *directory,const char *member,char *path,size_t capacity){
    if(!directory||!member||!path||!capacity)return 0;
    auto base=key(directory);if(base!="cataclysm/race_kushan/" && base!="cataclysm/race_kushan")return 0;
    auto input=key(member);if(input.empty())return 0;
    auto dot=input.rfind('.');if(dot==input.npos)return 0;
    auto extension=input.substr(dot);
    if(extension!=".lod"&&extension!=".geo"&&extension!=".peo"&&extension!=".mex"&&extension!=".mad")return 0;
    std::string output="R1/";size_t start=0;
    while(start<input.size()){
        auto end=input.find('/',start);if(end==input.npos)end=input.size();
        auto component=input.substr(start,end-start);
        if(component.size()>1 && component.front()=='k')component.erase(0,1);
        output+=component;if(end<input.size())output+='/';start=end+1;
    }
    if(output.size()+1>capacity)return 0;
    std::memcpy(path,output.c_str(),output.size()+1);return 1;
}
static int cataMeshIdentity(const char *path, const std::string &prefix,
                            const std::string &stemPrefix, char *ship,
                            size_t capacity, unsigned *lod)
{
    if (!path || !ship || !lod) return 0;
    const auto member = key(path);
    const size_t shipStart = prefix.size();
    if (member.compare(0, shipStart, prefix) != 0) return 0;

    const auto shipEnd = member.find('/', shipStart);
    if (shipEnd == member.npos || shipEnd == shipStart ||
        member.compare(shipEnd, 8, "/rl0/lod") != 0 || member.size() <= shipEnd + 10) return 0;
    const char level = member[shipEnd + 8];
    if (level < '0' || level > '4' || member[shipEnd + 9] != '/') return 0;

    const auto type = member.substr(shipStart, shipEnd - shipStart);
    const auto basename = member.substr(shipEnd + 10);
    if (type.size() >= capacity || (basename != stemPrefix + type + ".geo" &&
                                    basename != stemPrefix + type + ".peo")) return 0;
    std::memcpy(ship, type.c_str(), type.size() + 1);
    *lod = static_cast<unsigned>(level - '0');
    return 1;
}
extern "C" int hwCataTaiidanMeshIdentity(const char *path,char *ship,size_t capacity,unsigned *lod)
{
    return cataMeshIdentity(path, "cataclysm/race_taidan/t", "t", ship, capacity, lod);
}
extern "C" int hwCataRaiderMeshIdentity(const char *path,char *ship,size_t capacity,unsigned *lod)
{
    return cataMeshIdentity(path, "cataclysm/race_p1/", "", ship, capacity, lod);
}
extern "C" int hwCataMission1StartingFleet(CataStartingFleet *fleet,char *error,size_t capacity){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(fleet)std::memset(fleet,0,sizeof(*fleet));
    try{
        if(!fleet)throw std::runtime_error("Missing Cataclysm starting-fleet output");
        int size=-1;auto *bytes=(const char*)hwCataDataView("warp.script",&size);
        if(!bytes || size<1 || size>1024*1024)throw std::runtime_error("Cataclysm starting-fleet definitions are missing");
        CataStartingFleet parsed{};std::istringstream lines(scriptText(bytes,size));std::string line;
        int level=0;bool found=false,resources=false;
        auto identifier=[](const std::string& value){
            if(value.empty() || value.size()>63)return false;
            for(unsigned char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
            return true;
        };
        while(std::getline(lines,line)){
            auto comment=line.find(';');if(comment!=line.npos)line.erase(comment);
            comment=line.find("//");if(comment!=line.npos)line.erase(comment);
            std::replace(line.begin(),line.end(),',',' ');
            std::istringstream fields(line);std::string command;fields>>command;
            for(char& c:command)if(c>='A'&&c<='Z')c+=32;
            if(command=="level"){
                if(!(fields>>level))throw std::runtime_error("Invalid Cataclysm starting-fleet level");
                if(level==1){if(found)throw std::runtime_error("Duplicate Cataclysm Mission 1 starting fleet");found=true;}
                continue;
            }
            if(level!=1)continue;
            if(command=="resources"){
                if(resources || !(fields>>parsed.resources) || parsed.resources<0)throw std::runtime_error("Invalid Cataclysm starting resources");
                resources=true;
            }else if(command=="ships"){
                std::string name;int count;
                if(!(fields>>name>>count) || !identifier(name) || count<0 || count>1000 || parsed.shipCount>=32)
                    throw std::runtime_error("Invalid Cataclysm starting ship");
                auto& ship=parsed.ships[parsed.shipCount++];std::snprintf(ship.name,sizeof(ship.name),"%s",name.c_str());ship.count=count;
            }else if(command=="tech"){
                std::string name;
                if(!(fields>>name) || !identifier(name) || parsed.technologyCount>=32)throw std::runtime_error("Invalid Cataclysm starting technology");
                std::snprintf(parsed.technologies[parsed.technologyCount++],64,"%s",name.c_str());
            }
        }
        if(!found || !resources || !parsed.shipCount)throw std::runtime_error("Cataclysm Mission 1 starting fleet is incomplete");
        *fleet=parsed;if(error&&capacity)*error=0;return 1;
    }catch(const std::exception& e){if(error&&capacity)std::snprintf(error,capacity,"%s",e.what());return 0;}
}
extern "C" const char *hwCataDataName(unsigned i){std::lock_guard<std::recursive_mutex> lock(mutex);return i<names.size()?names[i].c_str():nullptr;}
extern "C" float hwCataWeaponDamage(unsigned weaponClass,unsigned armorClass){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        if(weaponClass>=20 || armorClass>=3 || ownedDirectory.empty())throw std::runtime_error("Unsupported Cataclysm weapon/armour class");
        if(!weaponDamageLoaded){
            /* Retail damage is a weapon-class x armour-class table in the
               owned executable, not DamageLo/Hi in SHP. Read data only; never
               load, execute, modify or redistribute the original executable. */
            const auto& bytes=executableData();
            const float signature[]={80,75,50,16,14,9};size_t found=0,count=0;
            for(size_t at=0;at+sizeof(weaponDamage)<=bytes.size();at+=4)
                if(!std::memcmp(bytes.data()+at,signature,sizeof(signature))){found=at;count++;}
            if(count!=1)throw std::runtime_error("Cataclysm weapon table could not be identified uniquely");
            std::array<float,60> table;std::memcpy(table.data(),bytes.data()+found,sizeof(table));
            for(float value:table)if(!std::isfinite(value)||value<0||value>100000)
                throw std::runtime_error("Invalid Cataclysm weapon table");
            for(unsigned i=54;i<60;i++)if(table[i]!=0)throw std::runtime_error("Unsupported Cataclysm weapon table layout");
            weaponDamage=table;weaponDamageLoaded=true;
        }
        return weaponDamage[weaponClass*3+armorClass];
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm weapons] %s\n",e.what());return -1;}
}
extern "C" const char *hwCataOriginalText(unsigned address){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        auto offset=originalDataOffset(address,1);const auto& bytes=executableData();size_t length=0;
        while(length<4096 && offset+length<bytes.size() && bytes[offset+length])length++;
        if(length==4096 || offset+length>=bytes.size())throw std::runtime_error("Unterminated original text");
        originalDataOffset(address,length+1);
        return (const char*)bytes.data()+offset;
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm text] %08X: %s\n",address,e.what());return nullptr;}
}
extern "C" const char *hwCataLocalizedText(unsigned table,unsigned language){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        if(language>=3)language=0;
        auto offset=originalDataOffset(table+language*4,4);return hwCataOriginalText(u32(executableData().data()+offset));
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm text] %08X: %s\n",table,e.what());return nullptr;}
}
#include "CataclysmEnergyFx.inc"
extern "C" const void *hwCataCompatibleShipView(const char *name,int *size){
    std::lock_guard<std::recursive_mutex> lock(mutex);if(size)*size=-1;
    try{
        if(!name)return nullptr;auto path=key(name);if(path.rfind("cataclysm/",0)==0)path.erase(0,10);
        if(path.rfind("etg/cata_",0)==0)return hwCataCompatibleEffectView(name,size);
        if(path.size()<4 || path.substr(path.size()-4)!=".shp")return hwCataDataView(name,size);
        auto cacheKey="@compatible/"+path;auto found=cache.find(cacheKey);
        if(found==cache.end()){
            int length;auto *bytes=(const char*)hwCataDataView(path.c_str(),&length);if(!bytes)return nullptr;
            std::istringstream lines(scriptText(bytes,length));std::ostringstream out;std::string line;
            bool pending=false,inGun=false;std::map<std::string,std::string> gun;
            const char *sounds[]={"GS_LargeEnergyCannon","GS_LargeIonCannon","GS_LargePlasmaBomb","GS_LargeProjectile",
                "GS_MediumEnergyCannon","GS_MediumIonCannon","GS_MediumPlasmaBomb","GS_MediumProjectile","GS_MineLauncher",
                "GS_MissleLauncher","GS_SmallEnergyCannon","GS_SmallIonCannon","GS_SmallPlasmaBomb","GS_SmallProjectile",
                "GS_VeryLargeEnergyCannon","GS_VeryLargeIonCannon","GS_VeryLargePlasmaBomb","GS_Laser"};
            while(std::getline(lines,line)){
                std::string text=line;auto comment=text.find(';');if(comment!=text.npos)text.erase(comment);
                comment=text.find("//");if(comment!=text.npos)text.erase(comment);
                std::istringstream fields(text);std::string field,value;fields>>field>>value;field=key(field);
                if(!inGun && field=="gun")pending=true;
                if(pending && field=="{"){inGun=true;pending=false;gun.clear();}
                if(inGun && field=="}"){
                    /* Explicit HW1 damage entries are left alone. Cataclysm
                       weapons use the original class table and DamageMult. */
                    if(gun.count("startupgradelevel")){
                        auto weapon=key(gun["startupgradelevel"]),sizeName=key(gun["gunsize"]);
                        int sizeIndex=sizeName=="gunsize_small"?0:sizeName=="gunsize_medium"?1:sizeName=="gunsize_large"?2:sizeName=="gunsize_verylarge"?3:-1;
                        int sound=-1,damageClass=-1;bool disabled=false;const char *bullet="BULLET_Projectile";
                        if(sizeIndex<0)throw std::runtime_error("Unsupported Cataclysm gun size");
                        const int projectile[]={13,7,3,-1},energy[]={10,4,0,14},ion[]={11,5,1,15},plasma[]={12,6,2,16};
                        if(weapon=="weapon_projectile")sound=projectile[sizeIndex];
                        else if(weapon=="weapon_energy")sound=energy[sizeIndex];
                        else if(weapon=="weapon_ion"){sound=ion[sizeIndex];bullet="BULLET_Beam";}
                        else if(weapon=="weapon_plasmabomb"){sound=plasma[sizeIndex];bullet="BULLET_PlasmaBomb";}
                        else if(weapon=="weapon_missile")sound=9;
                        else if(weapon=="weapon_mine")sound=8;
                        else if(weapon=="weapon_laser"){sound=17;bullet="BULLET_Laser";}
                        else if(weapon=="weapon_biggun" || weapon=="weapon_beastinfect"){
                            /* Special commands cannot fire as ordinary guns in
                               Mission 1. Keep their indices and MEX hardpoints. */
                            sound=3;damageClass=weapon=="weapon_biggun"?18:19;disabled=true;
                        }
                        if(sound<0)throw std::runtime_error("Unsupported Cataclysm weapon class: "+weapon);
                        if(damageClass<0)damageClass=sound;
                        /* Somtaaw missile launchers require later research and
                           a special-fire command, neither available in M1. */
                        if(weapon=="weapon_missile" &&
                           (path=="race_sect/sacolyte.shp" || path=="race_sect/sacv.shp"))disabled=true;
                        float multiplier=0;std::istringstream factor(gun["damagemult"]);std::string trailing;
                        if(!(factor>>multiplier)||(factor>>trailing)||!std::isfinite(multiplier)||multiplier<0)
                            throw std::runtime_error("Invalid Cataclysm DamageMult");
                        float base=hwCataWeaponDamage(damageClass,0);if(base<0)throw std::runtime_error("Owned Cataclysm weapon table is unavailable");
                        out<<"CataWeapon TRUE\nCataWeaponClass "<<damageClass<<"\nCataWeaponDisabled "<<(disabled?"TRUE":"FALSE")<<"\n";
                        out<<"DamageLo "<<base*multiplier<<"\nDamageHi "<<base*multiplier<<"\nSoundType "<<sounds[sound]<<"\nBulletType "<<bullet<<"\n";
                    }
                    inGun=false;
                }else if(inGun && !field.empty() && field!="{")gun[field]=value;
                auto replace=[&](const char *from,const char *to){size_t at=line.find(from);if(at!=line.npos)line.replace(at,std::strlen(from),to);};
                replace("GUN_HiddenGimble","GUN_Gimble");
                replace("GUN_VisibleGimble","GUN_NewGimble");
                replace("NAVLIGHT_FadeOnOff","NAVLIGHT_Default");
                out<<line<<'\n';
            }
            if(inGun||pending)throw std::runtime_error("Truncated Cataclysm gun definition");
            auto compatible=out.str();found=cache.emplace(cacheKey,std::vector<unsigned char>(compatible.begin(),compatible.end())).first;
        }
        if(size)*size=(int)found->second.size();return found->second.data();
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm ship] %s: %s\n",name?name:"",e.what());return nullptr;}
}
extern "C" int hwCataShipEconomy(const char *raceDirectory,const char *shipStem,CataShipEconomy *economy,char *error,size_t capacity){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(economy)std::memset(economy,0,sizeof(*economy));
    try{
        if(!raceDirectory || !shipStem || !economy)throw std::runtime_error("Missing Cataclysm economy identity");
        auto race=key(raceDirectory),stem=key(shipStem);
        if(race.rfind("cataclysm/",0)==0)race.erase(0,10);
        while(!race.empty()&&race.back()=='/')race.pop_back();
        if((race!="race_sect"&&race!="race_kushan"&&race!="race_taidan"&&race!="race_derelict"&&race!="race_p1") || stem.empty() ||
           stem.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=stem.npos)
            throw std::runtime_error("Invalid Cataclysm economy identity");
        auto number=[&](const std::string& path,const std::string& wanted,bool optional=false){
            int size;auto *bytes=(const char*)hwCataDataView(path.c_str(),&size);
            if(!bytes || size<1 || size>1024*1024)throw std::runtime_error("Missing Cataclysm economy definition: "+path);
            std::istringstream lines(scriptText(bytes,size));std::string line;int result=0;bool found=false;
            while(std::getline(lines,line)){
                auto at=line.find(';');if(at!=line.npos)line.erase(at);
                at=line.find("//");if(at!=line.npos)line.erase(at);
                std::istringstream fields(line);std::string name;fields>>name;
                if(key(name)!=wanted)continue;
                std::string trailing;
                if(found || !(fields>>result) || (fields>>trailing) || result < -1 || result>1000000)
                    throw std::runtime_error("Invalid Cataclysm economy value: "+wanted);
                found=true;
            }
            if(!found && !optional)throw std::runtime_error("Missing Cataclysm economy field: "+wanted);
            return result;
        };
        auto path=race+"/"+stem+".shp";CataShipEconomy parsed{};
        /* Retail derelict ship statics start at zero and omit support fields.
           They are mission objects, never player construction choices. */
        parsed.supportRequired=number(path,"supportunits",race=="race_derelict");
        parsed.supportProvided=number(path,"providessu",race=="race_derelict");
        parsed.typeLimit=race=="race_derelict"?0:number("unitlimitcaps.script","cap"+stem);
        parsed.totalLimit=number("unitlimitcaps.script","shiptotalcaps");
        int size;auto bytes=(const char *)hwCataDataView(path.c_str(),&size);
        std::istringstream lines(scriptText(bytes,size));std::string line;
        while(std::getline(lines,line)){
            std::istringstream fields(line);std::string name;fields>>name;
            name=key(name);
            if(name=="fowradius"){
                if(!(fields>>parsed.sensorRadius) || !std::isfinite(parsed.sensorRadius) || parsed.sensorRadius<0 || parsed.sensorRadius>10000000)
                    throw std::runtime_error("Invalid owned sensor radius");
            }else if(name=="upgradebydocking"){
                std::string value;fields>>value;value=key(value);
                if(value!="true" && value!="false")throw std::runtime_error("Invalid owned upgrade docking value");
                parsed.upgradeByDocking=value=="true";
            }else if(name=="upgradetimemodifier"){
                if(!(fields>>parsed.upgradeTimeModifier) || !std::isfinite(parsed.upgradeTimeModifier) || parsed.upgradeTimeModifier<0)
                    throw std::runtime_error("Invalid owned upgrade time modifier");
            }
        }
        if(parsed.supportRequired<0 || parsed.supportProvided<0 || parsed.totalLimit<1)
            throw std::runtime_error("Invalid Cataclysm support budget");
        *economy=parsed;if(error&&capacity)*error=0;return 1;
    }catch(const std::exception& e){if(error&&capacity)std::snprintf(error,capacity,"%s",e.what());return 0;}
}
extern "C" int hwCataShipNavigation(const char *shipStem,CataShipNavigation *navigation,char *error,size_t capacity){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(navigation)std::memset(navigation,0,sizeof(*navigation));
    try{
        if(!shipStem||!navigation)throw std::runtime_error("Missing Cataclysm navigation identity");
        int size=0;auto bytes=(const char*)hwCataDataView("aiship.script",&size);
        if(!bytes||size<1)throw std::runtime_error("Owned Cataclysm AIShip.script is missing");
        std::istringstream lines(scriptText(bytes,size));std::string line;bool found=false;
        while(std::getline(lines,line)){
            std::replace(line.begin(),line.end(),',',' ');
            std::istringstream fields(line);std::string name,ship;fields>>name>>ship;
            if(key(name)!="rowdetails"||key(ship)!=key(shipStem))continue;
            if(found||!(fields>>navigation->priority>>navigation->avoidBy)||navigation->priority<0||navigation->priority>127||
               !std::isfinite(navigation->avoidBy)||navigation->avoidBy<0)
                throw std::runtime_error("Invalid original Cataclysm right-of-way entry");
            found=true;
        }
        /* Original global arrays are zero-initialized for unlisted modules. */
        if(error&&capacity)*error=0;return 1;
    }catch(const std::exception& e){if(error&&capacity)std::snprintf(error,capacity,"%s",e.what());return 0;}
}
extern "C" int hwCataAudioPath(const char *name,char *path,size_t capacity){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!name || !path || !capacity || ownedDirectory.empty())return 0;
    if(std::strcmp(name,"SectSpeech.vce") && std::strcmp(name,"Music4Cat.wxd"))return 0;
    auto file=ownedDirectory/name;auto text=file.string();
    if(!fs::is_regular_file(file) || text.size()>=capacity)return 0;
    std::memcpy(path,text.c_str(),text.size()+1);return 1;
}
extern "C" int hwCataAnimaticPaths(int sequence, char *movie, size_t movieCapacity,
                                   char *script, size_t scriptCapacity)
{
    if (!movie || !movieCapacity || !script || !scriptCapacity) return 0;
    movie[0] = script[0] = '\0';
    if (sequence < 0) return 0;

    std::lock_guard<std::recursive_mutex> lock(mutex);
    int size;
    const char *listing = static_cast<const char *>(hwCataDataView("movies/animatics.lst", &size));
    if (!listing || size <= 0 || ownedDirectory.empty()) return 0;

    std::istringstream lines(scriptText(listing, size));
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        int index;
        std::string name;
        if (!(fields >> index >> name) || index != sequence) continue;
        if (name.size() > 31 || name.find_first_not_of(
             "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != name.npos) return 0;

        auto path = ownedDirectory / "Movies" / (name + ".bik");
        std::error_code error;
        if (!fs::is_regular_file(path, error)) return 0;
        auto pathText = path.u8string();
        std::string scriptPath = "Cataclysm\\Movies\\" + name + ".script";
        if (pathText.size() >= movieCapacity || scriptPath.size() >= scriptCapacity) return 0;
        std::memcpy(movie, pathText.c_str(), pathText.size() + 1);
        std::memcpy(script, scriptPath.c_str(), scriptPath.size() + 1);
        return 1;
    }
    return 0;
}
extern "C" int hwCataResearchDuration(const char *technology){
    if(!technology)return -1;int size=0;auto bytes=(const char *)hwCataDataView("researchdepend.script",&size);
    if(!bytes)return -1;
    std::istringstream lines(scriptText(bytes,size));std::string line,wanted="SPRS";wanted+=technology;
    while(std::getline(lines,line)){
        auto comment=line.find(';');if(comment!=line.npos)line.resize(comment);
        std::istringstream fields(line);std::string name;int duration;
        if(fields>>name>>duration && name==wanted && duration>0 && duration<86400)return duration;
    }
    return -1;
}
extern "C" int hwCataTechnologyIndex(const char *technology){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        if(!technology)return -1;
        const auto& bytes=executableData();
        for(unsigned row=0;row<43;row++){
            auto at=originalDataOffset(0x008901a8+row*8,8);
            unsigned index=u32(bytes.data()+at)&65535u,address=u32(bytes.data()+at+4);
            if(!address)break;
            const char *name=hwCataOriginalText(address);if(!name)return -1;
            if(key(name)==key(technology))return index<42?(int)index:-1;
        }
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm technologies] %s\n",e.what());}
    return -1;
}
extern "C" unsigned hwCataTechnologyAbilityMask(unsigned technology){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        unsigned mask=0;const auto& bytes=executableData();
        /* 00514140 walks 25 {technology,primary,secondary} data records. */
        auto at=originalDataOffset(0x0072b9e8,25*12);
        for(unsigned row=0;row<25;row++,at+=12)
            if(u32(bytes.data()+at)==technology)mask|=u32(bytes.data()+at+4)|u32(bytes.data()+at+8);
        return mask;
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm upgrade map] %s\n",e.what());return 0;}
}
extern "C" unsigned hwCataShipAllowedUpgradeMask(unsigned originalShip){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        if(originalShip>=145)return 0;
        auto at=originalDataOffset(0x0072b7a0+originalShip*4,4);
        return u32(executableData().data()+at);
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm hull upgrades] %s\n",e.what());return 0;}
}
extern "C" int hwCataSpeechPhraseRandom(unsigned event,int variable,CataSpeechRandom random,CataSpeechPhrase *phrase,char *error,size_t capacity){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        if(!phrase)throw std::runtime_error("Missing speech output");
        *phrase={};int sentenceSize=0,phraseSize=0;
        auto sentence=(const unsigned char *)hwCataDataView("soundfx/sectsentence.lut",&sentenceSize);
        auto bank=(const unsigned char *)hwCataDataView("soundfx/sectphrase.lut",&phraseSize);
        auto word=[](const unsigned char *p){return unsigned(p[0])|(unsigned(p[1])<<8);};
        if(!sentence || !bank || sentenceSize<16 || phraseSize<12 ||
           std::memcmp(sentence,"LUT1",4) || std::memcmp(bank,"LUT2",4) || u32(sentence+4)!=u32(bank+4))
            throw std::runtime_error("Owned speech lookup tables are missing or incompatible");
        unsigned columns=word(sentence+8),events=word(sentence+10),actors=word(sentence+12);
        unsigned index=event&0x7ffu;
        if(columns<4 || actors!=1 || index>=events || 16ull+2ull*columns*events*actors>(unsigned)sentenceSize)
            throw std::runtime_error("Unsupported owned speech lookup dimensions");
        auto row=sentence+16+2*columns*index;
        unsigned variations=word(row+2),variables=word(row+4),base=word(row+6);
        if(!variations || !variables || variable<0 || (unsigned)variable>=variables)
            throw std::runtime_error("Campaign speech event is absent from the owned speech bank");
        unsigned bankColumns=word(bank+8),bankRows=word(bank+10),selected=base+(unsigned)variable;
        if(bankColumns<4 || selected>=bankRows || 12ull+4ull*bankColumns*bankRows>(unsigned)phraseSize)
            throw std::runtime_error("Invalid owned speech phrase table");
        if(random && variations>1){
            if(variations>8 || uint64_t(base)+uint64_t(variations)*variables>bankRows)
                throw std::runtime_error("Invalid owned speech variation table");
            auto& rotation=speechRotations[{index,variable}];
            if(!rotation){
                // Original 00522300: cumulative byte weights, adjacent swaps,
                // then a packed nibble sequence consumed by 005223c0. State
                // stays in our side table; owned phrase bytes are read-only.
                unsigned order[8],weights[8],cumulative=0;
                for(unsigned v=0;v<variations;v++){
                    unsigned value=bank[12+4*bankColumns*(selected+v*variables)+4];
                    weights[v]=(value-cumulative)&255u;cumulative=value;order[v]=v;
                }
                for(unsigned v=1;v<variations;v++){
                    unsigned slot=random()%(variations-1);
                    if(weights[slot]<=weights[slot+1]){
                        std::swap(weights[slot],weights[slot+1]);std::swap(order[slot],order[slot+1]);
                    }
                }
                for(unsigned v=variations;v>0;--v)rotation=(rotation<<4)|(order[v-1]+1);
            }
            unsigned variation=(rotation&15u)-1;rotation>>=4;
            if(variation>=variations)throw std::runtime_error("Invalid owned speech rotation");
            selected+=variation*variables;
        }
        auto entry=bank+12+4*bankColumns*selected;
        unsigned count=u32(entry+8)&255u;
        if(!count || count>32 || count>bankColumns-3)throw std::runtime_error("Invalid owned speech patch count");
        phrase->checksum=u32(sentence+4);phrase->bitrate=word(sentence+14);
        phrase->durationMilliseconds=u32(entry);phrase->count=count;
        for(unsigned i=0;i<count;i++){
            phrase->offsets[i]=u32(entry+12+4*i);
            if(phrase->offsets[i]==0xffffffffu)throw std::runtime_error("Missing owned speech patch");
        }
        if(error && capacity)*error=0;return 1;
    }catch(const std::exception& ex){if(error && capacity)std::snprintf(error,capacity,"%s",ex.what());return 0;}
}
extern "C" int hwCataSpeechPhrase(unsigned event,int variable,CataSpeechPhrase *phrase,char *error,size_t capacity){
    return hwCataSpeechPhraseRandom(event,variable,nullptr,phrase,error,capacity);
}
extern "C" const void *hwCataDataView(const char *name,int *size){
    std::lock_guard<std::recursive_mutex> lock(mutex);if(size)*size=-1;
    if(!name)return nullptr;auto normalized=key(name);if(normalized.rfind("cataclysm/",0)==0)normalized.erase(0,10);
    auto at=entries.find(normalized);if(at==entries.end())return nullptr;
    try {
        auto found=cache.find(normalized);
        if(found==cache.end()){
            auto data=expand(at->second);
            if(normalized.size()>4 && normalized.substr(normalized.size()-4)==".nis")cinematic(data);
            if(normalized.size()>4 && (normalized.substr(normalized.size()-4)==".geo" ||
                normalized.substr(normalized.size()-4)==".peo"))mesh(data);
            if(normalized.size()>4 && normalized.substr(normalized.size()-4)==".lod"){
                std::string text(data.begin(),data.end());size_t pos=0;
                while((pos=text.find(".peo",pos))!=text.npos){text.replace(pos,4,".geo");pos+=4;}
                data.assign(text.begin(),text.end());
            }
            found=cache.emplace(normalized,std::move(data)).first;
        }
        if(size)*size=(int)found->second.size();return found->second.data();
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm] %s: %s\n",normalized.c_str(),e.what());return nullptr;}
}
