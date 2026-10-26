// A white-box, CPU-only test keeps weak-ownership observations private to this
// test translation unit. Do not also compile RemasteredVisualAsset.cpp into
// this target. The game gains no cache-statistics/debug API.
#include "../src/Modern/RemasteredVisualAsset.cpp"
#include "NativeShipHodFixture.h"
#include <array>
#include <thread>

static void require(bool value,const char *message) {
    if(!value) throw std::runtime_error(message);
}
static void captureDraw(void *user,const HwRmSubsystemDraw *draw) {
    static_cast<std::vector<HwRmSubsystemDraw> *>(user)->push_back(*draw);
}
int main() {
    try {
        const auto bytes=sample(false,true);
        char error[512]={};
        std::array<HwRmVisualAsset *,5> assets{};
        std::array<std::array<char,512>,5> errors{};
        std::vector<std::thread> threads;
        // Equivalent to loading the same package for its five mesh LODs.
        // Simultaneous reads must coalesce rather than parse five copies.
        for(size_t i=0;i<assets.size();++i) threads.emplace_back([&,i] {
            assets[i]=hwRmVisualAssetCreate(bytes.data(),bytes.size(),nullptr,0,
                errors[i].data(),errors[i].size());
        });
        for(auto &thread:threads) thread.join();
        threads.clear();
        for(size_t i=0;i<assets.size();++i) require(assets[i]!=nullptr,errors[i].data());
        SceneSource source;
        auto shared=read(bytes.data(),bytes.size(),&source);
        std::weak_ptr<HwRmHodScene> observed=shared;
        const auto *material=hwRmVisualAssetMaterial(assets[0],0,0);
        require(material!=nullptr,"shared asset material missing");
        for(size_t i=1;i<assets.size();++i) {
            require(assets[i]!=assets[0],"mutable asset objects were aliased");
            require(hwRmVisualAssetMaterial(assets[i],0,0)==material,
                "identical inputs retained duplicate material/scene data");
            auto *a=hwRmVisualInstanceCreate(assets[0]);
            auto *b=hwRmVisualInstanceCreate(assets[i]);
            require(a&&b,"scene sharing lost an instance");
            std::vector<HwRmSubsystemDraw> da,db;
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(a),0,1,
                nullptr,nullptr,captureDraw,&da,error,sizeof(error))!=0,error);
            require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(b),0,1,
                nullptr,nullptr,captureDraw,&db,error,sizeof(error))!=0,error);
            require(!da.empty()&&da.size()==db.size(),"shared scene topology changed");
            for(size_t j=0;j<da.size();++j) {
                require(da[j].part==db[j].part && da[j].material==db[j].material,
                    "identical inputs retained duplicate geometry storage");
            }
            hwRmVisualInstanceFree(a);hwRmVisualInstanceFree(b);
        }
        shared.reset();
        require(!observed.expired(),"asset did not retain its shared scene");

        // The hash is a filter, never model identity: directly force a hash
        // collision through the same private comparison used by production.
        auto different=bytes;
        const unsigned char pixel[]={10,20,30,255};
        const auto texel=std::search(different.begin(),different.end(),
            std::begin(pixel),std::end(pixel));
        require(texel!=different.end(),"fixture texture missing");*texel=11;
        SharedScene collision{};collision.hash=123;collision.source=source;
        require(sameSceneSource(collision,123,bytes.data(),bytes.size()),
            "exact source verification rejected identical bytes");
        require(!sameSceneSource(collision,123,different.data(),different.size()),
            "hash collision aliased different source bytes");
        auto *other=hwRmVisualAssetCreate(different.data(),different.size(),nullptr,0,error,sizeof(error));
        require(other!=nullptr,error);
        require(hwRmVisualAssetMaterial(other,0,0)!=material,
            "different source bytes reused the old parsed scene");

        // Concurrent first access to the lazy image pool must publish one
        // stable pool, despite independent assets sharing the parsed scene.
        std::array<HwRmHodTextureView,5> textures{};
        std::array<bool,5> textureOk{};
        for(size_t i=0;i<assets.size();++i) threads.emplace_back([&,i] {
            textureOk[i]=hwRmVisualAssetTexture(assets[i],0,0,"$diffuse",0,
                &textures[i],errors[i].data(),errors[i].size())!=0;
        });
        for(auto &thread:threads) thread.join();threads.clear();
        for(size_t i=0;i<assets.size();++i) {
            require(textureOk[i],errors[i].data());
            require(textures[i].bytes==textures[0].bytes && textures[i].size==4 &&
                textures[i].bytes[0]==10 && textures[i].bytes[1]==20,
                "shared texture decode changed bytes or allocated multiple pools");
        }
        HwRmHodTextureView changed{};
        require(hwRmVisualAssetTexture(other,0,0,"$diffuse",0,&changed,error,sizeof(error))!=0,error);
        require(changed.bytes!=textures[0].bytes&&changed.bytes[0]==11,
            "new source texture aliased the previous version");

        // Use new assets here because creating instances above froze the five
        // LOD definitions. Layers, mounts and instance controls must not share.
        auto *left=hwRmVisualAssetCreate(bytes.data(),bytes.size(),nullptr,0,error,sizeof(error));
        auto *right=hwRmVisualAssetCreate(bytes.data(),bytes.size(),nullptr,0,error,sizeof(error));
        require(left&&right,"independent definitions missing");
        require(hwRmVisualAssetAttach(left,"shared-gun","Gun","Root",HW_RM_TURRET,
            bytes.data(),bytes.size(),error,sizeof(error))!=0,error);
        require(hwRmVisualAssetAttach(right,"shared-gun","Gun","Root",HW_RM_TURRET,
            bytes.data(),bytes.size(),error,sizeof(error))!=0,error);
        require(hwRmVisualAssetMaterial(left,1,0)==material &&
            hwRmVisualAssetMaterial(right,1,0)==material,
            "identical hull/components retained duplicate immutable scenes");
        require(left->models[0].bytes==source && right->models[0].bytes==source,
            "attachments retained duplicate source-verification byte buffers");
        require(!hwRmVisualAssetAttach(left,"shared-gun","WrongBytes","Root",HW_RM_TURRET,
            different.data(),different.size(),error,sizeof(error)),
            "shared component key accepted different source bytes");
        float mount[16]={1,0,0,0,0,1,0,0,0,0,1,0,10,0,0,1};
        require(hwRmVisualAssetMount(left,"Hull",mount)!=0,"independent mount rejected");
        require(hwRmVisualAssetBindMarker(left,"OnlyLeft","Hull","Root",error,sizeof(error))!=0,error);
        require(hwRmVisualAssetBindGun(left,0,"Hull","Root","Root","Root","Root",error,sizeof(error))!=0,error);
        require(hwRmVisualAssetBindGun(right,0,"Hull","Root","Root","Root","Root",error,sizeof(error))!=0,error);
        auto *li=hwRmVisualInstanceCreate(left),*ri=hwRmVisualInstanceCreate(right);
        require(li&&ri,"independent instances missing");
        float leftRest[16],rightRest[16],after[16];
        require(hwRmVisualInstanceGunMuzzle(li,0,1,leftRest,error,sizeof(error))!=0,error);
        require(hwRmVisualInstanceGunMuzzle(ri,0,1,rightRest,error,sizeof(error))!=0,error);
        require(leftRest[12]==10&&rightRest[12]==0,"authored mounts leaked across assets");
        require(hwRmVisualInstanceMarker(li,"OnlyLeft",1,after,error,sizeof(error))!=0,error);
        require(!hwRmVisualInstanceMarker(ri,"OnlyLeft",1,after,error,sizeof(error)),
            "marker definitions leaked across assets");
        require(hwRmVisualInstanceSetGun(li,0,.25f,0,3,error,sizeof(error))!=0,error);
        require(hwRmVisualInstanceGunMuzzle(ri,0,1,after,error,sizeof(error))!=0,error);
        require(std::memcmp(after,rightRest,sizeof(after))==0,"gun pose leaked across assets");
        require(hwRmVisualAssetMaterial(left,0,0)==hwRmVisualAssetMaterial(right,0,0),
            "independent mutable assets lost immutable scene sharing");

        hwRmVisualAssetFree(assets[0]);assets[0]=nullptr;
        HwRmHodTextureView alive{};
        require(hwRmVisualAssetTexture(assets[1],0,0,"$diffuse",0,&alive,error,sizeof(error))!=0,error);
        require(alive.bytes==textures[0].bytes&&alive.bytes[1]==20,
            "releasing one LOD invalidated another LOD's texture");
        for(auto *asset:assets) hwRmVisualAssetFree(asset);
        hwRmVisualAssetFree(left);hwRmVisualAssetFree(right);
        require(!observed.expired(),"releasing definitions invalidated active instances");
        require(hwRmVisualInstanceGunMuzzle(li,0,1,after,error,sizeof(error))!=0,error);
        std::vector<HwRmSubsystemDraw> releasedComponents;
        require(hwRmSubsystemVisit(hwRmVisualInstanceSubsystem(li),0,1,nullptr,nullptr,
            captureDraw,&releasedComponents,error,sizeof(error))!=0,error);
        require(releasedComponents.size()==6,"owner release lost a hull or attached component");
        hwRmVisualInstanceFree(li);
        require(!observed.expired(),"releasing one instance invalidated another");
        require(hwRmVisualInstanceGunMuzzle(ri,0,1,after,error,sizeof(error))!=0,error);
        require(std::memcmp(after,rightRest,sizeof(after))==0,"surviving instance pose changed");
        hwRmVisualInstanceFree(ri);
        require(observed.expired(),"weak cache retained an unused parsed model");
        hwRmVisualAssetFree(other);
        auto reloaded=read(bytes.data(),bytes.size());
        require(reloaded&&observed.expired(),"expired scene could not be loaded independently");
        reloaded.reset();
        std::puts("Shared HOD scene checks passed: five LODs, full-byte identity, concurrent decode, independent definitions/poses, lifetime and weak eviction.");
        return 0;
    } catch(const std::exception &e) {
        std::fprintf(stderr,"Shared HOD scene failure: %s\n",e.what());return 1;
    }
}
