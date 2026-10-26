#include "RemasteredSoundBank.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void message(char* error,size_t capacity,const char* text){if(error&&capacity)std::snprintf(error,capacity,"%s",text);}
struct Reader {
    const unsigned char *p;size_t n,at=0;
    unsigned integer(){if(at>n||n-at<4)throw std::runtime_error("truncated sound bank");unsigned v=0;for(unsigned i=0;i<4;++i)v|=unsigned(p[at++])<<(i*8);return v;}
    float number(){unsigned v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f)||std::abs(f)>1e9f)throw std::runtime_error("invalid sound bank number");return f;}
};
bool identity(const std::string& name,bool sample=false){
    if(name.size()>511||name.find("..")!=name.npos||name.find("//")!=name.npos||(!name.empty()&&(name.front()=='/'||name.back()=='/')))return false;
    for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'||(sample&&c=='.')))return false;
    return true;
}
void apply(HwRmSoundProfile& target,const HwRmSoundProfile* source){
    if(!source)return;
    for(unsigned i=0;i<30;++i)if(source->mask&(1u<<i))target.values[i]=source->values[i];
    if(source->mask&(1u<<19)){target.envelope=source->envelope;target.envelopeCount=source->envelopeCount;}
    if(source->mask&(1u<<20)){target.effectMask=source->effectMask;std::copy(source->effects,source->effects+28,target.effects);}
    if(source->randomSamples>=0)target.randomSamples=source->randomSamples;
    target.mask|=source->mask;
}
}
struct HwRmSoundBank {
    std::vector<char> strings;
    std::vector<HwRmSoundNode> nodes;
    std::vector<unsigned> children;
    std::vector<HwRmSoundProfile> profiles;
    std::vector<HwRmSoundEnvelope> envelopes;
};
extern "C" HwRmSoundBank* hwRmSoundBankRead(const void* data,size_t size,char* error,size_t capacity){
    try{
        if(!data||size<28||size>16*1024*1024||std::memcmp(data,"OWSFX001",8))throw std::runtime_error("invalid sound bank storage/version");
        Reader r{static_cast<const unsigned char*>(data),size,8};
        unsigned nodes=r.integer(),edges=r.integer(),profiles=r.integer(),envelopes=r.integer(),strings=r.integer();
        if(!nodes||nodes>4096||edges>4096||profiles>1024||envelopes>5120||!strings||strings>1024*1024||
            size_t(28)+size_t(nodes)*28+size_t(edges)*4+size_t(profiles)*252+size_t(envelopes)*48+strings!=size)
            throw std::runtime_error("invalid sound bank table bounds");
        auto out=std::make_unique<HwRmSoundBank>();out->nodes.resize(nodes);out->children.resize(edges);out->profiles.resize(profiles);out->envelopes.resize(envelopes);
        struct Record{unsigned name,sample,parent,profile,first,count,kind;};std::vector<Record> records;
        for(unsigned i=0;i<nodes;++i)records.push_back({r.integer(),r.integer(),r.integer(),r.integer(),r.integer(),r.integer(),r.integer()});
        for(auto& edge:out->children){edge=r.integer();if(edge>=nodes)throw std::runtime_error("invalid sound child index");}
        struct Range{unsigned first,count;};std::vector<Range> ranges;
        for(auto& p:out->profiles){
            p.mask=r.integer();p.randomSamples=static_cast<int>(r.integer());unsigned first=r.integer(),count=r.integer();p.effectMask=r.integer();
            if((p.mask&0xc0000000u)||p.randomSamples< -1||p.randomSamples>1||(p.effectMask&0xffe00000u)||count>5||first>envelopes||count>envelopes-first||bool(count)!=bool(p.mask&(1u<<19)))
                throw std::runtime_error("invalid sound profile flags/range");
            for(float& v:p.values)v=r.number();for(float& v:p.effects)v=r.number();p.envelopeCount=count;ranges.push_back({first,count});
            if((p.mask&(1u<<2))&&p.values[2]<0)throw std::runtime_error("negative sound gain");
            if((p.mask&(1u<<9))&&(p.values[9]<1||p.values[9]>1024||p.values[9]!=std::floor(p.values[9])))throw std::runtime_error("invalid sound polyphony");
        }
        for(auto& point:out->envelopes){point.distance=r.number();point.volume=r.number();point.reverb=r.number();point.duration=r.number();for(float& v:point.equalizer)v=r.number();if(point.distance<0||point.volume<0)throw std::runtime_error("invalid sound envelope");}
        out->strings.assign(static_cast<const char*>(data)+r.at,static_cast<const char*>(data)+size);
        if(out->strings.front()!=0||out->strings.back()!=0)throw std::runtime_error("unterminated sound strings");
        auto text=[&](unsigned offset)->const char*{if(offset>=strings||(offset&&out->strings[offset-1])||!std::memchr(out->strings.data()+offset,0,strings-offset))throw std::runtime_error("invalid sound string offset");return out->strings.data()+offset;};
        for(unsigned i=0;i<profiles;++i){auto& p=out->profiles[i];auto range=ranges[i];p.envelope=range.count?out->envelopes.data()+range.first:nullptr;
            for(unsigned k=1;k<range.count;++k)if(p.envelope[k].distance<p.envelope[k-1].distance)throw std::runtime_error("unordered sound envelope");}
        unsigned roots=0;std::vector<unsigned> references(nodes,0);
        for(unsigned i=0;i<nodes;++i){const auto& record=records[i];auto& node=out->nodes[i];node.name=text(record.name);node.sample=text(record.sample);node.parent=static_cast<int>(record.parent);
            if(!identity(node.name)||record.kind>1||bool(*node.sample)!=bool(record.kind)||
               (record.kind&&(!identity(node.sample,true)||std::string(node.sample)!="remasteredaudio/sfx/etg/"+std::string(node.name)+".fda"))||
               (record.parent!=0xffffffffu&&(record.parent>=nodes||record.parent==i))||
               (record.profile!=0xffffffffu&&record.profile>=profiles)||record.first>edges||record.count>edges-record.first||
               (record.kind&&record.count)||(i&&std::strcmp(out->nodes[i-1].name,node.name)>=0))throw std::runtime_error("invalid sound node identity/hierarchy");
            node.profile=record.profile==0xffffffffu?nullptr:&out->profiles[record.profile];node.firstChild=record.first;node.childCount=record.count;
            if(node.parent<0){++roots;if(*node.name||record.kind)throw std::runtime_error("invalid sound root");}
            for(unsigned k=0;k<record.count;++k){unsigned child=out->children[record.first+k];if(records[child].parent!=i||++references[child]!=1)throw std::runtime_error("ambiguous sound child ownership");}
        }
        if(roots!=1||references[0])throw std::runtime_error("invalid sound root count");
        for(unsigned i=1;i<nodes;++i){if(references[i]!=1)throw std::runtime_error("orphan sound node");unsigned steps=0;int parent=int(i);while(parent>=0){if(++steps>64)throw std::runtime_error("cyclic/deep sound hierarchy");parent=out->nodes[parent].parent;}}
        message(error,capacity,"");return out.release();
    }catch(const std::exception& e){message(error,capacity,e.what());return nullptr;}
}
extern "C" void hwRmSoundBankFree(HwRmSoundBank* bank){delete bank;}
extern "C" unsigned hwRmSoundBankCount(const HwRmSoundBank* bank){return bank?unsigned(bank->nodes.size()):0;}
extern "C" const HwRmSoundNode* hwRmSoundBankAt(const HwRmSoundBank* bank,unsigned node){return bank&&node<bank->nodes.size()?&bank->nodes[node]:nullptr;}
extern "C" int hwRmSoundBankFind(const HwRmSoundBank* bank,const char* name){
    try{
    if(!bank||!name)return -1;std::string key;for(const unsigned char* p=reinterpret_cast<const unsigned char*>(name);*p;++p){if(key.size()>=511)return -1;key+=(*p>='A'&&*p<='Z')?char(*p+32):char(*p);}
    if(!identity(key))return -1;auto found=std::lower_bound(bank->nodes.begin(),bank->nodes.end(),key,[](const HwRmSoundNode& node,const std::string& name){return node.name<name;});
    return found!=bank->nodes.end()&&found->name==key?int(found-bank->nodes.begin()):-1;
    }catch(const std::exception&){return -1;}
}
extern "C" int hwRmSoundBankSelect(const HwRmSoundBank* bank,const char* name,unsigned random,HwRmSoundSelection* output,char* error,size_t capacity){
    return hwRmSoundBankSelectStep(bank,name,random,0,output,error,capacity);
}
extern "C" int hwRmSoundBankSelectStep(const HwRmSoundBank* bank,const char* name,unsigned random,unsigned step,HwRmSoundSelection* output,char* error,size_t capacity){
    if(!output){message(error,capacity,"missing sound selection output");return 0;}int index=hwRmSoundBankFind(bank,name);if(index<0){message(error,capacity,"unknown native sound cue");return 0;}
    HwRmSoundSelection selection{};selection.playlistNode=-1;selection.profile.randomSamples=-1;selection.profile.values[2]=1;selection.profile.values[9]=1;
    unsigned ancestors[64],count=0;for(int n=index;n>=0;n=bank->nodes[n].parent)ancestors[count++]=unsigned(n);
    while(count)apply(selection.profile,bank->nodes[ancestors[--count]].profile);
    for(unsigned depth=0;depth<64;++depth){const auto& node=bank->nodes[index];if(*node.sample){selection.node=unsigned(index);*output=selection;message(error,capacity,"");return 1;}
        if(!node.childCount){message(error,capacity,"empty native sound container");return 0;}
        if(selection.playlistNode<0 && (selection.profile.values[25]!=0||selection.profile.values[27]!=0)){
            selection.playlistNode=index;selection.playlistCount=node.childCount;
            const unsigned entry=selection.profile.values[26]!=0?random%node.childCount:step%node.childCount;
            index=int(bank->children[node.firstChild+entry]);random=random*1664525u+1013904223u;
            apply(selection.profile,bank->nodes[index].profile);continue;
        }
        if(node.childCount>1&&selection.profile.values[26]==0&&selection.profile.randomSamples!=1){message(error,capacity,"ambiguous native sound container selection");return 0;}
        index=int(bank->children[node.firstChild+random%node.childCount]);random=random*1664525u+1013904223u;apply(selection.profile,bank->nodes[index].profile);
    }
    message(error,capacity,"sound container depth exceeded");return 0;
}
extern "C" int hwRmSoundBankEnvelope(const HwRmSoundSelection* selection,float distance,HwRmSoundEnvelope* output){
    if(!selection||!output||!std::isfinite(distance)||distance<0)return 0;
    HwRmSoundEnvelope value{};value.distance=distance;value.volume=1;for(float& eq:value.equalizer)eq=1;
    const auto& p=selection->profile;if(p.envelopeCount){const auto* a=p.envelope;const auto* b=a;
        if(!a||p.envelopeCount>5)return 0;
        for(unsigned i=1;i<p.envelopeCount;++i){b=p.envelope+i;if(distance<=b->distance)break;a=b;}
        float t=b->distance>a->distance?std::clamp((distance-a->distance)/(b->distance-a->distance),0.f,1.f):0;
        value.volume=a->volume+(b->volume-a->volume)*t;value.reverb=a->reverb+(b->reverb-a->reverb)*t;value.duration=a->duration+(b->duration-a->duration)*t;
        for(unsigned i=0;i<8;++i)value.equalizer[i]=a->equalizer[i]+(b->equalizer[i]-a->equalizer[i])*t;
    }
    value.volume*=p.values[2];*output=value;return 1;
}
