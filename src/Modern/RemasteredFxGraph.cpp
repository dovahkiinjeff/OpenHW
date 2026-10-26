#include "RemasteredFxGraph.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
namespace {
void message(char* p,size_t n,const char* text){if(p&&n)std::snprintf(p,n,"%s",text);}
struct Reader {
    const unsigned char* data;size_t size,position=0;
    unsigned integer(){if(position>size||size-position<4)throw std::runtime_error("truncated native FX graph");
        unsigned v=0;for(unsigned i=0;i<4;++i)v|=unsigned(data[position++])<<(8*i);return v;}
    float number(){unsigned v=integer();float n;std::memcpy(&n,&v,4);
        if(!std::isfinite(n)||std::abs(n)>1e30f)throw std::runtime_error("invalid FX graph number");return n;}
};
unsigned char fold(unsigned char c){return c>='A'&&c<='Z'?c+32:c;}
bool same(const char* a,const char* b){if(!a||!b)return false;for(;;++a,++b){if(fold(*a)!=fold(*b))return false;if(!*a)return true;}}
bool identity(const char* name){if(!name||!*name||std::strlen(name)>255||*name==' '||name[std::strlen(name)-1]==' ')return false;
    for(const unsigned char* p=(const unsigned char*)name;*p;++p)
        if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p==' '))return false;return true;}
unsigned dimensions(unsigned type){return type==HW_RM_FX_CURVE_FLOAT?1:type==HW_RM_FX_CURVE_VECTOR?3:type==HW_RM_FX_CURVE_COLOUR?4:0;}
bool edge(const HwRmFxNode& node,const HwRmFxProperty& prop){
    if(node.style==HW_RM_FX_RING&&(!std::strcmp(prop.name,"birthspawn_fx")||!std::strcmp(prop.name,"spawn_fx")))return true;
    if(node.style==HW_RM_FX_SPRAY)return !std::strcmp(prop.name,"particle_fx");
    if(node.style!=HW_RM_FX_COMBO||prop.name[0]!='f'||prop.name[1]!='x'||!prop.name[2])return false;
    for(const char* p=prop.name+2;*p;++p)if(*p<'0'||*p>'9')return false;return true;
}
}
struct HwRmFxGraph {
    std::vector<char> strings;
    std::vector<float> numbers;
    std::vector<HwRmFxProperty> properties;
    std::vector<HwRmFxNode> nodes;
};
extern "C" HwRmFxGraph* hwRmFxGraphRead(const void* data,size_t size,char* error,size_t capacity){
    try {
        if(!data||size<24||size>64*1024*1024||std::memcmp(data,"OWFX0001",8))throw std::runtime_error("invalid native FX graph version");
        Reader r{(const unsigned char*)data,size,8};unsigned nodes=r.integer(),props=r.integer(),values=r.integer(),strings=r.integer();
        if(!nodes||nodes>8192||props>1000000||values>8000000||!strings||strings>4*1024*1024||
            size_t(24)+size_t(nodes)*16+size_t(props)*20+size_t(values)*4+strings!=size)
            throw std::runtime_error("invalid native FX graph table bounds");
        struct Node {unsigned name,style,first,count;};struct Prop {unsigned name,type,text,first,count;};
        std::vector<Node> ns(nodes);std::vector<Prop> ps(props);
        for(auto& n:ns)n={r.integer(),r.integer(),r.integer(),r.integer()};
        for(auto& p:ps)p={r.integer(),r.integer(),r.integer(),r.integer(),r.integer()};
        auto out=std::make_unique<HwRmFxGraph>();out->numbers.reserve(values);
        for(unsigned i=0;i<values;++i)out->numbers.push_back(r.number());
        out->strings.assign((const char*)data+r.position,(const char*)data+size);
        if(out->strings.front()||out->strings.back())throw std::runtime_error("invalid native FX graph string pool");
        auto text=[&](unsigned offset)->const char*{
            if(offset>=strings||(offset&&out->strings[offset-1])||!std::memchr(out->strings.data()+offset,0,strings-offset))
                throw std::runtime_error("invalid native FX graph string offset");
            const char* value=out->strings.data()+offset;
            if(std::strlen(value)>4096)throw std::runtime_error("oversized FX graph string");
            for(const unsigned char* p=(const unsigned char*)value;*p;++p)if(*p>127)throw std::runtime_error("non-ASCII FX graph string");return value;
        };
        out->properties.resize(props);unsigned usedNumbers=0;
        for(unsigned i=0;i<props;++i){const auto& p=ps[i];
            if(p.type>8||p.first!=usedNumbers||p.first>values||p.count>values-p.first)throw std::runtime_error("invalid FX property values");
            usedNumbers+=p.count;auto& view=out->properties[i];view={text(p.name),text(p.text),p.type,p.count,p.count?out->numbers.data()+p.first:nullptr};
            if(!*view.name||std::strlen(view.name)>127)throw std::runtime_error("invalid FX property name");
            for(const unsigned char* ch=(const unsigned char*)view.name;*ch;++ch)
                if(!((*ch>='a'&&*ch<='z')||(*ch>='0'&&*ch<='9')||*ch=='_'||*ch=='%'||*ch==' '||*ch=='-'||*ch=='@'||*ch=='.'))
                    throw std::runtime_error("invalid FX property spelling");
            const unsigned dim=dimensions(p.type);
            if(p.type==HW_RM_FX_STRING){if(p.count)throw std::runtime_error("string FX property contains numbers");}
            else {
                if(*view.text||!p.count)throw std::runtime_error("numeric FX property contains string/empty values");
                if(dim){if(p.count%(dim+1)||p.count/(dim+1)>4096)throw std::runtime_error("invalid FX curve dimensions");
                    float previous=-1;for(unsigned k=0;k<p.count;k+=dim+1){float time=view.values[k];
                        if(time<0||time>1||time<previous)throw std::runtime_error("invalid FX curve order");previous=time;}}
                else if(p.type<=HW_RM_FX_INT){if(p.count!=1||(p.type!=HW_RM_FX_FLOAT&&view.values[0]!=std::trunc(view.values[0])))throw std::runtime_error("invalid FX scalar");}
                else if(p.type==HW_RM_FX_COLOUR&&p.count!=4)throw std::runtime_error("invalid FX colour");
                else if(p.type==HW_RM_FX_FRAME_TIMES&&(std::strcmp(view.name,"@frames.timestart")||p.count>4096))throw std::runtime_error("invalid FX frame timing");
            }
        }
        if(usedNumbers!=values)throw std::runtime_error("unowned FX graph numbers");
        out->nodes.resize(nodes);unsigned usedProps=0;
        for(unsigned i=0;i<nodes;++i){const auto& n=ns[i];
            if(n.style>7||n.first!=usedProps||n.count>513||n.first>props||n.count>props-n.first)throw std::runtime_error("invalid FX graph node");
            usedProps+=n.count;auto& view=out->nodes[i];view={text(n.name),n.style,n.count,n.count?out->properties.data()+n.first:nullptr};
            if(!identity(view.name)||(i&&std::strcmp(out->nodes[i-1].name,view.name)>=0))throw std::runtime_error("invalid/duplicate FX identity");
            std::unordered_set<std::string> names;for(unsigned p=0;p<n.count;++p)
                if(!names.insert(view.properties[p].name).second)throw std::runtime_error("duplicate FX property");
        }
        if(usedProps!=props)throw std::runtime_error("unowned FX graph properties");
        std::vector<unsigned char> states(nodes);std::vector<unsigned> heights(nodes);
        std::function<void(unsigned,unsigned)> verify=[&](unsigned index,unsigned depth){
            if(states[index]==1||depth>64)throw std::runtime_error("cyclic/deep FX graph");if(states[index]==2)return;
            states[index]=1;const auto& node=out->nodes[index];
            for(unsigned p=0;p<node.propertyCount;++p){const auto& prop=node.properties[p];if(!edge(node,prop))continue;
                if(prop.type!=HW_RM_FX_STRING)throw std::runtime_error("invalid FX graph edge type");if(!*prop.text)continue;
                int child=hwRmFxGraphFind(out.get(),prop.text);if(child<0)throw std::runtime_error("unresolved FX graph edge");verify(unsigned(child),depth+1);
                heights[index]=std::max(heights[index],1+heights[unsigned(child)]);}
            if(heights[index]>64)throw std::runtime_error("deep FX graph");
            states[index]=2;
        };
        for(unsigned i=0;i<nodes;++i)verify(i,0);
        message(error,capacity,"");return out.release();
    }catch(const std::exception& e){message(error,capacity,e.what());return nullptr;}
}
extern "C" void hwRmFxGraphFree(HwRmFxGraph* graph){delete graph;}
extern "C" unsigned hwRmFxGraphCount(const HwRmFxGraph* graph){return graph?unsigned(graph->nodes.size()):0;}
extern "C" const HwRmFxNode* hwRmFxGraphAt(const HwRmFxGraph* graph,unsigned index){return graph&&index<graph->nodes.size()?&graph->nodes[index]:nullptr;}
extern "C" int hwRmFxGraphFind(const HwRmFxGraph* graph,const char* name){
    if(!graph||!name||std::strlen(name)>255)return -1;
    char key[256];unsigned n=0;for(;name[n];++n)key[n]=char(fold((unsigned char)name[n]));key[n]=0;
    auto found=std::lower_bound(graph->nodes.begin(),graph->nodes.end(),key,[](const auto& node,const char* value){return std::strcmp(node.name,value)<0;});
    return found!=graph->nodes.end()&&!std::strcmp(found->name,key)?int(found-graph->nodes.begin()):-1;
}
extern "C" int hwRmFxGraphSelect(const HwRmFxGraph* graph,const char* choices,unsigned random){
    if(!graph||!choices||std::strlen(choices)>1023)return -1;
    const char* starts[32];unsigned lengths[32],count=0;const char* p=choices;
    while(*p&&count<32){while(*p==',')++p;if(!*p)break;starts[count]=p;
        while(*p&&*p!=',')++p;lengths[count]=unsigned(p-starts[count]);++count;}
    if(!count)return -1;unsigned index=random%count;if(lengths[index]>255)return -1;
    char name[256];std::memcpy(name,starts[index],lengths[index]);name[lengths[index]]=0;
    return hwRmFxGraphFind(graph,name);
}
extern "C" const HwRmFxProperty* hwRmFxGraphProperty(const HwRmFxNode* node,const char* name){
    if(node&&name)for(unsigned i=0;i<node->propertyCount;++i)if(same(node->properties[i].name,name))return node->properties+i;return nullptr;
}
extern "C" int hwRmFxGraphSample(const HwRmFxProperty* prop,float phase,float* output){
    if(!prop||!output||!std::isfinite(phase)||!prop->values||!prop->count)return 0;
    float value[4]={};unsigned dim=dimensions(prop->type);
    if(dim){const unsigned width=dim+1;if(prop->count%width)return 0;unsigned a=0,b=0;
        for(unsigned i=width;i<prop->count;i+=width){b=i;if(phase<prop->values[b])break;a=b;}
        const float dt=prop->values[b]-prop->values[a];float t=dt>0?std::clamp((phase-prop->values[a])/dt,0.f,1.f):0;
        for(unsigned i=0;i<dim;++i)value[i]=prop->values[a+i+1]+(prop->values[b+i+1]-prop->values[a+i+1])*t;
    }else if(prop->type<=HW_RM_FX_INT&&prop->count==1)value[0]=prop->values[0];
    else if(prop->type==HW_RM_FX_COLOUR&&prop->count==4)std::copy(prop->values,prop->values+4,value);
    else return 0;
    std::copy(value,value+4,output);return 1;
}
