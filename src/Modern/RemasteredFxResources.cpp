#include "RemasteredFxResources.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void message(char* p,size_t n,const char* s){if(p&&n)std::snprintf(p,n,"%s",s);}
struct Reader {const unsigned char* data;size_t size,position=0;
    unsigned u(){if(position>size||size-position<4)throw std::runtime_error("truncated FX bitmap index");unsigned v=0;for(unsigned i=0;i<4;++i)v|=unsigned(data[position++])<<(i*8);return v;}
    float f(){unsigned bits=u();float v;std::memcpy(&v,&bits,4);return v;}
};
bool path(const char* name){if(!name||std::strlen(name)>511||std::strstr(name,"..")||std::strstr(name,"//"))return false;
    if(std::strncmp(name,"art/fx/",7)&&std::strncmp(name,"effect/lensflare/",17)&&std::strncmp(name,"effect/trails/",14))return false;
    for(const unsigned char* p=(const unsigned char*)name;*p;++p)if(*p<32||*p>=127||*p==':'||*p=='\\'||(*p>='A'&&*p<='Z'))return false;return true;}
bool suffix(const char* name,const char* extension){size_t n=std::strlen(name),e=std::strlen(extension);return n>=e&&!std::strcmp(name+n-e,extension);}
}
struct HwRmFxResources {std::vector<char> strings;std::vector<HwRmFxBitmapFrame> frames;std::vector<HwRmFxBitmap> bitmaps;
    std::vector<HwRmFxMeshes> meshes;std::vector<const char*> choices;
    std::vector<HwRmFxFlare> flares;std::vector<HwRmFxHotspot> hotspots;};
extern "C" HwRmFxResources* hwRmFxResourcesRead(const void* data,size_t size,char* error,size_t capacity){
    try {
        if(!data||size<36||size>64*1024*1024||std::memcmp(data,"OWFXRS01",8))throw std::runtime_error("invalid native FX resource index version");
        Reader r{(const unsigned char*)data,size,8};unsigned count=r.u(),frames=r.u(),meshes=r.u(),choices=r.u(),flares=r.u(),hotspots=r.u(),strings=r.u();
        if(count>8192||frames>1000000||meshes>8192||choices>65536||flares>1024||hotspots>16384||!strings||strings>4*1024*1024||
            size_t(36)+size_t(count)*24+size_t(frames)*20+size_t(meshes)*12+size_t(choices)*4+size_t(flares)*48+size_t(hotspots)*32+strings!=size)
            throw std::runtime_error("invalid FX bitmap tables");
        struct Record {unsigned name,first,count;float fps;unsigned start,loops;};
        struct Frame {unsigned image,row,column,rows,columns;};std::vector<Record> records(count);std::vector<Frame> fs(frames);
        for(auto& b:records)b={r.u(),r.u(),r.u(),r.f(),r.u(),r.u()};
        for(auto& f:fs)f={r.u(),r.u(),r.u(),r.u(),r.u()};
        struct Group {unsigned name,first,count;};struct Flare {Group group;float values[9];};struct Hotspot {unsigned image;float values[7];};
        std::vector<Group> ms(meshes);std::vector<unsigned> cs(choices);std::vector<Flare> fl(flares);std::vector<Hotspot> hs(hotspots);
        for(auto& m:ms)m={r.u(),r.u(),r.u()};for(auto& c:cs)c=r.u();
        for(auto& f:fl){f.group={r.u(),r.u(),r.u()};for(float& v:f.values)v=r.f();}
        for(auto& h:hs){h.image=r.u();for(float& v:h.values)v=r.f();}
        auto out=std::make_unique<HwRmFxResources>();out->strings.assign((const char*)data+r.position,(const char*)data+size);
        if(out->strings.front()||out->strings.back())throw std::runtime_error("invalid FX bitmap strings");
        auto text=[&](unsigned offset){if(offset>=strings||(offset&&out->strings[offset-1])||!std::memchr(out->strings.data()+offset,0,strings-offset))throw std::runtime_error("invalid bitmap string offset");
            const char* value=out->strings.data()+offset;if(!path(value))throw std::runtime_error("unsafe FX bitmap resource");return value;};
        out->frames.resize(frames);out->bitmaps.resize(count);
        for(unsigned i=0;i<frames;++i){const auto& f=fs[i];auto& v=out->frames[i];v={text(f.image),f.row,f.column,f.rows,f.columns};
            if(!f.rows||!f.columns||f.rows>128||f.columns>128||f.row>=f.rows||f.column>=f.columns||(!suffix(v.image,".tga")&&!suffix(v.image,".dds")))throw std::runtime_error("invalid bitmap frame cell");}
        unsigned used=0;
        for(unsigned i=0;i<count;++i){const auto& b=records[i];auto& v=out->bitmaps[i];
            if(b.first!=used||!b.count||b.first>frames||b.count>frames-b.first||!std::isfinite(b.fps)||b.fps<0||b.fps>1000||b.start>=b.count||b.loops>65535)throw std::runtime_error("invalid bitmap animation");
            used+=b.count;v={text(b.name),b.count,b.start,b.loops,b.fps,out->frames.data()+b.first};
            if((!suffix(v.name,".anim")&&!suffix(v.name,".tga")&&!suffix(v.name,".dds"))||(i&&std::strcmp(out->bitmaps[i-1].name,v.name)>=0))throw std::runtime_error("invalid/duplicate bitmap identity");
        }
        if(used!=frames)throw std::runtime_error("unowned bitmap frames");
        out->choices.resize(choices);out->meshes.resize(meshes);out->hotspots.resize(hotspots);out->flares.resize(flares);
        for(unsigned i=0;i<choices;++i){out->choices[i]=text(cs[i]);if(!suffix(out->choices[i],".hod"))throw std::runtime_error("invalid native mesh choice");}
        used=0;for(unsigned i=0;i<meshes;++i){const auto& m=ms[i];auto& v=out->meshes[i];
            if(m.first!=used||!m.count||m.count>1024||m.first>choices||m.count>choices-m.first)throw std::runtime_error("invalid native mesh group");
            used+=m.count;v={text(m.name),m.count,out->choices.data()+m.first};
            if((!suffix(v.name,".hod")&&!suffix(v.name,".ahod"))||(i&&std::strcmp(out->meshes[i-1].name,v.name)>=0))throw std::runtime_error("invalid native mesh group identity");}
        if(used!=choices)throw std::runtime_error("unowned native mesh choices");
        for(unsigned i=0;i<hotspots;++i){auto& v=out->hotspots[i];const auto& h=hs[i];v.image=text(h.image);
            for(float value:h.values)if(!std::isfinite(value))throw std::runtime_error("invalid native flare value");
            v.offset=h.values[0];v.scale=h.values[1];v.rotation=h.values[2];std::copy(h.values+3,h.values+7,v.colour);
            if(v.scale<0||(!suffix(v.image,".tga")&&!suffix(v.image,".dds")))throw std::runtime_error("invalid native flare hotspot");}
        used=0;for(unsigned i=0;i<flares;++i){auto& v=out->flares[i];const auto& f=fl[i];const auto& g=f.group;
            if(g.first!=used||g.first>hotspots||g.count>hotspots-g.first)throw std::runtime_error("invalid flare group");
            used+=g.count;v.name=text(g.name);v.count=g.count;v.hotspots=g.count?out->hotspots.data()+g.first:nullptr;
            for(float value:f.values)if(!std::isfinite(value))throw std::runtime_error("invalid flare parameters");
            std::copy(f.values,f.values+4,v.fade);std::copy(f.values+4,f.values+8,v.washout);v.strength=f.values[8];
            if(!suffix(v.name,".flare")||(i&&std::strcmp(out->flares[i-1].name,v.name)>=0))throw std::runtime_error("invalid flare identity");}
        if(used!=hotspots)throw std::runtime_error("unowned native flare hotspots");
        message(error,capacity,"");return out.release();
    }catch(const std::exception& e){message(error,capacity,e.what());return nullptr;}
}
extern "C" void hwRmFxResourcesFree(HwRmFxResources* bitmaps){delete bitmaps;}
extern "C" unsigned hwRmFxBitmapCount(const HwRmFxResources* bitmaps){return bitmaps?unsigned(bitmaps->bitmaps.size()):0;}
extern "C" const HwRmFxBitmap* hwRmFxBitmapAt(const HwRmFxResources* bitmaps,unsigned index){return bitmaps&&index<bitmaps->bitmaps.size()?&bitmaps->bitmaps[index]:nullptr;}
namespace {
bool canonical(const char* source,char key[512],const char*& name){
    if(!source||std::strlen(source)>511)return false;unsigned i=0;
    for(;source[i];++i){unsigned char c=(unsigned char)source[i];key[i]=c=='\\'?'/':c>='A'&&c<='Z'?char(c+32):char(c);}key[i]=0;
    name=key;if(!std::strncmp(name,"data:",5)){name+=5;while(*name=='/')++name;}
    return path(name);
}
}
extern "C" const HwRmFxBitmap* hwRmFxBitmapFind(const HwRmFxResources* bitmaps,const char* source){
    char key[512];const char* name;if(!bitmaps||!canonical(source,key,name))return nullptr;
    auto found=std::lower_bound(bitmaps->bitmaps.begin(),bitmaps->bitmaps.end(),name,[](const auto& b,const char* value){return std::strcmp(b.name,value)<0;});
    return found!=bitmaps->bitmaps.end()&&!std::strcmp(found->name,name)?&*found:nullptr;
}
extern "C" unsigned hwRmFxMeshCount(const HwRmFxResources* r){return r?unsigned(r->meshes.size()):0;}
extern "C" const HwRmFxMeshes* hwRmFxMeshAt(const HwRmFxResources* r,unsigned i){return r&&i<r->meshes.size()?&r->meshes[i]:nullptr;}
extern "C" const HwRmFxMeshes* hwRmFxMeshFind(const HwRmFxResources* r,const char* source){
    char key[512];const char* name;if(!r||!canonical(source,key,name))return nullptr;
    auto found=std::lower_bound(r->meshes.begin(),r->meshes.end(),name,[](const auto& m,const char* value){return std::strcmp(m.name,value)<0;});
    return found!=r->meshes.end()&&!std::strcmp(found->name,name)?&*found:nullptr;
}
extern "C" const HwRmFxFlare* hwRmFxFlareFind(const HwRmFxResources* r,const char* source){
    char key[512];const char* name;if(!r||!canonical(source,key,name))return nullptr;
    auto found=std::lower_bound(r->flares.begin(),r->flares.end(),name,[](const auto& m,const char* value){return std::strcmp(m.name,value)<0;});
    return found!=r->flares.end()&&!std::strcmp(found->name,name)?&*found:nullptr;
}
extern "C" const HwRmFxBitmapFrame* hwRmFxBitmapSample(const HwRmFxBitmap* b,float age,unsigned random,int randomBitmap,float rateScale){
    if(!b||!b->frames||!b->frameCount||!std::isfinite(age)||age<0||!std::isfinite(rateScale)||rateScale<0)return nullptr;
    if(randomBitmap)return b->frames+(random%b->frameCount);
    double frame=std::floor(double(age)*b->fps*rateScale)+b->start;
    if(!std::isfinite(frame))return nullptr;
    // 007cb180/007cb280/007cb440 compare loopCount*frameCount without a
    // special case for zero. Random-frame atlases commonly author zero;
    // interpreting it as infinite playback animates those atlases wrongly.
    if(frame>=double(b->frameCount)*b->loops)frame=b->frameCount-1;
    else frame=std::fmod(frame,double(b->frameCount));
    return b->frames+unsigned(frame);
}
