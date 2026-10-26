// Validate the actual production background PSOs on the GPU, without launching
// or changing the game. Inputs are generated local cache scene files.
#include "../src/Modern/LegacyD3D12GL.cpp"
#include <d3d12sdklayers.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char *message) { if(!ok)throw std::runtime_error(message); }
struct Reader {
    std::ifstream stream;
    explicit Reader(const char *path):stream(path,std::ios::binary){
        require((bool)stream,"scene open");
        stream.seekg(0,std::ios::end);auto n=stream.tellg();
        require(n>=20&&n<=HW_RM_SCENE_MAX_BYTES,"scene file extent");
        stream.seekg(0);
    }
    template<class T> T value(){T v{};stream.read((char*)&v,sizeof(v));require((bool)stream,"truncated scene");return v;}
    std::vector<char> bytes(size_t n){require(n<=HW_RM_SCENE_MAX_BYTES,"oversized scene");std::vector<char> v(n);stream.read(v.data(),n);require((bool)stream,"truncated data");return v;}
};
int main(int argc,char **argv) try {
    require(argc>1,"pass one or more generated scene files");
    ComPtr<ID3D12Debug> debug;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    require(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))),"device");
    ComPtr<ID3D12InfoQueue> messages;device.As(&messages);
    require(hwlegacyd3d12::initialize(device.Get(),64,64),"raster initialize");
    unsigned total=0;
    for(int file=1;file<argc;++file) {
        Reader input(argv[file]);auto magic=input.bytes(8);
        require(!std::memcmp(magic.data(),"RMSC3\0\0\0",8),"scene magic");
        auto textures=input.value<unsigned>(),programs=input.value<unsigned>(),draws=input.value<unsigned>();
        require(textures<=128&&programs<=32&&draws<=256,"counts");
        for(unsigned i=0;i<textures;++i) {
            auto w=input.value<unsigned>(),h=input.value<unsigned>(),n=input.value<unsigned>();
            require((size_t)w*h*4==n,"texture extent");input.bytes(n);
        }
        std::vector<unsigned> ids;
        for(unsigned i=0;i<programs;++i) {
            auto code=input.bytes(input.value<unsigned>());
            auto id=hwglCreateBackgroundProgram(code.data(),code.size());require(id!=0,"program creation");ids.push_back(id);
        }
        for(unsigned i=0;i<draws;++i) {
            auto program=input.value<unsigned>(),flags=input.value<unsigned>(),count=input.value<unsigned>(),frames=input.value<unsigned>();
            require(program<ids.size()&&count%3==0&&count<=1000000&&frames<=1024,"draw extent");
            input.bytes(2*4+3*4+10*4+512*4+(size_t)frames*16*4+(size_t)count*80);
            require(backgroundPipelineFor(device.Get(),ids[program],flags)!=nullptr,"production material PSO");++total;
        }
        require(input.stream.peek()==std::char_traits<char>::eof(),"trailing scene data");
        std::cout<<argv[file]<<": "<<draws<<" production material PSOs validated\n";
    }
    if(messages)for(UINT64 i=0;i<messages->GetNumStoredMessages();++i) {
        SIZE_T length=0;messages->GetMessage(i,nullptr,&length);std::vector<char> storage(length);
        auto message=(D3D12_MESSAGE*)storage.data();messages->GetMessage(i,message,&length);
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    hwlegacyd3d12::shutdown();std::cout<<"PASS: "<<total<<" source material draws; no D3D12 validation errors\n";
    return 0;
} catch(const std::exception &error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
