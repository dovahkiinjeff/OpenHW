#include "RemasteredFxGraph.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
using Bytes=std::vector<unsigned char>;
static void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static void u(Bytes& b,unsigned value){for(unsigned i=0;i<4;++i)b.push_back((value>>(i*8))&255);}
static void f(Bytes& b,float value){unsigned bits;std::memcpy(&bits,&value,4);u(b,bits);}
static Bytes fixture(bool cycle=false){
    Bytes strings{0};auto text=[&](const char* s){unsigned n=unsigned(strings.size());strings.insert(strings.end(),s,s+std::strlen(s)+1);return n;};
    unsigned flash=text("flash"),root=text("root"),radius=text("radius"),duration=text("duration"),fx=text("fx1"),target=text(cycle?"root":"FLASH");
    Bytes b={'O','W','F','X','0','0','0','1'};for(unsigned v:{2u,3u,5u,unsigned(strings.size())})u(b,v);
    for(unsigned v:{flash,2u,0u,2u,root,0u,2u,1u})u(b,v);
    for(unsigned v:{radius,4u,0u,0u,4u,duration,1u,0u,4u,1u,fx,3u,target,5u,0u})u(b,v);
    for(float v:{0.f,2.f,1.f,8.f,5.f})f(b,v);b.insert(b.end(),strings.begin(),strings.end());return b;
}
int main(int argc,char** argv){try{
    auto bytes=fixture();char error[256];auto* graph=hwRmFxGraphRead(bytes.data(),bytes.size(),error,sizeof(error));check(graph!=nullptr,error);
    check(hwRmFxGraphCount(graph)==2&&hwRmFxGraphFind(graph,"FLASH")==0,"native FX identity failed");
    check(hwRmFxGraphSelect(graph,",flash,root,flash,",2)==0,"weighted FX choices were deduplicated");
    check(hwRmFxGraphSelect(graph,"missing,flash",0)==-1,"missing FX choice silently selected a different effect");
    check(hwRmFxGraphSelect(graph,"missing,flash",1)==0,"FX selection did not precede name lookup");
    auto* node=hwRmFxGraphAt(graph,0);float result[4]={};
    check(node&&node->style==HW_RM_FX_RING&&hwRmFxGraphSample(hwRmFxGraphProperty(node,"Radius"),.5f,result),"native radius sampling failed");
    check(result[0]==5,"authored radius was rescaled or sampled incorrectly");
    check(hwRmFxGraphSample(hwRmFxGraphProperty(node,"radius"),2,result)&&result[0]==8,"curve endpoint clamp failed");
    check(hwRmFxGraphSample(hwRmFxGraphProperty(node,"duration"),0,result)&&result[0]==5,"typed scalar failed");
    auto* edge=hwRmFxGraphProperty(hwRmFxGraphAt(graph,1),"Fx1");check(edge&&edge->type==HW_RM_FX_STRING&&!std::strcmp(edge->text,"FLASH"),"combo edge flattened/lost");
    check(!hwRmFxGraphSample(edge,0,result),"FX string sampled as a curve");hwRmFxGraphFree(graph);
    for(size_t length=0;length<bytes.size();++length)check(!hwRmFxGraphRead(bytes.data(),length,error,sizeof(error)),"truncated FX graph accepted");
    auto invalid=fixture(true);check(!hwRmFxGraphRead(invalid.data(),invalid.size(),error,sizeof(error)),"cyclic FX graph accepted");
    invalid=bytes;invalid[7]='0';check(!hwRmFxGraphRead(invalid.data(),invalid.size(),error,sizeof(error)),"unknown FX version accepted");
    invalid=bytes;invalid[24+16]=invalid[24];check(!hwRmFxGraphRead(invalid.data(),invalid.size(),error,sizeof(error)),"duplicate FX graph identity accepted");
    if(argc>1){std::ifstream file(argv[1],std::ios::binary);check(bool(file),"FX graph file missing");
        Bytes data((std::istreambuf_iterator<char>(file)),{});graph=hwRmFxGraphRead(data.data(),data.size(),error,sizeof(error));check(graph!=nullptr,error);
        unsigned properties=0;for(unsigned i=0;i<hwRmFxGraphCount(graph);++i)properties+=hwRmFxGraphAt(graph,i)->propertyCount;
        std::printf("Native FX source graph: %u nodes, %u typed properties retained\n",hwRmFxGraphCount(graph),properties);hwRmFxGraphFree(graph);}
    std::puts("Native FX graph bounds, edges and authored curve sampling passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
