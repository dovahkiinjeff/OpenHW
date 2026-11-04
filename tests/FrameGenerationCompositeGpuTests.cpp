// Exercise the real production PresentPS and the FG reuse shader offscreen.
// No game, window, installed asset, settings or capture is involved.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "../src/Modern/FrameGenerationCompositeShader.h"
#include "PresenterShaderSource.h"
using Microsoft::WRL::ComPtr;
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void check(HRESULT result,const char *message){require(SUCCEEDED(result),message);}
static ComPtr<ID3DBlob> compile(const std::string &source,const char *entry,const char *target){
    ComPtr<ID3DBlob> result,errors;
    const auto hr=D3DCompile(source.data(),source.size(),"ProductionPresenter",nullptr,nullptr,entry,target,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&result,&errors);
    if(FAILED(hr)&&errors)std::cerr<<static_cast<const char*>(errors->GetBufferPointer());
    check(hr,"shader compile");return result;
}
int main(int argc,char **argv)try {
    require(argc==2,"production ModernGraphics.cpp path required");
    auto source=readProductionPresenterShader(argv[1]);
    auto vs=compile(source,"PresentVS","vs_5_0"),ps=compile(source,"PresentPS","ps_5_0");
    auto fastPs=compile(hwmodern::frameGenerationCompositeShader,"CompositePS","ps_5_0");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"D3D11 offscreen device");
    ComPtr<ID3D11VertexShader> vertex;ComPtr<ID3D11PixelShader> production,fast;
    check(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex),"vertex");
    check(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&production),"production PS");
    check(device->CreatePixelShader(fastPs->GetBufferPointer(),fastPs->GetBufferSize(),nullptr,&fast),"reuse PS");
    constexpr unsigned width=128,height=96;
    struct Texture {ComPtr<ID3D11Texture2D> resource;ComPtr<ID3D11ShaderResourceView> srv;ComPtr<ID3D11RenderTargetView> rtv;};
    auto texture=[&](const std::vector<float> *pixels=nullptr){
        Texture result;D3D11_TEXTURE2D_DESC desc={};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=pixels?DXGI_FORMAT_R32G32B32A32_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        desc.Usage=pixels?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|(pixels?0:D3D11_BIND_RENDER_TARGET);
        D3D11_SUBRESOURCE_DATA initial={};if(pixels){initial.pSysMem=pixels->data();initial.SysMemPitch=width*4*sizeof(float);}
        check(device->CreateTexture2D(&desc,pixels?&initial:nullptr,&result.resource),"texture");
        check(device->CreateShaderResourceView(result.resource.Get(),nullptr,&result.srv),"texture SRV");
        if(!pixels)check(device->CreateRenderTargetView(result.resource.Get(),nullptr,&result.rtv),"texture RTV");
        return result;
    };
    std::vector<float> world(width*height*4),native=world,background=world,filtered=world;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        const unsigned i=(y*width+x)*4;world[i]=float(x)/width*1.3f;world[i+1]=float(y)/height*.9f;
        world[i+2]=float((x/7+y/5)%3)*.24f;world[i+3]=(x>width/2)?1.f:.25f;
        for(unsigned k=0;k<4;++k)native[i+k]=world[i+k];
        if(x<26&&y<31){native[i]=.95f;native[i+1]=.84f;native[i+2]=.01f;}
        // Alpha-only coverage and sub-threshold RGB differences must retain
        // the composed scene exactly as in PresentPS, never become fake HUD.
        if(x>58&&x<82){native[i+3]=.03f;native[i]+=.003f;}
        for(unsigned k=0;k<3;++k){background[i+k]=.003f*float(k+1)+.04f*float(x%17)/17;filtered[i+k]=.2f+float((x*3+y*7+k*5)%19)/23;}
        background[i+3]=0;filtered[i+3]=1;
    }
    auto sourceFrame=texture(&native),worldFrame=texture(&world),backgroundFrame=texture(&background),filteredFrame=texture(&filtered);
    auto hudless=texture(),reference=texture(),candidate=texture();
    D3D11_TEXTURE2D_DESC stagingDesc={};reference.resource->GetDesc(&stagingDesc);stagingDesc.BindFlags=0;
    stagingDesc.Usage=D3D11_USAGE_STAGING;stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;check(device->CreateTexture2D(&stagingDesc,nullptr,&readback),"readback");
    auto read=[&](const Texture &target){
        context->CopyResource(readback.Get(),target.resource.Get());D3D11_MAPPED_SUBRESOURCE mapped={};
        check(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"readback map");
        std::vector<unsigned char> pixels(width*height*4);
        for(unsigned y=0;y<height;++y)std::memcpy(pixels.data()+y*width*4,static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,width*4);
        context->Unmap(readback.Get(),0);return pixels;
    };
    D3D11_BUFFER_DESC cbDesc={};cbDesc.ByteWidth=256;cbDesc.Usage=D3D11_USAGE_DEFAULT;cbDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants;check(device->CreateBuffer(&cbDesc,nullptr,&constants),"constants");
    D3D11_SAMPLER_DESC sampleDesc={};sampleDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampleDesc.AddressU=sampleDesc.AddressV=sampleDesc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampleDesc.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;check(device->CreateSamplerState(&sampleDesc,&sampler),"sampler");
    context->VSSetShader(vertex.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11_VIEWPORT viewport={0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
    auto *samplerPointer=sampler.Get();context->PSSetSamplers(0,1,&samplerPointer);
    auto *constantsPointer=constants.Get();context->PSSetConstantBuffers(0,1,&constantsPointer);
    unsigned cases=0;
    for(unsigned resolved=0;resolved<2;++resolved)for(unsigned effects=0;effects<3;++effects)for(unsigned display=0;display<3;++display){
        std::array<unsigned,64> values={};auto floating=[&](unsigned index,float value){std::memcpy(&values[index],&value,4);};
        values[1]=effects==2?2:0;values[2]=1;values[3]=37;values[11]=resolved;
        floating(4,effects?.32f:0.f);floating(6,effects?.65f:0.f);floating(17,effects?.82f:0.f);
        floating(8,.39f);floating(9,.58f);floating(13,1);floating(14,.9f);floating(15,.7f);values[16]=effects?3:0;
        const unsigned displayControls=(display==1?730:display==2?1800:1000)|((display==1?1400:display==2?600:1000)<<16u);
        values[60]=displayControls;
        auto draw=[&](Texture &target,bool reuse,bool removeHud){
            ID3D11ShaderResourceView *nulls[19]={};context->PSSetShaderResources(0,19,nulls);
            auto *rtv=target.rtv.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
            if(reuse){std::array<unsigned,64> displayConstants={};displayConstants[0]=displayControls;context->UpdateSubresource(constants.Get(),0,nullptr,displayConstants.data(),0,0);
                ID3D11ShaderResourceView *views[]={sourceFrame.srv.Get(),worldFrame.srv.Get(),hudless.srv.Get()};context->PSSetShaderResources(0,3,views);context->PSSetShader(fast.Get(),nullptr,0);
            }else{values[10]=removeHud?2:0;context->UpdateSubresource(constants.Get(),0,nullptr,values.data(),0,0);
                ID3D11ShaderResourceView *views[]={sourceFrame.srv.Get(),nullptr,worldFrame.srv.Get(),filteredFrame.srv.Get(),nullptr,backgroundFrame.srv.Get()};context->PSSetShaderResources(0,6,views);context->PSSetShader(production.Get(),nullptr,0);}
            context->Draw(3,0);context->OMSetRenderTargets(0,nullptr,nullptr);
        };
        draw(hudless,false,true);draw(reference,false,false);draw(candidate,true,false);
        auto expected=read(reference),actual=read(candidate);unsigned differences=0,maxError=0;
        for(unsigned i=0;i<actual.size();++i)if(expected[i]!=actual[i]){++differences;maxError=std::max(maxError,unsigned(std::abs(int(expected[i])-int(actual[i]))));}
        if(differences)std::cerr<<"resolved="<<resolved<<" effects="<<effects<<" display="<<display<<" differences="<<differences<<" max="<<maxError<<'\n';
        require(differences==0,"FG display reuse must match every final RGBA byte");++cases;
    }
    std::cout<<"PASS: "<<cases<<" production presenter cases, exact RGBA parity with FG scene reuse\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
