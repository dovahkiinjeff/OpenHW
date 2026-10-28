#include "RemasteredVisualAsset.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace {
struct SceneDelete {void operator()(HwRmHodScene *p) const {hwRmHodSceneFree(p);}};
struct MadDelete {void operator()(HwRmMad *p) const {hwRmMadFree(p);}};
struct PresentationDelete {void operator()(HwRmPresentationEvents *p) const {hwRmPresentationEventsFree(p);}};
struct PlayerDelete {void operator()(HwRmPresentationPlayer *p) const {hwRmPresentationPlayerFree(p);}};
struct WeaponDelete {void operator()(HwRmWeaponPresentation *p) const {hwRmWeaponPresentationFree(p);}};
struct ActivePresentation {
    unsigned component,timeline;
    std::unique_ptr<HwRmPresentationPlayer,PlayerDelete> player;
    int stateRoot=-1;bool stateEnabled=false;
};
using Scene=std::shared_ptr<HwRmHodScene>;
using SceneSource=std::shared_ptr<const std::vector<unsigned char>>;
struct Model {std::string key;SceneSource bytes;Scene scene;};
struct Event {std::string name;std::vector<unsigned> clips;};
struct GunBinding {unsigned gun,component;std::string azimuth,declination,recoil,muzzle,presentation;
    std::unique_ptr<HwRmWeaponPresentation,WeaponDelete> weapon;};
struct MarkerBinding {unsigned component;std::string semantic,joint;};
struct SurfaceMarker {unsigned component;std::string name,parent;float position[3],normal[3];};
struct AnimationTarget {std::string source,target;};
void detail(char *error,size_t size,const char *value) {if(error && size) std::snprintf(error,size,"%s",value);}
struct SharedScene {
    std::uint64_t hash;
    SceneSource source;
    std::unique_ptr<HwRmHodScene,SceneDelete> scene;
};
bool sameSceneSource(const SharedScene &shared,std::uint64_t hash,
    const void *bytes,size_t size) {
    return shared.hash==hash && shared.source->size()==size &&
        std::memcmp(shared.source->data(),bytes,size)==0;
}
Scene read(const void *bytes,size_t size,SceneSource *source=nullptr) {
    if(!bytes || !size || size>64u*1024u*1024u)
        throw std::runtime_error("invalid fleet HOD input");
    const auto *first=static_cast<const unsigned char *>(bytes);
    std::uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<size;++i) hash=(hash^first[i])*1099511628211ull;
    // A model's parsed geometry is immutable. Layers, authored mounts, gun
    // controls and timelines remain separate for every asset/ship/LOD. Keep
    // only weak cache entries so unloading the final owner frees its model.
    static std::mutex cacheMutex;
    static std::vector<std::weak_ptr<SharedScene>> cache;
    std::lock_guard<std::mutex> lock(cacheMutex);
    for(auto at=cache.begin();at!=cache.end();) {
        auto shared=at->lock();
        if(!shared) {at=cache.erase(at);continue;}
        // The hash only rejects candidates. Full byte comparison is the
        // identity test, including when two inputs have colliding hashes.
        if(sameSceneSource(*shared,hash,bytes,size)) {
            if(source) *source=shared->source;
            return Scene(shared,shared->scene.get());
        }
        ++at;
    }
    HwRmHodScene *parsed=nullptr;char error[256];
    if(!hwRmHodSceneRead(bytes,size,&parsed,error,sizeof(error)))
        throw std::runtime_error(error);
    // Own the parsed scene before any allocation that may throw. Exact input
    // bytes also replace each attachment's old private verification copy.
    std::unique_ptr<HwRmHodScene,SceneDelete> parsedOwner(parsed);
    auto shared=std::make_shared<SharedScene>();
    shared->hash=hash;
    shared->scene=std::move(parsedOwner);
    shared->source=std::make_shared<const std::vector<unsigned char>>(first,first+size);
    cache.push_back(shared);
    if(source) *source=shared->source;
    return Scene(shared,shared->scene.get());
}
}
struct HwRmVisualAsset {
    Scene hull;
    std::unique_ptr<HwRmMad,MadDelete> animation;
    std::vector<Model> models;
    std::vector<Event> events;
    std::vector<GunBinding> guns;
    std::vector<MarkerBinding> markers;
    std::vector<SurfaceMarker> surfaceMarkers;
    std::vector<std::pair<unsigned,std::unique_ptr<HwRmPresentationEvents,PresentationDelete>>> presentation;
    std::vector<AnimationTarget> animationTargets;
    std::vector<std::string> allowedClips;
    HwRmSubsystemLayer *layer=nullptr;
    unsigned instances=0;
    bool frozen=false,released=false;
    ~HwRmVisualAsset() {hwRmSubsystemLayerFree(layer);}
};
extern "C" const HwRmHodMaterial* hwRmVisualAssetMaterial(const HwRmVisualAsset* asset,unsigned component,unsigned material){
    return asset&&!asset->released?hwRmHodSceneMaterial(hwRmSubsystemLayerScene(asset->layer,component),material):nullptr;
}
extern "C" int hwRmVisualAssetTexture(HwRmVisualAsset* asset,unsigned component,unsigned material,
    const char* parameter,unsigned mip,HwRmHodTextureView* view,char* error,size_t capacity){
    if(view)*view={};
    if(!asset||asset->released){detail(error,capacity,"invalid native visual asset");return 0;}
    // The asset owns mutable scenes. The layer borrows a const geometry view;
    // only the scene's lazy decompression cache is populated here.
    auto* scene=const_cast<HwRmHodScene*>(hwRmSubsystemLayerScene(asset->layer,component));
    return hwRmHodMaterialTexture(scene,material,parameter,mip,view,error,capacity);
}
struct HwRmVisualInstance {
    HwRmVisualAsset *asset;
    HwRmSubsystemInstance *subsystem;
    std::vector<ActivePresentation> playback;
    std::string phaseEvent;
    double presentationPhase=-1;
    bool presentationPhaseFirst=true;
};
namespace {
struct PresentationSink {unsigned component;HwRmVisualPresentationVisit visit;void *user;};
struct MarkerContext {HwRmVisualInstance *instance;const HwRmHodScene *scene;unsigned component;};
void dispatchPresentation(void *context,const HwRmPresentationEvent *event,double time) {
    auto& sink=*static_cast<PresentationSink*>(context);
    sink.visit(sink.user,sink.component,event,time);
}
int resolvePresentationMarker(void *context,const char *name) {
    auto& ctx=*static_cast<MarkerContext*>(context);
    const auto *scene=ctx.scene;int result=-1;
    // RM's joint lookup hashes with ASCII case folding (00792271, mode 1).
    // Return the original HOD spelling to the exact authored-pose API below.
    const auto sameName=[](const char* a,const char* b){
        for(;;++a,++b){unsigned x=(unsigned char)*a,y=(unsigned char)*b;
            if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
            if(x!=y)return false;if(!x)return true;}
    };
    for(unsigned i=0;i<hwRmHodSceneJointCount(scene);++i)
        if(sameName(hwRmHodSceneJoint(scene,i)->name,name)){
            if(result>=0)return -1; // ambiguous names must not win the draw
            result=(int)i;
        }
    if(result>=0)return result;
    // RM events also address generated hull-surface locations, not HOD joints.
    // Preserve explicit joints first; only recognize the six numbered faces.
    unsigned side=6,number=0;char tail=0;
    const char* faces[]={"right","left","top","bottom","front","back"};
    for(unsigned i=0;i<6;++i){const size_t n=std::strlen(faces[i]);
        if(!std::strncmp(name,faces[i],n)&&name[n]=='_'&&
            std::sscanf(name+n+1,"%u%c",&number,&tail)==1&&number<100){side=i;break;}}
    if(side==6)return -1;
    auto& cache=ctx.instance->asset->surfaceMarkers;
    for(unsigned i=0;i<cache.size();++i)
        if(cache[i].component==ctx.component&&cache[i].name==name)
            return int(hwRmHodSceneJointCount(scene)+i);
    SurfaceMarker marker{};marker.component=ctx.component;marker.name=name;
    double total=0;unsigned random=2166136261u;
    for(const char* c=name;*c;++c)random=(random^(unsigned char)*c)*16777619u;
    const unsigned axis=side/2;const float sign=(side&1)?-1.0f:1.0f;
    // Area-weighted reservoir sampling avoids favouring densely tessellated
    // pieces. Cache once per asset, then follow the parent joint every frame.
    for(unsigned m=0;m<hwRmHodSceneMeshCount(scene);++m){
        const auto* mesh=hwRmHodSceneMesh(scene,m);if(mesh->lod!=0)continue;
        for(unsigned p=0;p<mesh->partCount;++p){const auto& part=mesh->parts[p];
            for(unsigned t=0;t+2<part.indexCount;t+=3){
                const auto& a=part.vertices[part.indices[t]];
                const auto& b=part.vertices[part.indices[t+1]];
                const auto& c=part.vertices[part.indices[t+2]];
                float ab[3],ac[3],normal[3];
                for(unsigned k=0;k<3;++k){ab[k]=b.position[k]-a.position[k];ac[k]=c.position[k]-a.position[k];normal[k]=a.normal[k]+b.normal[k]+c.normal[k];}
                const double x=ab[1]*ac[2]-ab[2]*ac[1],y=ab[2]*ac[0]-ab[0]*ac[2],z=ab[0]*ac[1]-ab[1]*ac[0];
                float norm=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
                if(norm<0.00001f)continue;
                const double weight=std::sqrt(x*x+y*y+z*z)*std::max(0.0f,sign*normal[axis]/norm);
                if(weight<=0)continue;total+=weight;random=random*1664525u+1013904223u;
                if((double(random)/4294967296.0)*total>=weight)continue;
                marker.parent=mesh->parent;
                for(unsigned k=0;k<3;++k){marker.position[k]=(a.position[k]+b.position[k]+c.position[k])/3.0f;marker.normal[k]=normal[k]/norm;}
            }
        }
    }
    if(total<=0)return -1;
    cache.push_back(marker);return int(hwRmHodSceneJointCount(scene)+cache.size()-1);
}
}
extern "C" int hwRmVisualInstancePresentationMarker(HwRmVisualInstance *instance,unsigned component,
    const HwRmPresentationEvent *event,unsigned sample,float scale,float *matrix,char *error,size_t capacity) {
    if(!instance || !event || !matrix){detail(error,capacity,"invalid event marker request");return 0;}
    const auto *scene=hwRmSubsystemLayerScene(instance->asset->layer,component);
    if(!scene){detail(error,capacity,"missing event marker component");return 0;}
    if(!event->markers || !*event->markers)
        return hwRmSubsystemComponentTransform(instance->subsystem,component,scale,matrix,error,capacity);
    MarkerContext context{instance,scene,component};
    int joint=hwRmPresentationSelectMarker(event,sample,resolvePresentationMarker,&context);
    if(joint<0){detail(error,capacity,"no unambiguous event marker resolved");return 0;}
    if(unsigned(joint)>=hwRmHodSceneJointCount(scene)){
        const auto& marker=instance->asset->surfaceMarkers[unsigned(joint)-hwRmHodSceneJointCount(scene)];
        float parent[16];
        if(!hwRmSubsystemJointTransform(instance->subsystem,component,marker.parent.c_str(),scale,nullptr,nullptr,parent,error,capacity))return 0;
        const float position[]={marker.position[1]*scale,-marker.position[0]*scale,marker.position[2]*scale};
        const float normal[]={marker.normal[1],-marker.normal[0],marker.normal[2]};
        float n[3],right[3],up[3];
        for(unsigned k=0;k<3;++k){matrix[12+k]=parent[12+k];n[k]=0;
            for(unsigned j=0;j<3;++j){matrix[12+k]+=parent[j*4+k]*position[j];n[k]+=parent[j*4+k]*normal[j];}}
        float norm=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(norm<0.00001f)return 0;
        for(float& v:n)v/=norm;
        if(std::fabs(n[1])<0.9f){right[0]=n[2];right[1]=0;right[2]=-n[0];}
        else {right[0]=0;right[1]=-n[2];right[2]=n[1];}
        norm=std::sqrt(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);for(float& v:right)v/=norm;
        up[0]=n[1]*right[2]-n[2]*right[1];up[1]=n[2]*right[0]-n[0]*right[2];up[2]=n[0]*right[1]-n[1]*right[0];
        for(unsigned k=0;k<3;++k){matrix[k]=right[k];matrix[4+k]=up[k];matrix[8+k]=n[k];}
        matrix[3]=matrix[7]=matrix[11]=0;matrix[15]=1;return 1;
    }
    return hwRmSubsystemJointTransform(instance->subsystem,component,
        hwRmHodSceneJoint(scene,(unsigned)joint)->name,scale,nullptr,nullptr,matrix,error,capacity);
}
extern "C" int hwRmVisualInstanceStartPresentation(HwRmVisualInstance *instance,unsigned component,
    const char *name,double clock,HwRmVisualPresentationVisit visit,void *user) {
    if(!instance || !visit || !std::isfinite(clock))return 0;
    const auto *events=hwRmVisualInstancePresentation(instance,component);
    int timeline=hwRmPresentationFindTimeline(events,name);
    if(timeline<0)return 0;
    PresentationSink sink{component,visit,user};
    for(auto& active:instance->playback)if(active.component==component && active.timeline==(unsigned)timeline)
        return hwRmPresentationPlayerStart(active.player.get(),name,clock,dispatchPresentation,&sink);
    try {
        if(instance->playback.size()>=256)return 0;
        ActivePresentation active{component,(unsigned)timeline,
            std::unique_ptr<HwRmPresentationPlayer,PlayerDelete>(hwRmPresentationPlayerCreate(events))};
        if(!active.player)return 0;
        instance->playback.push_back(std::move(active));
        return hwRmPresentationPlayerStart(instance->playback.back().player.get(),name,clock,dispatchPresentation,&sink);
    }catch(const std::exception&){return 0;}
}
extern "C" int hwRmVisualInstanceUpdatePresentation(HwRmVisualInstance *instance,double clock,
    HwRmVisualPresentationVisit visit,void *user) {
    if(!instance || !visit || !std::isfinite(clock))return 0;
    int success=1;
    for(auto& active:instance->playback) {
        PresentationSink sink{active.component,visit,user};
        if(!hwRmPresentationPlayerUpdate(active.player.get(),clock,dispatchPresentation,&sink))success=0;
    }
    return success;
}
extern "C" void hwRmVisualInstanceStopPresentation(HwRmVisualInstance *instance) {
    if(instance){for(auto& active:instance->playback)hwRmPresentationPlayerStop(active.player.get());
        for(auto& active:instance->playback)active.stateEnabled=false;
        instance->phaseEvent.clear();instance->presentationPhase=-1;instance->presentationPhaseFirst=true;}
}
extern "C" int hwRmVisualInstancePresentationState(const HwRmVisualInstance *instance,unsigned component,unsigned timeline){
    if(instance)for(const auto& active:instance->playback)if(active.component==component&&active.timeline==timeline&&active.stateRoot>=0)return active.stateEnabled?1:0;
    return -1;
}
extern "C" int hwRmVisualInstanceSetPresentationState(HwRmVisualInstance *instance,unsigned component,
    const char *name,float magnitude,double clock,HwRmVisualPresentationVisit visit,void *user){
    if(!instance||!name||!visit||!std::isfinite(clock)||!std::isfinite(magnitude)||magnitude<0||magnitude>1)return 0;
    const auto* events=hwRmVisualInstancePresentation(instance,component);int root=hwRmPresentationFindTimeline(events,name);if(root<0)return 0;
    try {PresentationSink sink{component,visit,user};
        for(unsigned index=0;index<hwRmPresentationTimelineCount(events);++index){
            const auto* timeline=hwRmPresentationTimelineAt(events,index);int cursor=int(index);bool enabled=magnitude>0;
            while(cursor!=root&&cursor>=0){const auto* parent=hwRmPresentationTimelineAt(events,unsigned(cursor));
                enabled=enabled&&magnitude>=parent->minimum&&magnitude<=parent->maximum;cursor=parent->parent;}
            if(cursor!=root)continue;
            auto found=std::find_if(instance->playback.begin(),instance->playback.end(),[&](const auto& p){return p.component==component&&p.timeline==index;});
            if(found==instance->playback.end()){
                if(!enabled)continue;if(instance->playback.size()>=256)return 0;
                ActivePresentation active{component,index,std::unique_ptr<HwRmPresentationPlayer,PlayerDelete>(hwRmPresentationPlayerCreate(events))};
                if(!active.player)return 0;active.stateRoot=root;instance->playback.push_back(std::move(active));found=instance->playback.end()-1;
            }
            found->stateRoot=root;if(found->stateEnabled==enabled)continue;found->stateEnabled=enabled;
            if(enabled){if(!hwRmPresentationPlayerStart(found->player.get(),timeline->name,clock,dispatchPresentation,&sink))return 0;}
            else hwRmPresentationPlayerStop(found->player.get());
        }
        return 1;
    }catch(const std::exception&){return 0;}
}
extern "C" int hwRmVisualAssetBindPresentation(HwRmVisualAsset *asset,const char *slot,
    const void *bytes,size_t size,char *error,size_t capacity) {
    try {
        if(!asset||asset->frozen||asset->released)throw std::runtime_error("frozen/invalid presentation asset");
        int component=hwRmSubsystemLayerFindComponent(asset->layer,slot);
        if(component<0)throw std::runtime_error("missing presentation component");
        for(const auto& item:asset->presentation)if(item.first==(unsigned)component)
            throw std::runtime_error("duplicate component presentation events");
        std::unique_ptr<HwRmPresentationEvents,PresentationDelete> events(hwRmPresentationEventsRead(bytes,size,error,capacity));
        if(!events)return 0;
        asset->presentation.emplace_back((unsigned)component,std::move(events));
        detail(error,capacity,"");return 1;
    }catch(const std::exception& e){detail(error,capacity,e.what());return 0;}
}
extern "C" const HwRmPresentationEvents *hwRmVisualAssetPresentation(const HwRmVisualAsset *asset,unsigned component) {
    if(asset&&!asset->released)for(const auto& item:asset->presentation)if(item.first==component)return item.second.get();
    return nullptr;
}
extern "C" const HwRmPresentationEvents *hwRmVisualInstancePresentation(const HwRmVisualInstance *instance,unsigned component) {
    // Existing instances retain the asset after its owner releases the handle.
    if(instance)for(const auto& item:instance->asset->presentation)if(item.first==component)return item.second.get();
    return nullptr;
}
extern "C" HwRmVisualAsset *hwRmVisualAssetCreate(const void *hod,size_t hodSize,
    const void *mad,size_t madSize,char *error,size_t errorSize) {
    return hwRmVisualAssetCreateWithAnimationTargets(hod,hodSize,mad,madSize,nullptr,0,error,errorSize);
}
extern "C" HwRmVisualAsset *hwRmVisualAssetCreateWithAnimationTargets(const void *hod,size_t hodSize,
    const void *mad,size_t madSize,const HwRmAnimationTarget *targets,unsigned targetCount,
    char *error,size_t errorSize) {
    return hwRmVisualAssetCreateWithAnimationClips(hod,hodSize,mad,madSize,targets,targetCount,
        nullptr,0,error,errorSize);
}
extern "C" HwRmVisualAsset *hwRmVisualAssetCreateWithAnimationClips(const void *hod,size_t hodSize,
    const void *mad,size_t madSize,const HwRmAnimationTarget *targets,unsigned targetCount,
    const char *const *clips,unsigned clipCount,char *error,size_t errorSize) {
    try {
        if((mad==nullptr)!=(madSize==0) || targetCount>1024 ||
            (targetCount && (!targets || !mad)) || clipCount>32 || (clipCount && (!clips || !mad)))
            throw std::runtime_error("invalid visual animation bytes/targets");
        auto asset=std::make_unique<HwRmVisualAsset>();asset->hull=read(hod,hodSize);
        for(unsigned i=0;i<targetCount;++i) {
            const auto& target=targets[i];
            if(target.component || !target.sourceJoint || !*target.sourceJoint ||
                !target.targetJoint || !*target.targetJoint ||
                std::strlen(target.sourceJoint)>255 || std::strlen(target.targetJoint)>255)
                throw std::runtime_error("invalid hull animation alias");
            for(const auto& existing:asset->animationTargets)
                if(existing.source==target.sourceJoint || existing.target==target.targetJoint)
                    throw std::runtime_error("duplicate hull animation alias");
            unsigned matches=0;
            for(unsigned j=0;j<hwRmHodSceneJointCount(asset->hull.get());++j) {
                const auto *joint=hwRmHodSceneJoint(asset->hull.get(),j);
                if(!std::strcmp(joint->name,target.sourceJoint))
                    throw std::runtime_error("animation alias shadows authored hull joint");
                matches+=!std::strcmp(joint->name,target.targetJoint);
            }
            if(matches!=1) throw std::runtime_error("missing/ambiguous hull animation alias target");
            asset->animationTargets.push_back({target.sourceJoint,target.targetJoint});
        }
        if(mad) {
            HwRmMad *animation=nullptr;char message[256];
            if(!hwRmMadRead(mad,madSize,&animation,message,sizeof(message))) throw std::runtime_error(message);
            asset->animation.reset(animation);
            std::vector<const char*> joints;
            for(unsigned j=0;j<hwRmHodSceneJointCount(asset->hull.get());++j)
                joints.push_back(hwRmHodSceneJoint(asset->hull.get(),j)->name);
            for(const auto& target:asset->animationTargets) {
                bool used=false;
                for(unsigned ci=0;ci<hwRmMadClipCount(animation);++ci) {
                    const auto *clip=hwRmMadClip(animation,ci);
                    for(unsigned ti=0;ti<clip->trackCount;++ti) used|=target.source==clip->tracks[ti].joint;
                }
                if(!used) throw std::runtime_error("unused hull animation alias");
                joints.push_back(target.source.c_str());
            }
            if(!clipCount) {
                if(!hwRmMadValidateJoints(animation,joints.data(),unsigned(joints.size()),message,sizeof(message)))
                    throw std::runtime_error(message);
            } else for(unsigned i=0;i<clipCount;++i) {
                if(!clips[i] || !*clips[i] || std::strlen(clips[i])>255 ||
                   std::find(asset->allowedClips.begin(),asset->allowedClips.end(),clips[i])!=asset->allowedClips.end())
                    throw std::runtime_error("invalid/duplicate selected animation clip");
                const auto *clip=hwRmMadFindClip(animation,clips[i]);
                if(!clip || !clip->playable) throw std::runtime_error("missing/empty selected animation clip");
                for(unsigned t=0;t<clip->trackCount;++t) {
                    unsigned matches=0;
                    for(const char *joint:joints) matches+=!std::strcmp(joint,clip->tracks[t].joint);
                    if(matches!=1) throw std::runtime_error("missing/ambiguous selected animation joint");
                }
                asset->allowedClips.emplace_back(clips[i]);
            }
        }
        asset->layer=hwRmSubsystemLayerCreate(asset->hull.get(),error,errorSize);
        if(!asset->layer) return nullptr;
        detail(error,errorSize,"");return asset.release();
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return nullptr;}
}
extern "C" int hwRmVisualAssetAttach(HwRmVisualAsset *asset,const char *key,
    const char *slot,const char *anchor,unsigned role,const void *hod,size_t hodSize,
    char *error,size_t errorSize) {
    try {
        if(!asset || asset->frozen || asset->released || !key || !*key || std::strlen(key)>255 ||
            !hod || !hodSize || hodSize>64*1024*1024 || asset->models.size()>=256)
            throw std::runtime_error("invalid/frozen visual asset attachment");
        auto found=std::find_if(asset->models.begin(),asset->models.end(),
            [key](const Model& model){return model.key==key;});
        if(found!=asset->models.end()) {
            if(found->bytes->size()!=hodSize || std::memcmp(found->bytes->data(),hod,hodSize))
                throw std::runtime_error("shared visual model key has different source bytes");
            return hwRmSubsystemLayerAdd(asset->layer,slot,anchor,role,found->scene.get(),error,errorSize);
        }
        Model model;model.key=key;model.scene=read(hod,hodSize,&model.bytes);
        // Reserve before layer mutation: allocation failure cannot leave a
        // borrowed layer component referring to a destroyed temporary scene.
        asset->models.reserve(asset->models.size()+1);
        if(!hwRmSubsystemLayerAdd(asset->layer,slot,anchor,role,model.scene.get(),error,errorSize)) return 0;
        asset->models.push_back(std::move(model));detail(error,errorSize,"");return 1;
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" void hwRmVisualAssetFree(HwRmVisualAsset *asset) {
    if(asset) {asset->released=true;if(!asset->instances) delete asset;}
}
extern "C" int hwRmVisualAssetMount(HwRmVisualAsset *asset,const char *slot,const float *matrix) {
    return asset&&!asset->frozen&&!asset->released && hwRmSubsystemLayerMount(asset->layer,slot,matrix);
}
extern "C" int hwRmVisualAssetAuthorJoint(HwRmVisualAsset *asset,const char *slot,const char *joint,const char *parent,const float *matrix) {
    return asset&&!asset->frozen&&!asset->released&&hwRmSubsystemLayerAuthorJoint(asset->layer,slot,joint,parent,matrix);
}
extern "C" int hwRmVisualAssetBindAnimation(HwRmVisualAsset *asset,const char *name,
    const char *const *clips,unsigned count,char *error,size_t errorSize) {
    try {
        if(!asset || asset->frozen || asset->released || !asset->animation || !name || !*name ||
            std::strlen(name)>255 || !clips || !count || count>32 || asset->events.size()>=128)
            throw std::runtime_error("invalid/frozen visual event binding");
        for(const auto& event:asset->events) if(event.name==name)
            throw std::runtime_error("duplicate visual gameplay event");
        Event event;event.name=name;
        for(unsigned i=0;i<count;++i) {
            if(!clips[i]) throw std::runtime_error("null source event clip");
            if(!asset->allowedClips.empty() &&
               std::find(asset->allowedClips.begin(),asset->allowedClips.end(),clips[i])==asset->allowedClips.end())
                throw std::runtime_error("excluded source event clip");
            bool found=false;
            for(unsigned ci=0;ci<hwRmMadClipCount(asset->animation.get());++ci) {
                const auto *clip=hwRmMadClip(asset->animation.get(),ci);
                if(std::strcmp(clip->name,clips[i])==0) {
                    if(!clip->playable) throw std::runtime_error("empty source event clip");
                    if(std::find(event.clips.begin(),event.clips.end(),ci)!=event.clips.end())
                        throw std::runtime_error("duplicate source event clip");
                    event.clips.push_back(ci);found=true;break;
                }
            }
            if(!found) throw std::runtime_error(std::string("missing source event clip: ")+clips[i]);
        }
        asset->events.push_back(std::move(event));detail(error,errorSize,"");return 1;
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" HwRmVisualInstance *hwRmVisualInstanceCreate(HwRmVisualAsset *asset) {
    if(!asset || asset->released) return nullptr;
    try {
        auto instance=std::make_unique<HwRmVisualInstance>();instance->asset=asset;
        instance->subsystem=hwRmSubsystemInstanceCreate(asset->layer);
        if(!instance->subsystem) return nullptr;
        asset->frozen=true;++asset->instances;return instance.release();
    } catch(const std::exception&) {return nullptr;}
}
extern "C" HwRmVisualInstance *hwRmVisualInstanceSnapshot(const HwRmVisualInstance *source){
    if(!source)return nullptr;
    try {auto copy=std::make_unique<HwRmVisualInstance>();copy->asset=source->asset;
        copy->subsystem=hwRmSubsystemInstanceSnapshot(source->subsystem);if(!copy->subsystem)return nullptr;
        ++copy->asset->instances;return copy.release();}
    catch(const std::exception&){return nullptr;}
}
extern "C" int hwRmVisualInstanceSampleSourceClip(HwRmVisualInstance *instance,const char *name,
    double phase,char *error,size_t capacity){
    try {
        if(!instance||!name||!*name||!std::isfinite(phase)||phase<0||phase>1||!instance->asset->animation)
            throw std::runtime_error("invalid native presentation clip request");
        const HwRmMad *mad=instance->asset->animation.get();
        const HwRmMadClip *clip=hwRmMadFindClip(mad,name);
        if(!clip||!clip->playable)throw std::runtime_error("source presentation clip unavailable");
        unsigned index=unsigned(clip-hwRmMadClip(mad,0));
        std::vector<HwRmAnimationTarget> targets;
        for(const auto& target:instance->asset->animationTargets)
            targets.push_back({target.source.c_str(),0,target.target.c_str()});
        return hwRmSubsystemSampleMappedAnimations(instance->subsystem,0,mad,&index,&phase,1,
            targets.data(),unsigned(targets.size()),error,capacity);
    }catch(const std::exception& e){detail(error,capacity,e.what());return 0;}
}
extern "C" int hwRmVisualAssetBindGun(HwRmVisualAsset *asset,unsigned gun,
    const char *slot,const char *azimuth,const char *declination,const char *recoil,
    const char *muzzle,char *error,size_t errorSize) {
    try {
        if(!asset || asset->frozen || asset->released || gun>=64 || !slot || !muzzle)
            throw std::runtime_error("invalid/frozen native gun binding");
        GunBinding binding{};binding.gun=gun;
        unsigned joint=0;
        if(!hwRmSubsystemLayerFindJoint(asset->layer,slot,muzzle,&binding.component,&joint))
            throw std::runtime_error("missing/ambiguous native muzzle");
        binding.muzzle=muzzle;
        const char *names[]={azimuth,declination,recoil};
        std::string *targets[]={&binding.azimuth,&binding.declination,&binding.recoil};
        for(unsigned i=0;i<3;++i) if(names[i]) {
            unsigned component=0;
            if(!hwRmSubsystemLayerFindJoint(asset->layer,slot,names[i],&component,&joint) || component!=binding.component)
                throw std::runtime_error("missing/ambiguous native gun pivot");
            *targets[i]=names[i];
        }
        for(const auto& other:asset->guns) {
            if(other.gun==gun) throw std::runtime_error("duplicate native gun index");
            if(other.component==binding.component) {
                const std::string *a[]={&binding.azimuth,&binding.declination,&binding.recoil};
                const std::string *b[]={&other.azimuth,&other.declination,&other.recoil};
                for(auto x:a) for(auto y:b) if(!x->empty() && *x==*y)
                    throw std::runtime_error("native guns share a controlled pivot");
            }
        }
        asset->guns.push_back(std::move(binding));detail(error,errorSize,"");return 1;
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" unsigned hwRmVisualInstanceGunCount(const HwRmVisualInstance *instance) {
    return instance?unsigned(instance->asset->guns.size()):0;
}
extern "C" int hwRmVisualInstanceGunIndex(const HwRmVisualInstance *instance,unsigned ordinal,unsigned *gun) {
    if(!instance || !gun || ordinal>=instance->asset->guns.size()) return 0;
    *gun=instance->asset->guns[ordinal].gun;return 1;
}
extern "C" int hwRmVisualInstanceSetGun(HwRmVisualInstance *instance,unsigned gun,
    float azimuth,float declination,float recoil,char *error,size_t errorSize) {
    const HwRmGunControl control={gun,azimuth,declination,recoil};
    return hwRmVisualInstanceSetGuns(instance,&control,1,error,errorSize);
}
extern "C" int hwRmVisualInstanceSetGuns(HwRmVisualInstance *instance,
    const HwRmGunControl *guns,unsigned gunCount,char *error,size_t errorSize) {
    try {
        if(!instance || !guns || !gunCount || gunCount>64) throw std::runtime_error("invalid native gun batch");
        HwRmJointControlUpdate updates[192]{};unsigned count=0;
        for(unsigned gi=0;gi<gunCount;++gi) {
            const auto& control=guns[gi];const float azimuth=control.azimuth,declination=control.declination,recoil=control.recoil;
            if(!std::isfinite(azimuth) || !std::isfinite(declination) || !std::isfinite(recoil) ||
                std::abs(azimuth)>64 || std::abs(declination)>64 || recoil<0 || recoil>100000)
                throw std::runtime_error("invalid native gun controls");
            for(unsigned earlier=0;earlier<gi;++earlier) if(guns[earlier].gun==control.gun)
                throw std::runtime_error("duplicate native gun batch index");
            const auto found=std::find_if(instance->asset->guns.begin(),instance->asset->guns.end(),
                [&control](const GunBinding& binding){return binding.gun==control.gun;});
            if(found==instance->asset->guns.end()) throw std::runtime_error("unbound native gun index");
            const auto& binding=*found;
            const std::string *names[]={&binding.azimuth,&binding.declination,&binding.recoil};
            for(unsigned i=0;i<3;++i) if(!names[i]->empty()) {
                bool repeated=false;for(unsigned j=0;j<i;++j) if(*names[j]==*names[i]) repeated=true;
                if(repeated) continue;
                updates[count++]={binding.component,names[i]->c_str(),
                    *names[i]==binding.azimuth?azimuth:0.f,
                    *names[i]==binding.declination?declination:0.f,
                    *names[i]==binding.recoil?recoil:0.f};
            }
        }
        if(!count) {detail(error,errorSize,"");return 1;}
        return hwRmSubsystemSetJointControls(instance->subsystem,updates,count,error,errorSize);
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" int hwRmVisualInstanceGunMuzzle(HwRmVisualInstance *instance,unsigned gun,
    float scale,float *matrix,char *error,size_t errorSize) {
    if(instance) for(const auto& binding:instance->asset->guns) if(binding.gun==gun)
        return hwRmSubsystemJointTransform(instance->subsystem,binding.component,binding.muzzle.c_str(),
            scale,nullptr,nullptr,matrix,error,errorSize);
    detail(error,errorSize,"unbound native gun muzzle");return 0;
}
extern "C" int hwRmVisualInstanceDockEndpoint(const HwRmVisualInstance *instance,
    const char *name,float scale,float position[3]) {
    if(!instance || !name || !position || !std::isfinite(scale) || scale<=0 || scale>100000) return 0;
    const auto *hull=instance->asset->hull.get();
    for(unsigned i=0;i<hwRmHodSceneDockPathCount(hull);++i) {
        const auto *path=hwRmHodSceneDockPath(hull,i);
        if(!path || std::strcmp(path->name,name) || std::strcmp(path->parent,"world") || !path->pointCount) continue;
        const auto& point=path->points[path->pointCount-1];
        const float result[3]={point.position[1]*scale,-point.position[0]*scale,point.position[2]*scale};
        for(float value:result) if(!std::isfinite(value) || std::abs(value)>1000000) return 0;
        std::copy(result,result+3,position);return 1;
    }
    return 0;
}
extern "C" int hwRmVisualInstanceAimGuns(HwRmVisualInstance *instance,
    const HwRmGunAim *guns,unsigned gunCount,char *error,size_t errorSize) {
    try {
        if(!instance || !guns || !gunCount || gunCount>64)
            throw std::runtime_error("invalid native gun aim batch");
        HwRmGunControl controls[64]{};
        for(unsigned gi=0;gi<gunCount;++gi) {
            const auto& aim=guns[gi];auto& control=controls[gi];
            control.gun=aim.gun;control.recoil=aim.recoil;
            if(!std::isfinite(aim.recoil) || aim.recoil<0 || aim.recoil>100000)
                throw std::runtime_error("invalid native gun aim recoil");
            float length=0;
            for(float v:aim.heading) {
                if(!std::isfinite(v)) throw std::runtime_error("nonfinite native gun heading");
                length+=v*v;
            }
            if(!std::isfinite(length) || length<1e-12f)
                throw std::runtime_error("empty native gun heading");
            const auto found=std::find_if(instance->asset->guns.begin(),instance->asset->guns.end(),
                [&aim](const GunBinding& b){return b.gun==aim.gun;});
            if(found==instance->asset->guns.end()) throw std::runtime_error("unbound native gun aim");
            const auto& binding=*found;
            if(binding.azimuth.empty() && binding.declination.empty()) continue; // Fixed barrel.
            if(!binding.azimuth.empty() && binding.declination.empty()) {
                // A single-axis mount cannot chase the target out of its
                // hinge plane. Project the physical heading into that plane
                // and rotate only about local X (RM Y after basis conversion).
                // No tipping/second articulation is invented.
                float pivot[16],muzzle[16],target[3]{},tail[3]{};
                if(!hwRmSubsystemNeutralJointTransform(instance->subsystem,binding.component,binding.azimuth.c_str(),pivot,error,errorSize) ||
                   !hwRmSubsystemNeutralJointTransform(instance->subsystem,binding.component,binding.muzzle.c_str(),muzzle,error,errorSize)) return 0;
                for(unsigned c=0;c<3;++c) for(unsigned r=0;r<3;++r) {
                    target[c]+=pivot[c*4+r]*aim.heading[r];
                    tail[c]+=pivot[c*4+r]*muzzle[8+r];
                }
                control.azimuth=std::hypot(target[1],target[2])<1e-6f?0.f:
                    std::atan2(-target[1],target[2])-std::atan2(-tail[1],tail[2]);
                control.declination=0;
                continue;
            }
            float pivot[16],latitude[16],muzzle[16];
            if(!hwRmSubsystemNeutralJointTransform(instance->subsystem,binding.component,binding.azimuth.c_str(),pivot,error,errorSize) ||
               !hwRmSubsystemNeutralJointTransform(instance->subsystem,binding.component,binding.declination.c_str(),latitude,error,errorSize) ||
               !hwRmSubsystemNeutralJointTransform(instance->subsystem,binding.component,binding.muzzle.c_str(),muzzle,error,errorSize)) return 0;
            // Solve in the native mount frame, including authored rest rotations
            // and animated attachment parents. Never reuse classic model Euler angles.
            float target[3]{},relative[9]{},tail[3]{};
            for(unsigned col=0;col<3;++col) {
                for(unsigned row=0;row<3;++row) {
                    target[col]+=pivot[col*4+row]*aim.heading[row]/std::sqrt(length);
                    tail[col]+=latitude[col*4+row]*muzzle[8+row];
                    for(unsigned k=0;k<3;++k)
                        relative[col*3+row]+=pivot[row*4+k]*latitude[col*4+k];
                }
            }
            if(binding.azimuth==binding.declination) {
                for(unsigned i=0;i<9;++i) relative[i]=(i%4==0)?1.f:0.f;
            }
            float a=std::atan2(-target[1],target[2]);
            float d=std::atan2(target[0],std::hypot(target[1],target[2]));
            bool converged=false;
            for(unsigned iteration=0;iteration<24;++iteration) {
                const float ca=std::cos(a),sa=std::sin(a),cd=std::cos(d),sd=std::sin(d);
                const float v[]={cd*tail[0]+sd*tail[2],tail[1],-sd*tail[0]+cd*tail[2]};
                const float dv[]={v[2],0,-v[0]};float b[3]{},db[3]{};
                for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) {
                    b[r]+=relative[c*3+r]*v[c];db[r]+=relative[c*3+r]*dv[c];
                }
                const float predicted[]={b[0],ca*b[1]-sa*b[2],sa*b[1]+ca*b[2]};
                const float ja[]={0,-predicted[2],predicted[1]};
                const float jd[]={db[0],ca*db[1]-sa*db[2],sa*db[1]+ca*db[2]};
                float aa=1e-7f,dd=1e-7f,ad=0,ar=0,dr=0,squared=0;
                for(unsigned k=0;k<3;++k) {
                    const float residual=target[k]-predicted[k];squared+=residual*residual;
                    aa+=ja[k]*ja[k];dd+=jd[k]*jd[k];ad+=ja[k]*jd[k];
                    ar+=ja[k]*residual;dr+=jd[k]*residual;
                }
                if(squared<1e-8f) {converged=true;break;}
                const float determinant=aa*dd-ad*ad;
                a+=std::clamp((ar*dd-dr*ad)/determinant,-.5f,.5f);
                d+=std::clamp((dr*aa-ar*ad)/determinant,-.5f,.5f);
            }
            if(!converged) throw std::runtime_error("native turret heading solve failed");
            control.azimuth=a;control.declination=d;
        }
        // The solver stages the whole batch. Invalid inputs never reset or partly aim a ship.
        return hwRmVisualInstanceSetGuns(instance,controls,gunCount,error,errorSize);
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" int hwRmVisualAssetBindMarker(HwRmVisualAsset *asset,const char *semantic,
    const char *slot,const char *name,char *error,size_t errorSize) {
    try {
        if(!asset || asset->frozen || asset->released || !semantic || !*semantic ||
            std::strlen(semantic)>255 || !slot || !name || asset->markers.size()>=256)
            throw std::runtime_error("invalid/frozen native marker binding");
        for(const auto& marker:asset->markers) if(marker.semantic==semantic)
            throw std::runtime_error("duplicate native marker semantic");
        MarkerBinding binding{};unsigned joint=0;
        if(!hwRmSubsystemLayerFindJoint(asset->layer,slot,name,&binding.component,&joint))
            throw std::runtime_error("missing/ambiguous native semantic marker");
        binding.semantic=semantic;binding.joint=name;asset->markers.push_back(std::move(binding));
        detail(error,errorSize,"");return 1;
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" int hwRmVisualAssetBindGunPresentation(HwRmVisualAsset *asset,unsigned gun,
    const char *timeline,char *error,size_t capacity) {
    try {
        if(!asset || asset->released || asset->frozen || !timeline || !*timeline || std::strlen(timeline)>255)
            throw std::runtime_error("invalid/frozen weapon presentation binding");
        for(auto& binding:asset->guns)if(binding.gun==gun){
            if(!binding.presentation.empty())throw std::runtime_error("duplicate weapon presentation binding");
            const auto *events=hwRmVisualAssetPresentation(asset,binding.component);
            if(hwRmPresentationFindTimeline(events,timeline)<0)
                throw std::runtime_error("weapon presentation timeline missing from owning component");
            binding.presentation=timeline;detail(error,capacity,"");return 1;
        }
        throw std::runtime_error("weapon presentation refers to absent gameplay gun");
    }catch(const std::exception& e){detail(error,capacity,e.what());return 0;}
}
extern "C" int hwRmVisualAssetBindWeaponPresentation(HwRmVisualAsset *asset,unsigned gun,
    const void *bytes,size_t size,char *error,size_t capacity) {
    if(!asset || asset->released || asset->frozen){detail(error,capacity,"invalid/frozen weapon presentation asset");return 0;}
    for(auto& binding:asset->guns)if(binding.gun==gun){
        if(binding.weapon){detail(error,capacity,"duplicate weaponfire presentation binding");return 0;}
        binding.weapon.reset(hwRmWeaponPresentationRead(bytes,size,error,capacity));return binding.weapon?1:0;
    }
    detail(error,capacity,"weaponfire presentation refers to absent gun");return 0;
}
extern "C" const HwRmWeaponPresentation *hwRmVisualInstanceWeaponPresentation(const HwRmVisualInstance *instance,unsigned gun) {
    if(instance)for(const auto& binding:instance->asset->guns)if(binding.gun==gun)return binding.weapon.get();
    return nullptr;
}
extern "C" int hwRmVisualInstanceGunHasFx(const HwRmVisualInstance *instance,unsigned gun,HwRmVisualFxAvailable available,void *user){
    if(instance&&available)for(const auto& binding:instance->asset->guns)if(binding.gun==gun){
        if(binding.weapon){const auto* fire=hwRmWeaponPresentationAt(binding.weapon.get(),HW_RM_WEAPON_FIRE);if(fire&&*fire->fx&&available(user,fire->fx))return 1;}
        if(binding.presentation.empty())return 0;
        const auto* events=hwRmVisualInstancePresentation(instance,binding.component);
        const int timeline=hwRmPresentationFindTimeline(events,binding.presentation.c_str());
        for(unsigned i=0;i<hwRmPresentationEventCount(events);++i){const auto* event=hwRmPresentationEventAt(events,i);
            if(event->timeline==unsigned(timeline)&&*event->fx&&available(user,event->fx))return 1;}
        return 0;
    }
    return 0;
}
extern "C" int hwRmVisualInstanceGunHasSound(const HwRmVisualInstance *instance,unsigned gun) {
    if(instance)for(const auto& binding:instance->asset->guns)if(binding.gun==gun){
        if(binding.weapon){const auto* fire=hwRmWeaponPresentationAt(binding.weapon.get(),HW_RM_WEAPON_FIRE);if(fire&&*fire->sound)return 1;}
        if(binding.presentation.empty())return 0;
        const auto* events=hwRmVisualInstancePresentation(instance,binding.component);
        const int timeline=hwRmPresentationFindTimeline(events,binding.presentation.c_str());
        for(unsigned i=0;i<hwRmPresentationEventCount(events);++i){const auto* event=hwRmPresentationEventAt(events,i);if(event->timeline==unsigned(timeline)&&*event->sound)return 1;}
        return 0;
    }
    return 0;
}
extern "C" int hwRmVisualInstanceEventGun(const HwRmVisualInstance *instance,unsigned component,
    const HwRmPresentationEvent *event,unsigned *gun) {
    if(!instance||!event||!gun||!event->markers)return 0;
    const auto* events=hwRmVisualInstancePresentation(instance,component);
    const auto* timeline=events?hwRmPresentationTimelineAt(events,event->timeline):nullptr;
    if(!timeline)return 0;
    bool found=false;
    for(const auto& binding:instance->asset->guns)
        if(binding.component==component && binding.presentation==timeline->name && binding.muzzle==event->markers){
            if(found)return 0;
            *gun=binding.gun;found=true;
        }
    return found?1:0;
}
extern "C" int hwRmVisualInstanceFirePresentation(HwRmVisualInstance *instance,unsigned gun,
    double clock,HwRmVisualPresentationVisit visit,void *user) {
    if(instance)for(const auto& binding:instance->asset->guns)if(binding.gun==gun && !binding.presentation.empty())
        return hwRmVisualInstanceStartPresentation(instance,binding.component,binding.presentation.c_str(),clock,visit,user);
    return 0;
}
extern "C" int hwRmVisualInstanceMarker(HwRmVisualInstance *instance,const char *semantic,
    float scale,float *matrix,char *error,size_t errorSize) {
    if(instance && semantic) for(const auto& marker:instance->asset->markers) if(marker.semantic==semantic)
        return hwRmSubsystemJointTransform(instance->subsystem,marker.component,marker.joint.c_str(),
            scale,nullptr,nullptr,matrix,error,errorSize);
    detail(error,errorSize,"unbound native marker semantic");return 0;
}
extern "C" void hwRmVisualInstanceFree(HwRmVisualInstance *instance) {
    if(instance) {
        auto *asset=instance->asset;hwRmSubsystemInstanceFree(instance->subsystem);
        delete instance;--asset->instances;if(asset->released && !asset->instances) delete asset;
    }
}
extern "C" HwRmSubsystemInstance *hwRmVisualInstanceSubsystem(HwRmVisualInstance *instance) {
    return instance?instance->subsystem:nullptr;
}
extern "C" const HwRmMad *hwRmVisualInstanceAnimation(const HwRmVisualInstance *instance) {
    return instance?instance->asset->animation.get():nullptr;
}
extern "C" int hwRmVisualInstanceSampleEvent(HwRmVisualInstance *instance,const char *name,
    double phase,char *error,size_t errorSize) {
    try {
        if(!instance || !name) throw std::runtime_error("invalid visual event request");
        for(const auto& event:instance->asset->events) if(event.name==name) {
            std::vector<double> phases(event.clips.size(),phase);
            std::vector<HwRmAnimationTarget> targets;
            for(const auto& target:instance->asset->animationTargets)
                targets.push_back({target.source.c_str(),0,target.target.c_str()});
            return hwRmSubsystemSampleMappedAnimations(instance->subsystem,0,instance->asset->animation.get(),
                event.clips.data(),phases.data(),unsigned(phases.size()),targets.data(),unsigned(targets.size()),error,errorSize);
        }
        throw std::runtime_error(std::string("unbound visual gameplay event: ")+name);
    } catch(const std::exception& e) {detail(error,errorSize,e.what());return 0;}
}
extern "C" int hwRmVisualInstanceAnimationPresentation(HwRmVisualInstance *instance,const char *name,
    double phase,HwRmVisualPresentationVisit visit,void *user) {
    if(!instance || !name || !visit || !std::isfinite(phase) || phase<0 || phase>1)return 0;
    try {
        const Event* binding=nullptr;
        for(const auto& event:instance->asset->events)if(event.name==name){binding=&event;break;}
        if(!binding)return 0;
        const auto* events=hwRmVisualInstancePresentation(instance,0);
        if(!events)return 1;
        const bool start=instance->phaseEvent!=name || phase<instance->presentationPhase;
        if(!start && instance->presentationPhase==phase)return 1;
        const double previous=start?(phase>0?-1:0):
            (instance->presentationPhaseFirst?-1:instance->presentationPhase);
        std::vector<unsigned> timelines;
        for(unsigned clip:binding->clips){
            const auto* animation=hwRmMadClip(instance->asset->animation.get(),clip);
            int index=animation?hwRmPresentationFindTimeline(events,animation->name):-1;
            if(index>=0 && std::find(timelines.begin(),timelines.end(),unsigned(index))==timelines.end())
                timelines.push_back(unsigned(index));
        }
        PresentationSink sink{0,visit,user};
        for(unsigned timeline:timelines)
            if(!hwRmPresentationVisitPhase(events,timeline,previous,phase,start,dispatchPresentation,&sink))return 0;
        instance->phaseEvent=name;instance->presentationPhase=phase;
        instance->presentationPhaseFirst=phase==0;return 1;
    }catch(const std::exception&){return 0;}
}
