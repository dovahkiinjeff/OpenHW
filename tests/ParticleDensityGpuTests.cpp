// Headless verification of the actual production HLSL and packed upload data.
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void check(HRESULT hr,const char *message){require(SUCCEEDED(hr),message);}
int main(int argc,char **argv){try{
    require(argc==2,"presenter source path required");
    std::ifstream file(argv[1]);std::stringstream text;text<<file.rdbuf();std::string source=text.str();
    std::string marker="const char presenterShader[] = R\"(";
    auto start=source.find(marker);require(start!=std::string::npos,"presenter shader missing");start+=marker.size();
    auto end=source.find(")\";",start);require(end!=std::string::npos,"shader terminator missing");source=source.substr(start,end-start);
    // Execute the production candidate-selection block with two bricks whose
    // buffer order is opposite their actual ray entry order.
    auto orderStart=source.find("    uint remaining[8];");
    auto orderEnd=source.find("        DustVolume volume = DustVolumes[volumeIndex];",orderStart);
    require(orderStart!=std::string::npos&&orderEnd!=std::string::npos,"production ordering block missing");
    std::string ordering=source.substr(orderStart,orderEnd-orderStart);
    auto replaceAll=[](std::string &s,const std::string &from,const std::string &to){size_t at=0;while((at=s.find(from,at))!=std::string::npos){s.replace(at,from.size(),to);at+=to.size();}};
    replaceAll(ordering,"DustVolumeCount","2u");
    replaceAll(ordering,"DustTileMasks[tileBase+word]","(word==0u?3u:0u)");
    source+="\nRWStructuredBuffer<float> TestOutput : register(u0);\nvoid TestParticleOrder(){float3 rayOrigin=float3(65000,16000,16000),rayDirection=float3(-1,0,0);float sceneDistance=1e8;float transmittance=1.0;\n"+ordering+"TestOutput[134u+candidateIndex]=float(volumeIndex);}}\n";

    auto traversalEnd=source.find("        float volumeDistanceFade =",orderStart);
    require(traversalEnd!=std::string::npos,"production traversal prefix missing");
    std::string optimized=source.substr(orderStart,traversalEnd-orderStart);
    auto referencePath=std::filesystem::path(argv[1]).parent_path().parent_path().parent_path()/"tests/CloudTraversalReference.hlsl";
    std::ifstream referenceFile(referencePath);std::stringstream referenceText;referenceText<<referenceFile.rdbuf();
    std::string reference=referenceText.str();require(!reference.empty(),"baseline traversal fixture missing");
    source+=R"(
DustVolume TestTraversalVolume(uint index,uint scenario){
    DustVolume v=DustVolumes[0];
    v.position=float3(1000.0+float(255u-index)*128.0,16000,16000);
    v.size=float3(96,32000,32000);
    v.color=float3(float(index%7u)/7.0,float(index%11u)/11.0,float(index%13u)/13.0);
    if(scenario==1u && index%3u==0u)v.position.y+=48000;
    if(scenario==2u && index%5u==0u)v.enabled=0u;
    if(scenario==3u && index%2u==0u)v.density=0.0;
    if(scenario==4u){v.position.x=32000;v.size.x=64000;}
    if(scenario>=8u && scenario<=11u){
        v.shape=scenario-8u;
        v.rotationXSinCos=float2(.6,.8);v.rotationYSinCos=float2(-.8,.6);
        v.rotationZSinCos=float2(.8,.6);
        v.size=float3(1000,2000,4000);v.seedPadding=5200;
    }
    if(scenario==12u){v.size.x=128.0+float(index)*16.0;v.position.x=40000-v.size.x*.5;}

    return v;
}
bool TestTraversalInterval(inout uint calls,DustVolume v,DustRotationContext rotation,
    float3 origin,float3 direction,out float entry,out float exit){
    ++calls;return DustRayVolumeInterval(v,rotation,origin,direction,entry,exit);
}
)";
    for(unsigned version=0;version<2;++version){
        std::string body=version?optimized:reference;
        replaceAll(body,"DustVolumeCount","256u");
        replaceAll(body,"DustTileMasks[tileBase+word]","0xffffffffu");
        replaceAll(body,"DustVolumes[index]","TestTraversalVolume(index,scenario)");
        replaceAll(body,"DustVolumes[volumeIndex]","TestTraversalVolume(volumeIndex,scenario)");
        replaceAll(body,"DustRayVolumeInterval(","TestTraversalInterval(calls,");
        source+="\nfloat4 TestTraversal"+std::to_string(version)+R"((uint scenario,out uint calls){
    float3 rayOrigin=float3(65000,16000,16000),rayDirection=float3(-1,0,0);
    float sceneDistance=scenario==5u?32500.0:1e8;
    if(scenario==4u)rayOrigin.x=32000;
    bool froxelAvailable=scenario>=13u;float nearExitDistance=scenario==13u?50000.0:(scenario==14u?1e7:1000.0);
    float transmittance=scenario==6u?.0079:(scenario==7u?.008:1.0);
    float3 accumulated=0;calls=0;
)"+body+R"(
        // Identical deterministic integration isolates the actual production
        // selection, clipping, cutoff and interval reuse from noise/lighting.
        float opacity=scenario==0u?0.999:saturate((endDistance-startDistance)*.0001);
        accumulated+=transmittance*opacity*volume.color;
        transmittance*=1.0-opacity;
        if(startDistance<0 || endDistance<=startDistance)accumulated=-1;
    }
    return float4(accumulated,transmittance);
}
)";
    }

    source+=R"(
[numthreads(64,1,1)] void ParticleDensityTestCS(uint3 id:SV_DispatchThreadID) {
    if(id.x>=128u)return;
    DustVolume v=DustVolumes[0];
    float3 p=float3((float(id.x)+0.37)/128.0,
        (float((id.x*7u)%16u)+0.23)/16.0,
        (float((id.x*11u)%16u)+0.41)/16.0)*32000.0;
    TestOutput[id.x]=ParticleDensityAt(v,p);
    if(id.x==0u)TestOutput[128]=ParticleDensityAt(v,float3(-1,5000,5000));
    if(id.x==1u){v.density=4.0;TestOutput[129]=ParticleDensityAt(v,float3(16000,16000,16000));}
    if(id.x==2u){
        v.rotationXSinCos=v.rotationYSinCos=v.rotationZSinCos=float2(0,1);
        float entry,exit;
        bool hit=DustRayVolumeInterval(v,DustBuildRotationContext(v),float3(-1000,16000,16000),float3(1,0,0),entry,exit);
        TestOutput[130]=hit?entry:-1;TestOutput[131]=hit?exit:-1;
        v.density=17;v.absorption=8;TestOutput[132]=DustMaterialCoefficientScale(v);
        hit=DustRayVolumeInterval(v,DustBuildRotationContext(v),float3(33000,16000,16000),float3(-1,0,0),entry,exit);
        TestOutput[133]=hit?entry:-1;
        TestOutput[136]=DustTargetStepLength(v);
    }
    if(id.x==3u)TestParticleOrder();

}
[numthreads(32,1,1)] void TraversalReferenceCS(uint3 id:SV_DispatchThreadID){
    if(id.x>=16u)return;uint calls;float4 value=TestTraversal0(id.x,calls);uint base=137u+id.x*10u;
    [unroll]for(uint k=0u;k<4u;++k)TestOutput[base+k]=value[k];TestOutput[base+8u]=float(calls);
}
[numthreads(32,1,1)] void TraversalCandidateCS(uint3 id:SV_DispatchThreadID){
    if(id.x>=16u)return;uint calls;float4 value=TestTraversal1(id.x,calls);uint base=137u+id.x*10u;
    [unroll]for(uint k=0u;k<4u;++k)TestOutput[base+4u+k]=value[k];TestOutput[base+9u]=float(calls);
}
)";
    ComPtr<ID3DBlob> shader,errors;
    HRESULT compiled=D3DCompile(source.data(),source.size(),"ProductionParticleDensity",nullptr,nullptr,"ParticleDensityTestCS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&errors);
    if(FAILED(compiled)&&errors)std::cerr<<(const char *)errors->GetBufferPointer();check(compiled,"density shader compile");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"D3D12 device");
    D3D12_ROOT_PARAMETER roots[2]={};roots[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;roots[0].Descriptor.ShaderRegister=9;
    roots[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
    D3D12_ROOT_SIGNATURE_DESC desc={};desc.NumParameters=2;desc.pParameters=roots;
    ComPtr<ID3DBlob> signature;check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors),"root serialize");
    ComPtr<ID3D12RootSignature> root;check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)),"root create");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline={};pipeline.pRootSignature=root.Get();pipeline.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pso;HRESULT pipelineResult=device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&pso));
    if(FAILED(pipelineResult)){ComPtr<ID3D12InfoQueue> messages;if(SUCCEEDED(device.As(&messages)))for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T n=0;messages->GetMessage(i,nullptr,&n);std::vector<unsigned char> bytes(n);auto message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());if(SUCCEEDED(messages->GetMessage(i,message,&n)))std::cerr<<message->pDescription<<'\n';}}
    if(FAILED(pipelineResult)){
        std::cerr<<"Compute PSO HRESULT "<<std::hex<<pipelineResult<<std::dec<<'\n';
        ComPtr<ID3D12ShaderReflection> reflection;
        if(SUCCEEDED(D3DReflect(shader->GetBufferPointer(),shader->GetBufferSize(),IID_PPV_ARGS(&reflection)))){
            D3D12_SHADER_DESC description{};reflection->GetDesc(&description);
            for(UINT i=0;i<description.BoundResources;++i){D3D12_SHADER_INPUT_BIND_DESC resource{};reflection->GetResourceBindingDesc(i,&resource);std::cerr<<resource.Name<<" type="<<resource.Type<<" bind="<<resource.BindPoint<<'\n';}
        }
    }
    check(pipelineResult,"compute pipeline");
    auto traversalPipeline=[&](const char *entry){
        ComPtr<ID3DBlob> code,messages;
        HRESULT hr=D3DCompile(source.data(),source.size(),"CloudTraversal",nullptr,nullptr,entry,"cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&messages);
        if(FAILED(hr)&&messages)std::cerr<<(const char*)messages->GetBufferPointer();check(hr,"traversal shader compile");
        auto desc=pipeline;desc.CS={code->GetBufferPointer(),code->GetBufferSize()};ComPtr<ID3D12PipelineState> result;
        check(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&result)),entry);return result;
    };
    auto referencePso=traversalPipeline("TraversalReferenceCS"),candidatePso=traversalPipeline("TraversalCandidateCS");

    auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE heapType,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags){
        D3D12_HEAP_PROPERTIES heap={};heap.Type=heapType;
        D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;
        ComPtr<ID3D12Resource> r;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"buffer create");return r;
    };
    constexpr size_t stride=144,gridValues=17*17*17;
    auto input=buffer(stride*(256+(gridValues+35)/36),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
    auto output=buffer(297*sizeof(float),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto readback=buffer(297*sizeof(float),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
    void *mapped=nullptr;check(input->Map(0,nullptr,&mapped),"map upload");
    std::memset(mapped,0,(size_t)input->GetDesc().Width);
    float metadata[36]={};unsigned enabled=1,shape=4;std::memcpy(metadata,&enabled,4);std::memcpy(metadata+1,&shape,4);
    metadata[4]=metadata[5]=metadata[6]=16000;metadata[7]=2;metadata[8]=metadata[9]=metadata[10]=32000;metadata[18]=.00012f;
    metadata[25]=metadata[27]=metadata[29]=1;
    std::memcpy(mapped,metadata,sizeof(metadata));
    metadata[4]=48000;std::memcpy((unsigned char *)mapped+stride,metadata,sizeof(metadata));
    std::vector<float> grid(gridValues);
    for(size_t i=0;i<grid.size();i++)grid[i]=(float)(i%17)/16.0f;
    std::memcpy((unsigned char *)mapped+256*stride,grid.data(),grid.size()*sizeof(float));input->Unmap(0,nullptr);
    D3D12_COMMAND_QUEUE_DESC qd={};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");
    ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"allocator");
    ComPtr<ID3D12GraphicsCommandList> command;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),pso.Get(),IID_PPV_ARGS(&command)),"command list");
    command->SetComputeRootSignature(root.Get());command->SetComputeRootShaderResourceView(0,input->GetGPUVirtualAddress());command->SetComputeRootUnorderedAccessView(1,output->GetGPUVirtualAddress());command->Dispatch(2,1,1);
    D3D12_RESOURCE_BARRIER uav={};uav.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;uav.UAV.pResource=output.Get();
    command->ResourceBarrier(1,&uav);command->SetPipelineState(referencePso.Get());command->Dispatch(1,1,1);
    command->ResourceBarrier(1,&uav);command->SetPipelineState(candidatePso.Get());command->Dispatch(1,1,1);

    D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=output.Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command->ResourceBarrier(1,&barrier);command->CopyBufferRegion(readback.Get(),0,output.Get(),0,297*sizeof(float));check(command->Close(),"command close");
    ID3D12CommandList *lists[]={command.Get()};queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");check(queue->Signal(fence.Get(),1),"signal");
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");check(fence->SetEventOnCompletion(1,event),"fence event");require(WaitForSingleObject(event,30000)==WAIT_OBJECT_0,"GPU completion");CloseHandle(event);
    check(readback->Map(0,nullptr,&mapped),"map result");auto values=(const float *)mapped;
    for(unsigned i=0;i<128;i++){float expected=2*(i+.37f)/128;require(std::isfinite(values[i])&&std::abs(values[i]-expected)<.00003f,"GPU trilinear packing mismatch");}
    require(values[128]==0,"density leaked outside brick");require(std::abs(values[129]-2)<.00003f,"GPU density did not scale linearly");
    require(std::abs(values[130]-1000)<.01f&&std::abs(values[131]-33000)<.01f&&std::abs(values[133]-1000)<.01f,"particle brick support changed with ray direction");
    require(values[132]==1,"brick normalization peak amplified material opacity");
    require(values[134]==1&&values[135]==0,"bricks composited in buffer order instead of ray entry order");
    require(std::abs(values[136]-500)<.001f,"particle sampling cadence does not resolve its grid");
    for(unsigned scenario=0;scenario<16;++scenario){unsigned base=137+scenario*10;
        require(std::memcmp(values+base,values+base+4,4*sizeof(float))==0,"cloud traversal changed composited RGBA");
        require(values[base+9]<=values[base+8],"cloud traversal added interval tests");
        std::cout<<"Traversal scenario "<<scenario<<": "<<values[base+8]<<" -> "<<values[base+9]<<" interval tests; RGBA bit-identical.\n";
    }
    require(values[146]<values[145]/100,"opaque prefix did not eliminate remaining traversal");
    readback->Unmap(0,nullptr);
    std::cout<<"Production GPU checks passed: density sampling, exact brick support, peak-independent extinction, ray-entry ordering and grid sampling cadence.\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
