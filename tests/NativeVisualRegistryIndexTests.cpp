#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include "RemasteredVisualAsset.h"

static long failAllocation=-1;
static size_t registryAllocations;
static void *registryMalloc(size_t bytes) {
    if(failAllocation==0){failAllocation=-1;return nullptr;}
    if(failAllocation>0)--failAllocation;
    auto *result=std::malloc(bytes);if(result)++registryAllocations;return result;
}
static void *registryCalloc(size_t count,size_t bytes) {
    void *result=registryMalloc(count*bytes);if(result)std::memset(result,0,count*bytes);return result;
}
static void registryFree(void *value) {if(value)--registryAllocations;std::free(value);}
#define malloc registryMalloc
#define calloc registryCalloc
#define free registryFree
extern "C" {
#ifndef HW_REGISTRY_TEST_SOURCE
#define HW_REGISTRY_TEST_SOURCE "../src/Game/NativeVisualRegistry.c"
#endif
#include HW_REGISTRY_TEST_SOURCE
}
#undef malloc
#undef calloc
#undef free

struct HwRmVisualAsset {unsigned instances=0,releases=0;};
struct HwRmVisualInstance {HwRmVisualAsset *asset;unsigned id;double phase=-1;};
static bool failInstance;
static unsigned nextInstance=1;
static std::unordered_set<HwRmVisualInstance*> liveInstances;
static std::vector<unsigned> stopped;
extern "C" HwRmVisualInstance *hwRmVisualInstanceCreate(HwRmVisualAsset *asset) {
    if(failInstance){failInstance=false;return nullptr;}
    auto *instance=new HwRmVisualInstance{asset,nextInstance++};
    liveInstances.insert(instance);++asset->instances;return instance;
}
extern "C" void hwRmVisualInstanceFree(HwRmVisualInstance *instance) {
    if(!instance || !liveInstances.erase(instance))std::abort();
    --instance->asset->instances;delete instance;
}
extern "C" void hwRmVisualAssetFree(HwRmVisualAsset *asset) {
    if(asset->instances)std::abort();++asset->releases;
}
extern "C" int hwRmVisualInstanceGunMuzzle(HwRmVisualInstance*,unsigned,float,float*,char*,size_t){return 1;}
extern "C" int hwRmVisualInstanceMarker(HwRmVisualInstance*,const char*,float,float*,char*,size_t){return 1;}
extern "C" const HwRmMad *hwRmVisualInstanceAnimation(const HwRmVisualInstance*) {return reinterpret_cast<const HwRmMad*>(1);}
extern "C" int hwRmVisualInstanceSampleEvent(HwRmVisualInstance *instance,const char *event,double phase,char*,size_t) {
    if(!event)return 0;instance->phase=phase;return 1;
}
extern "C" int hwRmVisualInstanceAnimationPresentation(HwRmVisualInstance*,const char*,double,HwRmVisualPresentationVisit,void*){return 1;}
extern "C" int hwRmVisualInstanceFirePresentation(HwRmVisualInstance*,unsigned,double,HwRmVisualPresentationVisit,void*){return 1;}
extern "C" int hwRmVisualInstanceUpdatePresentation(HwRmVisualInstance*,double,HwRmVisualPresentationVisit,void*){return 1;}
extern "C" void hwRmVisualInstanceStopPresentation(HwRmVisualInstance *instance){stopped.push_back(instance->id);}
extern "C" const HwRmWeaponPresentation *hwRmVisualInstanceWeaponPresentation(const HwRmVisualInstance*,unsigned){return nullptr;}
extern "C" const HwRmWeaponPresentationSlot *hwRmWeaponPresentationAt(const HwRmWeaponPresentation*,unsigned){return nullptr;}

static void require(bool okay,const char *message){if(!okay)throw std::runtime_error(message);}
static void ignoreEvent(void*,const void*,HwRmVisualInstance*,float,unsigned,const HwRmPresentationEvent*,double){}
static void failureTests() {
    int mesh,ship;HwRmVisualAsset asset;
    failAllocation=0;
    require(!meshNativeVisualRegister(&mesh,&asset,1,0),"failed owner allocation was published");
    require(!asset.releases && !meshNativeVisualAcquire(&mesh,&ship),"failed registration transferred ownership");
    require(meshNativeVisualRegister(&mesh,&asset,1,0)!=0,"registration did not recover");
    failAllocation=0;
    require(!meshNativeVisualAcquire(&mesh,&ship) && !asset.instances,"failed visual allocation was published");
    failInstance=true;const auto allocations=registryAllocations;
    require(!meshNativeVisualAcquire(&mesh,&ship) && registryAllocations==allocations,"failed instance leaked registry entry");
    failAllocation=0;
    require(!meshNativeVisualSampleShipEvent(&ship,"Open",1),"missing pose allocation should not publish an instance");
    auto *instance=meshNativeVisualAcquire(&mesh,&ship);
    require(instance && instance->phase==-1,"failed remembered pose became active");
    meshNativeVisualReleaseShip(&ship);
    failAllocation=1; // The pose entry succeeds; its event-name allocation fails.
    meshNativeVisualSampleShipEvent(&ship,"Open",.5);
    instance=meshNativeVisualAcquire(&mesh,&ship);
    require(instance && instance->phase==-1,"failed remembered event changed pose");
    meshNativeVisualSampleShipEvent(&ship,"Open",.75);
    require(instance->phase==.75,"remembered pose failed to recover");
    meshNativeVisualReleaseShip(&ship);meshNativeVisualReleaseMesh(&mesh);
    require(asset.releases==1 && registryAllocations==0 && liveInstances.empty(),"failure recovery leaked ownership");
}
static void fleetTests() {
    constexpr unsigned ships=8192,lods=3;
    std::vector<int> shipKeys(ships);int meshKeys[lods];HwRmVisualAsset assets[lods];
    std::vector<HwRmVisualInstance*> instances(ships*lods);
    std::vector<unsigned long long> tokens(ships*lods);
    for(unsigned l=0;l<lods;++l)require(meshNativeVisualRegister(&meshKeys[l],&assets[l],float(l+1),l)!=0,"fleet mesh registration");
    for(unsigned i=0;i<ships;++i)for(unsigned l=0;l<lods;++l) {
        auto *instance=instances[i*lods+l]=meshNativeVisualAcquire(&meshKeys[l],&shipKeys[i]);
        require(instance!=nullptr,"fleet instance missing");tokens[i*lods+l]=meshNativeVisualInstanceToken(instance);
        require(tokens[i*lods+l]!=0,"fleet token missing");
    }
    require(liveInstances.size()==ships*lods,"fleet/LOD instances merged");
    const auto start=std::chrono::steady_clock::now();
    constexpr unsigned queries=100000;
    for(unsigned q=0;q<queries;++q) {
        const unsigned i=(q*7919u)%ships,l=q%lods;float scale=0;
        require(meshNativeVisualAcquire(&meshKeys[l],&shipKeys[i])==instances[i*lods+l],"fleet lookup changed instance");
        require(meshNativeVisualInstanceToken(instances[i*lods+l])==tokens[i*lods+l],"fleet token changed");
        require(meshNativeVisualFindInstance(&shipKeys[i],tokens[i*lods+l],&scale)==instances[i*lods+l] && scale==float(l+1),"token/scale lookup crossed LOD");
    }
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::printf("REGISTRY %u ships x %u LODs, %u acquire/token/find triplets: %.3f ms\n",ships,lods,queries,elapsed);
    for(unsigned i=0;i<ships;i+=2) {
        meshNativeVisualReleaseShip(&shipKeys[i]);meshNativeVisualReleaseShip(&shipKeys[i]);
        float scale=-1;
        for(unsigned l=0;l<lods;++l) {
            require(!meshNativeVisualFindInstance(&shipKeys[i],tokens[i*lods+l],&scale) && scale==-1,"released ship token still indexed");
            auto *replacement=meshNativeVisualAcquire(&meshKeys[l],&shipKeys[i]);
            require(replacement && meshNativeVisualInstanceToken(replacement)!=tokens[i*lods+l],"reused ship address inherited token");
        }
    }
    meshNativeVisualReleaseMesh(&meshKeys[1]);
    for(unsigned i=0;i<ships;++i) {
        float scale=-1;
        require(!meshNativeVisualAcquire(&meshKeys[1],&shipKeys[i]),"released mesh still indexed");
        require(meshNativeVisualAcquire(&meshKeys[0],&shipKeys[i]) && meshNativeVisualAcquire(&meshKeys[2],&shipKeys[i]),"mesh release removed another LOD");
        require(!meshNativeVisualFindInstance(&shipKeys[i],tokens[i*lods+1],&scale),"released mesh token still indexed");
    }
    HwRmVisualAsset replacement;
    require(meshNativeVisualRegister(&meshKeys[1],&replacement,4,1)!=0,"reused mesh address could not register");
    require(meshNativeVisualAcquire(&meshKeys[1],&shipKeys[0])->asset==&replacement,"reused mesh resolved stale asset");
    meshNativeVisualSetPresentationSink(ignoreEvent,nullptr,nullptr);
    require(meshNativeVisualNotifyAnimation(&meshKeys[0],&shipKeys[0],"Open",.5)!=0,"presentation activation failed");
    require(!meshNativeVisualNotifyAnimation(&meshKeys[2],&shipKeys[0],"Open",.5),"camera LOD duplicated presentation ownership");
    meshNativeVisualSetPresentationSink(nullptr,nullptr,nullptr);
    for(unsigned i=0;i<ships;++i)meshNativeVisualReleaseShip(&shipKeys[i]);
    for(unsigned l=0;l<lods;++l)meshNativeVisualReleaseMesh(&meshKeys[l]);
    require(replacement.releases==1 && registryAllocations==0 && liveInstances.empty(),"fleet release leaked index entries");
    for(const auto& asset:assets)require(asset.releases==1,"asset ownership changed");
}
int main() {
    try {failureTests();fleetTests();failureTests();std::puts("Native visual registry lifetime/index tests passed");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
