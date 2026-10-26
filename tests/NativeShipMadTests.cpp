#include "RemasteredShipMad.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using Bytes = std::vector<unsigned char>;
static void le(Bytes &b, unsigned v) { for (unsigned i=0;i<4;++i) b.push_back((v>>(8*i))&255); }
static void be(Bytes &b, unsigned v) { for (int i=3;i>=0;--i) b.push_back((v>>(8*i))&255); }
static void f(Bytes &b, float v) { unsigned bits; std::memcpy(&bits,&v,4); le(b,bits); }
static void d(Bytes &b, double v) {
    uint64_t bits; std::memcpy(&bits,&v,8); le(b,unsigned(bits)); le(b,unsigned(bits>>32));
}
static void chunk(Bytes &b, const char *tag, const Bytes &body, bool form=false) {
    const char *outer=form?"FORM":tag; b.insert(b.end(),outer,outer+4); be(b,unsigned(body.size()+(form?4:0)));
    if(form) b.insert(b.end(),tag,tag+4); b.insert(b.end(),body.begin(),body.end());
}
static unsigned str(Bytes &b, const char *value) {
    unsigned offset=unsigned(b.size()); b.insert(b.end(),value,value+std::strlen(value)+1); return offset;
}
enum Fault { None, BadIndex, WrongJoint, NaNKey, UnorderedKey, BadAggregate, DuplicateChannel, BadName, BadRange };
static Bytes sample(Fault fault=None) {
    Bytes body, v; le(v,0x104); chunk(body,"VERS",v);
    Bytes strings; const auto open=str(strings,"Open"), close=str(strings,"Close"), empty=str(strings,"damageclose");
    const auto joint=str(strings,"moshipdoor"), other=str(strings,"OtherJoint");
    const auto curveName=str(strings,"moshipdoor_translateX"); chunk(body,"STRI",strings);
    Bytes info; le(info,30); le(info,3); le(info,2); le(info,2); le(info,fault==BadAggregate?3:2); chunk(body,"INFO",info);
    Bytes curves;
    // Intentionally duplicate channel names: the two clips use different curves.
    for(unsigned i=0;i<2;++i) {
        le(curves,fault==BadName?curveName+1:curveName); le(curves,2);
        d(curves,fault==NaNKey?std::numeric_limits<double>::quiet_NaN():0.0); d(curves,double(i)*10);
        f(curves,1); f(curves,0); f(curves,.8f); f(curves,.6f);
        d(curves,fault==UnorderedKey?-1.0:1.0); d(curves,double(i)*10+1);
        f(curves,.8f); f(curves,.6f); f(curves,1); f(curves,0);
        le(curves,0); le(curves,0);
    }
    chunk(body,"CURV",curves);
    Bytes marks;
    for(unsigned i=0;i<3;++i) {
        le(marks,i==0?open:i==1?close:empty);
        f(marks,i==2?std::numeric_limits<float>::max():0.f);
        f(marks,i==2?-std::numeric_limits<float>::max():fault==BadRange?-1.f:1.f);
        f(marks,0); f(marks,1); le(marks,i==2?0:1);
        if(i==2) continue;
        le(marks,fault==WrongJoint?other:joint); le(marks,fault==DuplicateChannel?2:1);
        le(marks,fault==BadIndex?99:i);
        if(fault==DuplicateChannel) le(marks,i);
    }
    chunk(body,"MARK",marks); Bytes result; chunk(result,"MAD ",body,true); return result;
}
static void require(bool ok, const char *reason) { if(!ok) throw std::runtime_error(reason); }
static Bytes read(const std::filesystem::path &path) {
    std::ifstream file(path,std::ios::binary); require(bool(file),"source MAD missing");
    return Bytes((std::istreambuf_iterator<char>(file)),{});
}
int main(int argc,char **argv) {
    try {
        HwRmMad *mad=nullptr; char error[512]; auto bytes=sample();
        require(hwRmMadRead(bytes.data(),bytes.size(),&mad,error,sizeof(error))!=0,error);
        require(hwRmMadClipCount(mad)==3 && hwRmMadCurveCount(mad)==2 && hwRmMadFrameRate(mad)==30,"header counts");
        const auto *open=hwRmMadFindClip(mad,"Open"), *close=hwRmMadFindClip(mad,"Close");
        require(open && close && open->playable && close->playable,"clip lookup");
        require(open->tracks[0].curveIndices[0]==0 && close->tracks[0].curveIndices[0]==1,"duplicate curve names collapsed");
        require(hwRmMadCurve(mad,1)->keys[0].value==10,"indexed curve data lost");
        require(std::abs(hwRmMadCurve(mad,0)->keys[0].outTangent[1]-.6f)<.00001f,"tangent data lost");
        require(!hwRmMadFindClip(mad,"damageclose")->playable,"empty placeholder made playable");
        require(!hwRmMadFindClip(mad,"open") && !hwRmMadCurve(mad,99) && !hwRmMadClip(mad,99),"unchecked lookup");
        const double defaults[9]={4,5,6,0,0,0,1,1,1}; double values[9];
        require(hwRmMadSampleTrack(mad,0,0,.5,defaults,values,error,sizeof(error))!=0,error);
        require(std::abs(values[0]-.5)<1e-7 && values[1]==5 && values[8]==1,"Hermite/default channel sample");
        require(hwRmMadSampleTrack(mad,0,0,.25,defaults,values,error,sizeof(error))!=0,error);
        require(std::abs(values[0]-.2265625)<1e-7,"authored tangents replaced with linear interpolation");
        require(hwRmMadSampleTrack(mad,1,0,.5,defaults,values,error,sizeof(error))!=0,error);
        require(std::abs(values[0]-10.5)<1e-7,"duplicate curve index sample collapsed");
        require(hwRmMadSampleTrack(mad,0,0,-10,defaults,values,error,sizeof(error)) && values[0]==0,"clip start clamp");
        require(hwRmMadSampleTrack(mad,0,0,10,defaults,values,error,sizeof(error)) && values[0]==1,"clip end clamp");
        values[0]=99;
        require(!hwRmMadSampleTrack(mad,2,0,0,defaults,values,error,sizeof(error)) && values[0]==99,"empty clip mutated output");
        require(!hwRmMadSampleTrack(mad,0,0,std::numeric_limits<double>::quiet_NaN(),defaults,values,error,sizeof(error)),"accepted NaN sample time");
        const char *joints[]={"Root","moshipdoor"};
        require(hwRmMadValidateJoints(mad,joints,2,error,sizeof(error))!=0,error);
        require(!hwRmMadValidateJoints(mad,joints,1,error,sizeof(error)),"accepted missing joint");
        const char *unusedDuplicate[]={"Root","moshipdoor","NavLight","NavLight"};
        require(hwRmMadValidateJoints(mad,unusedDuplicate,4,error,sizeof(error))!=0,error);
        const char *animatedDuplicate[]={"Root","moshipdoor","moshipdoor"};
        require(!hwRmMadValidateJoints(mad,animatedDuplicate,3,error,sizeof(error)),"accepted ambiguous animated joint");
        hwRmMadFree(mad); mad=nullptr; hwRmMadFree(nullptr);
        for(size_t size=0;size<bytes.size();++size) {
            require(!hwRmMadRead(bytes.data(),size,&mad,error,sizeof(error)),"accepted truncated MAD");
            require(!mad,"partial MAD published on failure");
        }
        for(Fault fault:{BadIndex,WrongJoint,NaNKey,UnorderedKey,BadAggregate,DuplicateChannel,BadName,BadRange}) {
            bytes=sample(fault);
            require(!hwRmMadRead(bytes.data(),bytes.size(),&mad,error,sizeof(error)),"accepted malformed MAD");
            require(!mad,"malformed output published");
        }
        require(!hwRmMadRead(nullptr,100,&mad,error,sizeof(error)),"accepted null data");
        if(argc>1) {
            unsigned files=0;
            for(const auto &entry:std::filesystem::recursive_directory_iterator(argv[1])) {
                if(!entry.is_regular_file() || entry.path().extension()!=".mad") continue;
                bytes=read(entry.path());
                require(hwRmMadRead(bytes.data(),bytes.size(),&mad,error,sizeof(error))!=0,error);
                for(unsigned ci=0;ci<hwRmMadClipCount(mad);++ci) {
                    const auto *clip=hwRmMadClip(mad,ci);if(!clip->playable) continue;
                    for(unsigned ti=0;ti<clip->trackCount;++ti) for(unsigned step=0;step<=20;++step) {
                        const double time=clip->start+(clip->end-clip->start)*step/20.;
                        require(hwRmMadSampleTrack(mad,ci,ti,time,defaults,values,error,sizeof(error))!=0,error);
                        for(double value:values) require(std::isfinite(value),"nonfinite real source sample");
                    }
                }
                std::printf("%s: %u clips, %u curves\n",entry.path().filename().string().c_str(),hwRmMadClipCount(mad),hwRmMadCurveCount(mad));
                if(entry.path().stem()=="kus_resourcecontroller") require(hwRmMadCurveCount(mad)==120,"controller curves collapsed");
                if(entry.path().stem()=="kus_supportfrigate") {
                    require(!hwRmMadFindClip(mad,"damageclose")->playable,"support placeholder activated");
                    require(hwRmMadFindClip(mad,"open4")->trackCount==2,"independent docking arm lost");
                }
                hwRmMadFree(mad); mad=nullptr; ++files;
            }
            require(files==13,"expected 13 local Kushan MAD companions");
        }
        std::puts("Native RM MAD tests passed"); return 0;
    } catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
