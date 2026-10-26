#include "RemasteredSubsystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
namespace {
using Matrix=std::array<float,16>;
Matrix identity() {return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r)
        for(unsigned k=0;k<4;++k) out[c*4+r]+=a[k*4+r]*b[c*4+k];
    return out;
}
Matrix basis(float scale=1.f) {return {0,-scale,0,0,scale,0,0,0,0,0,scale,0,0,0,0,1};}
Matrix local(const HwRmHodJoint& joint,float scale) {
    const float x=joint.rotation[0],y=joint.rotation[1],z=joint.rotation[2];
    const float cx=std::cos(x),sx=std::sin(x),cy=std::cos(y),sy=std::sin(y),cz=std::cos(z),sz=std::sin(z);
    const Matrix rx={1,0,0,0,0,cx,sx,0,0,-sx,cx,0,0,0,0,1};
    const Matrix ry={cy,0,-sy,0,0,1,0,0,sy,0,cy,0,0,0,0,1};
    const Matrix rz={cz,sz,0,0,-sz,cz,0,0,0,0,1,0,0,0,0,1};
    const Matrix inverse={0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1};
    Matrix out=multiply(multiply(basis(),multiply(rz,multiply(ry,rx))),inverse);
    out[12]=joint.position[1]*scale;out[13]=-joint.position[0]*scale;out[14]=joint.position[2]*scale;
    return out;
}
struct AnimatedJoint {
    HwRmHodJoint joint;
    std::array<float,3> scale;
};
struct JointControl {float azimuth,declination,recoil;};
Matrix controlledLocal(Matrix base,const JointControl& control) {
    const float cx=std::cos(control.azimuth),sx=std::sin(control.azimuth);
    const float cy=std::cos(control.declination),sy=std::sin(control.declination);
    const Matrix rx={1,0,0,0,0,cx,sx,0,0,-sx,cx,0,0,0,0,1};
    const Matrix ry={cy,0,-sy,0,0,1,0,0,sy,0,cy,0,0,0,0,1};
    base=multiply(multiply(base,rx),ry);
    for(unsigned axis=0;axis<3;++axis) base[12+axis]-=base[8+axis]*control.recoil;
    return base;
}
Matrix animatedLocal(const AnimatedJoint& pose,float modelScale) {
    Matrix out=local(pose.joint,modelScale);
    // A maps RM X to -HW Y, RM Y to HW X, and RM Z to HW Z.
    const float hwScale[]={pose.scale[1],pose.scale[0],pose.scale[2]};
    for(unsigned c=0;c<3;++c) for(unsigned r=0;r<3;++r) out[c*4+r]*=hwScale[c];
    return out;
}
void validate(const Matrix& m) {
    for(float v:m) if(!std::isfinite(v)) throw std::runtime_error("nonfinite subsystem pose");
    if(std::abs(m[3])>1e-6 || std::abs(m[7])>1e-6 || std::abs(m[11])>1e-6 || std::abs(m[15]-1)>1e-6)
        throw std::runtime_error("non-affine subsystem pose");
}
void message(char *error,size_t size,const char *value) {if(error && size) std::snprintf(error,size,"%s",value);}
bool visibleSurface(const HwRmHodMaterial *material) {
    // innateSS is the RM subsystem-selection/effect proxy, not ship hull.
    // Preserve its hierarchy/markers but never invent an opaque raster/DXR
    // surface for its untextured UI/effect geometry.
    return material && std::strcmp(material->shader,"innateSS")!=0 && std::strcmp(material->shader,"innatess")!=0;
}
struct Component {
    std::string slot;
    unsigned role, anchor;
    const HwRmHodScene *scene;
    std::vector<int> parents;
    std::vector<Matrix> restLocal;
    std::array<std::vector<const HwRmHodMesh *>,16> meshesForLod;
    std::unordered_map<std::string,unsigned> joints;
    std::unordered_map<std::string,unsigned> multiplicity;
    Matrix mountFrame=identity(), authoredMount=identity();
    std::unordered_map<unsigned,Matrix> authoredJoints;
};
void prepareComponentMeshes(Component &component);

Component component(const char *slot,unsigned role,unsigned anchor,const HwRmHodScene *scene) {
    if(!scene || !slot || !*slot || std::strlen(slot)>256) throw std::runtime_error("invalid subsystem definition");
    Component c;c.slot=slot;c.role=role;c.anchor=anchor;c.scene=scene;
    const unsigned count=hwRmHodSceneJointCount(scene);
    if(!count) throw std::runtime_error("subsystem has no hierarchy");
    for(unsigned i=0;i<count;++i) {
        const auto *j=hwRmHodSceneJoint(scene,i);c.joints.emplace(j->name,i);++c.multiplicity[j->name];
        c.restLocal.push_back(local(*j,1));
    }
    for(unsigned i=0;i<count;++i) {
        const auto *j=hwRmHodSceneJoint(scene,i);
        if(!*j->parent) c.parents.push_back(-1);
        else {
            if(c.multiplicity[j->parent]!=1) throw std::runtime_error("ambiguous subsystem parent");
            c.parents.push_back(int(c.joints.at(j->parent)));
        }
    }
    prepareComponentMeshes(c);
    return c;
}
bool compatibleMaterialLod(const Component& component,const HwRmHodMesh *candidate) {
    /* The RM corvettes visibly change UVs/detail, and the Raider fighter's
       source LOD drops from 2616 triangles to 175 (then 48). Keep these
       ships' highest-detail RM records until compatible reductions are
       authored. The smaller Kadeshi ships likewise contain placeholder/baked
       low LODs (the Swarmer swaps its textured hull for nomat vertex colors).
       Their UVs cannot sample the native hull texture correctly. The Kadeshi
       mothership has compatible authored LODs and retains normal reduction.
       Never substitute classic geometry. */
    if(!component.joints.count("P1StandardCorvette") &&
       !component.joints.count("P1MissileCorvette") &&
       !component.joints.count("P1Fighter") &&
       !component.joints.count("P2Swarmer") &&
       !component.joints.count("P2AdvanceSwarmer") &&
       !component.joints.count("P2FuelPod") &&
       !component.joints.count("P2MultiBeamFrigate")) return true;
    const HwRmHodMesh *reference=candidate;
    for(unsigned i=0;i<hwRmHodSceneMeshCount(component.scene);++i) {
        const auto *mesh=hwRmHodSceneMesh(component.scene,i);
        if(!std::strcmp(mesh->name,candidate->name) && mesh->lod<reference->lod) reference=mesh;
    }
    return candidate->lod==reference->lod;
}
void prepareComponentMeshes(Component &c) {
    // Source hierarchy/meshes are immutable for the layer lifetime. Keep the
    // exact existing selection policy and source draw order for every LOD.
    for(unsigned lod=0;lod<c.meshesForLod.size();++lod) {
        std::unordered_map<std::string,const HwRmHodMesh*> selected;
        for(unsigned i=0;i<hwRmHodSceneMeshCount(c.scene);++i) {
            const auto *mesh=hwRmHodSceneMesh(c.scene,i);
            if(!compatibleMaterialLod(c,mesh)) continue;
            auto& best=selected[mesh->name];
            if(!best || (mesh->lod<=lod && (best->lod>lod || mesh->lod>best->lod)) ||
                (mesh->lod>lod && best->lod>lod && mesh->lod<best->lod)) best=mesh;
        }
        for(unsigned i=0;i<hwRmHodSceneMeshCount(c.scene);++i) {
            const auto *mesh=hwRmHodSceneMesh(c.scene,i);
            if(selected.at(mesh->name)==mesh) c.meshesForLod[lod].push_back(mesh);
        }
    }
}
Matrix restLocal(const Component &c,unsigned index,float scale) {
    Matrix result=c.restLocal[index];
    const auto *joint=hwRmHodSceneJoint(c.scene,index);
    // Preserve the original multiplication order, including signed zero.
    result[12]=joint->position[1]*scale;
    result[13]=-joint->position[0]*scale;
    result[14]=joint->position[2]*scale;
    return result;
}

}
struct HwRmSubsystemLayer {std::vector<Component> components;bool frozen=false,releaseRequested=false;unsigned instances=0;};
struct HwRmSubsystemInstance {
    HwRmSubsystemLayer *layer;
    std::vector<unsigned char> visible;
    std::vector<std::unordered_map<unsigned,AnimatedJoint>> animation;
    std::vector<std::unordered_map<unsigned,JointControl>> controls;
};
extern "C" HwRmSubsystemLayer *hwRmSubsystemLayerCreate(const HwRmHodScene *hull,char *error,size_t size) {
    try {
        auto layer=std::make_unique<HwRmSubsystemLayer>();layer->components.push_back(component("Hull",HW_RM_HULL,0,hull));
        message(error,size,"");return layer.release();
    } catch(const std::exception& e) {message(error,size,e.what());return nullptr;}
}
extern "C" int hwRmSubsystemLayerAdd(HwRmSubsystemLayer *layer,const char *slot,const char *anchor,
                                      unsigned role,const HwRmHodScene *model,char *error,size_t size) {
    try {
        if(!layer || layer->frozen || !anchor || role<HW_RM_TURRET || role>HW_RM_ATTACHMENT || layer->components.size()>=256)
            throw std::runtime_error("invalid/frozen subsystem layer");
        if(!slot) throw std::runtime_error("null subsystem slot");
        for(const auto& c:layer->components) if(c.slot==slot) throw std::runtime_error("duplicate subsystem slot");
        const auto& hull=layer->components[0];
        const auto found=hull.multiplicity.find(anchor);
        if(found==hull.multiplicity.end() || found->second!=1) throw std::runtime_error("missing/ambiguous hull attachment anchor");
        auto c=component(slot,role,hull.joints.at(anchor),model);
        /* The mirrored assault-cannon sockets use Direction/Rest markers to
           define their frame. Their Position Euler frame alone points the
           opposite way (Gun2) or sideways (Gun4); blindly attaching there
           rolls the physical turret into the hull. Keep the source HOD intact. */
        if(hull.joints.count("StandardFrigate") &&
           hull.joints.count("Hardpoint_Gun4_Position_ExportHelper") &&
           (!std::strcmp(anchor,"Hardpoint_Gun2_Position") ||
            !std::strcmp(anchor,"Hardpoint_Gun4_Position"))) {
            const std::string prefix=std::string(anchor).substr(0,std::strlen(anchor)-9);
            const auto *direction=hwRmHodSceneJoint(hull.scene,hull.joints.at(prefix+"_Direction"));
            const auto *rest=hwRmHodSceneJoint(hull.scene,hull.joints.at(prefix+"_Rest"));
            float x[]={direction->position[1],-direction->position[0],direction->position[2]};
            float z[]={rest->position[1],-rest->position[0],rest->position[2]};
            auto normalize=[](float *v) {
                const float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
                if(n<1e-6f) throw std::runtime_error("degenerate assault cannon mount frame");
                for(unsigned i=0;i<3;++i) v[i]/=n;
            };
            normalize(x);const float dot=x[0]*z[0]+x[1]*z[1]+x[2]*z[2];
            for(unsigned i=0;i<3;++i) z[i]-=dot*x[i];normalize(z);
            const float y[]={z[1]*x[2]-z[2]*x[1],z[2]*x[0]-z[0]*x[2],z[0]*x[1]-z[1]*x[0]};
            for(unsigned i=0;i<3;++i) {c.mountFrame[i]=x[i];c.mountFrame[4+i]=y[i];c.mountFrame[8+i]=z[i];}
        }
        layer->components.push_back(std::move(c));
        message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
extern "C" void hwRmSubsystemLayerFree(HwRmSubsystemLayer *layer) {
    // Instances borrow the definition. Never destroy it from under an active
    // ship; the caller must release instances before releasing static data.
    if(layer) {layer->releaseRequested=true;if(!layer->instances) delete layer;}
}
extern "C" int hwRmSubsystemLayerMount(HwRmSubsystemLayer *layer,const char *slot,const float *matrix) {
    if(!layer||layer->frozen||!slot||!matrix)return 0;
    try {Matrix m;std::copy(matrix,matrix+16,m.begin());validate(m);
        for(auto& component:layer->components)if(component.slot==slot){component.authoredMount=m;return 1;}
    }catch(...){}return 0;
}
extern "C" unsigned hwRmSubsystemLayerCount(const HwRmSubsystemLayer *layer) {return layer?unsigned(layer->components.size()):0;}
extern "C" int hwRmSubsystemLayerFindComponent(const HwRmSubsystemLayer *layer,const char *slot) {
    if(layer&&slot)for(unsigned i=0;i<layer->components.size();++i)if(layer->components[i].slot==slot)return (int)i;
    return -1;
}
extern "C" const HwRmHodScene* hwRmSubsystemLayerScene(const HwRmSubsystemLayer* layer,unsigned component){
    return layer&&component<layer->components.size()?layer->components[component].scene:nullptr;
}
extern "C" int hwRmSubsystemLayerAuthorJoint(HwRmSubsystemLayer *layer,const char *slot,const char *name,const char *parent,const float *matrix) {
    if(!layer||layer->frozen||!slot||!name||!parent||!matrix)return 0;
    try {Matrix m;std::copy(matrix,matrix+16,m.begin());validate(m);
        for(auto& c:layer->components)if(c.slot==slot){
            auto found=c.joints.find(name);if(found==c.joints.end()||c.multiplicity.at(name)!=1)return 0;
            int p=-1;if(std::strcmp(parent,"-")){auto target=c.joints.find(parent);if(target==c.joints.end()||c.multiplicity.at(parent)!=1)return 0;p=int(target->second);}
            unsigned steps=0;for(int ancestor=p;ancestor>=0;ancestor=c.parents[ancestor])if(unsigned(ancestor)==found->second||++steps>c.parents.size())return 0;
            c.parents[found->second]=p;c.authoredJoints[found->second]=m;return 1;
        }
    }catch(...){}return 0;
}
extern "C" int hwRmSubsystemLayerFindJoint(const HwRmSubsystemLayer *layer,const char *slot,
    const char *name,unsigned *componentIndex,unsigned *jointIndex) {
    if(!layer || !slot || !name || !componentIndex || !jointIndex) return 0;
    for(unsigned ci=0;ci<layer->components.size();++ci) {
        const auto& c=layer->components[ci];if(c.slot!=slot) continue;
        const auto found=c.multiplicity.find(name);
        if(found==c.multiplicity.end() || found->second!=1) return 0;
        *componentIndex=ci;*jointIndex=c.joints.at(name);return 1;
    }
    return 0;
}
extern "C" HwRmSubsystemInstance *hwRmSubsystemInstanceCreate(HwRmSubsystemLayer *layer) {
    if(!layer || layer->releaseRequested) return nullptr;
    try {
        auto instance=std::make_unique<HwRmSubsystemInstance>();instance->layer=layer;
        instance->visible.assign(layer->components.size(),1);
        instance->animation.resize(layer->components.size());
        instance->controls.resize(layer->components.size());
        layer->frozen=true;++layer->instances;
        return instance.release();
    } catch(const std::exception&) {return nullptr;}
}
extern "C" void hwRmSubsystemInstanceFree(HwRmSubsystemInstance *instance) {
    if(instance) {
        auto *layer=instance->layer;--layer->instances;delete instance;
        if(layer->releaseRequested && !layer->instances) delete layer;
    }
}
extern "C" HwRmSubsystemInstance *hwRmSubsystemInstanceSnapshot(const HwRmSubsystemInstance *instance){
    if(!instance)return nullptr;
    try {auto copy=std::make_unique<HwRmSubsystemInstance>(*instance);++copy->layer->instances;return copy.release();}
    catch(const std::exception&){return nullptr;}
}
extern "C" int hwRmSubsystemSetVisible(HwRmSubsystemInstance *instance,unsigned index,int visible) {
    if(!instance || index>=instance->visible.size()) return 0;instance->visible[index]=visible?1:0;return 1;
}
extern "C" int hwRmSubsystemSetJointControls(HwRmSubsystemInstance *instance,
    const HwRmJointControlUpdate *updates,unsigned count,char *error,size_t size) {
    try {
        if(!instance || !updates || !count || count>192) throw std::runtime_error("invalid native joint control batch");
        HwRmSubsystemInstance staged;staged.layer=instance->layer;staged.controls=instance->controls;
        for(unsigned i=0;i<count;++i) {
            const auto& update=updates[i];
            if(!hwRmSubsystemSetJointControl(&staged,update.component,update.jointName,
                update.azimuth,update.declination,update.recoil,error,size)) return 0;
        }
        instance->controls.swap(staged.controls);message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
extern "C" int hwRmSubsystemClearAnimation(HwRmSubsystemInstance *instance,unsigned componentIndex) {
    if(!instance || componentIndex>=instance->animation.size()) return 0;
    instance->animation[componentIndex].clear();return 1;
}
extern "C" int hwRmSubsystemSetJointControl(HwRmSubsystemInstance *instance,unsigned ci,
    const char *name,float azimuth,float declination,float recoil,char *error,size_t size) {
    try {
        if(!instance || ci>=instance->controls.size() || !name ||
            !std::isfinite(azimuth) || !std::isfinite(declination) || !std::isfinite(recoil) ||
            std::abs(azimuth)>64 || std::abs(declination)>64 || recoil<0 || recoil>100000)
            throw std::runtime_error("invalid native joint control");
        const auto& c=instance->layer->components[ci];
        const auto found=c.multiplicity.find(name);
        if(found==c.multiplicity.end() || found->second!=1)
            throw std::runtime_error(std::string("missing/ambiguous control joint: ")+name);
        instance->controls[ci][c.joints.at(name)]={azimuth,declination,recoil};
        message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
extern "C" int hwRmSubsystemClearJointControls(HwRmSubsystemInstance *instance,unsigned ci) {
    if(!instance || ci>=instance->controls.size()) return 0;
    instance->controls[ci].clear();return 1;
}
extern "C" int hwRmSubsystemSampleAnimation(HwRmSubsystemInstance *instance,unsigned ci,
    const HwRmMad *mad,unsigned clipIndex,double phase,char *error,size_t size) {
    return hwRmSubsystemSampleAnimations(instance,ci,mad,&clipIndex,&phase,1,error,size);
}
extern "C" int hwRmSubsystemSampleAnimations(HwRmSubsystemInstance *instance,unsigned ci,
    const HwRmMad *mad,const unsigned *clipIndices,const double *phases,unsigned count,char *error,size_t size) {
    return hwRmSubsystemSampleMappedAnimations(instance,ci,mad,clipIndices,phases,count,nullptr,0,error,size);
}
extern "C" int hwRmSubsystemSampleMappedAnimations(HwRmSubsystemInstance *instance,unsigned ci,
    const HwRmMad *mad,const unsigned *clipIndices,const double *phases,unsigned count,
    const HwRmAnimationTarget *targets,unsigned targetCount,char *error,size_t size) {
    try {
        if(!instance || ci>=instance->animation.size() || !clipIndices || !phases || !count || count>32 ||
            targetCount>1024 || (targetCount && !targets))
            throw std::runtime_error("invalid subsystem animation request");
        std::unordered_map<std::string,const HwRmAnimationTarget*> routing;
        for(unsigned i=0;i<targetCount;++i) {
            const auto& target=targets[i];
            if(!target.sourceJoint || !*target.sourceJoint || std::strlen(target.sourceJoint)>4096 ||
                !target.targetJoint || !*target.targetJoint || std::strlen(target.targetJoint)>4096 ||
                target.component>=instance->animation.size() ||
                !routing.emplace(target.sourceJoint,&target).second)
                throw std::runtime_error("invalid/duplicate animation target");
            const auto& component=instance->layer->components[target.component];
            const auto found=component.multiplicity.find(target.targetJoint);
            if(found==component.multiplicity.end() || found->second!=1)
                throw std::runtime_error("missing/ambiguous animation target");
        }
        // Stage fresh maps rather than copying every existing ship/component
        // pose on each animation tick. Commit only touched maps after all
        // validation succeeds; map swaps cannot allocate or partially fail.
        std::vector<std::unordered_map<unsigned,AnimatedJoint>> next(instance->animation.size());
        std::vector<unsigned char> touched(next.size(),0);
        // Maintain the classic sampler's replacement semantics even for an
        // empty selected clip: only explicitly affected components are reset.
        if(!targetCount) {next[ci].clear();touched[ci]=1;}
        for(unsigned selected=0;selected<count;++selected) {
        const unsigned clipIndex=clipIndices[selected];const double phase=phases[selected];
        if(!std::isfinite(phase) || phase<0 || phase>1)
            throw std::runtime_error("invalid subsystem animation phase");
        const auto *clip=hwRmMadClip(mad,clipIndex);
        if(!clip || !clip->playable) throw std::runtime_error("unplayable subsystem animation");
        const double time=clip->start+phase*(clip->end-clip->start);
        for(unsigned ti=0;ti<clip->trackCount;++ti) {
            const char *sourceName=clip->tracks[ti].joint;
            const auto target=routing.find(sourceName);
            const unsigned componentIndex=target==routing.end()?ci:target->second->component;
            const char *name=target==routing.end()?sourceName:target->second->targetJoint;
            const auto& c=instance->layer->components[componentIndex];
            if(!touched[componentIndex]) {next[componentIndex].clear();touched[componentIndex]=1;}
            auto& sampled=next[componentIndex];
            const auto found=c.multiplicity.find(name);
            if(found==c.multiplicity.end() || found->second!=1)
                throw std::runtime_error(std::string("missing/ambiguous animated joint: ")+name);
            const unsigned ji=c.joints.at(name);
            const auto *rest=hwRmHodSceneJoint(c.scene,ji);
            double defaults[9]={rest->position[0],rest->position[1],rest->position[2],
                rest->rotation[0],rest->rotation[1],rest->rotation[2],1,1,1},values[9];
            char detail[256];
            if(!hwRmMadSampleTrack(mad,clipIndex,ti,time,defaults,values,detail,sizeof(detail)))
                throw std::runtime_error(detail);
            AnimatedJoint pose{};pose.joint=*rest;
            for(unsigned axis=0;axis<3;++axis) {
                pose.joint.position[axis]=float(values[axis]);
                pose.joint.rotation[axis]=float(values[axis+3]);
                pose.scale[axis]=float(values[axis+6]);
                if(!std::isfinite(pose.scale[axis]) || pose.scale[axis]<=0)
                    throw std::runtime_error("unsupported nonpositive animated scale");
            }
            validate(animatedLocal(pose,1));
            const auto previous=sampled.find(ji);
            if(previous!=sampled.end()) {
                for(unsigned axis=0;axis<3;++axis) {
                    if(previous->second.joint.position[axis]!=pose.joint.position[axis] ||
                       previous->second.joint.rotation[axis]!=pose.joint.rotation[axis] ||
                       previous->second.scale[axis]!=pose.scale[axis])
                        throw std::runtime_error(std::string("conflicting animation clips at joint: ")+name);
                }
            } else sampled.emplace(ji,pose);
        }
        }
        for(unsigned componentIndex=0;componentIndex<next.size();++componentIndex)
            if(touched[componentIndex]) instance->animation[componentIndex].swap(next[componentIndex]);
        message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
namespace {
Matrix world(unsigned index,const Component& c,std::vector<Matrix>& matrices,std::vector<unsigned char>& states,
             const Matrix& attachment,float scale,unsigned componentIndex,
             const std::unordered_map<unsigned,AnimatedJoint>& animation,
             const std::unordered_map<unsigned,JointControl>& controls,
             HwRmSubsystemPoseCallback callback,void *user) {
    if(states[index]==2) return matrices[index];
    if(states[index]==1) throw std::runtime_error("cyclic subsystem pose hierarchy");
    states[index]=1;const auto *joint=hwRmHodSceneJoint(c.scene,index);Matrix pose=restLocal(c,index,scale);
    const auto animated=animation.find(index);
    if(animated!=animation.end()) pose=animatedLocal(animated->second,scale);
    const auto authored=c.authoredJoints.find(index);
    if(authored!=c.authoredJoints.end()){pose=authored->second;for(unsigned i=12;i<15;i++)pose[i]*=scale;}
    const auto control=controls.find(index);
    if(control!=controls.end()) pose=controlledLocal(pose,control->second);
    if(c.parents[index]<0) {Matrix mount=c.authoredMount;for(unsigned i=12;i<15;i++)mount[i]*=scale;pose=multiply(mount,authored!=c.authoredJoints.end()?pose:multiply(c.mountFrame,pose));}
    if(callback) {
        Matrix replacement=pose;
        if(callback(user,componentIndex,index,c.slot.c_str(),joint->name,pose.data(),replacement.data())) pose=replacement;
    }
    validate(pose);
    const int parent=c.parents[index];
    const Matrix base=parent<0?attachment:world(unsigned(parent),c,matrices,states,attachment,scale,componentIndex,animation,controls,callback,user);
    matrices[index]=multiply(base,pose);validate(matrices[index]);states[index]=2;return matrices[index];
}
}
extern "C" int hwRmSubsystemVisit(HwRmSubsystemInstance *instance,unsigned lod,float scale,
                                    HwRmSubsystemPoseCallback pose,void *poseUser,
                                    HwRmSubsystemDrawCallback draw,void *drawUser,char *error,size_t size) {
    try {
        if(!instance || !draw || lod>15 || !std::isfinite(scale) || scale<=0 || scale>100000)
            throw std::runtime_error("invalid subsystem render request");
        const auto& components=instance->layer->components;
        std::vector<std::vector<Matrix>> poses(components.size());std::vector<HwRmSubsystemDraw> draws;
        for(unsigned ci=0;ci<components.size();++ci) {
            const auto& c=components[ci];const unsigned count=hwRmHodSceneJointCount(c.scene);
            poses[ci].resize(count);std::vector<unsigned char> states(count,0);
            const Matrix attachment=ci?poses[0].at(c.anchor):identity();
            for(unsigned j=0;j<count;++j) world(j,c,poses[ci],states,attachment,scale,ci,instance->animation[ci],instance->controls[ci],pose,poseUser);
            if(!instance->visible[ci]) continue;
            for(const auto *mesh:c.meshesForLod[lod]) {
                const Matrix matrix=multiply(poses[ci].at(c.joints.at(mesh->parent)),basis(scale));validate(matrix);
                for(unsigned p=0;p<mesh->partCount;++p) {
                    const auto *part=&mesh->parts[p];if(!part->indexCount) continue;
                    const auto *material=hwRmHodSceneMaterial(c.scene,part->material);
                    if(!visibleSurface(material)) continue;
                    HwRmSubsystemDraw record{};record.component=ci;record.role=c.role;record.slot=c.slot.c_str();
                    record.mesh=mesh;record.part=part;record.material=material;
                    std::copy(matrix.begin(),matrix.end(),record.matrix);draws.push_back(record);
                }
            }
        }
        for(const auto& record:draws) draw(drawUser,&record);
        message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
extern "C" int hwRmSubsystemComponentTransform(HwRmSubsystemInstance *instance,unsigned component,
    float scale,float *result,char *error,size_t size) {
    try {
        if(!instance || !result || component>=instance->layer->components.size() ||
            !std::isfinite(scale) || scale<=0 || scale>100000)
            throw std::runtime_error("invalid component frame request");
        const auto& target=instance->layer->components[component];
        Matrix attachment=identity();
        if(component){
            const auto& hull=instance->layer->components[0];
            std::vector<Matrix> matrices(hull.parents.size());
            std::vector<unsigned char> states(hull.parents.size(),0);
            attachment=world(target.anchor,hull,matrices,states,identity(),scale,0,
                instance->animation[0],instance->controls[0],nullptr,nullptr);
        }
        Matrix mount=target.authoredMount;for(unsigned i=12;i<15;++i)mount[i]*=scale;
        const Matrix matrix=multiply(attachment,multiply(mount,target.mountFrame));validate(matrix);
        std::copy(matrix.begin(),matrix.end(),result);message(error,size,"");return 1;
    }catch(const std::exception& e){message(error,size,e.what());return 0;}
}
extern "C" int hwRmSubsystemJointTransform(HwRmSubsystemInstance *instance,unsigned component,
    const char *name,float scale,HwRmSubsystemPoseCallback pose,void *poseUser,
    float *result,char *error,size_t size) {
    try {
        if(!instance || !name || !result || component>=instance->layer->components.size() ||
            !std::isfinite(scale) || scale<=0 || scale>100000)
            throw std::runtime_error("invalid subsystem marker request");
        const auto& target=instance->layer->components[component];
        const auto found=target.multiplicity.find(name);
        if(found==target.multiplicity.end() || found->second!=1)
            throw std::runtime_error(std::string("missing/ambiguous subsystem marker: ")+name);
        std::vector<std::vector<Matrix>> poses(component+1);
        for(unsigned ci=0;ci<=component;++ci) {
            const auto& c=instance->layer->components[ci];
            const unsigned count=hwRmHodSceneJointCount(c.scene);
            poses[ci].resize(count);std::vector<unsigned char> states(count,0);
            const Matrix attachment=ci?poses[0].at(c.anchor):identity();
            for(unsigned j=0;j<count;++j)
                world(j,c,poses[ci],states,attachment,scale,ci,instance->animation[ci],instance->controls[ci],pose,poseUser);
        }
        const auto& matrix=poses[component].at(target.joints.at(name));
        std::copy(matrix.begin(),matrix.end(),result);
        message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
extern "C" int hwRmSubsystemNeutralJointTransform(HwRmSubsystemInstance *instance,unsigned component,
    const char *name,float *result,char *error,size_t size) {
    try {
        if(!instance || !name || !result || component>=instance->layer->components.size())
            throw std::runtime_error("invalid neutral gun frame request");
        const auto& target=instance->layer->components[component];
        const auto found=target.multiplicity.find(name);
        if(found==target.multiplicity.end() || found->second!=1)
            throw std::runtime_error("missing/ambiguous neutral gun frame");
        const std::unordered_map<unsigned,JointControl> noControls;
        Matrix attachment=identity();
        if(component) {
            const auto& hull=instance->layer->components[0];
            std::vector<Matrix> matrices(hull.parents.size());
            std::vector<unsigned char> states(hull.parents.size(),0);
            attachment=world(target.anchor,hull,matrices,states,identity(),1,0,
                instance->animation[0],noControls,nullptr,nullptr);
        }
        std::vector<Matrix> matrices(target.parents.size());
        std::vector<unsigned char> states(target.parents.size(),0);
        const auto matrix=world(target.joints.at(name),target,matrices,states,attachment,1,component,
            instance->animation[component],noControls,nullptr,nullptr);
        std::copy(matrix.begin(),matrix.end(),result);message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
extern "C" int hwRmSubsystemVisitHierarchy(HwRmSubsystemInstance *instance,unsigned lod,
    float scale,unsigned maxNodes,HwRmSubsystemNodeCallback visit,void *user,char *error,size_t size) {
    try {
        if(!instance || !visit || !maxNodes || maxNodes>65536 || lod>15 ||
            !std::isfinite(scale) || scale<=0 || scale>100000)
            throw std::runtime_error("invalid subsystem hierarchy request");
        std::vector<HwRmSubsystemNode> nodes;
        for(unsigned ci=0;ci<instance->layer->components.size();++ci) {
            const auto& c=instance->layer->components[ci];
            const unsigned base=unsigned(nodes.size()),count=hwRmHodSceneJointCount(c.scene);
            for(unsigned ji=0;ji<count;++ji) {
                const auto *joint=hwRmHodSceneJoint(c.scene,ji);
                HwRmSubsystemNode node{};node.index=unsigned(nodes.size());node.component=ci;node.role=c.role;
                node.slot=c.slot.c_str();node.jointName=joint->name;
                node.parent=c.parents[ji]<0?(ci?int(c.anchor):-1):int(base)+c.parents[ji];
                const auto animated=instance->animation[ci].find(ji);
                Matrix matrix=animated==instance->animation[ci].end()?restLocal(c,ji,scale):animatedLocal(animated->second,scale);
                const auto authored=c.authoredJoints.find(ji);
                if(authored!=c.authoredJoints.end()){matrix=authored->second;for(unsigned i=12;i<15;i++)matrix[i]*=scale;}
                const auto control=instance->controls[ci].find(ji);
                if(control!=instance->controls[ci].end()) matrix=controlledLocal(matrix,control->second);
                if(c.parents[ji]<0) {Matrix mount=c.authoredMount;for(unsigned i=12;i<15;i++)mount[i]*=scale;matrix=multiply(mount,authored!=c.authoredJoints.end()?matrix:multiply(c.mountFrame,matrix));}
                validate(matrix);std::copy(matrix.begin(),matrix.end(),node.localMatrix);
                const Matrix vertex=identity();std::copy(vertex.begin(),vertex.end(),node.vertexMatrix);nodes.push_back(node);
            }
            if(!instance->visible[ci]) continue;
            for(const auto *mesh:c.meshesForLod[lod]) {
                for(unsigned pi=0;pi<mesh->partCount;++pi) {
                    const auto *part=&mesh->parts[pi];if(!part->indexCount) continue;
                    const auto *material=hwRmHodSceneMaterial(c.scene,part->material);
                    if(!visibleSurface(material)) continue;
                    HwRmSubsystemNode node{};node.index=unsigned(nodes.size());node.component=ci;node.role=c.role;
                    node.parent=int(base+c.joints.at(mesh->parent));node.slot=c.slot.c_str();node.jointName=mesh->name;
                    node.part=part;node.material=material;
                    const Matrix matrix=identity(),vertex=basis(scale);
                    std::copy(matrix.begin(),matrix.end(),node.localMatrix);std::copy(vertex.begin(),vertex.end(),node.vertexMatrix);
                    nodes.push_back(node);
                }
            }
            if(nodes.size()>maxNodes) throw std::runtime_error("native hierarchy exceeds renderer object limit");
        }
        for(const auto& node:nodes) visit(user,&node);
        message(error,size,"");return 1;
    } catch(const std::exception& e) {message(error,size,e.what());return 0;}
}
