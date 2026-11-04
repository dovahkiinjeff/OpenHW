#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "../src/Modern/DustFroxelRootSignature.h"
#include "PresenterShaderSource.h"
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr,const char *message){if(FAILED(hr))throw std::runtime_error(message);}
int main(int argc,char **argv)try{
    if(argc!=2)throw std::runtime_error("production presenter source required");
    auto source=readProductionPresenterShader(argv[1]);
    D3D_SHADER_MACRO macros[]={{"DUST_FROXEL_COMPUTE","1"},{nullptr,nullptr}};
    ComPtr<ID3DBlob> shader,errors;HRESULT result=D3DCompile(source.data(),source.size(),"ProductionDustFroxel",macros,nullptr,
        "BuildDustFroxelCS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&errors);
    if(FAILED(result)&&errors)std::cerr<<static_cast<const char*>(errors->GetBufferPointer());check(result,"production cloud compute compile");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");
    ComPtr<ID3D12InfoQueue> info;device.As(&info);
    ComPtr<ID3D12RootSignature> root;check(hwmodern::createDustFroxelRootSignature(device.Get(),&root),"production compute root");
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc={};desc.pRootSignature=root.Get();desc.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline;check(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&pipeline)),"production cloud compute PSO");
    if(info)for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T bytes=0;info->GetMessage(i,nullptr,&bytes);std::vector<unsigned char> data(bytes);auto *message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        if(SUCCEEDED(info->GetMessage(i,message,&bytes))&&message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<message->pDescription<<'\n';throw std::runtime_error("D3D12 cloud pipeline validation error");}}
    std::cout<<"PASS: production BuildDustFroxelCS and actual compute root support scalar density grid t16\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
