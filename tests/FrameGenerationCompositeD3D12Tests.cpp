#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <climits>
#include <cstring>
#include "../src/Modern/FrameGenerationCompositeShader.h"
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr,const char *message){if(FAILED(hr))throw std::runtime_error(message);}
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
struct PresenterConstants {unsigned displayControls;};
struct TestFrame {ComPtr<ID3D12Resource> frameGenerationHudless;ComPtr<ID3D12DescriptorHeap> frameGenerationCompositeSrvs;};
static struct {
    ComPtr<ID3D12Device> device;TestFrame frames[3];unsigned activeFrameIndex=0;
    ComPtr<ID3D12RootSignature> frameGenerationCompositeRoot;ComPtr<ID3D12PipelineState> frameGenerationCompositePipeline;
    bool frameGenerationCompositeFailed=false;ComPtr<ID3D12Resource> sourceTexture,worldTexture,backBuffers[3];
    ComPtr<ID3D12DescriptorHeap> rtvHeap;ComPtr<ID3D12GraphicsCommandList> commandList;
    UINT rtvDescriptorSize=0,srvDescriptorSize=0,width=16,height=16;
}presenter;
constexpr DXGI_FORMAT presenterFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
static bool compileShader(const char *entry,const char *target,ComPtr<ID3DBlob> &out,const char*,const char*,
    const D3D_SHADER_MACRO *macros,const char *name,const char *source){
    ComPtr<ID3DBlob> errors;HRESULT hr=D3DCompile(source,std::strlen(source),name,macros,nullptr,entry,target,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&out,&errors);
    if(FAILED(hr)&&errors)std::cerr<<static_cast<const char*>(errors->GetBufferPointer());return SUCCEEDED(hr);
}
static void transition(ID3D12GraphicsCommandList *commands,ID3D12Resource *resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
    D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=resource;
    barrier.Transition.StateBefore=before;barrier.Transition.StateAfter=after;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commands->ResourceBarrier(1,&barrier);
}
#include "../src/Modern/FrameGenerationComposite.inc"
int main()try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&presenter.device)),"device");
    ComPtr<ID3D12InfoQueue> info;presenter.device.As(&info);
    presenter.rtvDescriptorSize=presenter.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    presenter.srvDescriptorSize=presenter.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc={};heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heapDesc.NumDescriptors=4;
    check(presenter.device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&presenter.rtvHeap)),"RTVs");
    auto texture=[&](unsigned slot,DXGI_FORMAT format,D3D12_RESOURCE_STATES state){
        D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=presenter.width;desc.Height=presenter.height;
        desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=format;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        ComPtr<ID3D12Resource> result;check(presenter.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&result)),"texture");
        auto rtv=presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=SIZE_T(slot)*presenter.rtvDescriptorSize;
        presenter.device->CreateRenderTargetView(result.Get(),nullptr,rtv);return result;
    };
    presenter.backBuffers[0]=texture(0,presenterFormat,D3D12_RESOURCE_STATE_PRESENT);
    presenter.sourceTexture=texture(1,DXGI_FORMAT_R32G32B32A32_FLOAT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    presenter.worldTexture=texture(2,DXGI_FORMAT_R32G32B32A32_FLOAT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    presenter.frames[0].frameGenerationHudless=texture(3,presenterFormat,D3D12_RESOURCE_STATE_RENDER_TARGET);
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC queueDesc={};check(presenter.device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)),"queue");
    ComPtr<ID3D12CommandAllocator> allocator;check(presenter.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"allocator");
    check(presenter.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&presenter.commandList)),"commands");
    ComPtr<ID3D12Fence> fence;check(presenter.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"fence event");
    auto desc=presenter.backBuffers[0]->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint={};UINT64 bytes=0;
    presenter.device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC readDesc={};readDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    readDesc.Width=bytes;readDesc.Height=readDesc.DepthOrArraySize=readDesc.MipLevels=1;readDesc.SampleDesc.Count=1;readDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> read;check(presenter.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&readDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&read)),"readback");
    for(unsigned scenario=0;scenario<2;++scenario){
        if(scenario){check(allocator->Reset(),"allocator reset");check(presenter.commandList->Reset(allocator.Get(),nullptr),"list reset");
            transition(presenter.commandList.Get(),presenter.sourceTexture.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
            transition(presenter.commandList.Get(),presenter.worldTexture.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
            transition(presenter.commandList.Get(),presenter.frames[0].frameGenerationHudless.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);}
        const float source[]={.5f,.25f,.8f,.75f},world[]={.1f,.3f,.2f,.35f},scene[]={.2f,.4f,.7f,.1f};
        const float *colors[]={nullptr,source,scenario?world:source,scene};
        for(unsigned i=1;i<4;++i){auto rtv=presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=SIZE_T(i)*presenter.rtvDescriptorSize;
            presenter.commandList->ClearRenderTargetView(rtv,colors[i],0,nullptr);}
        transition(presenter.commandList.Get(),presenter.sourceTexture.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(presenter.commandList.Get(),presenter.worldTexture.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(presenter.commandList.Get(),presenter.frames[0].frameGenerationHudless.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        PresenterConstants constants={800u|(1400u<<16u)};require(recordFrameGenerationComposite(0,constants),"production FG composite helper");
        transition(presenter.commandList.Get(),presenter.backBuffers[0].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from={};from.pResource=presenter.backBuffers[0].Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION to={};to.pResource=read.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;
        presenter.commandList->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        transition(presenter.commandList.Get(),presenter.backBuffers[0].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
        check(presenter.commandList->Close(),"close");ID3D12CommandList *commands[]={presenter.commandList.Get()};queue->ExecuteCommandLists(1,commands);
        check(queue->Signal(fence.Get(),scenario+1),"signal");check(fence->SetEventOnCompletion(scenario+1,event),"completion");WaitForSingleObject(event,INFINITE);
        unsigned char *pixels=nullptr;D3D12_RANGE range={0,SIZE_T(bytes)};check(read->Map(0,&range,reinterpret_cast<void**>(&pixels)),"map");
        for(unsigned y=0;y<presenter.height;++y)for(unsigned x=0;x<presenter.width;++x)for(unsigned k=0;k<4;++k){
            const float desired=k==3?source[3]:(scenario?std::pow(source[k]*.8f,1.f/1.4f):scene[k]);
            const int expected=int(std::lround(desired*255));require(std::abs(int(pixels[y*footprint.Footprint.RowPitch+x*4+k])-expected)<=1,"composite pixel mismatch");}
        read->Unmap(0,nullptr);
    }
    if(info)for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T size=0;info->GetMessage(i,nullptr,&size);std::vector<unsigned char> data(size);auto *message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        if(SUCCEEDED(info->GetMessage(i,message,&size))&&message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<message->pDescription<<'\n';throw std::runtime_error("D3D12 debug validation error");}}
    CloseHandle(event);std::cout<<"PASS: actual D3D12 FG composite helper PSO/root/SRV tables, two fenced draws, native UI and exact scene pixels\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
