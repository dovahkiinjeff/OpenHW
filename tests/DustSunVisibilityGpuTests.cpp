#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "PresenterShaderSource.h"
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr,const char *message){if(FAILED(hr))throw std::runtime_error(message);}
static std::string read(const char *path){std::ifstream f(path);std::stringstream s;s<<f.rdbuf();return s.str();}
int main(int argc,char **argv)try{
    if(argc<3)throw std::runtime_error("production source and reference fixture required");
    auto source=readProductionPresenterShader(argv[1]);size_t begin,end;
    auto baseline=source;begin=baseline.find("float DustProceduralSunVisibility(");end=baseline.find("// Resolve visibility",begin);
    auto fixture=read(argv[2]);if(fixture.empty()||end==std::string::npos)throw std::runtime_error("reference missing");baseline.replace(begin,end-begin,fixture);
    const char wrapper[]=R"(
RWTexture2D<float4> TestOutput : register(u0);
[numthreads(8,8,1)] void TestSunCS(uint3 id : SV_DispatchThreadID) {
    if(any(id.xy>=64u))return;
    float3 u,v;DustSunBasis(u,v);float3 sun=normalize(DustSunDirectionValid.xyz);
    float3 center=DustGridMinWidth.xyz+DustGridSizeHeight.xyz*0.5;
    float3 local=float3((float(id.x)+0.37)/64.0-0.5,
        (float((id.x*7u+id.y*11u)%64u)+0.23)/64.0-0.5,
        (float(id.y)+0.41)/64.0-0.5)*24000.0;
    // Exact and immediately adjacent blend/cascade boundaries.
    if(id.x<12u)local.x=(float(id.x)-5.0)*400.0;
    float3 p=center+u*local.x+v*local.y+sun*local.z;
    float3 origin=center-sun*6000.0+u*((float(id.x)+0.5)/64.0-0.5)*4000.0+
        v*((float(id.y)+0.5)/64.0-0.5)*4000.0;
    float3 ray=normalize(sun+u*(float(id.x%7u)-3.0)*.03);
    float3 scattering=AtmosphericSunScattering(origin,ray,id.y<32u?100000.0:6000.0);
    TestOutput[id.xy]=float4(DustProceduralSunVisibility(p,true),
        DustProceduralSunVisibility(p,false),scattering.x,scattering.y+scattering.z);
}
)";
    auto compile=[&](std::string text){text+=wrapper;ComPtr<ID3DBlob> code,errors;auto hr=D3DCompile(text.data(),text.size(),"ProductionSunVisibility",nullptr,nullptr,"TestSunCS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
        if(FAILED(hr)&&errors)std::cerr<<static_cast<const char*>(errors->GetBufferPointer());check(hr,"compile");return code;};
    auto referenceCode=compile(baseline),candidateCode=compile(source);
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"offscreen device");
    ComPtr<ID3D11ComputeShader> shaders[2];check(device->CreateComputeShader(referenceCode->GetBufferPointer(),referenceCode->GetBufferSize(),nullptr,&shaders[0]),"reference CS");
    check(device->CreateComputeShader(candidateCode->GetBufferPointer(),candidateCode->GetBufferSize(),nullptr,&shaders[1]),"candidate CS");
    constexpr UINT extent=32;
    std::vector<float> pixels(extent*extent*extent*4);
    auto grid=[&](bool isFar){
        for(UINT z=0;z<extent;++z)for(UINT y=0;y<extent;++y)for(UINT x=0;x<extent;++x){UINT at=((z*extent+y)*extent+x)*4;
            pixels[at]=((x*7+y*11)%13<5 && z<11+(x%5))?0.f:1.f;
            pixels[at+1]=.3f;pixels[at+2]=.7f;pixels[at+3]=((x+y*3)%7<2 && z<(isFar?18:25))?.2f:1.f;}
        D3D11_TEXTURE3D_DESC desc={};desc.Width=desc.Height=desc.Depth=extent;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data={pixels.data(),extent*16,extent*extent*16};ComPtr<ID3D11Texture3D> texture;check(device->CreateTexture3D(&desc,&data,&texture),"grid texture");
        ComPtr<ID3D11ShaderResourceView> srv;check(device->CreateShaderResourceView(texture.Get(),nullptr,&srv),"grid SRV");return srv;
    };
    auto nearGrid=grid(false),farGrid=grid(true);auto *nearView=nearGrid.Get(),*farView=farGrid.Get();context->CSSetShaderResources(12,1,&nearView);context->CSSetShaderResources(14,1,&farView);
    D3D11_TEXTURE2D_DESC outputDesc={};outputDesc.Width=outputDesc.Height=64;outputDesc.MipLevels=outputDesc.ArraySize=1;outputDesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    outputDesc.SampleDesc.Count=1;outputDesc.Usage=D3D11_USAGE_DEFAULT;outputDesc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> output,staging;check(device->CreateTexture2D(&outputDesc,nullptr,&output),"output");outputDesc.Usage=D3D11_USAGE_STAGING;outputDesc.BindFlags=0;outputDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(device->CreateTexture2D(&outputDesc,nullptr,&staging),"staging");ComPtr<ID3D11UnorderedAccessView> uav;check(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav),"output UAV");
    auto *uavPointer=uav.Get();context->CSSetUnorderedAccessViews(0,1,&uavPointer,nullptr);
    D3D11_SAMPLER_DESC samplerDesc={};samplerDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;samplerDesc.AddressU=samplerDesc.AddressV=samplerDesc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;samplerDesc.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;check(device->CreateSamplerState(&samplerDesc,&sampler),"sampler");auto *samplerPointer=sampler.Get();context->CSSetSamplers(0,1,&samplerPointer);
    std::array<unsigned,64> values={};auto f=[&](unsigned i,float v){std::memcpy(&values[i],&v,4);};
    f(7,1);f(18,float(extent));f(19,3);f(36+3,1.2f);f(40,.6f);f(41,.3f);f(42,1);f(43,1);
    f(44,1.7f);f(45,2.1f);f(46,1.3f);for(UINT i=0;i<3;++i){f(52+i,-2000);f(56+i,4000);}f(55,float(extent));f(59,float(extent));
    D3D11_BUFFER_DESC cbDesc={};cbDesc.ByteWidth=256;cbDesc.Usage=D3D11_USAGE_DEFAULT;cbDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ComPtr<ID3D11Buffer> constants;
    check(device->CreateBuffer(&cbDesc,nullptr,&constants),"constants");context->UpdateSubresource(constants.Get(),0,nullptr,values.data(),0,0);auto *constantPointer=constants.Get();context->CSSetConstantBuffers(0,1,&constantPointer);
    auto dispatch=[&](unsigned version){context->CSSetShader(shaders[version].Get(),nullptr,0);context->Dispatch(8,8,1);};
    std::vector<float> result[2];
    for(unsigned version=0;version<2;++version){dispatch(version);context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped={};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"read map");result[version].resize(64*64*4);
        for(UINT y=0;y<64;++y)std::memcpy(result[version].data()+y*64*4,static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,64*16);context->Unmap(staging.Get(),0);}
    float maxError=0;unsigned differences=0,contributing=0;
    for(UINT i=0;i<result[0].size();++i){float error=std::fabs(result[0][i]-result[1][i]);maxError=std::max(maxError,error);if(error>0.000001f)++differences;if(i%4>=2&&result[0][i]>0)++contributing;}
    std::cout<<"visibility+atmosphere samples="<<result[0].size()<<" maxError="<<maxError<<" differences="<<differences<<" nonzero scattering="<<contributing<<'\n';
    if(differences||!contributing)throw std::runtime_error("sun visibility output mismatch or trivial fixture");
    if(argc>3){
        std::vector<double> times[2];for(UINT round=0;round<6;++round)for(UINT version=0;version<2;++version){
            D3D11_QUERY_DESC desc={};desc.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;ComPtr<ID3D11Query> disjoint,start,finish;check(device->CreateQuery(&desc,&disjoint),"disjoint");desc.Query=D3D11_QUERY_TIMESTAMP;check(device->CreateQuery(&desc,&start),"timestamp");check(device->CreateQuery(&desc,&finish),"timestamp");
            context->Begin(disjoint.Get());context->End(start.Get());for(UINT j=0;j<32;++j)dispatch(version);context->End(finish.Get());context->End(disjoint.Get());context->Flush();
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data={};while(context->GetData(disjoint.Get(),&data,sizeof(data),0)==S_FALSE)Sleep(1);UINT64 first=0,last=0;check(context->GetData(start.Get(),&first,sizeof(first),0),"start data");check(context->GetData(finish.Get(),&last,sizeof(last),0),"end data");
            if(!data.Disjoint&&round)times[version].push_back(double(last-first)*1000.0/double(data.Frequency)/32.0);}
        for(UINT i=0;i<2;++i){std::sort(times[i].begin(),times[i].end());std::cout<<(i?"candidate":"reference")<<" median GPU ms="<<times[i][times[i].size()/2]<<'\n';}}
    return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
