#include "CataclysmData.h"
#include "CataclysmScriptIds.h"
#include "../src/Game/CampaignMissionPaths.h"
#include "../src/Game/AssetPath.h"
#include "../src/Game/CataclysmSlipgateRecord.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <set>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void put(std::vector<unsigned char>& data,size_t offset,uint32_t value){std::memcpy(data.data()+offset,&value,4);}
struct Member {std::string name;std::vector<unsigned char> bytes;uint32_t expanded;};
static void archive(const std::filesystem::path& path,const std::vector<Member>& members){
    std::vector<unsigned char> data(15+32*members.size());std::memcpy(data.data(),"RBF1.23",7);put(data,7,(uint32_t)members.size());put(data,11,1);
    for(size_t i=0;i<members.size();i++){
        const auto& m=members[i];size_t record=15+32*i;
        put(data,record+8,(uint32_t)m.name.size()|0x00670000u);put(data,record+12,(uint32_t)m.bytes.size());put(data,record+16,m.expanded);put(data,record+20,(uint32_t)data.size());
        unsigned char previous=0xd5;for(unsigned char c:m.name){data.push_back(previous^c);previous=c;}data.push_back(0);
        data.insert(data.end(),m.bytes.begin(),m.bytes.end());
    }
    std::ofstream out(path,std::ios::binary);out.write((const char*)data.data(),data.size());
}
int main(int argc,char** argv){
    char error[512];int size;unsigned failures=0;
    try{
        CataSlipgateRecord gate{};
        require(cataParseSlipgateRecord(",70000.0,-200000.0,0.0,1/2,0.0,dSlipgateLarge",&gate) &&
            gate.x==70000 && gate.y==-200000 && gate.index==1 && gate.total==2,"Original slipgate scene grammar rejected");
        require(!cataParseSlipgateRecord(",0,0,0,0/2,0,dSlipgateLarge",&gate) &&
            !cataParseSlipgateRecord(",0,0,0,1/2,0,dMessagePod",&gate),"Invalid gate definition accepted");
        char missionDirectory[SP_MISSION_PATH_CAPACITY], missionFile[SP_MISSION_PATH_CAPACITY];
        require(hwCampaignMissionPaths(MISSION_CATA_1_HIIGARA,0,missionDirectory,sizeof(missionDirectory),missionFile,sizeof(missionFile)),
            "Cataclysm mission path did not fit production buffers");
        const std::string missionPath=std::string(missionDirectory)+missionFile;
        require(missionPath=="Cataclysm\\SinglePlayer\\Mission01\\Mission01.mission",
            "Cataclysm startup path was corrupted");
        require(hwCampaignMissionPaths(MISSION_CATA_2_OUTSKIRTS,0,missionDirectory,sizeof(missionDirectory),missionFile,sizeof(missionFile)) &&
            std::string(missionDirectory)+missionFile=="Cataclysm\\SinglePlayer\\Mission02\\Mission02.mission",
            "Cataclysm Mission 2 resolved into the HW1 campaign namespace");
        require(hwCataclysmAdjacentMission(MISSION_CATA_1_HIIGARA, 1) == MISSION_CATA_2_OUTSKIRTS &&
            hwCataclysmAdjacentMission(MISSION_CATA_2_OUTSKIRTS, -1) == MISSION_CATA_1_HIIGARA &&
            hwCataclysmAdjacentMission(MISSION_CATA_2_OUTSKIRTS, 1) == MISSION_ENUM_NOT_INITIALISED &&
            hwCataclysmAdjacentMission(MISSION_1_KHARAK_SYSTEM, 1) == MISSION_ENUM_NOT_INITIALISED,
            "Cataclysm's jump crossed into the Homeworld campaign or missed its next level");
        struct {char directory[32];unsigned guard;} small={{0},0x12345678};
        require(!hwCampaignMissionPaths(MISSION_CATA_1_HIIGARA,0,small.directory,sizeof(small.directory),missionFile,sizeof(missionFile)) &&
            small.guard==0x12345678 && !small.directory[0],"Short mission buffer was not safely rejected");
        require(hwCampaignMissionPaths(MISSION_1_KHARAK_SYSTEM,0,missionDirectory,sizeof(missionDirectory),missionFile,sizeof(missionFile)) &&
            std::string(missionDirectory)+missionFile=="SinglePlayer\\Mission01\\Mission01.mission","HW1 mission path changed");
        require(hwCampaignMissionPaths(MISSION_5B_TURANIC_RAIDER_PLANETOID,1,missionDirectory,sizeof(missionDirectory),missionFile,sizeof(missionFile)) &&
            std::string(missionDirectory)+missionFile=="SinglePlayer\\Mission05_OEM\\Mission05_OEM.mission","Raider Retreat path changed");
        char visual[256];
        const char* engineDirectory="Cataclysm\\RACE_SECT\\";
        const char* engineMesh="sMothershipEnginesLower\\Rl0\\LOD0\\sMothershipEnginesLower.geo";
        struct {char path[80];unsigned guard;} oldMeshBuffer={{0},0x12345678};
        char enginePath[FL_Path];
        require(!fileBuildPath(oldMeshBuffer.path,sizeof(oldMeshBuffer.path),engineDirectory,engineMesh) &&
            oldMeshBuffer.guard==0x12345678 && !oldMeshBuffer.path[0],"Oversized engine path was not safely rejected");
        require(fileBuildPath(enginePath,sizeof(enginePath),engineDirectory,engineMesh) &&
            std::string(enginePath)==std::string(engineDirectory)+engineMesh,"Long engine path was truncated");
        require(hwCataAnimationPath("Cataclysm\\RACE_TAIDAN\\","tHeavyInterceptor.shp",visual,sizeof(visual)) &&
            !std::strcmp(visual,"cataclysm/race_taidan/theavyinterceptor.mad"),"Taiidan animation lost its owned race prefix");
        require(hwCataAnimationPath("Cataclysm/RACE_SECT/","sAcolyte.shp",visual,sizeof(visual)) &&
            !std::strcmp(visual,"cataclysm/race_sect/sacolyte.mad"),"Somtaaw animation lost its owned ship name");
        require(!hwCataAnimationPath("R1/","HeavyInterceptor.shp",visual,sizeof(visual)),"OpenHW animations were redirected into owned data");
        require(!hwCataAnimationPath("Cataclysm/RACE_TAIDAN/","../outside.shp",visual,sizeof(visual)),"Animation traversal was accepted");
        require(cataScriptHash("mission01")==0x087e6ef1u && cataScriptHash("FleetIntel")==0x0a8a880cu &&
            cataScriptHash("Objectives")==0x098ddc43u && cataScriptHash("TaiidanBombers")==0x04fcebd3u,"Retail script IDs do not match recovered mission");
        cataScriptScopedKey(0x0a8a880c,0x80001234,visual,sizeof(visual));
        require(!std::strcmp(visual,"C00000000_00001234"),"Explicit global variable scope failed");
        cataScriptScopedKey(0x0a8a880c,0x00001234,visual,sizeof(visual));
        require(!std::strcmp(visual,"C0A8A880C_00001234"),"Team variable scope failed");
        cataScriptVariableKey(0x0a8a880c,0x8afa30c4,visual,sizeof(visual));
        require(!std::strcmp(visual,"PlayNislet"),"Recovered intro request did not reach cinematic runtime");
        cataScriptVariableKey(0x0a8a880c,0x8c641d25,visual,sizeof(visual));
        require(!std::strcmp(visual,"Nislet1Complete"),"Recovered intro completion gate did not reach cinematic runtime");
        require(hwCataOpenHWVisualPath("Cataclysm\\RACE_KUSHAN\\","kHeavyInterceptor.lod",visual,sizeof(visual)) &&
            !std::strcmp(visual,"R1/heavyinterceptor.lod"),"Kushan model did not select OpenHW");
        require(hwCataOpenHWVisualPath("Cataclysm/RACE_KUSHAN/","kHeavyInterceptor/rl0/lod0/kHeavyInterceptor.mex",visual,sizeof(visual)) &&
            !std::strcmp(visual,"R1/heavyinterceptor/rl0/lod0/heavyinterceptor.mex"),"Kushan hardpoints did not select OpenHW");
        require(!hwCataOpenHWVisualPath("Cataclysm/RACE_KUSHAN/","kHeavyInterceptor.shp",visual,sizeof(visual)),"Kushan gameplay was replaced with HW1 data");
        require(!hwCataOpenHWVisualPath("Cataclysm/RACE_SECT/","sAcolyte.lod",visual,sizeof(visual)),"Somtaaw visual was redirected");
        unsigned nativeLod=0;
        require(hwCataRaiderMeshIdentity("Cataclysm\\RACE_P1\\P1StandardCorvette\\Rl0\\LOD3\\P1StandardCorvette.geo",
                    visual,sizeof(visual),&nativeLod) &&
                !std::strcmp(visual,"p1standardcorvette") && nativeLod==3,"Raider corvette did not resolve to its native identity");
        require(hwCataRaiderMeshIdentity("cataclysm/race_p1/p1fighter/rl0/lod1/p1fighter.peo",
                    visual,sizeof(visual),&nativeLod) &&
                !std::strcmp(visual,"p1fighter") && nativeLod==1,"Raider fighter did not resolve to its native identity");
        require(!hwCataRaiderMeshIdentity("cataclysm/race_p1/p1fighter/rl0/lod1/p1standardcorvette.geo",
                    visual,sizeof(visual),&nativeLod),"Mismatched Raider hull was redirected");
        require(hwCataTaiidanMeshIdentity("Cataclysm\\RACE_TAIDAN\\tIonCannonFrigate\\Rl0\\LOD3\\tIonCannonFrigate.geo",
                    visual,sizeof(visual),&nativeLod) &&
                !std::strcmp(visual,"ioncannonfrigate") && nativeLod==3,"Taiidan hull did not resolve to its native identity");
        require(hwCataTaiidanMeshIdentity("cataclysm/race_taidan/tcarrier/rl0/lod1/tcarrier.peo",
                    visual,sizeof(visual),&nativeLod) &&
                !std::strcmp(visual,"carrier") && nativeLod==1,"Packed Taiidan hull did not resolve to its native identity");
        for (const char *path : {
                "Cataclysm/RACE_TAIDAN/tcarrier/rl0/lod0/tattackbomber.geo",
                "Cataclysm/RACE_TAIDAN/tcarrier/rl0/lod5/tcarrier.geo",
                "Cataclysm/RACE_TAIDAN/tcarrier/rl0/lod0/tcarrier.mex",
                "Cataclysm/RACE_BEAST/tcarrier/rl0/lod0/tcarrier.geo",
                "Cataclysm/RACE_TAIDAN/tcarrier/../rl0/lod0/tcarrier.geo"})
            require(!hwCataTaiidanMeshIdentity(path,visual,sizeof(visual),&nativeLod),
                    "Noncanonical Taiidan hull was redirected to a native package");
        if(argc>1){
            require(hwCataDataOpen(argv[1],error,sizeof(error))!=0,error);
            for (const char *name : {"Mission02.mission", "ResourceSphere_1.missphere",
                                     "ResourceSphere_2.missphere", "ResourceSphere_3.missphere", "Decorations"}) {
                const auto source = std::string("Cataclysm\\SinglePlayer\\Mission02\\") + name;
                require(hwCataDataView(source.c_str(), &size) && size > 0, source.c_str());
            }
            const auto *raider = static_cast<const char *>(hwCataCompatibleShipView(
                "Cataclysm/RACE_P1/P1StandardCorvette.shp", &size));
            require(raider != nullptr, "Owned Raider corvette gameplay missing");
            const std::string_view raiderSettings(raider, size);
            const std::string_view projectileClass("CataWeaponClass 7\n");
            const auto firstGun = raiderSettings.find(projectileClass);
            require(firstGun != raiderSettings.npos &&
                    raiderSettings.find(projectileClass, firstGun + projectileClass.size()) != raiderSettings.npos,
                    "Raider corvette lost one of its two medium projectile weapons");
            for (const char *stem : {"Cataclysm/RACE_SECT/sMimic",
                                     "Cataclysm/RACE_SECT/sMCV",
                                     "Cataclysm/RACE_KUSHAN/kAdvanceSupportFrigate",
                                     "Cataclysm/RACE_P1/p1StandardCorvette"}) {
                const auto hull = std::string(stem) + ".shp";
                const auto lod = std::string(stem) + ".lod";
                require(hwCataCompatibleShipView(hull.c_str(), &size) && size > 0, hull.c_str());
                require(hwCataDataView(lod.c_str(), &size) && size > 0, lod.c_str());
            }
            {
                char movie[1024], script[112];
                require(hwCataAnimaticPaths(1,movie,sizeof(movie),script,sizeof(script)),
                    "Owned transition from Cataclysm Mission 1 to Mission 2 cannot be resolved");
                std::ifstream video(std::filesystem::u8path(movie),std::ios::binary);
                char signature[4]={};
                require(video.read(signature,sizeof(signature)) &&
                    (!std::memcmp(signature,"BIK",3)||!std::memcmp(signature,"KB2",3)),
                    "Owned transition path does not point to a Bink video");
                require(hwCataDataView(script,&size)!=nullptr && size>0,"Owned timed dialogue is missing");
                CataSpeechPhrase phrase{};
                require(hwCataSpeechPhrase(0x40100000u|204u,0,&phrase,error,sizeof(error)) && phrase.count>0,
                    "Owned transition dialogue does not resolve through the Cataclysm bank");
            }
            require(hwCataTechnologyIndex("TECH_REPAIR")==6 && hwCataTechnologyIndex("TECH_SALVAGE")==7 &&
                hwCataTechnologyIndex("TECH_LINK")==24 && hwCataTechnologyIndex("TECH_MIMIC")==25,
                "Original technology identity table changed");
            require(hwCataTechnologyAbilityMask(6)==0x40000u && hwCataTechnologyAbilityMask(7)==0x80000u &&
                hwCataTechnologyAbilityMask(24)==0x18u && hwCataTechnologyAbilityMask(25)==0x20u,
                "Original research-to-ability mapping changed");
            require((hwCataShipAllowedUpgradeMask(14)&0xc0000u)==0xc0000u &&
                !(hwCataShipAllowedUpgradeMask(14)&0x20u),"Worker hull upgrade permissions changed");
            require(hwCataShipAllowedUpgradeMask(122)==0,"Pod acquired player ability permissions");
            {
                const char *script=hwCataEnergyFxScript();require(script!=nullptr,"Owned energy FX profile missing");
                std::string energy(script);
                require(energy.find("R1,GS_LargeEnergyCannon,Cata_bulletnewenergylarge01.ebg")!=energy.npos,
                    "Kushan capital energy weapons lost Cataclysm energy bolts");
                require(energy.find("R2,GS_LargeEnergyCannon,Cata_bulletnewenergylarge01.ebg")!=energy.npos,
                    "Taiidan capital energy weapons lost Cataclysm energy bolts");
                require(energy.find("LoadEffect Cata_repair_corvette.ebg")!=energy.npos,
                    "Owned Worker repair beam was not registered");
                std::istringstream lines(energy);std::string line;unsigned effects=0;
                while(std::getline(lines,line))if(line.rfind("LoadEffect ",0)==0){
                    std::string path="Cataclysm/ETG/"+line.substr(11);
                    auto source=(const char*)hwCataCompatibleEffectView(path.c_str(),&size);
                    require(source&&size>0,"Missing owned energy effect dependency");++effects;
                }
                require(effects>4,"Energy muzzle/projectile/impact dependencies were not resolved");
                const auto *salvage=static_cast<const char *>(
                    hwCataCompatibleEffectView("Cataclysm/ETG/Cata_p1salvage.ebg",&size));
                require(salvage&&size>0,"Missing owned Turanic salvage effect");
                const std::string_view salvageScript(salvage,size);
                require(salvageScript.find("label(P1Salvage)")!=salvageScript.npos &&
                    salvageScript.find("setMeshAnimation(P1Salvage,")!=salvageScript.npos &&
                    salvageScript.find("setMeshAnimation(P1salvage,")==salvageScript.npos,
                    "Turanic salvage references an undefined mesh animation label");
                std::printf("Validated %u owned energy FX scripts and their racial event mappings.\n",effects);
            }
            for(const char *path:{"Cataclysm/RACE_KUSHAN/kStandardDestroyer.shp",
                    "Cataclysm/RACE_TAIDAN/tStandardDestroyer.shp",
                    "Cataclysm/RACE_KUSHAN/kHeavyCruiser.shp",
                    "Cataclysm/RACE_TAIDAN/tHeavyCruiser.shp"}){
                auto *translated=(const char*)hwCataCompatibleShipView(path,&size);
                require(translated!=nullptr,"Owned capital turret conversion failed");
                std::string converted(translated,size);
                require(converted.find("GUN_VisibleGimble")==converted.npos &&
                    converted.find("GUN_NewGimble")!=converted.npos,
                    "Owned capital weapon has an unsupported turret type");
                require(converted.find("GS_LargeEnergyCannon")!=converted.npos,
                    "Owned capital energy weapon class was lost");
            }
            require(hwCataNavLightTexturePath("etg\\textures\\star32",visual,sizeof(visual)) &&
                !std::strcmp(visual,"cataclysm/etg/textures/star32"),
                "Navigation light registered the HW1 texture path instead of the owned texture");
            require(!hwCataNavLightTexturePath("etg/textures/absent",visual,sizeof(visual)),
                "Missing Cataclysm texture would reach the registry");
            auto* engineLod=(const char*)hwCataDataView("Cataclysm/RACE_SECT/sMothershipEnginesLower.lod",&size);
            require(engineLod!=nullptr,"Owned lower-engine LOD script missing");
            std::string engineLodText(engineLod,size);
            std::replace(engineLodText.begin(),engineLodText.end(),'\r','\n');
            std::istringstream engineLodLines(engineLodText);
            std::string lodLine;unsigned lowerEngineMeshes=0;
            while(std::getline(engineLodLines,lodLine)){
                std::istringstream fields(lodLine);std::string directive,mesh;
                fields>>directive>>mesh;
                if(directive.rfind("pMeshFile",0)!=0)continue;
                require(fileBuildPath(enginePath,sizeof(enginePath),engineDirectory,mesh.c_str()),
                    "Owned lower-engine mesh path exceeds the loader capacity");
                require(hwCataDataView(enginePath,&size)!=nullptr,enginePath);
                lowerEngineMeshes++;
            }
            require(lowerEngineMeshes==5,"Owned lower-engine mesh load did not cover all levels");
            require(hwCataDataView(missionPath.c_str(),&size)!=nullptr && size>0,"Production startup path could not open owned Mission 1");
            auto originalLabel=hwCataOriginalText(0x008c83a8);
            require(originalLabel && std::string(originalLabel)=="DestroyBombers","Owned executable campaign text layout differs from recovery");
            require(hwCataOriginalText(0x008c7f90) && !_stricmp(hwCataOriginalText(0x008c7f90),"hyperspace"),
                "Final objective did not match the native hyperspace handshake");
            require(hwCataResearchDuration("TECH_LINK")==175,"Mission 1 Link research duration differs from owned definition");
            const unsigned speechEvents[]={0x100010d6,0x100010d8,0x10001162,0x100020da,0x100020dc,
                0x100020de,0x1000215c,0x1000215d,0x1000215e,0x1000215f,0x10002160,0x10002161,
                0x10002163,0x10002164,0x10002165,0x10002166,0x10002167,0x100080d7,0x100080d9,
                0x100080db,0x100080dd,0x020012ca};
            for(auto event:speechEvents){CataSpeechPhrase phrase{};require(hwCataSpeechPhrase(event,0,&phrase,error,sizeof(error))!=0,error);
                require(phrase.count>0 && phrase.bitrate>0 && phrase.durationMilliseconds>0,"Invalid Mission 1 speech descriptor");}
            std::puts("Resolved all 22 Mission 1 speech events and original campaign text.");
            for(unsigned ship:{0u,1u,4u,14u,16u,6u,18u}){
                std::set<unsigned> variants;
                for(unsigned v=0;v<4;v++){
                    CataSpeechPhrase phrase{};
                    require(hwCataSpeechPhraseRandom(0,(int)ship,[]{return 0u;},&phrase,error,sizeof(error))!=0,error);
                    require(phrase.count>0&&phrase.bitrate==512,"Invalid original selection voice");
                    char voicePath[1024];require(hwCataAudioPath("SectSpeech.vce",voicePath,sizeof(voicePath))!=0,"Missing selection bank");
                    std::ifstream voice(voicePath,std::ios::binary);char infoHeader[4]={};uint32_t subtitleSize=~0u;
                    voice.seekg(phrase.offsets[0]);voice.read(infoHeader,4);voice.read(reinterpret_cast<char*>(&subtitleSize),4);
                    require(voice && !std::memcmp(infoHeader,"INFO",4) && subtitleSize==0,
                        "Ship selection resolved narration/subtitles instead of an original acknowledgement");
                    variants.insert(phrase.offsets[0]);
                }
                require(variants.size()==4,"Original selection variation rotation repeated before exhausting its clips");
            }
            for(unsigned event:{0u,77u}){
                CataSpeechPhrase phrase{};require(hwCataSpeechPhraseRandom(event,0,[]{return 0u;},&phrase,error,sizeof(error))!=0,error);
                require(phrase.count>0,"Missing original group/Kuun-Lan selection acknowledgement");
            }
            for(const char *bank:{"SectSpeech.vce","Music4Cat.wxd"}){
                char path[1024];require(hwCataAudioPath(bank,path,sizeof(path))!=0,"Missing owned audio bank");
                std::ifstream file(path,std::ios::binary);uint32_t header[2]{};file.read((char *)header,sizeof(header));
                const char *lookup=bank[0]=='S'?"soundfx/sectsentence.lut":"soundfx/music4cat.wxh";
                auto bytes=(const unsigned char *)hwCataDataView(lookup,&size);uint32_t checksum=0;
                require(bytes && size>=8,"Missing owned audio lookup");std::memcpy(&checksum,bytes+4,4);
                require(file.good() && header[1]==checksum,"Owned audio and lookup checksums differ");
            }
            CataStartingFleet starting{};
            require(hwCataMission1StartingFleet(&starting,error,sizeof(error))!=0,error);
            std::printf("Mission 1 starting data: %d RU, %u ship entries, %u technologies.\n",starting.resources,starting.shipCount,starting.technologyCount);
            for(const auto& pair:std::vector<std::pair<const char*,const char*>>{
                {"soundfx/shipcmnevents.lut","soundfx/ships.bnk"},
                {"soundfx/gunevents.lut","soundfx/guns.bnk"},
                {"soundfx/shipevents.lut","soundfx/ships.bnk"},
                {"soundfx/specexpevents.lut","soundfx/specialeffects.bnk"},
                {"soundfx/spechitevents.lut","soundfx/specialeffects.bnk"},
                {"soundfx/uievents.lut","soundfx/ui.bnk"}}){
                int lutSize=0,bankSize=0;
                auto lut=(const unsigned char*)hwCataDataView(pair.first,&lutSize);
                auto bank=(const unsigned char*)hwCataDataView(pair.second,&bankSize);
                require(lut&&bank&&lutSize>=14&&bankSize>=12,"Missing owned Cataclysm sound table/bank");
                uint32_t lutChecksum,bankChecksum,patchCount;
                std::memcpy(&lutChecksum,lut+4,4);std::memcpy(&bankChecksum,bank+4,4);std::memcpy(&patchCount,bank+8,4);
                require(lutChecksum==bankChecksum&&patchCount>0,"Cataclysm sound table references another bank");
                uint16_t variations,events,objects;
                std::memcpy(&variations,lut+8,2);std::memcpy(&events,lut+10,2);std::memcpy(&objects,lut+12,2);
                require(lutSize==14+int(objects)*events*(variations+1)*2,"Invalid owned sound table dimensions");
                for(unsigned row=0;row<unsigned(objects)*events;++row){
                    int16_t count;std::memcpy(&count,lut+14+row*(variations+1)*2,2);
                    require(count>=0&&count<=variations,"Invalid original sound variation count");
                    for(int v=0;v<count;++v){int16_t patch;std::memcpy(&patch,lut+16+(row*(variations+1)+v)*2,2);
                        require(patch>=0&&unsigned(patch)<patchCount,"Owned sound event points outside its bank");}
                }
                if(!std::strcmp(pair.first,"soundfx/shipcmnevents.lut")){
                    auto patch=[&](unsigned ship,unsigned event){int16_t value;std::memcpy(&value,lut+16+(ship*events+event)*(variations+1)*2,2);return value;};
                    require(patch(1,0)==22&&patch(1,1)==-1,"Acolyte must use its retail engine and no invented ambient loop");
                    require(patch(14,0)==26&&patch(14,1)==44,"Worker lost its distinct retail engine and ambient loop");
                    require(patch(1,6)==52&&patch(1,7)==110,"Acolyte gun-arm sounds differ from retail");
                }
            }
            std::puts("Verified owned Cataclysm ship, gun, effect and UI sound bindings, including distinct Acolyte/Worker loops.");
            CataShipNavigation kushanCruiser{},taiidanCruiser{},workerNavigation{};
            require(hwCataShipNavigation("kHeavyCruiser",&kushanCruiser,error,sizeof(error))!=0,error);
            require(hwCataShipNavigation("tHeavyCruiser",&taiidanCruiser,error,sizeof(error))!=0,error);
            require(hwCataShipNavigation("sWorker",&workerNavigation,error,sizeof(error))!=0,error);
            require(kushanCruiser.priority==2&&taiidanCruiser.priority==4&&
                kushanCruiser.avoidBy==1200&&taiidanCruiser.avoidBy==1200,
                "Kushan/Taiidan cruiser right-of-way identities were merged");
            require(workerNavigation.priority==1&&workerNavigation.avoidBy==400,"Worker navigation used HW1 collector defaults");
            CataShipEconomy hull{},bay{},recon{};
            require(hwCataShipEconomy("race_sect","sMothership",&hull,error,sizeof(error))!=0,error);
            require(hwCataShipEconomy("race_sect","sMothershipDockingBay",&bay,error,sizeof(error))!=0,error);
            require(hwCataShipEconomy("race_sect","sRecon",&recon,error,sizeof(error))!=0,error);
            CataShipEconomy worker{},acolyte{},acv{};
            require(hwCataShipEconomy("race_sect","sWorker",&worker,error,sizeof(error))!=0,error);
            require(hwCataShipEconomy("race_sect","sAcolyte",&acolyte,error,sizeof(error))!=0,error);
            require(hwCataShipEconomy("race_sect","sACV",&acv,error,sizeof(error))!=0,error);
            require(!worker.upgradeByDocking && worker.upgradeTimeModifier==1.2f &&
                    acolyte.upgradeByDocking && acv.upgradeByDocking,
                    "Somtaaw retrofit rules differ from the owned hull definitions");
            CataShipEconomy pod{};
            require(hwCataShipEconomy("race_derelict","dMessagePod",&pod,error,sizeof(error))!=0,error);
            require(pod.supportRequired==0 && pod.supportProvided==0 && pod.typeLimit==0 && pod.sensorRadius==0,
                "Mission pod acquired a construction/support budget");
            require(hwCataCompatibleShipView("Cataclysm/RACE_DERELICT/dMessagePod.shp",&size)!=nullptr,
                "Owned mission pod ship definition did not load");
            unsigned gateRecords=0;
            for(unsigned member=0;member<hwCataDataCount();member++){
                std::string path=hwCataDataName(member);
                if(path.rfind("singleplayer/mission02/",0)!=0 || path.find(".missphere")==path.npos)continue;
                int length;const char *bytes=(const char*)hwCataDataView(path.c_str(),&length);require(bytes!=nullptr,path.c_str());
                std::istringstream lines(std::string(bytes,length));std::string line;
                while(std::getline(lines,line)){
                    std::istringstream fields(line);std::string kind;fields>>kind;if(kind!="Slipgate")continue;
                    std::string record;std::getline(fields,record);require(cataParseSlipgateRecord(record.c_str(),&gate),"Owned slipgate record did not parse");gateRecords++;
                }
            }
            require(gateRecords==2,"Mission 2 slipgate pair is incomplete");
            require(hwCataCompatibleShipView("Cataclysm/RACE_DERELICT/dSlipgateLarge.shp",&size)!=nullptr,
                "Owned slipgate hull definition did not load");
            for(const char *ship:{"P1Fighter","P1StandardCorvette","P1MissileCorvette","P1Mothership"}){
                CataShipEconomy raider{};std::string definition="Cataclysm/RACE_P1/";definition+=ship;definition+=".shp";
                require(hwCataCompatibleShipView(definition.c_str(),&size)!=nullptr,definition.c_str());
                require(hwCataShipEconomy("race_p1",ship,&raider,error,sizeof(error))!=0,error);
            }
            std::printf("Mission 1 support capacity: %d; Recon cost: %d support units.\n",hull.supportProvided+bay.supportProvided,recon.supportRequired);
            int missionSize;auto *mission=(const char*)hwCataDataView("singleplayer/mission01/mission01.mission",&missionSize);
            require(mission!=nullptr,"Owned mission definition missing");
            std::string missionText(mission,missionSize);for(char& c:missionText)if(c=='\r')c='\n';
            std::istringstream missionLines(missionText);std::string line;unsigned definitions=0,animations=0,dockHosts=0,nonHosts=0,navTextures=0;
            while(std::getline(missionLines,line)){
                for(char& c:line)if(c==',')c=' ';
                std::istringstream fields(line);std::string command,race,ship;fields>>command>>race>>ship;
                if(command!="IncludeShips")continue;
                if(!race.empty()&&race.back()==',')race.pop_back();
                if(race!="RACE_SECT"&&race!="RACE_KUSHAN"&&race!="RACE_TAIDAN")continue;
                std::string path="Cataclysm/"+race+"/"+ship+".shp";
                require(hwCataCompatibleShipView(path.c_str(),&size)!=nullptr,path.c_str());definitions++;
                if(race!="RACE_KUSHAN"){
                    auto* raw=(const char*)hwCataDataView(path.c_str(),&size);
                    require(raw!=nullptr,path.c_str());
                    std::istringstream shipLines(std::string(raw,size));std::string shipLine;
                    bool receives=false;std::vector<std::string> ports;
                    while(std::getline(shipLines,shipLine)){
                        for(char& c:shipLine)if(c==',')c=' ';
                        std::istringstream commandLine(shipLine);std::string binding,value;commandLine>>binding>>value;
                        if(binding=="canReceiveShips" || binding=="canReceiveResources" ||
                           binding=="canReceiveShipsPermanently" || binding=="canReceiveShipsForRetire")
                            receives|=(value=="TRUE" || value=="1");
                        if(binding=="DockPoint")ports.push_back(value);
                        if(binding=="NavLight"){
                            std::string type,texture;
                            float on,off,delay,radius;
                            unsigned lod;
                            if((commandLine>>type>>on>>off>>delay>>radius>>lod>>texture) &&
                                _stricmp(texture.c_str(),"NULL")){
                                require(hwCataNavLightTexturePath(texture.c_str(),visual,sizeof(visual)),texture.c_str());
                                navTextures++;
                            }
                        }
                        if(binding!="BindAnimations")continue;
                        require(hwCataAnimationPath(("Cataclysm/"+race+"/").c_str(),(ship+".shp").c_str(),visual,sizeof(visual)),
                            "Owned animation path resolution failed");
                        require(hwCataDataView(visual,&size)!=nullptr,visual);animations++;
                    }
                    if(race=="RACE_TAIDAN"){
                        if(receives){
                            require(ship=="tCarrier","Mission 1 owned docking host needs a compatibility policy");
                            for(const char* required:{"In1","In2","Out1","Out2","Frigate","Res0","Res1"})
                                require(std::find(ports.begin(),ports.end(),required)!=ports.end(),required);
                            dockHosts++;
                        }else nonHosts++;
                        if(ship=="tResourceCollector"){
                            require(!receives && std::find(ports.begin(),ports.end(),"Fight0")==ports.end(),
                                "Owned collector unexpectedly advertises HW1 refueling latches");
                        }
                    }
                }
                CataShipEconomy economy{};
                require(hwCataShipEconomy(race.c_str(),ship.c_str(),&economy,error,sizeof(error))!=0,error);
            }
            require(definitions>0,"Mission 1 ship inclusion table was not parsed");
            for(const char *member:{"btg/mission01.btg","hsf/mission01.hsf","derelicts/homeworld.shp",
                "derelicts/planetoforigin.shp","nis/nislet01.nis","nis/nislet01.script","soundfx/music4cat.wxh"})
                require(hwCataDataView(member,&size)!=nullptr,member);
            std::printf("Adapted %u Mission 1 ship/weapon definitions from owned data.\n",definitions);
            require(animations>0,"Mission 1 animation dependencies were not exercised");
            std::printf("Resolved %u Mission 1 owned animation dependencies.\n",animations);
            require(dockHosts>0 && nonHosts>0,"Owned docking host policy was not exercised");
            std::printf("Checked %u owned Taiidan docking hosts and %u non-host definitions.\n",dockHosts,nonHosts);
            require(navTextures>0,"Mission 1 navigation-light textures were not exercised");
            std::printf("Resolved %u owned navigation-light texture references.\n",navTextures);
            auto *hullView=(const char*)hwCataCompatibleShipView("Cataclysm/race_sect/sMothership.shp",&size);
            require(hullView!=nullptr,"Kuun-Lan weapon definition missing");
            std::string hullDefinition(hullView,size);
            require(hullDefinition.find("CataWeaponClass 18\nCataWeaponDisabled TRUE")!=hullDefinition.npos,
                "Mission 1 incorrectly enabled the Kuun-Lan siege cannon");
            unsigned packedMeshes=0,ordinaryMeshes=0;
            for(unsigned i=0;i<hwCataDataCount();i++){
                const char* name=hwCataDataName(i);
                auto* bytes=hwCataDataView(name,&size);
                if(!bytes){std::fprintf(stderr,"Unreadable: %s\n",name);failures++;continue;}
                std::string member(name);auto suffix=member.size()>4?member.substr(member.size()-4):"";
                if(suffix==".peo" || suffix==".geo"){
                    uint32_t meshVersion=0;
                    require(size>=68,"Adapted mesh header is truncated");
                    std::memcpy(&meshVersion,(const char*)bytes+8,4);
                    require(meshVersion==0x402,"Owned mesh bypassed native-layout conversion");
                    if(suffix==".peo")packedMeshes++;else ordinaryMeshes++;
                }
            }
            require(packedMeshes>0 && ordinaryMeshes>0,"Owned packed and ordinary mesh paths were not exercised");
            std::printf("Validated geometry spans for %u packed and %u ordinary meshes.\n",packedMeshes,ordinaryMeshes);
            std::printf("Validated %u owned archive members; %u failures.\n",hwCataDataCount(),failures);hwCataDataClose();return failures?1:0;
        }
        auto folder=std::filesystem::temp_directory_path()/std::filesystem::path("openhw-cata-"+std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directory(folder);
        struct Cleanup {std::filesystem::path folder;~Cleanup(){hwCataDataClose();std::filesystem::remove_all(folder);}} cleanup{folder};
        std::vector<Member> members;
        for(const char* name:{"singleplayer/mission01/mission01.mission","race_sect/smothership.shp","race_sect/sacolyte.shp","race_sect/sworker.shp"})members.push_back({name,{'b','a','s','e'},4});
        members.push_back({"compressed",{0x00,0x0a,0x00},6}); // match window offset 1, length 6: six initial zeroes
        std::vector<unsigned char> geo(320);std::memcpy(geo.data(),"RMF99ba",7);
        put(geo,8,0x500);put(geo,40,2);put(geo,68+44,184);put(geo,68+52,0x3f800000);
        members.push_back({"hierarchy.geo",geo,(uint32_t)geo.size()});
        members.push_back({"hierarchy.peo",geo,(uint32_t)geo.size()});
        std::vector<unsigned char> oldTag(384);std::memcpy(oldTag.data(),"RMF97ba",7);
        put(oldTag,8,0x402);put(oldTag,40,1);
        put(oldTag,68+8,3);put(oldTag,68+12,1);put(oldTag,68+20,1);
        put(oldTag,68+24,256);put(oldTag,68+28,304);put(oldTag,68+32,352);put(oldTag,68+36,216);
        put(oldTag,68+52,0x3f800000);
        members.push_back({"oldtag.peo",oldTag,(uint32_t)oldTag.size()});
        auto invalidGeometry=geo;put(invalidGeometry,68+8,0x3f800000u);
        members.push_back({"invalid.peo",invalidGeometry,(uint32_t)invalidGeometry.size()});
        std::vector<unsigned char> nis(244);std::memcpy(nis.data(),"Crannnberry",10);
        put(nis,12,0x101);put(nis,36,2);put(nis,40,132);put(nis,132+12,2);put(nis,132+56+12,7);
        members.push_back({"intro.nis",nis,(uint32_t)nis.size()});
        std::string warp="WarpEnabled 0\nLevelToWarpTo 11\nLevel 1\nResources 2000\nShips sRecon, 2\nShips sWorker, 2\nShips sMothershipDockingBay, 1\nTech TECH_DRIVE_FIGHTER\nLevel 2\nResources 9000\nShips sACV, 15\n";
        members.push_back({"warp.script",std::vector<unsigned char>(warp.begin(),warp.end()),(uint32_t)warp.size()});
        std::string economy="supportUnits 2 ; required\nprovidesSU 30\nNUMBER_OF_GUNS 1\nGUN 0\n{\nType GUN_HiddenGimble\nGunSize GUNSIZE_SMALL\nStartUpgradeLevel WEAPON_PROJECTILE\nEndUpgradeLevel WEAPON_ENERGY\nDamageMult 2\n}\n";
        members.push_back({"race_sect/srecon.shp",std::vector<unsigned char>(economy.begin(),economy.end()),(uint32_t)economy.size()});
        std::string visible="NUMBER_OF_GUNS 1\nGUN 0\n{\nType GUN_VisibleGimble\nGunSize GUNSIZE_LARGE\nStartUpgradeLevel WEAPON_ENERGY\nDamageMult 0.8\nBulletSpeed 1600\nFireTime 2.7\nminturnangle -110\nmaxturnangle 110\n}\n";
        members.push_back({"race_kushan/kstandarddestroyer.shp",std::vector<unsigned char>(visible.begin(),visible.end()),(uint32_t)visible.size()});
        std::string caps="ShipTotalCaps 297\nCapsRecon -1\n";
        members.push_back({"unitlimitcaps.script",std::vector<unsigned char>(caps.begin(),caps.end()),(uint32_t)caps.size()});
        const std::string movieList="; transition index\r\n1 LocalMovie\r\n2 Missing\r\n3 ../escape\r\n";
        members.push_back({"movies/animatics.lst",{movieList.begin(),movieList.end()},(uint32_t)movieList.size()});
        std::filesystem::create_directory(folder/"Movies");
        std::ofstream(folder/"Movies"/"LocalMovie.bik",std::ios::binary).write("BIKi",4);
        archive(folder/"cataclysm.big",members);
        std::vector<unsigned char> executable(1024);executable[0]='M';executable[1]='Z';put(executable,0x3c,128);
        std::memcpy(executable.data()+128,"PE\0\0",4);executable[152]=0x0b;executable[153]=1;
        float damageTable[60]={80,75,50,16,14,9};damageTable[39]=7;
        std::memcpy(executable.data()+512,damageTable,sizeof(damageTable));
        {std::ofstream out(folder/"cataclysm.exe",std::ios::binary);out.write((const char*)executable.data(),executable.size());}
        archive(folder/"update102.big",{{"race_sect/sacolyte.shp",{'p','a','t','c','h'},5}});
        require(hwCataDataOpen(folder.u8string().c_str(),error,sizeof(error))!=0,error);
        {
            char movie[1024],script[112],shortPath[4];
            require(hwCataAnimaticPaths(1,movie,sizeof(movie),script,sizeof(script)) &&
                std::filesystem::u8path(movie).filename()=="LocalMovie.bik" &&
                !std::strcmp(script,"Cataclysm\\Movies\\LocalMovie.script"),
                "Movie did not resolve relative to owned data");
            require(!hwCataAnimaticPaths(1,shortPath,sizeof(shortPath),script,sizeof(script)) &&
                !shortPath[0] && !script[0],"Movie paths were partially written on overflow");
            require(!hwCataAnimaticPaths(2,movie,sizeof(movie),script,sizeof(script)) &&
                !hwCataAnimaticPaths(3,movie,sizeof(movie),script,sizeof(script)),
                "Missing or out-of-directory movie path was accepted");
        }
        auto view=hwCataDataView("CATACLYSM/RACE_SECT\\SACOLYTE.SHP",&size);
        require(view && size==5 && !std::memcmp(view,"patch",5),"Patch precedence / normalized namespace failed");
        CataStartingFleet starting{};
        require(hwCataMission1StartingFleet(&starting,error,sizeof(error))!=0,error);
        require(starting.resources==2000 && starting.shipCount==3 && starting.ships[1].count==2 &&
            !std::strcmp(starting.ships[2].name,"sMothershipDockingBay") && starting.technologyCount==1 &&
            !std::strcmp(starting.technologies[0],"TECH_DRIVE_FIGHTER"),"Starting fleet used the wrong campaign level");
        CataShipEconomy budget{};
        require(hwCataShipEconomy("Cataclysm/RACE_SECT/","sRecon",&budget,error,sizeof(error))!=0,error);
        require(budget.supportRequired==2 && budget.supportProvided==30 && budget.typeLimit==-1 && budget.totalLimit==297,
            "Support capacity and explicit type limits were not read from owned data");
        require(!hwCataShipEconomy("R1","sRecon",&budget,error,sizeof(error)),"HW1 economy accepted as Cataclysm");
        view=hwCataCompatibleShipView("Cataclysm/RACE_SECT/sRecon.shp",&size);
        require(view!=nullptr,"Cataclysm weapon compatibility failed");
        std::string compatible((const char*)view,size);
        require(compatible.find("DamageLo 14\nDamageHi 14")!=compatible.npos &&
            compatible.find("SoundType GS_SmallProjectile")!=compatible.npos &&
            compatible.find("Type GUN_Gimble")!=compatible.npos,"Weapon class / original damage multiplier was lost");
        require(compatible.find("CataWeaponClass 13\nCataWeaponDisabled FALSE")!=compatible.npos,
            "Ordinary Cataclysm projectile was disabled or assigned the wrong class");
        view=hwCataCompatibleShipView("Cataclysm/RACE_KUSHAN/kStandardDestroyer.shp",&size);
        require(view!=nullptr,"Visible Cataclysm turret conversion failed");
        std::string articulated((const char*)view,size);
        require(articulated.find("GUN_VisibleGimble")==articulated.npos &&
            articulated.find("Type GUN_NewGimble")!=articulated.npos,
            "Visible turret was not mapped to an engine-supported gun type");
        for(const char *setting:{"SoundType GS_LargeEnergyCannon","CataWeaponClass 0\nCataWeaponDisabled FALSE",
                "DamageMult 0.8","BulletSpeed 1600","FireTime 2.7","minturnangle -110","maxturnangle 110"})
            require(articulated.find(setting)!=articulated.npos,"Visible energy turret lost original weapon behavior");
        view=hwCataDataView("race_sect/srecon.shp",&size);
        require(view && std::string((const char*)view,size)==economy,"Compatibility changed original ship bytes");
        view=hwCataDataView("compressed",&size);require(view && size==6,"Compressed member failed");
        for(int i=0;i<size;i++)require(((const unsigned char*)view)[i]==0,"LZSS window mismatch");
        view=hwCataDataView("hierarchy.geo",&size);require(view && size==320,"Mesh conversion failed");
        uint32_t link=0,transform=0;std::memcpy(&link,(const char*)view+68+40,4);std::memcpy(&transform,(const char*)view+68+48,4);
        require(link==180 && transform==0x3f800000,"Mesh hierarchy remap corrupted transform");
        view=hwCataDataView("hierarchy.peo",&size);require(view && size==320,"Packed mesh conversion failed");
        uint32_t version=0;std::memcpy(&version,(const char*)view+8,4);
        std::memcpy(&link,(const char*)view+68+40,4);
        std::memcpy(&transform,(const char*)view+68+48,4);
        require(version==0x402 && link==180 && transform==0x3f800000,"Packed mesh bypassed hierarchy adaptation");
        require(!hwCataDataView("invalid.peo",&size),"Invalid packed geometry reached the native mesh loader");
        view=hwCataDataView("oldtag.peo",&size);require(view && size==384,"Legacy-tagged extended mesh was not adapted");
        uint32_t normals=0,polygons=0;
        std::memcpy(&normals,(const char*)view+68+28,4);std::memcpy(&polygons,(const char*)view+68+32,4);
        require(normals==352 && polygons==216,"Legacy mesh tag selected the wrong object layout");
        view=hwCataDataView("intro.nis",&size);require(view && size==244,"NIS adaptation failed");
        uint32_t kushan=0,asteroid=0;std::memcpy(&kushan,(const char*)view+132+12,4);std::memcpy(&asteroid,(const char*)view+132+56+12,4);
        require(kushan==0 && asteroid==6,"NIS race mapping selected the wrong ship models");
        require(!hwCataDataView("../compressed",&size),"Traversal accepted");
        hwCataDataClose();require(!hwCataDataReady(),"Close retained mount");
        archive(folder/"cataclysm.big",{{"../unsafe",{'x'},1}});
        require(!hwCataDataOpen(folder.u8string().c_str(),error,sizeof(error)),"Unsafe archive accepted");
        require(!hwCataDataReady(),"Failed mount retained partial state");
        std::puts("Cataclysm data contracts passed.");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
