#include "../src/Game/OpenHWArchive.h"
#include "../src/Modern/OpenHWMods.h"
#include "../src/Modern/RemasteredVisualPackage.h"
#include "../src/Modern/OpenHWShipDocument.h"
#include "../src/Modern/RemasteredFxGraph.h"
#include "../src/Modern/RemasteredFxResources.h"
#include "../src/Modern/RemasteredSoundBank.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <set>
#include <cmath>
#include <array>
#include <algorithm>
// Optional migration check: ignore formatting/comments, not gameplay values.
static std::multiset<std::string> settingsValues(const std::string& text){
    std::multiset<std::string> result;std::istringstream input(text);std::string line,gun;bool countSeen=false;
    while(std::getline(input,line)){
        auto comment=line.find(';');if(comment!=line.npos)line.resize(comment);
        comment=line.find("//");if(comment!=line.npos)line.resize(comment);
        std::istringstream row(line);std::string key,value,normalized;if(!(row>>key))continue;
        if(key=="GUN"){row>>gun;continue;}if(key=="{")continue;if(key=="}"){gun.clear();continue;}
        if(key=="NUMBER_OF_GUNS")countSeen=true;
        normalized=gun+":"+key;while(row>>value)normalized+=" "+value;result.insert(normalized);
    }if(!countSeen)result.insert(":NUMBER_OF_GUNS 0");return result;
}
static std::filesystem::path library;
static std::filesystem::path overlay;
static std::vector<void*> allocations;
static int read(void *user,const char *name,const void **bytes,size_t *size){
    int n;const std::string path=!std::strncmp(name,"@/",2)?name+2:*static_cast<std::string*>(user)+name;
    if(!overlay.empty())for(const auto& candidate:{overlay/"Data"/path,overlay/path}) {
        std::ifstream in(candidate,std::ios::binary|std::ios::ate);if(!in)continue;auto length=in.tellg();
        if(length<=0||length>64*1024*1024)return 0;void* memory=malloc(size_t(length));if(!memory)return 0;in.seekg(0);
        if(!in.read(static_cast<char*>(memory),length)){free(memory);return 0;}allocations.push_back(memory);*bytes=memory;*size=size_t(length);return 1;
    }
    *bytes=openhwArchiveView(path.c_str(),&n);*size=n<0?0:static_cast<size_t>(n);if(*bytes&&n>0)return 1;
    if(library.empty())return 0;
    std::ifstream input(library/path,std::ios::binary|std::ios::ate);if(!input)return 0;
    auto length=input.tellg();if(length<=0||length>64*1024*1024)return 0;void* memory=malloc(size_t(length));if(!memory)return 0;input.seekg(0);
    if(!input.read(static_cast<char*>(memory),length)){free(memory);return 0;}allocations.push_back(memory);*bytes=memory;*size=size_t(length);return 1;
}
static void release(void*,const void* bytes){for(auto i=allocations.begin();i!=allocations.end();++i)if(*i==bytes){free(*i);allocations.erase(i);return;}}
static int configure(void*,HwRmVisualAsset*,const char*,char*,size_t){return 1;}
struct AuthorNode {unsigned component;int parent;std::string slot,name;bool geometry;std::array<float,16> matrix;};
static void captureAuthorNode(void* user,const HwRmSubsystemNode* node){
    auto& rows=*static_cast<std::vector<AuthorNode>*>(user);AuthorNode row{node->component,node->parent,node->slot,node->jointName,node->part!=nullptr,{}};
    std::copy(node->localMatrix,node->localMatrix+16,row.matrix.begin());rows.push_back(row);
}
static bool authorRoundTrip(HwRmVisualPackage* package,const std::string& text,const std::string& type,std::string& root,char*error,size_t capacity){
    const float scale=hwRmVisualPackageScale(package);auto* asset=hwRmVisualPackageTakeAsset(package);auto* instance=hwRmVisualInstanceCreate(asset);
    if(!instance){hwRmVisualAssetFree(asset);return false;}
    std::vector<AuthorNode> nodes;bool ok=hwRmSubsystemVisitHierarchy(hwRmVisualInstanceSubsystem(instance),0,scale,255,captureAuthorNode,&nodes,error,capacity)!=0;
    std::ostringstream declarations;declarations.precision(9);
    for(const auto& node:nodes){if(!node.component||node.geometry)continue;const auto&m=node.matrix;
        const float y=std::asin(std::clamp(-m[2],-1.f,1.f)),x=std::abs(m[2])<.999999f?std::atan2(m[6],m[10]):std::atan2(-m[9],m[5]),z=std::abs(m[2])<.999999f?std::atan2(m[1],m[0]):0;
        const auto parent=node.parent>=0&&nodes.at(node.parent).component==node.component?nodes.at(node.parent).name:"-";
        declarations<<"joint "<<node.slot<<' '<<node.name<<' '<<parent<<' '<<m[12]/scale<<' '<<m[13]/scale<<' '<<m[14]/scale<<' '<<x*57.29577951f<<' '<<y*57.29577951f<<' '<<z*57.29577951f<<'\n';
    }
    auto rewritten=text;rewritten.insert(rewritten.find("[Ship]"),declarations.str());
    auto* saved=ok?hwRmVisualPackageRead(type.c_str(),rewritten.data(),rewritten.size(),read,release,configure,&root,error,capacity):nullptr;
    if(!saved)ok=false;
    if(saved){auto* savedAsset=hwRmVisualPackageTakeAsset(saved);auto* savedInstance=hwRmVisualInstanceCreate(savedAsset);
        std::vector<AuthorNode> result;if(!savedInstance||!hwRmSubsystemVisitHierarchy(hwRmVisualInstanceSubsystem(savedInstance),0,scale,255,captureAuthorNode,&result,error,capacity)||result.size()!=nodes.size())ok=false;
        if(ok)for(size_t i=0;i<nodes.size();i++)for(unsigned j=0;j<16;j++)if(std::abs(nodes[i].matrix[j]-result[i].matrix[j])>.002f){std::snprintf(error,capacity,"joint round-trip changed %s.%s",nodes[i].slot.c_str(),nodes[i].name.c_str());ok=false;}
        hwRmVisualInstanceFree(savedInstance);hwRmVisualAssetFree(savedAsset);hwRmVisualPackageFree(saved);
    }
    hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);return ok;
}
int main(int argc,char **argv){
    if(argc==7&&!std::strcmp(argv[1],"--publish-ship")){
        overlay=argv[2];openhwArchiveSetLooseRoot(argv[2]);std::string root=std::filesystem::path(argv[3]).parent_path().generic_string()+"/";
        std::ifstream input(argv[4],std::ios::binary);std::string text((std::istreambuf_iterator<char>(input)),{});char error[1024]{};void* complete=nullptr;size_t length=0;
        if(!openhwShipDocumentComplete(text.data(),text.size(),read,release,&root,&complete,&length,error,sizeof(error))){fprintf(stderr,"%s\n",error);return 1;}
        auto* package=hwRmVisualPackageRead(argv[5],complete,length,read,release,configure,&root,error,sizeof(error));if(!package){fprintf(stderr,"%s\n",error);openhwShipDocumentFree(complete);return 1;}hwRmVisualPackageFree(package);
        int ok=openhwModPublish(argv[2],argv[6],0,argv[3],(char*)complete,length,0,error,sizeof(error));openhwShipDocumentFree(complete);printf("%s\n",error);return ok==1?0:1;
    }
    if((argc==5||argc==6) && !std::strcmp(argv[1],"--loose-ship")) {
        if(argc==6)library=argv[5];
        overlay=argv[2];std::string root=std::filesystem::path(argv[3]).parent_path().generic_string()+"/";
        const void* bytes=nullptr;size_t size=0;char error[1024]{};
        std::string empty;
        if(!read(&empty,argv[3],&bytes,&size)){fprintf(stderr,"Cannot read definition\n");return 1;}
        auto* package=hwRmVisualPackageRead(argv[4],bytes,size,read,release,configure,&root,error,sizeof(error));
        if(!package){fprintf(stderr,"%s\n",error);release(nullptr,bytes);return 1;}
        void* gameplay=nullptr;size_t length=0;
        if(!openhwShipDocumentCompile(bytes,size,read,release,&root,1,&gameplay,&length,error,sizeof(error))){fprintf(stderr,"%s\n",error);return 1;}
        auto* asset=hwRmVisualPackageTakeAsset(package);auto* instance=hwRmVisualInstanceCreate(asset);
        if(!instance)return 1;
        const unsigned count=hwRmVisualInstanceGunCount(instance);
        for(unsigned ordinal=0;ordinal<count;++ordinal){unsigned gun;float matrix[16];
            if(!hwRmVisualInstanceGunIndex(instance,ordinal,&gun)||
               !hwRmVisualInstanceSetGun(instance,gun,.15f,.1f,0,error,sizeof(error))||
               !hwRmVisualInstanceGunMuzzle(instance,gun,hwRmVisualPackageScale(package),matrix,error,sizeof(error))){fprintf(stderr,"%s\n",error);return 1;}
            printf("Gun %u: aiming and muzzle valid\n",gun);
        }
        printf("Validated %u native gun bindings. Compiled gameplay:\n%s\n",count,(char*)gameplay);
        openhwShipDocumentFree(gameplay);hwRmVisualInstanceFree(instance);hwRmVisualAssetFree(asset);hwRmVisualPackageFree(package);release(nullptr,bytes);return 0;
    }
    if(argc<2||argc>4)return 2;char error[1024]{};if(argc>=3)library=std::filesystem::path(argv[2]);if(argc==4)overlay=std::filesystem::path(argv[3]);
    if(!openhwArchiveMount(std::filesystem::path(argv[1]).wstring().c_str(),error,sizeof(error))){fprintf(stderr,"%s\n",error);return 1;}
    if(!library.empty()) {
        const auto presentation=library/"OpenHW-Presentation.obig";
        if(std::filesystem::is_regular_file(presentation) &&
           !openhwArchiveMountPresentation(presentation.wstring().c_str(),error,sizeof(error))) {
            fprintf(stderr,"Presentation archive: %s\n",error);return 1;
        }
    }
    if(openhwArchiveView("RemasteredFX/Native/effects.fxbank",nullptr)) {
        int graphSize=0,resourceSize=0,soundSize=0;
        const auto* graphBytes=openhwArchiveView("RemasteredFX/Native/effects.fxbank",&graphSize);
        const auto* resourceBytes=openhwArchiveView("RemasteredFX/Native/effects.fxresources",&resourceSize);
        const auto* soundBytes=openhwArchiveView("RemasteredAudio/sfx/effects.sfxbank",&soundSize);
        if(!resourceBytes||!soundBytes){fprintf(stderr,"Incomplete native presentation banks\n");return 1;}
        auto* graph=hwRmFxGraphRead(graphBytes,graphSize,error,sizeof(error));
        if(!graph){fprintf(stderr,"FX bank: %s\n",error);return 1;}
        auto* resources=hwRmFxResourcesRead(resourceBytes,resourceSize,error,sizeof(error));
        if(!resources){fprintf(stderr,"FX resources: %s\n",error);hwRmFxGraphFree(graph);return 1;}
        auto* sounds=hwRmSoundBankRead(soundBytes,soundSize,error,sizeof(error));
        if(!sounds){fprintf(stderr,"Sound bank: %s\n",error);hwRmFxResourcesFree(resources);hwRmFxGraphFree(graph);return 1;}
        printf("Validated native presentation: %u FX nodes, %u sound nodes.\n",
               hwRmFxGraphCount(graph),hwRmSoundBankCount(sounds));
        hwRmSoundBankFree(sounds);hwRmFxResourcesFree(resources);hwRmFxGraphFree(graph);
    }
    const void *cursor=openhwArchiveView("cursorbitmaps/cursor.lif",nullptr);
    if(!cursor || openhwArchiveView("CursorBitmaps//cursor.LiF",nullptr)!=cursor ||
       openhwArchiveView("CursorBitmaps\\\\cursor.LiF",nullptr)!=cursor ||
       openhwArchiveView("cursorbitmaps//../cursor.lif",nullptr)){
        fprintf(stderr,"Startup cursor missing or legacy path normalization failed\n");return 1;
    }
    unsigned definitions=0;
    if(std::getenv("OPENHW_VERIFY_EMBEDDED_TEXTURES")){
        unsigned hods=0,bindings=0;std::set<std::string> shaders;
        for(unsigned i=0;i<openhwArchiveCount();++i){
            const std::string path=openhwArchiveName(i);
            if(path.size()<4||path.substr(path.size()-4)!=".hod")continue;
            int size=0;const void* bytes=openhwArchiveView(path.c_str(),&size);HwRmHodScene* scene=nullptr;
            if(!hwRmHodSceneRead(bytes,size,&scene,error,sizeof(error))){fprintf(stderr,"HOD %s: %s\n",path.c_str(),error);return 1;}
            for(unsigned m=0;m<hwRmHodSceneMaterialCount(scene);++m){const auto* material=hwRmHodSceneMaterial(scene,m);shaders.insert(material->shader);
                for(unsigned p=0;p<material->parameterCount;++p){const auto& parameter=material->parameters[p];if(parameter.type!=5)continue;
                    const auto* texture=hwRmHodSceneTexture(scene,parameter.texture);
                    if(!texture){hwRmHodSceneFree(scene);fprintf(stderr,"Invalid texture index in %s\n",path.c_str());return 1;}
                    for(unsigned mip=0;mip<texture->mipCount;++mip){HwRmHodTextureView view{};
                        if(!hwRmHodMaterialTexture(scene,m,parameter.name,mip,&view,error,sizeof(error))||!view.bytes||!view.size){fprintf(stderr,"Embedded texture %s: %s\n",path.c_str(),error);hwRmHodSceneFree(scene);return 1;}
                    }++bindings;
                }
            }hwRmHodSceneFree(scene);++hods;
        }
        printf("Verified original embedded texture mips: %u HODs, %u material bindings. Shaders:",hods,bindings);
        for(const auto& shader:shaders)printf(" %s",shader.c_str());printf("\n");
    }
    if(std::getenv("OPENHW_VERIFY_SHIP_ROSTER")){
        unsigned count=0;
        for(unsigned i=0;i<openhwArchiveCount();++i){
            const std::string source=openhwArchiveName(i);const auto slash=source.find('/');
            if(slash==source.npos||source.find('/',slash+1)!=source.npos||source.size()<4||source.substr(source.size()-4)!=".shp")continue;
            const auto race=source.substr(0,slash),name=source.substr(slash+1);
            if((race!="r1"&&race!="r2"&&race!="p1"&&race!="p2"&&race!="p3"&&race!="traders")||name=="mine.shp"||name=="missile.shp")continue;
            const std::string path=source.substr(0,source.size()-4)+"/rmnative/ship.owshipdef";
            char resolved[32768];
            if(!openhwModOverride(path.c_str(),resolved,sizeof(resolved))){fprintf(stderr,"Production lookup missed definition: %s\n",path.c_str());return 1;}
            std::ifstream input(resolved,std::ios::binary);std::string text((std::istreambuf_iterator<char>(input)),{});
            if(text.rfind("OPENHW_SHIP_2",0)!=0){fprintf(stderr,"Production lookup opened old definition: %s\n",resolved);return 1;}
            std::string root=path.substr(0,path.find_last_of('/')+1);void* compiled=nullptr;size_t length=0;
            if(!openhwShipDocumentCompile(text.data(),text.size(),read,release,&root,1,&compiled,&length,error,sizeof(error))){fprintf(stderr,"Roster %s: %s\n",source.c_str(),error);return 1;}
            int originalSize=0;const void* original=openhwArchiveView(source.c_str(),&originalSize);
            bool same=original&&settingsValues(std::string(static_cast<const char*>(original),originalSize))==settingsValues(std::string(static_cast<char*>(compiled),length));
            openhwShipDocumentFree(compiled);if(!same){fprintf(stderr,"Roster settings changed: %s\n",source.c_str());return 1;}++count;
        }
        printf("Production Data lookup and complete gameplay verified for %u ship definitions.\n",count);
    }
    for(unsigned i=0;i<openhwArchiveCount();i++){
        std::string path=openhwArchiveName(i);
        if(path.size()<14||path.substr(path.size()-14)!="ship.owshipdef")continue;
        size_t size;const void *bytes;std::string empty;if(!read(&empty,path.c_str(),&bytes,&size))return 1;
        std::string text(static_cast<const char*>(bytes),size),type;
        auto at=text.find("\ntype ");if(at==std::string::npos){fprintf(stderr,"Missing identity: %s\n",path.c_str());return 1;}
        at+=6;type=text.substr(at,text.find_first_of("\r\n",at)-at);
        std::string root=path.substr(0,path.find_last_of('/')+1);
        if(text.find("\nvisual legacy\n")!=std::string::npos || text.find("\nvisual legacy\r\n")!=std::string::npos) {
            // Editable Classic-only ships have complete gameplay documents
            // but intentionally no native HOD (e.g. P3 frigate/destroyer).
            void* settings=nullptr;size_t length=0;
            if(!openhwShipDocumentCompile(bytes,size,read,release,&root,1,&settings,&length,error,sizeof(error))){fprintf(stderr,"Legacy gameplay %s: %s\n",path.c_str(),error);return 1;}
            openhwShipDocumentFree(settings);release(nullptr,bytes);++definitions;continue;
        }
        HwRmVisualPackage *package=hwRmVisualPackageRead(type.c_str(),bytes,size,read,release,configure,&root,error,sizeof(error));
        if(!package){fprintf(stderr,"%s: %s\n",path.c_str(),error);return 1;}
        char gameplay[256];
        if(text.rfind("OPENHW_SHIP_2",0)==0) {
            void* settings=nullptr;size_t length;
            if(!openhwShipDocumentCompile(bytes,size,read,release,&root,1,&settings,&length,error,sizeof(error))){fprintf(stderr,"Gameplay %s: %s\n",path.c_str(),error);return 1;}
            if(std::getenv("OPENHW_VERIFY_MIGRATION")){
                int originalSize=0;const void* original=openhwArchiveView((root+"gameplay.shp").c_str(),&originalSize);
                if(!original||settingsValues(std::string(static_cast<const char*>(original),originalSize))!=settingsValues(std::string(static_cast<const char*>(settings),length))){fprintf(stderr,"Migration changed gameplay values: %s\n",path.c_str());openhwShipDocumentFree(settings);return 1;}
            }
            openhwShipDocumentFree(settings);
            if(std::getenv("OPENHW_VERIFY_MIGRATION")&&!authorRoundTrip(package,text,type,root,error,sizeof(error))){fprintf(stderr,"Authoring %s: %s\n",path.c_str(),error);return 1;}
        } else if(type.compare(0,9,"Derelict_") && (!hwRmShipDefinitionGameplay(bytes,size,type.c_str(),gameplay,sizeof(gameplay))||
           !openhwArchiveView((root+gameplay).c_str(),nullptr))){fprintf(stderr,"Missing gameplay: %s\n",path.c_str());return 1;}
        hwRmVisualPackageFree(package);release(nullptr,bytes);definitions++;
    }
    if(!openhwArchiveView("r1/mothership.shp",nullptr)||!definitions){fprintf(stderr,"Incomplete OpenHW base package\n");return 1;}
    unsigned subsystems=0;
    if(!library.empty()) {
        std::ifstream catalog(library/"Subsystems/catalog.txt");std::string name;
        while(std::getline(catalog,name)){if(!name.empty()&&name.back()=='\r')name.pop_back();if(name.empty())continue;
            std::string root="Subsystems/"+name+"/";const void* bytes;size_t size;
            if(!read(&root,"subsystem.owsubsystem",&bytes,&size)){fprintf(stderr,"Missing subsystem: %s\n",name.c_str());return 1;}
            const std::string identity="Subsystem_"+name;
            auto* package=hwRmVisualPackageRead(identity.c_str(),bytes,size,read,release,configure,&root,error,sizeof(error));release(nullptr,bytes);
            if(!package){fprintf(stderr,"Invalid subsystem %s: %s\n",name.c_str(),error);return 1;}hwRmVisualPackageFree(package);subsystems++;
        }
        std::string root="r1/standardfrigate/rmnative/";int size;const void* bytes=openhwArchiveView((root+"ship.owshipdef").c_str(),&size);
        if(!bytes)return 1;std::string text(static_cast<const char*>(bytes),size);
        text+="\nsubsystem TestSubsystem Hardpoint_Gun0_Position 3 kus_assaultgun\nmount TestSubsystem 1 2 3 0 45 90\n";
        auto* package=hwRmVisualPackageRead("StandardFrigate",text.data(),text.size(),read,release,configure,&root,error,sizeof(error));
        if(!package){fprintf(stderr,"Shared subsystem mount failed: %s\n",error);return 1;}hwRmVisualPackageFree(package);
    }
    printf("Validated %u resources and %u native definitions, LODs 0..4; %u library subsystems.\n",openhwArchiveCount(),definitions,subsystems);return 0;
}
