#include "RemasteredFxPlayback.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
namespace {
constexpr unsigned capacity=32768,rootCapacity=4096;
constexpr unsigned trailCapacity=1024,trailSamples=512;
using Matrix=std::array<float,16>;using Vec=std::array<float,3>;
float scalar(const HwRmFxNode* node,const char* name,float phase,float fallback){
    float v[4];return hwRmFxGraphSample(hwRmFxGraphProperty(node,name),phase,v)?v[0]:fallback;
}
Vec vector(const HwRmFxNode* node,const char* name,float phase){float v[4]={};hwRmFxGraphSample(hwRmFxGraphProperty(node,name),phase,v);return {v[0],v[1],v[2]};}
const char* text(const HwRmFxNode* node,const char* name){auto* p=hwRmFxGraphProperty(node,name);return p&&p->type==HW_RM_FX_STRING?p->text:"";}
unsigned next(unsigned& value){return value=value*1664525u+1013904223u;}
float random(unsigned& value){return float(double(next(value))/4294967296.0);}
Vec transform(const Matrix& m,const Vec& v){return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2],m[1]*v[0]+m[5]*v[1]+m[9]*v[2],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]};}
float duration(const HwRmFxNode* node){return scalar(node,node->style==HW_RM_FX_SPRAY?"emitter_duration":"duration",0,0);}
bool loop(const HwRmFxNode* node){return scalar(node,node->style==HW_RM_FX_SPRAY?"emitter_loop":node->style==HW_RM_FX_LENSFLARE?"infinite":"loop",0,0)!=0;}
bool comboEdge(const HwRmFxProperty& p){if(p.type!=HW_RM_FX_STRING||p.name[0]!='f'||p.name[1]!='x'||!p.name[2])return false;
    for(const char* n=p.name+2;*n;++n)if(*n<'0'||*n>'9')return false;return *p.text!=0;}
bool finite(const float* value,unsigned count){if(!value)return false;for(unsigned i=0;i<count;++i)if(!std::isfinite(value[i]))return false;return true;}
struct Root {Matrix frame{},previous{};Vec velocity{};uint64_t anchor=0;unsigned references=0;float length=0;bool alive=true;};
struct Instance {
    Matrix frame{};Vec velocity{};unsigned node=0,root=0,random=0,dynamics=0;
    float scale=1,carry=0,drag=1,strength=0;double born=0,last=0;
    bool active=false,attached=true,first=true;
    int trail=-1;
    int parent=-1;uint64_t token=0,parentToken=0;
    float radiusMultiplier=1,rate=1;Vec spin{},spinMultiplier{1,1,1};
};
struct Trail {std::array<HwRmFxTrailPoint,trailSamples> points{};unsigned count=0;bool active=false;};
}
struct HwRmFxPlayback {
    const HwRmFxGraph* graph;HwRmFxPlaybackOps ops;void* user;
    std::vector<Instance> instances;std::vector<unsigned> free;
    std::array<Root,rootCapacity> roots{};unsigned count=0,rejected=0;double clock=0;
    std::vector<Trail> trails;
    std::vector<unsigned> freeRoots;
    uint64_t nextToken=1;
};
namespace {
void release(HwRmFxPlayback& p,unsigned index){auto& item=p.instances[index];auto& root=p.roots[item.root];
    if(item.trail>=0)p.trails[item.trail]=Trail{};
    if(!--root.references){if(root.anchor)p.ops.releaseAnchor(p.user,root.anchor);root=Root{};p.freeRoots.push_back(item.root);}
    item=Instance{};p.free.push_back(index);--p.count;
}
void sampleTrail(HwRmFxPlayback& p,Instance& item,float age);
unsigned required(const HwRmFxPlayback& p,unsigned node,unsigned depth=0){
    const auto* fx=hwRmFxGraphAt(p.graph,node);if(!fx||depth>64)return capacity+1;
    if(fx->style!=HW_RM_FX_COMBO){unsigned count=1;
        if(fx->style==HW_RM_FX_RING&&*text(fx,"birthspawn_fx"))count+=required(p,unsigned(hwRmFxGraphFind(p.graph,text(fx,"birthspawn_fx"))),depth+1);
        return count;}unsigned count=0;
    for(unsigned i=0;i<fx->propertyCount;++i)if(comboEdge(fx->properties[i])){
        int child=hwRmFxGraphFind(p.graph,fx->properties[i].text);if(child<0)return capacity+1;
        count+=required(p,unsigned(child),depth+1);if(count>capacity)return count;}
    return count;
}
unsigned requiredTrails(const HwRmFxPlayback& p,unsigned node){
    const auto* fx=hwRmFxGraphAt(p.graph,node);if(fx->style==HW_RM_FX_TRAIL)return 1;
    if(fx->style!=HW_RM_FX_COMBO){return fx->style==HW_RM_FX_RING&&*text(fx,"birthspawn_fx")?requiredTrails(p,unsigned(hwRmFxGraphFind(p.graph,text(fx,"birthspawn_fx")))):0;}unsigned count=0;
    for(unsigned i=0;i<fx->propertyCount;++i)if(comboEdge(fx->properties[i])){
        count+=requiredTrails(p,unsigned(hwRmFxGraphFind(p.graph,fx->properties[i].text)));
        if(count>trailCapacity)return trailCapacity+1;}
    return count;
}
bool trailBudget(const HwRmFxPlayback& p,unsigned node,unsigned count){
    unsigned need=requiredTrails(p,node);if(!need)return true;unsigned available=0;
    for(const auto& trail:p.trails)if(!trail.active)++available;
    return count<=available/need;
}
void instantiate(HwRmFxPlayback& p,unsigned node,const Instance& source){
    const auto* fx=hwRmFxGraphAt(p.graph,node);
    if(fx->style==HW_RM_FX_COMBO){unsigned seed=source.random;
        for(unsigned i=0;i<fx->propertyCount;++i)if(comboEdge(fx->properties[i])){
            Instance child=source;child.random=next(seed);instantiate(p,unsigned(hwRmFxGraphFind(p.graph,fx->properties[i].text)),child);}return;}
    unsigned index=p.free.back();p.free.pop_back();p.instances[index]=source;
    auto& item=p.instances[index];item.node=node;item.active=true;item.token=p.nextToken++;
    if(fx->style==HW_RM_FX_RING){unsigned seed=item.random;const char* spinNames[]={"spinx","spiny","spinz"};const char* epsilonNames[]={"spinepsilonx","spinepsilony","spinepsilonz"};
        item.radiusMultiplier=1+random(seed)*scalar(fx,"radiusepsilon",0,0);
        item.rate=std::max(.000001f,1+random(seed)*scalar(fx,"rateepsilon",0,0));
        for(unsigned axis=0;axis<3;++axis)item.spinMultiplier[axis]=1+random(seed)*scalar(fx,epsilonNames[axis],0,0);
        if(scalar(fx,"spinrandom",0,0)!=0)for(unsigned axis=0;axis<3;++axis)if(random(seed)>.5f)item.spinMultiplier[axis]=-item.spinMultiplier[axis];
        for(unsigned axis=0;axis<3;++axis)if(scalar(fx,spinNames[axis],0,0)!=0)item.spin[axis]=random(seed)*6.283185307179586f;
    }
    if(fx->style==HW_RM_FX_TRAIL){unsigned t=0;while(t<trailCapacity&&p.trails[t].active)++t;
        if(t==trailCapacity){item=Instance{};p.free.push_back(index);++p.rejected;return;}
        item.trail=int(t);p.trails[t].active=true;}
    ++p.roots[item.root].references;++p.count;
    if(item.trail>=0)sampleTrail(p,item,0);
    if(fx->style==HW_RM_FX_RING&&*text(fx,"birthspawn_fx")){
        Instance birth;birth.root=item.root;birth.parent=int(index);birth.parentToken=item.token;birth.scale=item.scale;
        birth.random=next(item.random);birth.born=item.born;birth.last=item.last;birth.attached=true;
        birth.frame={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        instantiate(p,unsigned(hwRmFxGraphFind(p.graph,text(fx,"birthspawn_fx"))),birth);
    }
}
Matrix frame(const HwRmFxPlayback& p,const Instance& item){
    Matrix result=item.frame;if(item.parent>=0&&p.instances[item.parent].active&&p.instances[item.parent].token==item.parentToken){
        result=frame(p,p.instances[item.parent]);const Vec offset=transform(result,{item.frame[12],item.frame[13],item.frame[14]});
        for(unsigned axis=0;axis<3;++axis)result[12+axis]+=offset[axis];
    }else if(item.attached){const auto& root=p.roots[item.root];result=root.frame;
        const Vec local={item.frame[12],item.frame[13],item.frame[14]};const Vec offset=transform(result,local);
        for(unsigned axis=0;axis<3;++axis)result[12+axis]+=offset[axis];}return result;
}
void sampleTrail(HwRmFxPlayback& p,Instance& item,float age){
    auto& trail=p.trails[item.trail];const auto* node=hwRmFxGraphAt(p.graph,item.node);
    float length=std::max(.0001f,scalar(node,"length",0,.1f));
    unsigned remove=0;while(remove+1<trail.count&&age-trail.points[remove+1].age>length)++remove;
    if(remove){std::move(trail.points.begin()+remove,trail.points.begin()+trail.count,trail.points.begin());trail.count-=remove;}
    const auto m=frame(p,item);HwRmFxTrailPoint head{{m[12],m[13],m[14]},age};
    if(!trail.count){trail.points[0]=head;trail.count=1;return;}
    /* 007f9bac updates the current endpoint each tick and commits a new
       history sample after 0.1s. Camera redraws never append points. */
    if(trail.count==1)trail.points[trail.count++]=head;
    trail.points[trail.count-1]=head;
    if(age-trail.points[trail.count-2].age>.1f){
        if(trail.count==trailSamples){++p.rejected;return;}
        trail.points[trail.count++]=head;
    }
}
void move(HwRmFxPlayback& p,Instance& item,float dt){
    if(dt<=0)return;auto& root=p.roots[item.root];
    if(item.dynamics==10&&root.alive){Vec delta;float norm=0;
        for(unsigned axis=0;axis<3;++axis){delta[axis]=root.frame[12+axis]-item.frame[12+axis];norm+=delta[axis]*delta[axis];}
        norm=std::sqrt(norm);if(norm>.00001f)for(unsigned axis=0;axis<3;++axis)item.velocity[axis]+=delta[axis]/norm*item.strength*dt;
    }
    // RM 007ee269: linear drag over a clamped 0.1-second interval, not
    // pow(drag,dt) and not a particle-size multiplier.
    float damping=std::max(0.f,1-(1-item.drag)*std::clamp(dt/.1f,0.f,1.f));
    for(unsigned axis=0;axis<3;++axis){item.velocity[axis]*=damping;item.frame[12+axis]+=item.velocity[axis]*dt;}
}
void emit(HwRmFxPlayback& p,Instance& item,const HwRmFxNode* node,float phase,float dt,double clock){
    const int child=hwRmFxGraphFind(p.graph,text(node,"particle_fx"));if(child<0){++p.rejected;return;}
    float rate=std::max(0.f,scalar(node,"emitter_rate",phase,0));
    double amount=double(rate)*dt+item.carry;unsigned count=amount>capacity?capacity+1:unsigned(amount);
    item.carry=float(amount-std::floor(amount));
    if(item.first&&scalar(node,"force_first",phase,0)!=0){count=1;item.carry=0;}
    unsigned need=required(p,unsigned(child));
    if(count&&(!need||count>p.free.size()/need||!trailBudget(p,unsigned(child),count))){++p.rejected;return;}
    Matrix emitter=frame(p,item);Vec offset=vector(node,"emitter_offset",phase),volume=vector(node,"emitter_volume",phase);
    offset=transform(emitter,offset);for(unsigned axis=0;axis<3;++axis)emitter[12+axis]+=offset[axis]*item.scale;
    float speed=scalar(node,"particle_speed",phase,0),deviation=scalar(node,"emitter_deviation",phase,0)*.01745329238474369f;
    if(scalar(node,"emitter_scalespeed",phase,0)!=0)speed*=item.scale;
    const float particleScale=scalar(node,"particle_scale",phase,1)*item.scale;
    const unsigned dynamics=unsigned(std::clamp(scalar(node,"particle_dynamics",phase,0),0.f,10.f));
    for(unsigned i=0;i<count;++i){
        Instance particle;particle.root=item.root;particle.random=next(item.random);particle.scale=particleScale;
        particle.attached=dynamics==6;particle.dynamics=dynamics;particle.drag=scalar(node,"emitter_drag",phase,1);
        particle.strength=scalar(node,"gravwell_strength",phase,9.81f);particle.frame=emitter;
        const Vec point={(random(item.random)-.5f)*volume[0]*item.scale,(random(item.random)-.5f)*volume[1]*item.scale,(random(item.random)-.5f)*volume[2]*item.scale};
        const Vec world=transform(emitter,point);for(unsigned axis=0;axis<3;++axis)particle.frame[12+axis]+=world[axis];
        if(item.attached&&count){const auto& owner=p.roots[item.root];float blend=float(i)/float(count);
            for(unsigned axis=0;axis<3;++axis)particle.frame[12+axis]+=(owner.previous[12+axis]-owner.frame[12+axis])*(1-blend);}
        // RM 007f51ea samples half the authored cone angle and full azimuth.
        float angle=random(item.random)*deviation*.5f,azimuth=random(item.random)*6.283185307179586f;
        Vec velocity={std::sin(angle)*std::cos(azimuth)*speed,std::sin(angle)*std::sin(azimuth)*speed,std::cos(angle)*speed};
        particle.velocity=transform(emitter,velocity);
        float inherit=scalar(node,"emitter_inheritvelocity",phase,0);
        for(unsigned axis=0;axis<3;++axis)particle.velocity[axis]+=p.roots[item.root].velocity[axis]*inherit;
        if(particle.attached){
            particle.frame={1,0,0,0,0,1,0,0,0,0,1,0,point[0],point[1],point[2],1};particle.velocity=velocity;
            for(unsigned axis=0;axis<3;++axis)particle.frame[12+axis]+=vector(node,"emitter_offset",phase)[axis]*item.scale;
        }
        double lead=rate>0?std::max(0.,double(dt)-double(i)/rate):0;
        particle.born=clock-lead;particle.last=particle.born;
        move(p,particle,float(lead));particle.last=clock;
        instantiate(p,unsigned(child),particle);
    }
}
}
extern "C" HwRmFxPlayback* hwRmFxPlaybackCreate(const HwRmFxGraph* graph,const HwRmFxPlaybackOps* ops,void* user){
    if(!graph||!ops||!ops->anchor||!ops->releaseAnchor||!ops->draw)return nullptr;
    try {auto out=std::make_unique<HwRmFxPlayback>();out->graph=graph;out->ops=*ops;out->user=user;
        out->instances.resize(capacity);out->free.reserve(capacity);for(unsigned i=capacity;i--;)out->free.push_back(i);
        out->trails.resize(trailCapacity);
        out->freeRoots.reserve(rootCapacity);for(unsigned i=rootCapacity;i--;)out->freeRoots.push_back(i);
        return out.release();}catch(...){return nullptr;}
}
extern "C" void hwRmFxPlaybackReset(HwRmFxPlayback* p){if(!p)return;
    for(auto& root:p->roots){if(root.anchor)p->ops.releaseAnchor(p->user,root.anchor);root=Root{};}
    p->free.clear();for(unsigned i=capacity;i--;){p->instances[i]=Instance{};p->free.push_back(i);}p->count=0;p->clock=0;
    for(auto& trail:p->trails)trail=Trail{};
    p->freeRoots.clear();for(unsigned i=rootCapacity;i--;)p->freeRoots.push_back(i);
}
extern "C" void hwRmFxPlaybackFree(HwRmFxPlayback* p){hwRmFxPlaybackReset(p);delete p;}
extern "C" int hwRmFxPlaybackSpawn(HwRmFxPlayback* p,const char* choices,unsigned seed,double clock,const float* matrix,const float* velocity,float scale,float length,uint64_t anchor){
    if(!p||!std::isfinite(clock)||clock<p->clock||!finite(matrix,16)||!finite(velocity,3)||!std::isfinite(scale)||scale<=0||!std::isfinite(length)||length<0)return 0;
    int index=hwRmFxGraphSelect(p->graph,choices,seed);if(index<0)return 0;
    unsigned count=required(*p,unsigned(index));if(!count||count>p->free.size()||!trailBudget(*p,unsigned(index),1)){++p->rejected;return 0;}
    if(p->freeRoots.empty()){++p->rejected;return 0;}unsigned root=p->freeRoots.back();p->freeRoots.pop_back();
    auto& owner=p->roots[root];std::copy(matrix,matrix+16,owner.frame.begin());owner.previous=owner.frame;
    std::copy(velocity,velocity+3,owner.velocity.begin());owner.anchor=anchor;owner.length=length;owner.alive=true;
    Instance first;first.root=root;first.random=seed;first.scale=scale;first.born=first.last=clock;
    first.frame={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    instantiate(*p,unsigned(index),first);
    if(!owner.references){owner=Root{};p->freeRoots.push_back(root);return 0;}
    p->clock=clock;return 1;
}
extern "C" int hwRmFxPlaybackUpdate(HwRmFxPlayback* p,double clock){
    if(!p||!std::isfinite(clock)||clock<p->clock)return 0;
    for(auto& root:p->roots)if(root.references){root.previous=root.frame;
        if(root.anchor&&!p->ops.anchor(p->user,root.anchor,root.frame.data(),root.velocity.data(),&root.length)){
            p->ops.releaseAnchor(p->user,root.anchor);root.anchor=0;root.alive=false;}}
    for(unsigned i=0;i<capacity;++i){auto& item=p->instances[i];if(!item.active)continue;
        if(item.parent>=0&&(!p->instances[item.parent].active||p->instances[item.parent].token!=item.parentToken)){release(*p,i);continue;}
        if(item.attached&&!p->roots[item.root].alive){release(*p,i);continue;}
        const auto* node=hwRmFxGraphAt(p->graph,item.node);const float life=duration(node);double age=(clock-item.born)*item.rate;
        if(age<0||life<=0){release(*p,i);continue;}
        bool expired=!loop(node)&&age>=life;
        double end=expired?item.born+life/item.rate:clock;
        float phase=float(loop(node)?std::fmod(age,life)/life:std::min(age/double(life),1.)),dt=float(end-item.last);
        if(node->style==HW_RM_FX_SPRAY&&dt>0)emit(*p,item,node,phase,dt,end);
        if(expired){
            if(node->style==HW_RM_FX_RING&&*text(node,"spawn_fx")){
                int child=hwRmFxGraphFind(p->graph,text(node,"spawn_fx"));
                if(child>=0&&required(*p,unsigned(child))<=p->free.size()&&trailBudget(*p,unsigned(child),1)){
                    Instance death;death.root=item.root;death.random=next(item.random);death.scale=item.scale;
                    death.frame=frame(*p,item);death.velocity=item.velocity;death.attached=false;death.born=death.last=end;
                    instantiate(*p,unsigned(child),death);
                }else ++p->rejected;
            }
            release(*p,i);continue;
        }
        if(node->style==HW_RM_FX_RING){const char* names[]={"spinx","spiny","spinz"};
            // 007f42e3: spin values * 10 * degrees-to-radians * effective dt.
            for(unsigned axis=0;axis<3;++axis)item.spin[axis]+=scalar(node,names[axis],phase,0)*10*.01745329238474369f*dt*item.rate*item.spinMultiplier[axis];}
        move(*p,item,dt);if(item.trail>=0)sampleTrail(*p,item,float(age));
        if(dt>0)item.first=false;item.last=clock;
    }
    p->clock=clock;return 1;
}
extern "C" void hwRmFxPlaybackDraw(HwRmFxPlayback* p){if(!p)return;
    for(const auto& item:p->instances)if(item.active){const auto* node=hwRmFxGraphAt(p->graph,item.node);if(node->style==HW_RM_FX_SPRAY)continue;
        const float life=duration(node);if(life<=0)continue;HwRmFxDraw draw{};draw.node=node;draw.scale=item.scale;draw.random=item.random;
        draw.age=float((p->clock-item.born)*item.rate);draw.phase=loop(node)?std::fmod(draw.age,life)/life:std::clamp(draw.age/life,0.f,1.f);
        std::copy(item.spin.begin(),item.spin.end(),draw.spin);draw.radiusMultiplier=item.radiusMultiplier;
        const Matrix m=frame(*p,item);std::copy(m.begin(),m.end(),draw.matrix);draw.length=p->roots[item.root].length;
        if(item.trail>=0){draw.trail=p->trails[item.trail].points.data();draw.trailCount=p->trails[item.trail].count;}
        p->ops.draw(p->user,&draw);}
}
extern "C" unsigned hwRmFxPlaybackCount(const HwRmFxPlayback* p){return p?p->count:0;}
extern "C" unsigned hwRmFxPlaybackRejected(const HwRmFxPlayback* p){return p?p->rejected:0;}
