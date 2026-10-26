#include "RemasteredAudioDecode.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include "Types.h"
#include "fqcodec.h"
#include "fquant.h"
#undef min
#undef max
#undef swap
#define CHECK(x) do {if(!(x)){std::printf("failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void be32(std::vector<unsigned char>& b,unsigned v)
{for(int i=3;i>=0;--i)b.push_back((unsigned char)(v>>(i*8)));}
static void chunk(std::vector<unsigned char>&b,const char *tag,const std::vector<unsigned char>&data)
{b.insert(b.end(),tag,tag+4);be32(b,(unsigned)data.size());b.insert(b.end(),data.begin(),data.end());if(data.size()%2)b.push_back(0);}
static std::vector<unsigned char> fixture()
{
    std::vector<unsigned char> b={'F','O','R','M',0,0,0,0,'A','I','F','C'};
    std::vector<unsigned char> c={0,1,0,0,0,1,0,16,0x40,0x0e,0xac,0x44,0,0,0,0,0,0,'C','O','M','P',16};
    const char text[]="Relic Codec v1.6";c.insert(c.end(),text,text+sizeof(text));
    chunk(b,"COMM",c);
    std::vector<unsigned char> d(266);d[8]=8;chunk(b,"SSND",d);
    unsigned n=(unsigned)b.size()-8;for(int i=0;i<4;++i)b[4+i]=(unsigned char)(n>>(24-i*8));return b;
}
static int compareLegacy(const std::vector<unsigned char>& b,const HwRmAudioInfo& info,const int16_t *pcm)
{
    float frequency[2][256],time[512],previous[512]={0};unsigned char exponent[257]={0};
    unsigned blocks=info.blocks,size=info.bitrate/8;int worst=0;
    fqInitDequant();fqDecBlock(frequency[0],frequency[1],time,previous,FQ_MINIT,FQ_MINIT);
    for(unsigned block=0;block<blocks;++block){
        char padded[260]={0};std::memcpy(padded,b.data()+info.dataOffset+(size_t)block*size*info.channels,size);
        fqDequantBlock(padded,frequency[0],frequency[1],exponent,260,info.bitrate,info.codecRate==22050?128:256);
        fqDecBlock(frequency[0],frequency[1],time,previous,FQ_MDOUBLE,FQ_MDOUBLE);
        for(unsigned i=0;i<512;++i){
            int expected=(int)std::clamp(time[i],-32768.0f,32767.0f);
            worst=std::max(worst,std::abs(expected-pcm[((size_t)block*512+i)*info.channels]));
        }
    }
    return worst;
}
int main(int argc,char **argv)
{
    auto b=fixture();HwRmAudioInfo info;int16_t *pcm=nullptr;
    CHECK(hwRmAudioDecode(b.data(),b.size(),&pcm,&info));
    CHECK(info.channels==1 && info.frames==512 && info.sampleRate==44100);
    CHECK(std::all_of(pcm,pcm+512,[](int16_t s){return s==0;}));free(pcm);
    CHECK(hwRmAudioInspect(b.data(),b.size(),&info));
    {
        auto invalid=b;
        unsigned value=1u|(15u<<7)|(257u<<11); // coefficient index outside spectrum
        for(unsigned i=0;i<4;++i)invalid[info.dataOffset+i]=(unsigned char)(value>>(i*8));
        CHECK(hwRmAudioDecode(invalid.data(),invalid.size(),&pcm,&info));
        CHECK(info.concealedBlocks==1 && info.frames==512);
        CHECK(std::all_of(pcm,pcm+512,[](int16_t s){return s==0;}));free(pcm);
    }
    for(size_t n=0;n<b.size();++n)CHECK(!hwRmAudioInspect(b.data(),n,&info));
    b[12]='X';CHECK(!hwRmAudioInspect(b.data(),b.size(),&info));
    unsigned count=0,failed=0,concealedFiles=0,concealedBlocks=0;int worst=0;
    unsigned long long stereoBytes=0;
    if(argc>1)for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[1])){
        if(entry.path().extension()!=".fda")continue;
        std::ifstream f(entry.path(),std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});
        if(!hwRmAudioDecode(bytes.data(),bytes.size(),&pcm,&info)){
            std::printf("decode failed (%s): %s\n",hwRmAudioInspect(bytes.data(),bytes.size(),&info)?"bitstream":"container",entry.path().string().c_str());++failed;continue;
        }
        // The unbounded legacy decoder is not a valid oracle for corrupt blocks.
        int error=info.concealedBlocks?0:compareLegacy(bytes,info,pcm);worst=std::max(worst,error);
        if(info.concealedBlocks){++concealedFiles;concealedBlocks+=info.concealedBlocks;}
        stereoBytes+=(unsigned long long)info.frames*4;
        if(error>8){std::printf("transform mismatch %d: %s\n",error,entry.path().string().c_str());++failed;}
        free(pcm);++count;
        if(count%500==0){std::printf("decoded %u files\n",count);std::fflush(stdout);}
    }
    std::printf("FDA decoder: %u real files, %u failures, %u files/%u coefficient blocks concealed, max legacy PCM difference %d\n",count,failed,concealedFiles,concealedBlocks,worst);
    std::printf("Decoded stereo cache footprint: %llu bytes\n",stereoBytes);
    return failed?1:0;
}
