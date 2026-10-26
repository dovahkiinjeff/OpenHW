/* Included by RemasteredVisualPackage.cpp so all native tooling shares the
   exact same authoring parser as the game. No Lua or generated disk cache. */
#include "OpenHWShipDocument.h"
#include <map>
#include <set>
#include <cstdlib>

namespace openhwDocument {
struct Document {std::vector<std::string> model;std::string gameplay;};
static std::string trim(const std::string& s) {
    const auto a=s.find_first_not_of(" \t\r\n");
    return a==s.npos?std::string():s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
static Document parse(const std::string& text,const char* header) {
    std::istringstream input(text);std::string line,pending;Document result;bool body=false,seen=false,closed=false;
    if(!std::getline(input,line)||trim(line)!=header)throw std::runtime_error("invalid OpenHW document header");
    while(std::getline(input,line)) {
        auto clean=trim(line);
        if(!body && (!pending.empty() || clean.rfind("StartShipHardPointConfig(",0)==0)) {
            pending+=clean+" ";
            if(clean.find(')')==std::string::npos)continue;
            clean=trim(pending);pending.clear();
        }
        if(clean=="[Ship]"||clean=="[Weapons]") {if(seen)throw std::runtime_error("duplicate settings section");body=seen=true;continue;}
        if(clean=="[End]") {if(!body)throw std::runtime_error("unexpected settings end");body=false;closed=true;continue;}
        if(body)result.gameplay+=line+"\n";
        else if(!clean.empty())result.model.push_back(clean);
    }
    if(!seen||!closed||body||!pending.empty())throw std::runtime_error("missing/unterminated settings section or declaration");
    return result;
}
static std::vector<std::string> words(const std::string& line) {
    std::istringstream input(line);std::vector<std::string> result;std::string word;
    while(input>>word)result.push_back(word);return result;
}
static std::vector<std::string> call(const std::string& line) {
    auto start=line.find('('),end=line.rfind(')');std::vector<std::string> result;
    if(start==line.npos||end==line.npos||trim(line.substr(end+1))!=";")throw std::runtime_error("invalid hardpoint declaration");
    bool quote=false;std::string field;
    for(size_t i=start+1;i<end;i++) {char c=line[i];if(c=='\"'){quote=!quote;continue;}
        if(c==','&&!quote){result.push_back(trim(field));field.clear();}else field+=c;}
    if(quote)throw std::runtime_error("unterminated hardpoint string");result.push_back(trim(field));return result;
}
static std::string source(HwRmPackageReadFile read,HwRmPackageReleaseFile release,void* user,const std::string& path) {
    const void* data=nullptr;size_t size=0;
    if(!read||!release||!read(user,path.c_str(),&data,&size)||!data||size>1024*1024) {
        if(data&&release)release(user,data);throw std::runtime_error("cannot read subsystem: "+path);
    }
    std::string text(static_cast<const char*>(data),size);release(user,data);return text;
}
struct Guns {std::string settings;std::map<unsigned,std::string> blocks,cues,weaponFire;};
static Guns guns(const std::string& text) {
    Guns result;std::istringstream in(text);std::string line,block;int depth=0;bool active=false,opened=false;unsigned index=0;
    while(std::getline(in,line)) {
        auto w=words(line);
        if(!active&&w.size()>=2&&w[0]=="GUN") {index=integer(w[1],63);active=true;opened=false;block.clear();}
        else if(!active) {
            if(!w.empty()&&(w[0]=="FireAnimation"||w[0]=="WeaponFire"))
                throw std::runtime_error("weapon presentation settings must be inside a GUN block");
            if(w.empty()||w[0]!="NUMBER_OF_GUNS")result.settings+=line+"\n";continue;
        }
        else {
            auto clean=line.substr(0,line.find(';'));clean=clean.substr(0,clean.find("//"));
            auto fields=words(clean);
            if(!fields.empty() && fields[0]=="FireAnimation") {
                if(depth!=1 || fields.size()!=2 || !token(fields[1]) ||
                   !result.cues.emplace(index,fields[1]).second)
                    throw std::runtime_error("invalid/duplicate weapon FireAnimation");
                continue; // presentation metadata is not an HW1 balance field
            }
            if(!fields.empty() && fields[0]=="WeaponFire") {
                if(depth!=1 || fields.size()!=2 || !token(fields[1],true) ||
                   !result.weaponFire.emplace(index,fields[1]).second)
                    throw std::runtime_error("invalid/duplicate weapon WeaponFire");
                continue;
            }
            block+=line+"\n";
        }
        auto code=line.substr(0,line.find(';'));code=code.substr(0,code.find("//"));
        for(char c:code){if(c=='{'){depth++;opened=true;}else if(c=='}')depth--;}
        if(depth<0)throw std::runtime_error("unbalanced weapon block");
        if(opened&&depth==0){if(!result.blocks.emplace(index,block).second)throw std::runtime_error("duplicate weapon index");active=false;}
    }
    if(active)throw std::runtime_error("unterminated weapon block");return result;
}
static std::string compile(const std::string& text,HwRmPackageReadFile read,HwRmPackageReleaseFile release,void* user,bool gameplay,std::map<std::pair<std::string,unsigned>,unsigned>* completed=nullptr) {
    auto doc=parse(text,"OPENHW_SHIP_2");auto main=guns(doc.gameplay);
    std::string rig="OPENHW_SHIP_1\n";std::set<std::string> slots;unsigned component=0;
    std::map<std::pair<std::string,unsigned>,unsigned> assignments;
    std::map<std::string,std::vector<std::string>> hardpoints;
    for(const auto& line:doc.model){auto w=words(line);if(w[0]=="hardpoint") {
        if(w.size()!=9||!token(w[1])||!token(w[2])||!hardpoints.emplace(w[1],w).second)throw std::runtime_error("invalid/duplicate authored hardpoint");}}
    for(const auto& line:doc.model) {auto w=words(line);if(w[0]=="weaponSlot") {
        if(w.size()!=4||!token(w[1]))throw std::runtime_error("invalid weaponSlot");
        if(!assignments.emplace(std::make_pair(w[1],integer(w[2],63)),integer(w[3],63)).second)throw std::runtime_error("duplicate weaponSlot");}}
    std::set<unsigned> reserved;for(const auto& item:main.blocks)reserved.insert(item.first);
    for(const auto& item:assignments)if(!reserved.insert(item.second).second)throw std::runtime_error("overlapping ship weapon slots");
    std::set<std::pair<std::string,unsigned>> used;
    for(const auto& line:doc.model) {
        auto w=words(line);if(w[0]=="weaponSlot"||w[0]=="hardpoint")continue;
        if(w[0]=="weaponEvent" || w[0]=="weaponFire")throw std::runtime_error("author presentation settings inside their GUN block");
        if(line.rfind("StartShipHardPointConfig(",0)!=0){rig+=line+"\n";continue;}
        auto a=call(line);
        if(a.size()!=7||a[0]!="NewShipType"||!token(a[1])||!token(a[2])||!token(a[6])||a[4]!="Innate"||a[5]!="Ship"||(a[3]!="Weapon"&&a[3]!="Engine"&&a[3]!="Utility"))throw std::runtime_error("invalid innate subsystem declaration");
        if(!slots.insert(a[1]).second)throw std::runtime_error("duplicate hardpoint slot");
        const std::string root="@/Subsystems/"+a[6]+"/";
        auto subText=source(read,release,user,root+"subsystem.owsubsystem");
        // Legacy library entries are explicitly unarmed visual templates.
        // Their rig remains usable; configured weapons require a v2 section.
        const bool legacy=subText.rfind("OPENHW_SUBSYSTEM_1",0)==0;
        auto sub=parse(legacy?subText+"\n[Weapons]\n[End]\n":subText,legacy?"OPENHW_SUBSYSTEM_1":"OPENHW_SUBSYSTEM_2");
        auto weapons=guns(sub.gameplay);const unsigned role=a[3]=="Weapon"?1:a[3]=="Engine"?2:3;
        if(completed && role==1 && weapons.blocks.empty())throw std::runtime_error("Weapon subsystem "+a[1]+" ("+a[6]+") has no weapon configuration; select an armed subsystem");
        std::string anchor=a[2];auto authored=hardpoints.find(anchor);
        if(authored!=hardpoints.end())anchor=authored->second[2];
        else if(anchor.size()<9||anchor.substr(anchor.size()-9)!="_Position")anchor+="_Position";
        ++component;bool model=false;
        std::map<unsigned,unsigned> mapping;
        for(const auto& weapon:weapons.blocks) {
            auto key=std::make_pair(a[1],weapon.first);auto assigned=assignments.find(key);unsigned index=0;
            if(assigned!=assignments.end()){index=assigned->second;used.insert(key);}
            else {while(reserved.count(index))index++;if(index>63)throw std::runtime_error("ship exceeds 64 weapons");reserved.insert(index);}
            mapping[weapon.first]=index;main.blocks.emplace(index,weapon.second);
            if(completed)(*completed)[key]=index;
            auto cue=weapons.cues.find(weapon.first);
            if(cue!=weapons.cues.end())main.cues.emplace(index,cue->second);
            auto fire=weapons.weaponFire.find(weapon.first);
            if(fire!=weapons.weaponFire.end())main.weaponFire.emplace(index,
                fire->second.rfind("@/",0)==0?fire->second:root+fire->second);
        }
        std::set<unsigned> bound;
        for(const auto& row:sub.model) {
            auto f=words(row);
            if(f[0][0]=='#')continue;
            if(f[0]=="hull"&&f.size()==2&&token(f[1],true)){if(model)throw std::runtime_error("duplicate subsystem model");model=true;rig+="attach "+a[1]+" "+anchor+" "+std::to_string(role)+" "+(f[1].rfind("@/",0)==0?f[1]:root+f[1])+"\n";}
            else if(f[0]=="surface"&&f.size()==4&&f[1]=="0"&&token(f[3],true))rig+="surface "+std::to_string(component)+" "+f[2]+" "+(f[3].rfind("@/",0)==0?f[3]:root+f[3])+"\n";
            else if(f[0]=="presentation"&&f.size()==3&&f[1]=="Hull"&&token(f[2],true))
                rig+="presentation "+a[1]+" "+(f[2].rfind("@/",0)==0?f[2]:root+f[2])+"\n";
            else if(f[0]=="gun"&&f.size()==7&&mapping.count(integer(f[1],63))) {
                if(!bound.insert(integer(f[1],63)).second)throw std::runtime_error("duplicate subsystem weapon binding");
                rig+="gun "+std::to_string(mapping.at(integer(f[1],63)))+" "+a[1];for(unsigned j=3;j<7;j++)rig+=" "+f[j];rig+="\n";
            } else if(f[0]!="type"&&f[0]!="scale"&&f[0]!="mad")throw std::runtime_error("invalid subsystem model/weapon binding");
        }
        if(!model)throw std::runtime_error("subsystem has no HOD");
        if(bound.size()!=weapons.blocks.size())throw std::runtime_error("subsystem weapon has no joint binding");
        if(authored!=hardpoints.end()){rig+="mount "+a[1];for(unsigned i=3;i<9;i++)rig+=" "+authored->second[i];rig+="\n";}
    }
    if(used.size()!=assignments.size())throw std::runtime_error("weaponSlot refers to missing subsystem weapon");
    for(const auto& cue:main.cues)rig+="weaponEvent "+std::to_string(cue.first)+" "+cue.second+"\n";
    for(const auto& weapon:main.weaponFire)rig+="weaponFire "+std::to_string(weapon.first)+" "+weapon.second+"\n";
    rig+="gameplay __compiled__.shp\n";
    // Authoring indices are stable references, not array offsets. Removing a
    // turret may leave a gap: compact both views together, never only one.
    std::map<unsigned,unsigned> compact;unsigned count=0;
    for(const auto& item:main.blocks)compact[item.first]=count++;
    {std::istringstream input(rig);std::string line,rewritten;
     while(std::getline(input,line)){auto w=words(line);
      if((w.size()==7&&w[0]=="gun") || (w.size()==3&&(w[0]=="weaponEvent"||w[0]=="weaponFire"))){
       auto index=compact.find(integer(w[1],63));
       if(index==compact.end())throw std::runtime_error("joint binding refers to missing weapon settings");
       w[1]=std::to_string(index->second);line=w[0];for(unsigned i=1;i<w.size();i++)line+=" "+w[i];
      }rewritten+=line+"\n";
     }rig=rewritten;}
    if(!gameplay)return rig;
    std::string settings;std::istringstream input(main.settings);std::string line;
    while(std::getline(input,line)){auto w=words(line);
     if(w.size()>=3&&w[0]=="BindGun"&&w[2].rfind("Gun",0)==0){
      const auto end=w[2].find_first_not_of("0123456789",3);
      const auto digits=w[2].substr(3,end==std::string::npos?end:end-3);
      auto index=compact.find(integer(digits,63));if(index==compact.end())continue;
      const auto at=line.find(w[2],line.find(w[1])+w[1].size());line.replace(at,3+digits.size(),"Gun"+std::to_string(index->second));
     }settings+=line+"\n";
    }
    std::string output=settings+"\nNUMBER_OF_GUNS "+std::to_string(count)+"\n";
    for(const auto& item:main.blocks)output+="\nGUN "+std::to_string(compact.at(item.first))+"\n"+item.second;
    return output;
}
}
extern "C" int openhwShipDocumentCompile(const void* source,size_t size,HwRmPackageReadFile read,HwRmPackageReleaseFile release,void* user,int gameplay,void** output,size_t* outputSize,char* error,size_t capacity) {
    if(output)*output=nullptr;if(outputSize)*outputSize=0;
    try {if(!source||!size||size>1024*1024||!output||!outputSize)throw std::runtime_error("invalid ship document");
        auto text=openhwDocument::compile(std::string(static_cast<const char*>(source),size),read,release,user,gameplay!=0);
        *output=std::malloc(text.size()+1);if(!*output)throw std::bad_alloc();std::memcpy(*output,text.c_str(),text.size()+1);*outputSize=text.size();return 1;
    }catch(const std::exception& e){message(error,capacity,e.what());return 0;}
}
extern "C" void openhwShipDocumentFree(void* output){std::free(output);}
extern "C" int openhwShipDocumentComplete(const void* source,size_t size,HwRmPackageReadFile read,HwRmPackageReleaseFile release,void* user,void** output,size_t* outputSize,char* error,size_t capacity) {
    if(output)*output=nullptr;if(outputSize)*outputSize=0;
    try {
        if(!source||!size||size>1024*1024||!output||!outputSize)throw std::runtime_error("invalid ship document");
        std::string text(static_cast<const char*>(source),size);
        std::map<std::pair<std::string,unsigned>,unsigned> assignments;
        openhwDocument::compile(text,read,release,user,false,&assignments);
        auto doc=openhwDocument::parse(text,"OPENHW_SHIP_2");
        std::string result="OPENHW_SHIP_2\n";
        for(const auto& row:doc.model)if(openhwDocument::words(row)[0]!="weaponSlot")result+=row+"\n";
        for(const auto& item:assignments)result+="weaponSlot "+item.first.first+" "+std::to_string(item.first.second)+" "+std::to_string(item.second)+"\n";
        result+="\n[Ship]\n"+doc.gameplay+"[End]\n";
        *output=std::malloc(result.size()+1);if(!*output)throw std::bad_alloc();
        std::memcpy(*output,result.c_str(),result.size()+1);*outputSize=result.size();return 1;
    }catch(const std::exception& e){message(error,capacity,e.what());return 0;}
}
extern "C" int openhwShipDocumentLegacyVisual(const void* source,size_t size){
    try{if(!source||!size||size>1024*1024)return 0;auto doc=openhwDocument::parse(std::string(static_cast<const char*>(source),size),"OPENHW_SHIP_2");
        return std::find(doc.model.begin(),doc.model.end(),"visual legacy")!=doc.model.end();
    }catch(...){return 0;}
}
extern "C" int openhwShipDocumentNormalize(const void* source,size_t size,void** output,size_t* outputSize,char* error,size_t capacity){
    if(output)*output=nullptr;if(outputSize)*outputSize=0;
    try {if(!source||!size||size>1024*1024||!output||!outputSize)throw std::runtime_error("invalid ship document");
        auto doc=openhwDocument::parse(std::string(static_cast<const char*>(source),size),"OPENHW_SHIP_2");std::string text="OPENHW_SHIP_2\n";
        for(const auto& row:doc.model){
            if(row.rfind("StartShipHardPointConfig(",0)==0){
                const auto a=openhwDocument::call(row);
                if(a.size()!=7||a[0]!="NewShipType"||!token(a[1])||!token(a[2])||!token(a[6])||a[4]!="Innate"||a[5]!="Ship"||(a[3]!="Weapon"&&a[3]!="Engine"&&a[3]!="Utility"))throw std::runtime_error("invalid innate subsystem declaration");
                text+="StartShipHardPointConfig(NewShipType";
                for(size_t i=1;i<a.size();++i)text+=", \""+a[i]+"\"";
                text+=");\n";
            }else text+=row+"\n";
        }
        text+="\n[Ship]\n"+doc.gameplay+"[End]\n";
        *output=std::malloc(text.size()+1);if(!*output)throw std::bad_alloc();std::memcpy(*output,text.c_str(),text.size()+1);*outputSize=text.size();return 1;
    }catch(const std::exception&e){message(error,capacity,e.what());return 0;}
}
