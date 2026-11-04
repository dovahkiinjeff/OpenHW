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
#include "../src/Modern/GoodGodRaysShader.h"
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr,const char *message){if(FAILED(hr))throw std::runtime_error(message);}
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(int argc,char **argv)try{
    require(argc>=2,"production presenter source required");
    std::ifstream file(argv[1]);std::stringstream text;text<<file.rdbuf();auto source=text.str();
    // The raw base is retained separately when the runtime appends the port.
    const char *markers[]={"const char presenterBaseShader[] = R\"(","const char presenterShader[] = R\"("};
    size_t begin=std::string::npos;for(const char *marker:markers){begin=source.find(marker);if(begin!=std::string::npos){begin+=std::strlen(marker);break;}}
    require(begin!=std::string::npos,"presenter base missing");auto end=source.find(")\";",begin);require(end!=std::string::npos,"presenter terminator missing");source=source.substr(begin,end-begin);
    source+=hwmodern::goodGodRaysShader;
    auto compile=[&](const char *entry,const char *target,const std::string &shaderSource){
        ComPtr<ID3DBlob> code,errors;auto hr=D3DCompile(shaderSource.data(),shaderSource.size(),"ProductionGoodGodRays",nullptr,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
        if(FAILED(hr)&&errors)std::cerr<<static_cast<const char*>(errors->GetBufferPointer());check(hr,"compile production good-godrays");return code;};
    // Compile the actual pixel entry as well as executing its shared body.
    auto pixelCode=compile("GoodGodRaysPS","ps_5_0",source);
    source+=R"(
RWTexture2D<float4> TestOutput : register(u0);
[numthreads(8,8,1)] void TestGodRaysCS(uint3 id : SV_DispatchThreadID) {
    if(any(id.xy>=64u))return;
    float2 uv=(float2(id.xy)+0.5)/64.0;
    if(FrameIndex==0u){float2 result=GoodGodRaysEvaluate(uv,float2(id.xy)+.5);
        TestOutput[id.xy]=float4(result,float(GoodGodRaysStepCount()),GoodGodRaysNoise(float2(id.xy)+.5));}
    else if(FrameIndex==4u){float3 ray=DustRayUv00.xyz+uv.x*(DustRayUv10.xyz-DustRayUv00.xyz)+uv.y*(DustRayUv01SunStrength.xyz-DustRayUv00.xyz);
        float depth=DustVisibleSceneDistance(uv,ray);TestOutput[id.xy]=float4(AtmosphericSunScattering(DustCameraPositionMaxDistance.xyz,normalize(ray),depth),depth);}
    else if(FrameIndex==1u){float3 ray=DustRayUv00.xyz+uv.x*(DustRayUv10.xyz-DustRayUv00.xyz)+uv.y*(DustRayUv01SunStrength.xyz-DustRayUv00.xyz);
        float depth=DustVisibleSceneDistance(uv,ray);TestOutput[id.xy]=float4(GoodGodRaysUpsample(uv,depth),depth,0,0);}
    else TestOutput[id.xy]=float4(GoodGodRaysContribution(uv),1);
}
)";
    auto code=compile("TestGodRaysCS","cs_5_0",source);
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"device");
    ComPtr<ID3D11ComputeShader> shader;check(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader),"CS");context->CSSetShader(shader.Get(),nullptr,0);
    std::vector<float> voxels(32*32*32*4);
    auto lightGrid=[&](bool shadowed){
        for(unsigned z=0;z<32;++z)for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){unsigned i=((z*32+y)*32+x)*4;
            float visibility=shadowed&&((x>=15&&x<=17)||(y>=19&&y<=21))&&z<24?0.f:1.f;voxels[i]=visibility;voxels[i+1]=voxels[i+2]=0;voxels[i+3]=visibility;}
        D3D11_TEXTURE3D_DESC desc={};desc.Width=desc.Height=desc.Depth=32;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial={voxels.data(),32*16,32*32*16};ComPtr<ID3D11Texture3D> texture;check(device->CreateTexture3D(&desc,&initial,&texture),"light grid");ComPtr<ID3D11ShaderResourceView> srv;check(device->CreateShaderResourceView(texture.Get(),nullptr,&srv),"grid view");return srv;};
    auto lit=lightGrid(false),shadowed=lightGrid(true);
    auto texture=[&](unsigned width,unsigned height,DXGI_FORMAT format,const void *pixels,unsigned stride){
        D3D11_TEXTURE2D_DESC desc={};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial={pixels,width*stride,0};ComPtr<ID3D11Texture2D> result;check(device->CreateTexture2D(&desc,pixels?&initial:nullptr,&result),"texture");return result;};
    auto view=[&](ID3D11Texture2D *texture){ComPtr<ID3D11ShaderResourceView> result;check(device->CreateShaderResourceView(texture,nullptr,&result),"view");return result;};
    std::vector<float> depthValues(64*64,1.f);auto depthTexture=texture(64,64,DXGI_FORMAT_R32_FLOAT,depthValues.data(),4);auto depthView=view(depthTexture.Get());auto *depthPointer=depthView.Get();context->CSSetShaderResources(8,1,&depthPointer);
    std::vector<float> fieldValues(8*8*2);for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){fieldValues[(y*8+x)*2]=x<4?.5f:0.f;fieldValues[(y*8+x)*2+1]=x<4?500000.f:1000.f;}
    auto fieldTexture=texture(8,8,DXGI_FORMAT_R32G32_FLOAT,fieldValues.data(),8);auto fieldView=view(fieldTexture.Get());auto *fieldPointer=fieldView.Get();context->CSSetShaderResources(19,1,&fieldPointer);
    D3D11_TEXTURE2D_DESC outputDesc={};outputDesc.Width=outputDesc.Height=64;outputDesc.MipLevels=outputDesc.ArraySize=1;outputDesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    outputDesc.SampleDesc.Count=1;outputDesc.Usage=D3D11_USAGE_DEFAULT;outputDesc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> output,staging;check(device->CreateTexture2D(&outputDesc,nullptr,&output),"output");outputDesc.Usage=D3D11_USAGE_STAGING;outputDesc.BindFlags=0;outputDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&outputDesc,nullptr,&staging),"staging");
    ComPtr<ID3D11UnorderedAccessView> uav;check(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav),"UAV");auto *uavPointer=uav.Get();context->CSSetUnorderedAccessViews(0,1,&uavPointer,nullptr);
    D3D11_SAMPLER_DESC samplerDesc={};samplerDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;samplerDesc.AddressU=samplerDesc.AddressV=samplerDesc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;samplerDesc.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;check(device->CreateSamplerState(&samplerDesc,&sampler),"sampler");auto *samplerPointer=sampler.Get();context->CSSetSamplers(0,1,&samplerPointer);
    std::array<unsigned,64> values={};auto f=[&](unsigned i,float v){std::memcpy(&values[i],&v,4);};
    f(7,1);f(18,32);f(19,3);f(24,0);f(25,0);f(26,-6000);f(27,500000);f(28,-.3f);f(29,-.3f);f(30,1);f(31,-1.00002f);
    f(32,.3f);f(33,-.3f);f(34,1);f(35,-200);f(36,-.3f);f(37,.3f);f(38,1);f(39,1.2f);f(42,1);f(43,1);
    f(44,1.7f);f(45,2.1f);f(46,1.3f);f(47,1);for(unsigned i=0;i<3;++i){f(52+i,-2000);f(56+i,4000);}f(55,32);f(59,32);
    D3D11_BUFFER_DESC cb={};cb.ByteWidth=256;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ComPtr<ID3D11Buffer> constants;check(device->CreateBuffer(&cb,nullptr,&constants),"constants");auto *constantPointer=constants.Get();context->CSSetConstantBuffers(0,1,&constantPointer);
    auto bindGrid=[&](ID3D11ShaderResourceView *grid){context->CSSetShaderResources(12,1,&grid);context->CSSetShaderResources(14,1,&grid);};bindGrid(shadowed.Get());
    auto dispatch=[&]{context->UpdateSubresource(constants.Get(),0,nullptr,values.data(),0,0);context->Dispatch(8,8,1);};
    auto results=[&]{dispatch();context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped={};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"read map");std::vector<float> result(64*64*4);for(unsigned y=0;y<64;++y)std::memcpy(result.data()+y*64*4,static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,64*16);context->Unmap(staging.Get(),0);return result;};
    unsigned nonzero=0;const unsigned expectedSteps[]={24,40,60,90};
    for(unsigned quality=0;quality<4;++quality){values[16]=32u|(quality<<6u);auto result=results();auto repeated=results();require(result==repeated,"godrays noise must be static across repeated frames");
        for(unsigned i=0;i<result.size();i+=4){require(std::isfinite(result[i])&&std::isfinite(result[i+1]),"nonfinite ray output");require(result[i]>=0&&result[i]<=.98f,"unbounded intensity");require(result[i+2]==float(expectedSteps[quality]),"quality step count");require(result[i+3]>=0&&result[i+3]<1,"noise bound");if(result[i]>0)++nonzero;}}
    require(nonzero>0,"shadow contrast fixture must produce visible shafts");
    bindGrid(lit.Get());values[16]=32u;auto clear=results();for(unsigned i=0;i<clear.size();i+=4)require(clear[i]==0,"unshadowed sky must not wash out");bindGrid(shadowed.Get());
    f(7,0);auto off=results();for(unsigned i=0;i<off.size();i+=4)require(off[i]==0,"disabled godrays output");f(7,1);
    // Foreground clips before the sphere; background still reaches it.
    for(unsigned y=0;y<64;++y)for(unsigned x=32;x<64;++x)depthValues[y*64+x]=.9f;
    context->UpdateSubresource(depthTexture.Get(),0,nullptr,depthValues.data(),64*4,0);auto clipped=results();for(unsigned y=0;y<64;++y)for(unsigned x=32;x<64;++x)require(clipped[(y*64+x)*4]==0,"godrays behind foreground geometry");
    // The coarse JBU field contains light on the background only. All quality
    // modes must keep the foreground dark, including its boundary pixel.
    values[3]=1;for(unsigned q=0;q<4;++q){values[16]=32u|(q<<6u);auto result=results();for(unsigned y=0;y<64;++y)for(unsigned x=32;x<64;++x)require(result[(y*64+x)*4]==0,"JBU illumination crossed a depth edge");require(result[(32*64+10)*4]>.49f,"JBU lost compatible background illumination");}
    // Exact packed tint; brightness and invalid-field switches return zero.
    values[3]=2;values[16]=32u|4u|(255u<<8u);auto red=results();require(red[(32*64+10)*4]>0,"red tint missing");for(unsigned i=0;i<red.size();i+=4)require(red[i+1]==0&&red[i+2]==0,"tint channels leaked");
    values[16]=4u|(255u<<8u);auto invalid=results();for(unsigned i=0;i<invalid.size();i+=4)require(invalid[i]==0&&invalid[i+1]==0&&invalid[i+2]==0,"invalid field contributes light");
    values[16]=32u;f(7,0);auto disabled=results();for(unsigned i=0;i<disabled.size();i+=4)require(disabled[i]==0&&disabled[i+1]==0&&disabled[i+2]==0,"disabled contribution");f(7,1);
    // The engine also uses a very large finite sky-distance sentinel. Keep
    // sky/sky matches and foreground/sky rejections well defined at that scale.
    for(unsigned i=0;i<depthValues.size();++i)depthValues[i]=1.f;
    context->UpdateSubresource(depthTexture.Get(),0,nullptr,depthValues.data(),64*4,0);
    for(unsigned i=0;i<fieldValues.size();i+=2){fieldValues[i]=.5f;fieldValues[i+1]=3.0e30f;}
    context->UpdateSubresource(fieldTexture.Get(),0,nullptr,fieldValues.data(),8*8,0);
    f(27,3.0e30f);values[3]=1;auto hugeSky=results();for(unsigned i=0;i<hugeSky.size();i+=4){require(std::isfinite(hugeSky[i]),"large sky sentinel NaN");require(hugeSky[i]>.49f,"large sky sentinel must retain matching illumination");}
    f(27,500000);
    // Far camera and grazing/away rays remain finite and bounded.
    values[3]=0;for(float distance:{0.f,1000000.f,100000000.f}){f(26,-distance);for(float forward:{-1.f,.00001f,1.f}){f(30,forward);f(34,forward);f(38,forward);auto result=results();for(unsigned i=0;i<result.size();++i)require(std::isfinite(result[i]),"camera edge produced NaN/Inf");}}
    if(argc>2){
        // Clear only the field-valid routing flag: otherwise the retained
        // old fallback intentionally returns zero before doing any work.
        f(26,-6000);f(30,1);f(34,1);f(38,1);values[16]=(1u<<6u);std::vector<double> times[2];
        for(unsigned round=0;round<6;++round)for(unsigned version=0;version<2;++version){values[3]=version?0:4;D3D11_QUERY_DESC desc={};desc.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;ComPtr<ID3D11Query> disjoint,start,finish;check(device->CreateQuery(&desc,&disjoint),"disjoint");desc.Query=D3D11_QUERY_TIMESTAMP;check(device->CreateQuery(&desc,&start),"start");check(device->CreateQuery(&desc,&finish),"finish");context->Begin(disjoint.Get());context->End(start.Get());for(unsigned i=0;i<32;++i)dispatch();context->End(finish.Get());context->End(disjoint.Get());context->Flush();D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d={};while(context->GetData(disjoint.Get(),&d,sizeof(d),0)==S_FALSE)Sleep(1);UINT64 a=0,b=0;check(context->GetData(start.Get(),&a,sizeof(a),0),"timestamp a");check(context->GetData(finish.Get(),&b,sizeof(b),0),"timestamp b");if(!d.Disjoint&&round)times[version].push_back(double(b-a)*1000.0/d.Frequency/32.0);}
        for(unsigned version=0;version<2;++version){std::sort(times[version].begin(),times[version].end());std::cout<<"64x64 "<<(version?"bounded40step":"oldAtmospheric")<<" illumination median GPU ms="<<times[version][times[version].size()/2]<<'\n';}}
    std::cout<<"PASS: production GoodGodRaysPS compiled, bounded4quality marches, static noise, depth-clipped rays,4/16tap JBU without silhouette bleeding, tint/off/invalid flags, finite camera edges; nonzero samples="<<nonzero<<'\n';return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
